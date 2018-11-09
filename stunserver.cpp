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
void threadHandler(int , string clientAddr);


const int stun_Listening_Port = 15001;
const int MAXBUFFERSIZE = 10000;

typedef unordered_map <string, int> IPADDR_MAP;
unordered_map <string, int>::iterator it;

IPADDR_MAP ip_available, ip_unavailable, ip_sock_mapping;

void request_mapping(string &response, string clientAddress){

	// Getting client public ip address
	response = clientAddress;

	// Thread handling to keep connection alive
	ip_available.insert((pair<string,int>) make_pair(clientAddress, 0));
	for (it = ip_available.begin(); it != ip_available.end(); it++) {
		if (it->second == 0) {
			//Send redundant packet as the thread is not in use
				response = "0\r\n";
		}
	}
}

int request_bridging(string &response, string clientAddress, string destinationAddress){

	int srcSock, destSock;

	// Check if client is available and update accordingly
	
	if (ip_available[destinationAddress] == 0) {
		response = "1\n";
		ip_available[clientAddress] = 1;
		ip_available[destinationAddress] = 1;
		ip_unavailable.insert((pair<string,int>) make_pair(clientAddress, 0));
		ip_unavailable.insert((pair<string,int>) make_pair(destinationAddress, 0));

		//Thread handler of srcSock and destSock
		srcSock = ip_sock_mapping[clientAddress];
		destSock = ip_sock_mapping[destinationAddress];
		threadHandler(srcSock, clientAddress);
		threadHandler(destSock, destinationAddress);

	}
	
}


void processIncomingMessage(string message, string &response, string clientAddr){

    regex ws_re("\\s+");
    vector<string> result{
        sregex_token_iterator(message.begin(), message.end(), ws_re, -1), {}
    };
    string code = result[0];
    if (code == "1"){
        request_mapping(response, clientAddr);
    }  else if (code == "2"){
        request_bridging(response, result[1], result[2]);
    }
    else{
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


int main(int argc, char const *argv[])
{
    int serverSock, cnxnSock;
    string str;
	struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;

    serverSock=socket(AF_INET,SOCK_STREAM,0);
 	memset(&serverAddress,0,sizeof(serverAddress));
	serverAddress.sin_family=AF_INET;
	serverAddress.sin_addr.s_addr=htonl(INADDR_ANY);
	serverAddress.sin_port=htons(stun_Listening_Port);
	bind(serverSock,(struct sockaddr *)&serverAddress, sizeof(serverAddress));

    listen(serverSock,1);

    socklen_t sosize  = sizeof(clientAddress);

    while (1){
        cnxnSock = accept(serverSock,(struct sockaddr*)&clientAddress,&sosize);
        cout << "connected: " << inet_ntoa(clientAddress.sin_addr) << endl;
        // Insert <ip address, socket> mapping to map whenever receive connection
        ip_sock_mapping.insert((pair<string,int>) make_pair(inet_ntoa(clientAddress.sin_addr)), cnxnSock);
        thread slave(threadHandler, cnxnSock, inet_ntoa(clientAddress.sin_addr));
        slave.detach();
    }

    close(serverSock);
    return 0;
}
