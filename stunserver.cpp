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

void request_mapping(string &response, string clientAddress, int sock){

    // Insert <ip address, socket> mapping to map whenever receive connection
    ip_sock_mapping.insert((pair<string,int>) make_pair(clientAddress, sock));

	// response: ipAddress\r\n
	response = clientAddress;
}

int request_bridging(string &response, string clientAddress, string destinationAddress){

	// Check if client is available and update accordingly

	if (ip_available[destinationAddress] == 0 && ip_available[clientAddress == 0]) {
		response = "1\n";
        return 1;
	}

    return -1;

}

//sending info for bridging
void threadDataHandler(int srcSock, int destSock){

    int bytesRecved;
    char buffer[MAXBUFFERSIZE];

    if ((bytesRecved = recv(srcSock, buffer, MAXBUFFERSIZE, 0)) <= 0){
        return;
    }
    buffer[bytesRecved] = '\0';

    send(destSock, buffer, strlen(buffer), 0);

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

void processIncomingMessage(string message, string &response, string clientAddress, int sock){

    regex ws_re("\\s+");
    vector<string> result{
        sregex_token_iterator(message.begin(), message.end(), ws_re, -1), {}
    };

    string code = result[0];

    if (code == "1"){
        request_mapping(response, clientAddress, sock);
    } else if (code == "2"){
        int value = request_bridging(response, result[1], result[2]);

        if(value == 1){ //succesful in bridging
            start_bridging(result[1], result[2]);
        }

    } else{
        response = "0 Action is not defined!\r\n";
    }
}

void onStartConnection(int sock, string clientAddr){

    int bytesRecved;
    char buffer[MAXBUFFERSIZE];

    while(1){
        if ((bytesRecved = recv(sock, buffer, MAXBUFFERSIZE, 0)) <= 0){
            break;
        }
        buffer[bytesRecved] = '\0';

        string msg(buffer);
        string response;
        processIncomingMessage(msg, response, clientAddr, sock);
        send(sock, response.c_str(), response.length(), 0);
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
    string str, ip_address;
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
        // listen for incoming connections
        cnxnSock = accept(serverSock,(struct sockaddr*)&clientAddress,&sosize);
        cout << "connected: " << inet_ntoa(clientAddress.sin_addr) << endl;
        ip_address = inet_ntoa(clientAddress.sin_addr);

        onStartConnection(cnxnSock, ip_address);
    }

    close(serverSock);
    return 0;
}
