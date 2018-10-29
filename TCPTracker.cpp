#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <thread>
#include <regex>
#include "P2PDB.h"

using namespace std;

const int P2PPort = 15000;
const int MAXBUFFERSIZE = 10000;
string test = "This is the test string";
Knowledge_Base KB;

void handleList(string &response){
    if (!KB.isEmpty()){
        response = KB.listAllFiles() + "\r\n";
    } else{
        response = "0 There are currently no files in the network.\r\n";
    }
}

void handleSearch(string fileName, string &response){
    if (KB.containsFile(fileName)){
        response = "1" + KB.getFileInfo(fileName) + "\r\n";
    } else{
        response = "0 This file does not exist.\r\n";
    }
}

void handleDownload(string fileName, string &response){
    if (KB.containsFile(fileName)){
        response = "1" + KB.downloadFile(fileName) + "\r\n";
    } else{
        response = "0 This file does not exist.\r\n";
    }
}

void handleUpload(string ipAddr, string fileName, int fileSize, string &response){
    if (KB.containsFile(fileName)){
        response = "0 A file of this name already exists. Rename the file and try again.\r\n";
        return;
    }
    KB.uploadNewFile(ipAddr, fileName, fileSize);
    response = "1\r\n";
}

void handleExit(string clientAddr, string &response){
    KB.removePeer(clientAddr);
    response = "1 You selfish bastard =D.\r\n";
}

void handleUpdate(string clientAddr, vector<string> incomingMsg, string &response){
    vector<int> chunkIDList;
    for (int i=2; i < incomingMsg.size(); i++){ 
        chunkIDList.push_back(stoi(incomingMsg[i]));
    }
    if (KB.containsFile(incomingMsg[1])){
        KB.updatePeerFileChunkStatus(clientAddr, incomingMsg[1], chunkIDList);
        response = "1\r\n";
    } else{
        response = "0\r\n";
    }
    
}

void handleGetChunks(vector<string> &incomingMsg, string & response){
    vector<int> chunkIDList;
    for (int i=2; i < incomingMsg.size(); i++){
        chunkIDList.push_back(stoi(incomingMsg[i]));
    }
    if (KB.containsFile(incomingMsg[1])){
        response = "1";
        response += KB.getPeerForChunks(incomingMsg[1], chunkIDList) + "\r\n";
    } else{
        response = "0\r\n";
    }
}

void processIncomingMessage(string message, string &response, string clientAddr){

    regex ws_re("\\s+");
    vector<string> result{
        sregex_token_iterator(message.begin(), message.end(), ws_re, -1), {}
    };
    string code = result[0];
    if (code == "1"){
        handleList(response);
    } else if (code == "2"){
        handleSearch(result[1], response);
    } else if (code == "3"){
        handleDownload(result[1], response);
    } else if (code == "4"){
        handleUpload(clientAddr, result[1], stoi(result[2]), response);
    } else if (code == "5"){
        handleExit(clientAddr, response);
    } else if (code == "6"){ // update peer's chunk status
        handleUpdate(clientAddr, result, response);
    } else if (code == "7"){
        handleGetChunks(result,response);
    } else{
        response = "0 Action is not defined!\r\n";
    }
}

void threadHandler(int sock, string clientAddr){

    int bytesRecved;
    char buffer[MAXBUFFERSIZE];

    while (1){
        if ((bytesRecved = recv(sock, buffer, MAXBUFFERSIZE, 0)) <= 0){
            break;
        }
        buffer[bytesRecved] = '\0';
        //cout <<"Thread connecting to " << clientAddr << " has received " << buffer << endl;
        string msg(buffer);
        string response;
        processIncomingMessage(msg, response, clientAddr);
        send(sock, response.c_str(), response.length(), 0);
    }

    close(sock);

}


main(int argc, char const *argv[])
{
    int serverSock, cnxnSock;
    string str;
	struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;

    serverSock=socket(AF_INET,SOCK_STREAM,0);
 	memset(&serverAddress,0,sizeof(serverAddress));
	serverAddress.sin_family=AF_INET;
	serverAddress.sin_addr.s_addr=htonl(INADDR_ANY);
	serverAddress.sin_port=htons(P2PPort);
	bind(serverSock,(struct sockaddr *)&serverAddress, sizeof(serverAddress));

    listen(serverSock,1);

    socklen_t sosize  = sizeof(clientAddress);

    //Testing Area
    KB.uploadNewFile("1.1.1.1", "Test.txt", 1000000);
    KB.uploadNewFile("192.168.1.2", "Tatsh.txt", 2000000);
    KB.uploadNewFile("220.255.12.10", "Mirai.txt", 3000000);
    KB.uploadNewFile("120.245.13.10", "Osu.txt", 4000000);
    KB.uploadNewFile("150.15.243.109", "Wizard.txt", 5000000); 

    while (1){
        cnxnSock = accept(serverSock,(struct sockaddr*)&clientAddress,&sosize);
        cout << "connected: " << inet_ntoa(clientAddress.sin_addr) << endl;
        thread slave(threadHandler, cnxnSock, inet_ntoa(clientAddress.sin_addr));
        slave.detach();
    }

    close(serverSock);
    return 0;
}


