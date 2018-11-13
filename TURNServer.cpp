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
#include "TCPClient.h"
#include <math.h>
#include <sstream>
#include <sys/stat.h>

void processIncomingMessage(string, string*, string );
void threadHandler(int, string);


const int stun_Listening_Port = 15001;
const int MAXBUFFERSIZE = 10000;

typedef unordered_map <string, int> IPADDR_MAP;
unordered_map <string, int>::iterator it;

IPADDR_MAP ip_available, ip_unavailable, ip_sock_mapping;

mutex mutx;

void request_mapping(string &response, string clientAddress, int portNum, int sock){

    string ip_address_port = clientAddress + ":" + to_string(portNum);
    // Insert <ip address:port, socket> mapping to map whenever receive connection
    ip_sock_mapping.insert((pair<string,int>) make_pair(ip_address_port, sock));
    //ip_sock_mapping.insert((pair<string,int>) make_pair(clientAddress, sock));
    cout << "sock: " << sock << endl;
	
    // response: ipAddress\r\n
	response = ip_address_port + "\r\n";
    //response = clientAddress + "\r\n";
}

int request_bridging(string &response, string clientAddress, string destinationAddress){

	// Check if client is available and update accordingly
    int result = -1;

	if (ip_available[destinationAddress] == 0 && ip_available[clientAddress] == 0) {
		response = "1\r\n";
        return result = 1;
	}

    return result;

}

string getPortNum(string ip_address_port){

    string delimiter = ":";

    int delimiterPosition = ip_address_port.find(delimiter);

    string portNum = ip_address_port.substr(delimiterPosition + 1, ip_address_port.length());

    return portNum;
}

string getIPAddress(string ip_address_port){

    string delimiter = ":";
    int delimiterPosition = ip_address_port.find(delimiter);

    string clientAddress = ip_address_port.substr(0, delimiterPosition);

    return clientAddress;

}

string readMessage(int sock)
{
    char buffer[1] = {};
    string reply = "";
    int rcvd;

    while (buffer[0] != '\n') {
        rcvd = recv(sock , buffer , sizeof(buffer) , 0);
        cout << "rcvd" << rcvd << endl;
        if(rcvd < 0)
        {
            cout << "receive failed! Message" << endl;
            return NULL;
        }
        if (rcvd == 0){
            return reply;
        }
        reply += buffer[0];

        
    }

    cout << "reply:" << reply << endl;
    return reply;
}

string readFileChunks(int sock, int filesize){

    char buffer[filesize];
    int recvd;
    string reply = "";

    memset(&buffer[0], 0, sizeof(buffer));

    if((recvd = recv(sock , buffer , filesize, 0) < 0)){
        cout << "receive failed! File" << endl;
    }

    /*while(1){
        if((recvd = recv(sock , buffer , filesize, 0) < 0)){
            cout << "receive failed! File" << endl;
        } else
            break;
    */
    cout << "file recvd value: " << recvd << endl;

    reply = buffer;

    cout << "file reply: " << reply << endl; 

    return reply;

}

void init_bridging(string clientAddress, string destinationAddress){

    mutx.lock();
    ip_available[clientAddress] = 1;
    ip_available[destinationAddress] = 1;
    ip_unavailable.insert((pair<string,int>) make_pair(clientAddress, 0));
    ip_unavailable.insert((pair<string,int>) make_pair(destinationAddress, 0));
    mutx.unlock();
}

void end_bridging(string clientAddress, string destAddress){

    mutx.lock();
    ip_available[clientAddress] = 0;
    ip_available[destAddress] = 0;

    ip_unavailable.erase(clientAddress);
    ip_unavailable.erase(destAddress);
    mutx.unlock();
}

//sending info for bridging
void threadDataHandler(int srcSock, int destSock, string srcAddress, string destAddress){

    int bytesRecved;
    char buffer[MAXBUFFERSIZE];

    int senderCounter = 0;
    int receiverCounter = 0;
    int fileSize = 0;

    //source to destination
    while(senderCounter < 2){
        cout << "senderCounter: " << senderCounter << endl;
        cout << "src sock: " << srcSock << endl;
        cout << endl;
        string reply = readMessage(srcSock);

        send(destSock, reply.c_str(), reply.length(), 0);

        senderCounter++;
    }
    
    string reply;
    //destination to source
    while(receiverCounter < 3){

        cout << "dest Counter: " << receiverCounter << endl;
        cout << "dest sock: " << destSock << endl;

        if (receiverCounter == 0){
            reply = readMessage(destSock);

            //reply += "\r\n";

        } else if(receiverCounter == 1){ // receive file size
            
            reply = readMessage(destSock);

            fileSize = stoi(reply);

            //reply += "\r\n";

        } else if(receiverCounter == 2){ // read file chunks
            reply = readFileChunks(destSock, fileSize);
        }

        send(srcSock, reply.c_str(), reply.length(), 0);

        receiverCounter++;
    }

    end_bridging(srcAddress, destAddress);

}

void bridging(string srcAddress_port, string destAddress_port){

    init_bridging(srcAddress_port, destAddress_port);

    int srcSock, destSock;
    //Thread handler of srcSock and destSock
    srcSock = ip_sock_mapping[srcAddress_port];
    destSock = ip_sock_mapping[destAddress_port];

    cout << "mapping src sock" << srcAddress_port << endl;
    cout << "mapping dest sock" << destAddress_port << endl;
    cout << endl;

    thread bridge(threadDataHandler, srcSock, destSock, srcAddress_port, destAddress_port);

    bridge.detach();

}

void keepAlive(int sock){

    string response;

    // Thread handling to keep connection alive
    while(1){

        response = "0\r\n";

        send(sock, response.c_str(), response.length(), 0);
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

void establishConnection(int sock, string clientAddress, int portNum){

    int bytesRecved;
    char buffer[MAXBUFFERSIZE];

    if ((bytesRecved = recv(sock, buffer, MAXBUFFERSIZE, 0)) <= 0){
        return;
    }

    buffer[bytesRecved] = '\0';

    string message(buffer);
    string response;

    regex ws_re("\\s+");
    vector<string> result{
        sregex_token_iterator(message.begin(), message.end(), ws_re, -1), {}
    };

    string code = result[0];
    cout << "msg: " << message <<endl;

    if(code == "1"){
        request_mapping(response, clientAddress, portNum, sock);
        cout << response <<endl;
        send(sock, response.c_str(), response.length(), 0);
        //thread stayConnected(keepAlive, sock);
        //stayConnected.detach();

    } else if(code == "2"){
        string srcAddress = result[1];
        string destAddress = result[2];

        if(request_bridging(response, srcAddress, destAddress) == 1){
            bridging( srcAddress,  destAddress);

            send(sock, response.c_str(), response.length(), 0);

            cout << "handleNewConnection:" << response << endl;

        } else{

            response = "0\r\n";
            send(sock, response.c_str(), response.length(), 0);
        }
    }
    
}

int main(int argc, char const *argv[])
{

    int serverSock, cnxnSock;
    int portNum;
    string str, ip_address, ip_address_port;
	struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;

    serverSock=socket(AF_INET,SOCK_STREAM,0);
 	memset(&serverAddress,0,sizeof(serverAddress));
	serverAddress.sin_family=AF_INET;
	serverAddress.sin_addr.s_addr=htonl(INADDR_ANY);
	serverAddress.sin_port=htons(stun_Listening_Port);
	bind(serverSock,(struct sockaddr *)&serverAddress, sizeof(serverAddress));

    listen(serverSock,10);

    socklen_t sosize  = sizeof(clientAddress);

    while (1){
        // listen for incoming connections
        cnxnSock = accept(serverSock,(struct sockaddr*)&clientAddress,&sosize);
        cout << "connected: " << inet_ntoa(clientAddress.sin_addr) << endl;

        
        int reuse = 1;
        if (setsockopt(cnxnSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
            cout << "setsockopt(SO_REUSEADDR) failed" << endl;

        if (setsockopt(cnxnSock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0)
            cout << "setsockopt(SO_REUSEPORT) failed" << endl;
        
        ip_address = inet_ntoa(clientAddress.sin_addr);
        portNum = clientAddress.sin_port;
        cout << "main sock" << cnxnSock << endl;
        thread handleNewConnection(establishConnection, cnxnSock, ip_address, portNum);
        handleNewConnection.detach();
    }

    close(serverSock);

    return 0;
}
