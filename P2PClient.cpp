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
#include <sys/mman.h>

// Uncomment in Linux for child handling
// #include <sys/prctl.h>
// #include <fcntl.h>
#include "TCPClient.h"
#include <mutex>
#include <math.h>
#include <sstream>
#include <sys/stat.h>

#define MAXLINE 200


using namespace std;
typedef unordered_map<int, string> FILE_IPADDR_MAP;
const int DEFAULT_CHUNK_SIZE = 1024;

string p2pserver_address, stunserver_address, stunserver_port, current_public_ip, new_public_ip;
FILE_IPADDR_MAP file_map, file_map_failed, file_map_successful;
mutex mutx, mutx_for_failed, mutx_for_successful, ip_mutx;
void* shmem;

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
        case 1: // list files
            query = "1 " + input + returnchar;
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
            break;
        case 7: //Get chunk updates from server
            query = "7 " + input + returnchar;
            break;
        case 8: //Get chunk updates from server
            query = "8 " + input + returnchar;
            break;
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


int connectToClient(TCPClient &tcp_client, string ip_addr)
{
    int sock = -1;
    int count = 0;
    do{
        if(count != 0){
            tcp_client = *(new TCPClient());
        }

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

void updateTrackerOnPublicIP(){

    TCPClient server_connection;
    string query;

    query = generate_query(8, current_public_ip, new_public_ip);
    server_connection.send_data(query);
    reply = server_connection.read();
    server_connection.exit();

}

void updateNATOnPublicIP(){
    TCPClient NAT_connection;
    string query;

    query = current_public_ip + " " + new_public_ip;
    NAT_connection.send_data(query);
    reply = NAT_connection.read();
    NAT_connection.exit();
}


void updatePublicIP(){
    updateTrackerOnPublicIP();
    updateNATOnPublicIP();
    current_public_ip = new_public_ip;
    memcpy(shmem, (char *) current_public_ip.c_str(), sizeof((char *) current_public_ip.c_str()));
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
    TCPClient peerClient;
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

    if(connectToClient(peerClient, ipaddr) == -1){
        //Add pair back into filemap
        mutx.lock();
        file_map.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        mutx.unlock();
        return;
    }

    cout << "Sending : " + filename + " " + to_string(chunkid) << endl;
    peerClient.send_data(filename + " " + to_string(chunkid));
    reply = peerClient.read();

    if(reply[0] == '0') {
        cout << "RESPONSE: 0" << endl;
        mutx_for_failed.lock();
        file_map_failed.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        mutx_for_failed.unlock();
        return;
    } else if (peerClient.receiveAndWriteToFile(filepath + "/" + to_string(chunkid)) < 0){
        cout << "RESPONSE: 1" << endl;
        mutx_for_failed.lock();
        file_map_failed.insert((pair<int,string>) make_pair(chunkid, ipaddr));
        mutx_for_failed.unlock();
        return;
    }
    peerClient.exit();

    cout << "EXITED" << endl;
    mutx_for_successful.lock();
    file_map_successful.insert((pair<int,string>) make_pair(chunkid, ipaddr));
    mutx_for_successful.unlock();

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
	string terminatechar = "\0\0\r\n";
	send(sock, terminatechar.c_str(), terminatechar.length(), 0);
    }
    return true;
}



void handleDownloadRequestFromPeer(int sock, string clientAddr){

    int bytesRecved;
    char buffer[DEFAULT_CHUNK_SIZE];

    while (1){
        if ((bytesRecved = recv(sock, buffer, DEFAULT_CHUNK_SIZE, 0)) <= 0){
            break;
        }
        buffer[bytesRecved] = '\0';
        cout <<"Thread connecting to " << clientAddr << " has received " << buffer << endl;
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
            response = "1\n";
            send(sock, response.c_str(), response.length(), 0);
            sendfile(sock, filehandle);
            cout << "FILESENT" << endl;
		    sleep(2);
            fclose(filehandle);
            break;
        } else {
            cout << "RESPONSE: 0" << endl;
            response = "0\n";
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


void* create_shared_memory(size_t size) {
  // Our memory buffer will be readable and writable:
  int protection = PROT_READ | PROT_WRITE;

  // The buffer will be shared (meaning other processes can access it), but
  // anonymous (meaning third-party processes cannot obtain an address for it),
  // so only this process and its children will be able to use it:
  int visibility = MAP_ANONYMOUS | MAP_SHARED;

  // The remaining parameters to `mmap()` are not important for this use case,
  // but the manpage for `mmap` explains their purpose.
  return mmap(NULL, size, protection, visibility, 0, 0);
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
        cout << "Error connecting to server";
        return -1;
    }

    query = generate_query(1, "");
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

}

int stun_xor_addr(char * stun_server_ip,short stun_server_port, char * return_ip_port)
{
    struct sockaddr_in servaddr;
    struct sockaddr_in localaddr;
    unsigned char buf[MAXLINE];
    int sockfd, i;
    unsigned char bindingReq[20];

    int stun_method,msg_length;
    short attr_type;
    short attr_length;
    short port;
    int n;

    cout << string (stun_server_ip);

    //# create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); // UDP

    // server
    memset(&servaddr, 0, sizeof(servaddr)); //sets all bytes of servaddr to 0
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, stun_server_ip, &servaddr.sin_addr);
    servaddr.sin_port = htons(stun_server_port);

    // local
    memset(&localaddr, 0, sizeof(localaddr)); //sets all bytes of localaddr to 0
    localaddr.sin_family = AF_INET;
    //inet_pton(AF_INET, "192.168.0.181", &localaddr.sin_addr);
    localaddr.sin_port = htons(PORT);

    n = ::bind(sockfd,(struct sockaddr *)&localaddr,sizeof(localaddr));
    //printf("bind result=%d\n",n);

    printf("socket opened to  %s:%d  at local port %d\n",stun_server_ip,stun_server_port,PORT);

    //## first bind
    * (short *)(&bindingReq[0]) = htons(0x0001);    // stun_method
    * (short *)(&bindingReq[2]) = htons(0x0000);    // msg_length
    * (int *)(&bindingReq[4])   = htonl(0x2112A442);// magic cookie

    *(int *)(&bindingReq[8]) = htonl(0x63c7117e);   // transacation ID
    *(int *)(&bindingReq[12])= htonl(0x0714278f);
    *(int *)(&bindingReq[16])= htonl(0x5ded3221);



    cout << "Send data ...\n";
    n = sendto(sockfd, bindingReq, sizeof(bindingReq),0,(struct sockaddr *)&servaddr, sizeof(servaddr)); // send UDP
    if (n == -1)
    {
        cout << "Unable to send data to STUN server\n";
        return -1;
    }

    // time wait
    sleep(1);

    printf("Read recv ...\n");
    n = recvfrom(sockfd, buf, MAXLINE, 0, NULL,0); // recv UDP
    if (n == -1)
    {
        cout << "Unable to receive data to STUN server\n";
        return -2;
    }
    //printf("Response from server:\n");
    //write(STDOUT_FILENO, buf, n);

    if (*(short *)(&buf[0]) == htons(0x0101))
    {
        cout << "STUN binding success!\n";

        // parse XOR
        n = htons(*(short *)(&buf[2]));
        i = 20;
        while(i<sizeof(buf))
        {
            attr_type = htons(*(short *)(&buf[i]));
            attr_length = htons(*(short *)(&buf[i+2]));
            if (attr_type == 0x0020)
            {
                // parse : port, IP

                port = ntohs(*(short *)(&buf[i+6]));
                port ^= 0x2112;

                sprintf(return_ip_port,"%d.%d.%d.%d:%d",buf[i+8]^0x21,buf[i+9]^0x12,buf[i+10]^0xA4,buf[i+11]^0x42,port);
                break;
            }
            i += (4  + attr_length);
        }
    }

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0){
        perror("setsockopt(SO_REUSEADDR) failed");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0){
        perror("setsockopt(SO_REUSEPORT) failed");
    }

    close(sockfd);
    printf("socket closed !\n");

    return 0;
}

int main()
{
    pid_t pid;
    int op, mySock, cnxnSock;
    string str;
    char return_ip_port[50];
    struct sockaddr_in myAddress;
    struct sockaddr_in clientAddress;
    int count = 0;

    cout << "Enter IP address of P2P server: ";
    getline(cin, p2pserver_address);

    cout << "Enter IP address of STUN server: ";
    getline(cin, stunserver_address);

    cout << "Enter port of STUN server: ";
    getline(cin, stunserver_port);

    // Create shared memory between processes
    shmem = create_shared_memory(128);

    // Get public ip address
    stun_xor_addr((char *)stunserver_address.c_str(),(short) stoi(stunserver_port), return_ip_port);
    current_public_ip = string (return_ip_port);
    new_public_ip = string (return_ip_port);
    updatePublicIP();

    memset(return_ip_port, 0, sizeof(return_ip_port));

    cout << return_ip_port;

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
            if(count == 100){
                //wait for threads to complete
                sleep(2);

                int reuse = 1;
                if (setsockopt(mySock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0){
                    perror("setsockopt(SO_REUSEADDR) failed");
                }
                if (setsockopt(mySock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0){
                    perror("setsockopt(SO_REUSEPORT) failed");
                }

                close(mySock);
                stun_xor_addr((char *)stunserver_address.c_str(),(short) stoi(stunserver_port), return_ip_port);
                new_public_ip = string (return_ip_port);
                updatePublicIP();

                memset(return_ip_port, 0, sizeof(return_ip_port));

                mySock = socket(AF_INET, SOCK_STREAM, 0);
                memset(&myAddress,0,sizeof(myAddress));
                myAddress.sin_family = AF_INET;
                myAddress.sin_addr.s_addr = htonl(INADDR_ANY);
                myAddress.sin_port = htons(PORT);
                bind(mySock,(struct sockaddr *)&myAddress, sizeof(myAddress));
                listen(mySock,1);
                socklen_t sosize  = sizeof(clientAddress);

                count = 0;
            }

            cnxnSock = accept(mySock, (struct sockaddr*)&clientAddress, &sosize);
            cout << "connected: " << inet_ntoa(clientAddress.sin_addr) << endl;
            thread slave(handleDownloadRequestFromPeer, cnxnSock, inet_ntoa(clientAddress.sin_addr));
            slave.detach();
            count++;
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
