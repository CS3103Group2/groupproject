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

void request_mapping(string &response, string clientAddress_port, int sock){

    string delimiter = ":";
    //extract client IP Address
    string clientAddress = clientAddress_port.substr(0, clientAddress_port.find(delimiter));

    // Insert <ip address, socket> mapping to map whenever receive connection
    ip_sock_mapping.insert((pair<string,int>) make_pair(clientAddress_port, sock));

	// response: ipAddress\r\n
	response = clientAddress + "\r\n";
}

int request_bridging(string &response, string clientAddress, string destinationAddress){

	// Check if client is available and update accordingly

	if (ip_available[destinationAddress] == 0 && ip_available[clientAddress] == 0) {
		response = "1\r\n";
        return 1;
	}

    return -1;

}

string readMessage(int sock)
{
    char buffer[1] = {};
    string reply = "";
    int rcvd;

    while (buffer[0] != '\n') {
        rcvd = recv(sock , buffer , sizeof(buffer) , 0);
        if(rcvd < 0)
        {
            cout << "receive failed!" << endl;
            return NULL;
        }
        if (rcvd == 0){
            return reply;
        }
        reply += buffer[0];
    }
    return reply;
}

string readFileChunks(int sock, int filesize){

    char buffer[filesize];
    int recvd;
    string reply = "";

    if((recvd = recv(sock , buffer , filesize, 0) < 0)){
        cout << "receive failed!" << endl;
    }

    reply = buffer;

    return reply;

}

//sending info for bridging
void threadDataHandler(int srcSock, int destSock){

    int bytesRecved;
    char buffer[MAXBUFFERSIZE];

    int senderCounter = 0;
    int receiverCounter = 0;
    int fileSize = 0;

    //sender to receiver
    while(senderCounter < 2){

        string reply = readMessage(srcSock);

        send(destSock, reply.c_str(), reply.length(), 0);

        senderCounter++;
    }
    string reply;
    //receiver to sender
    while(receiverCounter < 3){

        if (receiverCounter == 0){
            reply = readMessage(destSock);

            reply += "\r\n";

        } else if(receiverCounter == 1){ // receive file size
            
            reply = readMessage(destSock);

            fileSize = stoi(reply);

            reply += "\r\n";

        } else if(receiverCounter == 2){ // read file chunks
            reply = readFileChunks(destSock, fileSize);
        }

        send(destSock, reply.c_str(), reply.length(), 0);

        receiverCounter++;
    }

}

void start_bridging(string clientAddress, string destinationAddress){

    int srcSock, destSock;

    ip_available[clientAddress] = 1;
    ip_available[destinationAddress] = 1;
    ip_unavailable.insert((pair<string,int>) make_pair(clientAddress, 0));
    ip_unavailable.insert((pair<string,int>) make_pair(destinationAddress, 0));

    //Thread handler of srcSock and destSock
    srcSock = ip_sock_mapping[clientAddress];
    destSock = ip_sock_mapping[destinationAddress];

    thread bridging(threadDataHandler, srcSock, destSock);

    bridging.join();
}

void complete_bridging(string clientAddress, string destAddress){
    ip_available[clientAddress] = 0;
    ip_available[destAddress] = 0;

    ip_unavailable.erase(clientAddress);
    ip_unavailable.erase(destAddress);
}

int processIncomingMessage(string message, string &response, string clientAddress_port, int sock){

    int toCloseSocket = 1; //1 to close, 0 stay open

    regex ws_re("\\s+");
    vector<string> result{
        sregex_token_iterator(message.begin(), message.end(), ws_re, -1), {}
    };

    string code = result[0];

    if (code == "1"){
        request_mapping(response, clientAddress_port, sock);
        toCloseSocket = 0;
    } else if (code == "2"){
        int value = request_bridging(response, result[1], result[2]);

        if(value == 1){ //succesful in bridging
            start_bridging(result[1], result[2]);
            response = "1";
            toCloseSocket = 0;
        } else{ 
            // not successful in connection, 
            // ignore the bridging, close the socket that request
            response = "0\r\n";
            return toCloseSocket = 1;
        }

        complete_bridging(result[1], result[2]);

    } else{
        response = "0 Action is not defined!";
    }

    response += "\r\n";

    return toCloseSocket;
}

void onStartConnection(int sock, string clientAddress_port){

    int bytesRecved;
    char buffer[MAXBUFFERSIZE];

    while(1){
        if ((bytesRecved = recv(sock, buffer, MAXBUFFERSIZE, 0)) <= 0){
            break;
        }
        buffer[bytesRecved] = '\0';

        string msg(buffer);
        string response;
        int result = processIncomingMessage(msg, response, clientAddress_port, sock);
        send(sock, response.c_str(), response.length(), 0);

        if(result == 1){
            break;
        }
    }

    close(sock);
}

void keepAlive(string &response){

    // Thread handling to keep connection alive
    while(1){

        //ip_available.insert((pair<string,int>) make_pair(clientAddress, 0));
        for (it = ip_available.begin(); it != ip_available.end(); it++) {
            if (it->second == 0) {
                //Send redundant packet as the thread is not in use
                response = "0\r\n";
                //get the socket with the given IP address
                int sock = ip_sock_mapping[it->first];
                send(sock, response.c_str(), response.length(), 0);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(30));
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
        
        ip_address = inet_ntoa(clientAddress.sin_addr);
        portNum = clientAddress.sin_port;

        ip_address_port = ip_address + ":" + std::to_string(portNum);
        onStartConnection(cnxnSock, ip_address_port);

        //hread onStart(onStartConnection, cnxnSock, ip_address);
        //onStart.join();
    }

    close(serverSock);
    return 0;
}
