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

// Uncomment in Linux for child handling
// #include <sys/prctl.h>
// #include <fcntl.h>
#include "TCPClient.h"
#include <mutex>
#include <math.h>
#include <sstream>
#include <sys/stat.h>



using namespace std;
typedef unordered_map<int, string> FILE_IPADDR_MAP;
const int DEFAULT_CHUNK_SIZE = 1024;

string p2pserver_address;
FILE_IPADDR_MAP file_map, file_map_failed, file_map_successful;
mutex mutx, mutx_for_failed, mutx_for_successful;

/******************************************************************************
***************************** Utilities  **************************************
******************************************************************************/

void displayOptions(){
    cout << "\n*-*-*-*-*-*-*-*-*-* OPTIONS *-*-*-*-*-*-*-*-*-*-*-*" << endl;
    cout << "1. List available files" << endl;
    cout << "2. Query for a specific file" << endl;
    cout << "3. Download a specific file from server" << endl;
    cout << "4. Upload file to P2P server" << endl;
    cout << "5. Exit" << endl;

    cout << "Enter your option: ";
}

string generate_query(int op, string input)
{
    string query;
    string returnchar = "\r\n";

    switch(op)
    {
        case 1: //
            break;
        case 2: //Query a file
			query = "2 " + input + returnchar;
            break;
        case 3: //Download a file
            query = "3 " + input + returnchar;
            break;
        case 4: // Upload a file
            query = "4 " + input + returnchar;
            break;
        case 5: //exit from p2p
            query = "5 " + input + returnchar;
            break;
        case 6: //Update server on availble chunks
            query = "6 " + input + returnchar;
        case 7: //Get chunk updates from server
            query = "7 " + input + returnchar;
        default:
            query = "" + returnchar;
    }

    return query;

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

    query = generate_query(7, filename + temp);
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

    int sock, count = 0;
    string query, temp;
    TCPClient server_connection;

    while((sock = server_connection.connectTo(p2pserver_address, PORT)) == -1){
        count++;
        if(count >= 5){
            return;
        }
    }

    for (pair<int, string> element : file_map_successful)
    {
    	temp += " " + to_string(element.first) + " " + element.second;
        mutx_for_successful.lock();
        file_map_successful.erase(element.first);
        mutx_for_successful.unlock();
    }

    query = generate_query(6, filename + temp);
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

    peerClient.send_data(filename + " " + to_string(chunkid));
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
    return 1;
}


void processDownloadFromClient(int sock, string clientAddr){

    int bytesRecved, chunkid;
    char buffer[SIZE];
    string filename, response;

    while (1){
        if ((bytesRecved = recv(sock, buffer, SIZE, 0)) <= 0){
            break;
        }

        buffer[bytesRecved] = '\0';
        // cout <<"Thread connecting to " << clientAddr << " has received " << buffer << endl;
        string msg(buffer);
        regex ws_re("\\s+");
        vector<string> result{
            sregex_token_iterator(msg.begin(), msg.end(), ws_re, -1), {}
        };

        filename = result[0];
        chunkid = stoi(result[1]);

        FILE *file = fopen((filename + "/" + to_string(chunkid)).c_str(), "rb");

        if(file){
            response = "0 \n";
            send(sock, response.c_str(), response.length(), 0);
            return;
        }

        fseek(file, 0, SEEK_END);
        unsigned long filesize = ftell(file);
        char *file_buffer = (char*)malloc(sizeof(char)*filesize);
        rewind(file);
        // store read data into buffer
        fread(file_buffer, sizeof(char), filesize, file);
        // send buffer to client
        send(sock, buffer, filesize, 0);
    }

    close(sock);

}

/*****************************************************************************
***************************** COMMANDS  **************************************
******************************************************************************/

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

    return 0;
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

    query = generate_query(2, filename);
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

    query = generate_query(3, filename);
    server_connection.send_data(query);
    reply = server_connection.read();
    server_connection.exit();

    if(reply[0] == '0'){
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
        file_map_failed.clear();
        file_map_successful.clear();

        for(i = 0; i < num_of_chunks; i++){
            chunkid = stoi(result[i + 4]);
            ipaddr = result[i + 5];
            file_map.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        }
    }

    downloadFileFromPeers(filename, num_of_chunks);
}

int getFileSize(ifstream *file) {

    file->seekg(0, ios::end);

    int filesize = file->tellg();

    file->seekg(ios::beg);

    return filesize;

}

// template to convert Number to string
template<typename T> string NumberToString(T Number) {
    stringstream ss;
    ss << Number;
    return ss.str();
}


int uploadFile(){
    // Main process
    // 1. Update tracker
    // 2. Spilt file chunks

    // get filename
    // get file size
    // spilt into chunks (num of chunks) store into array
    // inform the tracker in this format - 4. filename size chunkNum

    string filename, folder, query, reply;
    int fileSize, num_of_chunks;
    TCPClient server_connection;    

    cout << "\nEnter file to upload: ";
    cin >> filename;

    ifstream inputStream;
    inputStream.open(filename, ios::in | ios::binary); //open the file

    //check for valid file
    if ( !inputStream.is_open() ) {                 
        cout << "\nFile not found. Please choose another file to upload." << endl;
        return -1;
    }

    fileSize = getFileSize(&inputStream); //get size of the file 

    //cout << "Size " << fileSize << endl;

    num_of_chunks = ceil(fileSize / DEFAULT_CHUNK_SIZE);

    string filepath =  filename + "_chunks";

    // create a directory based on the filename for file chunks
    const int dir_err = mkdir(filepath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    cout << "Value " << dir_err << endl;

    if(dir_err != -1){
        cout << "Folder created!" << endl;
    }

    // ================== QUERY ========================
    if(connectToServer(server_connection) == -1){
        exit(1);
    }

    query = generate_query(4, filename + " " + to_string(fileSize) + " " + to_string(num_of_chunks) + "\n");
    server_connection.send_data(query);
    reply = server_connection.read();
    server_connection.exit();
    // ================== QUERY ========================

    if (inputStream.is_open()) {
        ofstream output;
        int counter = 1;

        string fullChunkName;

        // Create a buffer to hold each chunk
        char *buffer = new char[DEFAULT_CHUNK_SIZE];

        // Keep reading until end of file
        while (!inputStream.eof()) {

            // Build the chunk file name. Usually drive:\\filepath\\N
            // N represents the Nth chunk
            // Eg: \\hello.c_chunks\\4

            // Convert counter integer into string and append to name.
            string counterString = NumberToString(counter); 

            fullChunkName.clear();
            fullChunkName.append(filepath+"/");
            fullChunkName.append(counterString);

            // Open new chunk file name for output
            output.open(fullChunkName.c_str(),ios::out | ios::trunc | ios::binary);

            // If chunk file opened successfully, read from input and
            // write to output chunk. Then close.
            if (output.is_open()) {

                inputStream.read(buffer, DEFAULT_CHUNK_SIZE);

                // gcount() returns number of bytes read from stream.
                output.write(buffer, inputStream.gcount());

                output.close();

                counter++;
            }

        } 
        // Cleanup buffer
        delete (buffer);

        // Close input file stream.
        inputStream.close();

        cout << "Chunking complete! " << counter - 1 << " files created." << endl;
    } else {
            cout << "Error opening file!" << endl;
    }
    return 0;
}

int quit(){

}



int main()
{
    pid_t pid;
    int op, mySock, cnxnSock;
    string str;
    struct sockaddr_in myAddress;
    struct sockaddr_in clientAddress;

    cout << "Enter IP address of P2P server: ";
    getline(cin, p2pserver_address);

    pid = fork();

    if(pid == 0){
        // Uncomment for child handling (only on linux)
        // prctl(PR_SET_PDEATHSIG, SIGKILL);

        mySock = socket(AF_INET, SOCK_STREAM, 0);
     	memset(&myAddress,0,sizeof(myAddress));
    	myAddress.sin_family = AF_INET;
    	myAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    	myAddress.sin_port = htons(PORT);
    	bind(mySock,(struct sockaddr *)&myAddress, sizeof(myAddress));

        listen(mySock,1);

        socklen_t sosize  = sizeof(clientAddress);

        while (1){
            cnxnSock = accept(mySock, (struct sockaddr*)&clientAddress, &sosize);
            // cout << "connected: " << inet_ntoa(clientAddress.sin_addr) << endl;
            thread slave(processDownloadFromClient, cnxnSock, inet_ntoa(clientAddress.sin_addr));
            slave.detach();
        }

        close(mySock);
        return 0;

    } else {
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

    quit();

}
