#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <unordered_map>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netdb.h>
#include <vector>
#include <thread>
#include <regex>
#include "TCPClient.h"

using namespace std;
typedef unordered_map<int, string> FILE_IPADDR_MAP;

string p2pserver_address;
FILE_IPADDR_MAP file_map, file_map_failed, file_map_successful;
mutex mutx, mutx_for_failed, mutx_for_successful;

void displayOptions(){
    cout << "\n*-*-*-*-*-*-*-*-*-* OPTIONS *-*-*-*-*-*-*-*-*-*-*-*" << endl;
    cout << "1. List available files" << endl;
    cout << "2. Query for a specific file" << endl;
    cout << "3. Download a specific file from server" << endl;
    cout << "4. Upload file to P2P server" << endl;
    cout << "5. Exit" << endl;

    cout << "Enter your option: ";
}

/************** Utilities ********************* */


/// Returns the position of the 'Nth' occurrence of 'find' within 'str'.
/// Note that 1==find_Nth( "aaa", 2, "aa" ) [2nd "aa"]
/// - http://stackoverflow.com/questions/17023302/
// test case: assert(  3 == find_Nth( "My gorila ate the banana", 1 , "gorila") );

size_t find_Nth_occurence(const string &str, unsigned N, const string &find)
{
    if ( 0 == N )
    {
        return string::npos;
    }

    size_t pos, from = 0;
    unsigned i = 0;

    while ( i < N )
    {
        pos=str.find(find,from);
        if ( string::npos == pos )
        {
            break;
        }

        from = pos + 1; // from = pos + find.size();
        ++i;
    }

    return pos;
}

int connectToServer(TCPClient tcp_client)
{
    int sock = -1;
    int count = 0;
    string response = "y";
    do{
        sock = tcp_client.connectTo(p2pserver_address, PORT);
        count++;

        if (count == 5) {
            cout << "\nUnable to connectTo to P2P Server. Do you want to try again? [y/n]";
            getline(cin, response);
            count = 0;
        }

    } while(sock == -1 && response.compare("y") == 0);

    return sock;

}


int connectToClient(TCPClient tcp_client, string ip_addr)
{
    int sock = -1;
    int count = 0;
    do{
        sock = tcp_client.connectTo(ip_addr, PORT);
        count++;

        if (count == 5) {
            cout << "\nUnable to connectTo to " << ip_addr << " client.";
            count = 0;
            return -1;
        }

    } while(sock == -1);

    //TODO: handle peer client connection error

    return sock;

}

string generate_query(int op, string input)
{
    string query;

    switch(op)
    {
        case 1: //
            break;
        case 2: //Query a file
			query = "2 " + input;
            break;
        case 3: //Download a file
            query = "3 " + input;
            break;
        case 4: // Upload a file
            query = "";
            break;
        case 7:
            query = "7 " + input;
        default:
            query = "";
    }

    return query;

}

int getUpdateFromServer(string filename){

    int chunkid;
    string query, reply, temp;
    TCPClient server_connection;

    if(connectToServer(server_connection) == -1){
        return -1;
    }

    for (pair<int, string> element : file_map_failed)
    {
    	temp += " " + to_string(element.first) + " " + element.second;
        mutx_for_failed.lock();
        file_map_failed.erase(element.first);
        mutx_for_failed.unlock();
    }

    query = generate_query(7, filename + temp + "\n");
    server_connection.send_data(query);
    reply = server_connection.read();
    server_connection.exit();

    regex ws_re("\\s+");
    vector<string> result{
        sregex_token_iterator(reply.begin(), reply.end(), ws_re, -1), {}
    };

    if(result[0] == "0"){
        cout << "File is unavailable for download." << endl;
        exit(1);
    }

    for (int i = 1; i < (result.size() - 1); i += 2){
        mutx.lock();
        file_map.insert((pair<int,string>) make_pair(stoi(result[i]), result[i + 1]));
        mutx.unlock();
    }

    return 0;
}

void updateServerOnAvailableChunks(string filename){

    string query, temp;
    TCPClient server_connection;

    if(connectToServer(server_connection) == -1){
        return;
    }

    for (pair<int, string> element : file_map_successful)
    {
    	temp += " " + to_string(element.first) + " " + element.second;
        mutx_for_successful.lock();
        file_map_successful.erase(element.first);
        mutx_for_successful.unlock();
    }

    query = generate_query(6, filename + temp + "\n");
    server_connection.send_data(query);
    server_connection.exit();

}

void handleDownloadFromPeer(string filename){
    int chunkid, status;
    string ipaddr;
    TCPClient peerClient;
    string reply;

    mutx.lock();
    if(file_map.empty()){
        mutx.unlock();
        return;
    }
    auto random_pair = next(begin(file_map), (rand() % file_map.size()));
    chunkid = random_pair->first;
    ipaddr = random_pair->second;
    file_map.erase(chunkid);
    mutx.unlock();

    if(connectToClient(peerClient, ipaddr) == -1){
        //Add pair back into filemap
        mutx.lock();
        file_map.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        mutx.unlock();
    }

    peerClient.send_data(filename + " " + to_string(chunkid) + "\n");
    reply = peerClient.read();

    if(reply[0] == '0') {
        mutx_for_failed.lock();
        file_map_failed.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        mutx_for_failed.unlock();
    } else if (peerClient.receiveAndWriteToFile(filename + "/" + to_string(chunkid)) < 0){
        mutx_for_failed.lock();
        file_map_failed.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        mutx_for_failed.unlock();
    }
    peerClient.exit();

    mutx_for_successful.lock();
    file_map_successful.insert((pair<int,string>) make_pair(chunkid, ipaddr));
    mutx_for_successful.unlock();

    return;

}


int mergeAllFiles(string filename, int num_of_chunks){

    int i;
    ifstream read;
    ofstream finalout;
    string temp;

    finalout.open(filename);

    if(!finalout)
    {
        cout << "Oops something went wrong with the file." << endl;
    }

    for(i = 0; i < num_of_chunks; i++)
    {
        try {
          read.open(filename + "/" + to_string(i));
        }
        catch (ios_base::failure& e) {
          cerr << e.what() << '\n';
        }
        while(read.eof() == 0)
        {
            getline(read, temp);
            finalout << temp;
        }
        finalout << "\n";
        read.close();
        read.clear();
    }

    finalout.close();
    return 0;
}

int downloadFileFromPeers(string filename, int num_of_chunks){

    int count = 0;
    mutex mutx;

    while(!file_map.empty() && !file_map_failed.empty()){
        if(count == 2 || file_map.empty()){
            getUpdateFromServer(filename);
            count = 0;
            if(!file_map_successful.empty()){
                updateServerOnAvailableChunks(filename);
            }
        }

        thread thread_obj1(handleDownloadFromPeer, filename);
        thread thread_obj2(handleDownloadFromPeer, filename);
        thread thread_obj3(handleDownloadFromPeer, filename);
        thread thread_obj4(handleDownloadFromPeer, filename);
        thread thread_obj5(handleDownloadFromPeer, filename);

        thread_obj1.join();
        thread_obj2.join();
        thread_obj3.join();
        thread_obj4.join();
        thread_obj5.join();

        count++;
    }

    updateServerOnAvailableChunks(filename);
    mergeAllFiles(filename, num_of_chunks);

    cout << "Download successful!" << endl;
}

/**************** COMMANDS  ***********************
*/

int listFiles()
{
 int i, filesize;
    string filename, query, reply;
    TCPClient server_connection;

    if(connectToServer(server_connection) == -1){
        exit(1);
    }

    query = "1 \n";
    server_connection.send_data(query);
    reply = server_connection.read();
    server_connection.exit();

    cout << reply << endl;
}

int searchFile()
{
    int i, filesize, num_of_chunks;
    string filename, reply, query;
    TCPClient server_connection;

    cout << "Enter file to search: ";
    cin.get();
    getline(cin, filename);
    cout << "Confirmation of file that will be queried: " << filename << endl;

    if(connectToServer(server_connection) == -1){
        exit(1);
    }

    query = generate_query(2, filename + "\n");
    server_connection.send_data(query);
    reply = server_connection.read();
    server_connection.exit();

    if(reply[0] == '0'){
        cout << "\nFile does not exist. Please choose another file to search." << endl;
        return -1;
    } else {
    	cout << reply << endl;
	}
}

int downloadFile()
{
    int i, j, k, l, server_sock, num_of_chunks, filesize, chunkid, count;
    string filename, query, reply, chunkdetails, ipaddr;
    TCPClient server_connection;

    cout << "\nEnter file to download: ";
    getline(cin, filename);

    if(connectToServer(server_connection) == -1){
        exit(1);
    }

    query = generate_query(3, filename + "\n");
    server_connection.send_data(query);
    reply = server_connection.read();
    server_connection.exit();

    if(reply[0] == '0' || reply[0] == NULL){
        cout << "\nFile is not available. Please choose another file to download." << endl;
        return -1;
    } else {
        regex ws_re("\\s+");
        vector<string> result{
            sregex_token_iterator(reply.begin(), reply.end(), ws_re, -1), {}
        };

        filename = result[1];
        filesize = stoi(result[2]);
        num_of_chunks = stoi(result[3]);

        file_map.clear();

        for(i = 0; i < num_of_chunks; i++){
            chunkid = stoi(result[i + 4]);
            ipaddr = result[i + 5];
            file_map.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        }
    }

    downloadFileFromPeers(filename, num_of_chunks);
}

int uploadFile(){

}

int main()
{
    int op;
    cout << "Enter IP address of P2P server: ";
    getline(cin, p2pserver_address);

    do{
        displayOptions();
        cin >> op;
        switch(op){
            case 1:
                listFiles();
                break;
            case 2:
                searchFile();
                break;
            case 3:
                downloadFile();
                break;
            case 4:
                uploadFile();
                break;
            default:
                cout << "Invalid option. Please try again." << endl;
        }

    } while (op != 5);

}
