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
#include <mutex>
#include <signal.h>

// Uncomment in Linux for child handling
// #include <sys/prctl.h>
// #include <fcntl.h>
#include "TCPClient.h"
#include <mutex>
#include <math.h>
#include <sstream>
#include <sys/stat.h>

#define PORT 15000
#definr TUNRPORT 15001

using namespace std;
typedef unordered_map<int, string> FILE_IPADDR_MAP;
const int DEFAULT_CHUNK_SIZE = 1024;

string p2pserver_address, turnserver_address, my_public_ipaddr;
FILE_IPADDR_MAP file_map, file_map_failed, file_map_successful;
mutex mutx, mutx_for_failed, mutx_for_successful;
TCPClient persistentClient;

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
        case 6: //Update server on available chunks
            query = "6 " + input + returnchar;
        case 7: //Get chunk updates from server
            query = "7 " + input + returnchar;
        default:
            query = "" + returnchar;
    }

    return query;

}

int connectToServer(TCPClient &tcp_client)
{
    int sock = -1;
    int count = 0;
    string response = "y";
    do{
        tcp_client = *(new TCPClient());
        sock = tcp_client.connectTo(p2pserver_address, PORT);
        count++;

        if (count == 5) {
            cout << "\nUnable to connectTo to P2P Server. Do you want to try again? [y/n]";
            cin >> response;
            count = 0;
        }

    } while(sock == -1 && response.compare("y") == 0);

    return sock;

}


int connectToTURN(TCPClient &tcp_client)
{
    int sock = -1;
    int count = 0;
    string response = "y";
    do{
        tcp_client = *(new TCPClient());
        sock = tcp_client.connectTo(turnserver_address, TURNPORT);
        count++;

        if (count == 5) {
            cout << "\nUnable to connectTo to TURN Server. Do you want to try again? [y/n]";
            cin >> response;
            count = 0;
        }

    } while(sock == -1 && response.compare("y") == 0);

    return sock;

}


int connectToClient(string peer_ip_addr)
{
    TCPClient turn_connection;
    string request, reply;
    int sock, i;

    if((sock = connectToTURN(turn_connection)) < 0) {
        return -1;
    }

    request = "2 " + my_public_ipaddr + " " + peer_ip_addr + "\r\n";
    turn_connection.send_data(request);
    reply = turn_connection.read();
    for(i = 0; i < 2; i++){
        if(reply == "1"){
            cout << "Access granted by TURN to " << ip_addr << endl;
            turn_connection.exit();
            return sock;
        } else {
            sleep(rand() % 5);
            continue;
        }
    }
    turn_connection.exit();
    return  -1;

}


int getUpdateFromServer(string filename){

    int chunkid;
    string query, reply, temp;
    TCPClient server_connection;

    if(connectToServer(server_connection) == -1){
        return -1;
    }

    if(file_map_failed.empty()){
        return 1;
    }

    for (pair<int, string> element : file_map_failed)
    {
    	temp += " " + to_string(element.first) + " " + element.second;
        mutx_for_failed.lock();
        file_map_failed.erase(element.first);
        mutx_for_failed.unlock();
    }

    query = generate_query(7, filename + temp);
    cout << query;
    server_connection.send_data(query);
    reply = server_connection.read();
    server_connection.exit();

    regex ws_re("\\s+");
    vector<string> result{
        sregex_token_iterator(reply.begin(), reply.end(), ws_re, -1), {}
    };

    if(result[0] == "0"){
        cout << "File is unavailable for download. Server" << endl;
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



int getFileSize(ifstream *file) {
    file->seekg(0,ios::end);
    int filesize = file->tellg();
    file->seekg(ios::beg);
    return filesize;
}


void joinFile(string filename) {
    string fileName;

    // Create our output file
    ofstream outputfile;
    outputfile.open(filename.c_str(), ios::out | ios::binary);

    // If successful, loop through chunks matching chunkName
    if (outputfile.is_open()) {
        bool filefound = true;
        int counter = 1;
        int fileSize = 0;

        while (filefound) {

            filefound = false;
            // Build the filename
            fileName.clear();
            fileName.append(filename + "_chunks/");
            fileName.append(to_string(counter));

            // Open chunk to read
            ifstream fileInput;
            fileInput.open(fileName.c_str(), ios::in | ios::binary);
            // If chunk opened successfully, read it and write it to
            // output file.
            if (fileInput.is_open()) {
                filefound = true;
                fileSize = getFileSize(&fileInput);
                char *inputBuffer = new char[fileSize];
                fileInput.read(inputBuffer,fileSize);
                outputfile.write(inputBuffer,fileSize);
                delete(inputBuffer);
                fileInput.close();
            }
            counter++;
        }
        // Close output file.
        outputfile.close();

        // cout << "File assembly complete!" << endl;
    }

    else { cout << "Error: Unable to open file for output." << endl; }

}

void downloadChunkFromPeer(string filename){
    int chunkid, status;
    string ipaddr;
    string reply;
    string filepath = filename + "_chunks";

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

    if(connectToClient(ipaddr) == -1){
        //Add pair back into filemap
        mutx.lock();
        file_map.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        mutx.unlock();
        return;
    }

    persistentClient.send_data("1\r\n");
    cout << "Sending : " + filename + " " + to_string(chunkid) << endl;
    persistentClient.send_data(filename + " " + to_string(chunkid));
    reply = persistentClient.read();

    if(reply[0] == '0') {
        cout << "RESPONSE: 0" << endl;
        mutx_for_failed.lock();
        file_map_failed.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        mutx_for_failed.unlock();
        return;
    } else {

        cout << "RESPONSE: 1" << endl;
        reply = persistentClient.read();
        cout << "FILE SIZE: " + reply << endl;
        int filesize = stoi(reply);

        if (persistentClient.receiveAndWriteToFile(filepath + "/" + to_string(chunkid), filesize) < 0) {
            mutx_for_failed.lock();
            file_map_failed.insert((pair<int,string>) make_pair(chunkid, ipaddr));
            mutx_for_failed.unlock();
        }
        return;
    }
    mutx_for_successful.lock();
    file_map_successful.insert((pair<int,string>) make_pair(chunkid, ipaddr));
    mutx_for_successful.unlock();

    cout << "EXITED" << endl;

    return;

}

int downloadFromPeers(string filename, int num_of_chunks){

    int count = 0;
    mutex mutx;

    string filepath = filename + "_chunks";
    const int dir_err = mkdir(filepath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(dir_err != -1){
        cout << "Folder created!" << endl;
    }

    while(!file_map.empty() || !file_map_failed.empty()){
        if(count == 5 || file_map.empty()){
            getUpdateFromServer(filename);
            count = 0;
            if(!file_map_successful.empty()){
                updateServerOnAvailableChunks(filename);
            }
        }

        thread thread_obj1(downloadChunkFromPeer, filename);
        // thread thread_obj2(downloadChunkFromPeer, filename);
        // thread thread_obj3(downloadChunkFromPeer, filename);
        // thread thread_obj4(downloadChunkFromPeer, filename);
        // thread thread_obj5(downloadChunkFromPeer, filename);

        thread_obj1.join();
        // thread_obj2.join();
        // thread_obj3.join();
        // thread_obj4.join();
        // thread_obj5.join();

        count++;
    }

    updateServerOnAvailableChunks(filename);
    joinFile(filename);

    cout << "Download successful!" << endl;
    return 1;
}


bool senddata(int sock, void *buf, int buflen)
{
    unsigned char *pbuf = (unsigned char *) buf;

    while (buflen > 0)
    {
        int num = send(sock, pbuf, buflen, 0);
        if (num == -1)
        {
            return false;
        }

        pbuf += num;
        buflen -= num;
    }


    return true;
}


bool sendfile(int sock, FILE *f)
{

    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);
    if (filesize == EOF)
        return false;
    if (filesize > 0)
    {
        char buffer[DEFAULT_CHUNK_SIZE];
        do
        {
            size_t num = DEFAULT_CHUNK_SIZE;
            num = fread(buffer, 1, num, f);
            if (num < 1)
                return false;
            if (!senddata(sock, buffer, num))
                return false;
            filesize -= num;
        }
        while (filesize > 0);
    }
    return true;
}


unsigned int fsizeof_full(FILE *fp) {
    unsigned int size;
    while(getc(fp) != EOF)
        ++size;
    return size;
}


void handleDownloadRequestFromPeer(int sock){

    int bytesRecved;
    char buffer[DEFAULT_CHUNK_SIZE];

    while (1){
        if ((bytesRecved = recv(sock, buffer, DEFAULT_CHUNK_SIZE, 0)) <= 0){
            break;
        }
        buffer[bytesRecved] = '\0';
        cout << "Incoming client connecting to me" << end;
        cout << "Received :" << buffer << endl;
        string message(buffer);
        string response;

        regex ws_re("\\s+");
        vector<string> result{
            sregex_token_iterator(message.begin(), message.end(), ws_re, -1), {}
        };

        FILE *filehandle = fopen((result[0] + "_chunks/" + result[1]).c_str(), "rb");
        if (filehandle != NULL)
        {
            cout << "RESPONSE: 1" << endl;
            response = "1\r\n";
            send(sock, response.c_str(), response.length(), 0);


            response = to_string(fsizeof_full(filehandle)) + "\n";
            cout << "SEND FILESIZE: " << response << endl;
            send(sock, response.c_str(), response.length(), 0);

            sendfile(sock, filehandle);
            cout << "FILESENT" << endl;
		    sleep(2);
            fclose(filehandle);
            break;
        } else {
            cout << "RESPONSE: 0" << endl;
            response = "0\r\n";
            send(sock, response.c_str(), response.length(), 0);
            fclose(filehandle);
            break;
        }
    }
    cout << "FILE CLOSE" << endl;
    close(sock);
    cout << "SOCK CLOSE" << endl;
	return;
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
    reply = server_connection.readAllFiles(1024);
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
    cin >> filename;

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

        for(i = 0; i < (num_of_chunks*2); i+=2){
            chunkid = stoi(result[i + 4]);
            ipaddr = result[i + 5];
            file_map.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        }
    }

    downloadFromPeers(filename, num_of_chunks);
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
            fullChunkName.append(filepath + "/");
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

    TCPClient server_connection;
    string query, reply;

    // ================== QUERY ========================
    if(connectToServer(server_connection) == -1){
        exit(1);
    }

    //command: 5 ip_address
    query = generate_query(5, "");
    server_connection.send_data(query);
    reply = server_connection.read();
    server_connection.exit();
    // ================== QUERY ========================

    cout <<  " Exit Sucessfully. Bye." << endl;
    exit(0);
}

void exitHandler(int signum){
    if(signum == SIGINT){
        cout <<  " Program closed abruptedly." << endl;
        quit();
    }
}



int main()
{
    pid_t pid;
    int op, cnxnSock;
    string str, request, reply;
    struct sockaddr_in myAddress;
    struct sockaddr_in clientAddress;

    cout << "Enter IP address of P2P server: ";
    getline(cin, p2pserver_address);

    cout << "Enter IP address of TURN server: ";
    getline(cin, turnserver_address);

    if(connectToTURN(persistentClient) == -1){
        exit(1);
    }

    request = "1\r\n";
    cnxnSock = persistentClient.send_data(request);
    my_public_ipaddr = persistentClient.read();

    //terminating with ctrl-c
    if (signal(SIGINT, exitHandler) == SIG_ERR){
        cout <<  "Failed to register handler" << endl;
    }

    pid = fork();

    if(pid == 0){
        // Uncomment for child handling (only on linux)
        // prctl(PR_SET_PDEATHSIG, SIGKILL);

        while(1){
            reply = persistentClient.read();
            if(reply == "0"){ //keep-alive packet
                //do nothing
            } else if (reply == "1"){
                handleDownloadRequestFromPeer(cnxnSock);
            }
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
