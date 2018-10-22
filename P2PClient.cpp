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
#include "TCPClient.h"

using namespace std;
typedef unordered_map<int, string> FILE_IPADDR_MAP;

string p2pserver_address;
FILE_IPADDR_MAP file_map;

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
            break;
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
            break;
        case 3: //Download a file
            query = "3 " + input;
            break;
        case 4: // Upload a file
            query = "";
            break;
        default:
            query = "";
    }

    return input;

}

int handleDownloadFromPeer(mutex mutx, string filename){
    int chunkid;
    string ipaddr;
    TCPClient peerClient;

    mutx.lock();
    auto random_pair = next(begin(file_map), rand_between(0, file_map.size()));
    chunkid = random_pair.first;
    ipaddr = random_pair.second;
    file_map.erase(chunkid);
    mutx.unlock();

    if(connectToClient(peerClient, ipaddr) == -1){
        //TODO: Add back pair
    }

    peerClient.send_data(to_string(filename + " " + chunkid));


}

int downloadFileFromPeers(string filename){

    int count, i;
    mutex mutx;

    while(!file_map.empty()){
        thread thread_obj1(handleDownloadFromPeer, mutx, filename);
        thread thread_obj2(handleDownloadFromPeer, mutx, filename);
        thread thread_obj3(handleDownloadFromPeer, mutx, filename);
        thread thread_obj4(handleDownloadFromPeer, mutx, filename);
        thread thread_obj5(handleDownloadFromPeer, mutx, filename);

        thread_obj1.join();
        thread_obj2.join();
        thread_obj3.join();
        thread_obj4.join();
        thread_obj5.join();
    }


}

/**************** COMMANDS  ***********************
*/

int listFiles()
{

}

int searchFile()
{

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

    query = generate_query(3, filename);
    server_connection.send_data(query);
    reply = server_connection.read();
    server_connection.close();

    if(reply[0] == '0' || reply[0] == ''){
        cout << "\nFile is not available. Please choose another file to download." << endl;
        return -1;
    } else {
        int filesize_pos = find_Nth_occurence(" ", 2, reply);
        int numofchunks_pos = find_Nth_occurence(" ", 3, reply);
        int chunks_pos = find_Nth_occurence(" ", 4, reply);

        filename = reply.substr(2, filesize_pos - 2);
        filesize = stoi(reply.substr(filesize_pos + 1, numofchunks_pos - (filesize_pos + 1)));
        num_of_chunks = stoi(reply.substr(numofchunks_pos + 1, chunks_pos - (numofchunks_pos + 1)));
        chunkdetails = reply.substr(chunks_pos + 1);

        j = 0;
        count  = 1;
        k = find_Nth_occurence(" ", count, chunkdetails) + 1;
        l = find_Nth_occurence(" ", count + 1, chunkdetails);
        file_map.clear();

        for(i = 0; i < num_of_chunks; i++){
            chunkid = stoi(chunkdetails.substr(j, k - j - 1));
            ipaddr = chunkdetails.substr(k, l - k);
            file_map.insert(make_pair<int,string>(chunkid, ipaddr));
            if(i != (num_of_chunks - 1)) {
                j = l + 1;
                count += 2;
                k = find_Nth_occurence(" ", count, chunkdetails) + 1;
                l = find_Nth_occurence(" ", count + 1, chunkdetails);
            }
        }
    }

    downloadFileFromPeers(filename);
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
