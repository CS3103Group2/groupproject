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

typedef unordered_map<string, int> IPADDR_MAP;

IPADDR_MAP ip_available;

int connectToPeer(TCPClient &tcp_client, string ip_address)
{
    int sock = -1;
    int count = 0;
    string response = "y";
    do{
        tcp_client = *(new TCPClient());
        sock = tcp_client.connectTo(ip_address, PORT);
        count++;

        if (count == 5) {
            cout << "\nUnable to connectTo. Do you want to try again? [y/n]";
            cin >> response;
            count = 0;
        }

    } while(sock == -1 && response.compare("y") == 0);

    return sock;

}


bool detectIPAddress(const string& addr)
{
   struct sockaddr_in sa;
   if (inet_pton(AF_INET, addr.c_str(), &(sa.sin_addr)))
       return true;
   return false;
}


void request_download(string dest_ip, string &response){
	TCPClient dest_connection;
	string src_ip;

	// Request <IP>
	if(ip_available.find(dest_ip)){
		ip_available[dest_ip] = 0; //no longer available to connect to
		ip_available[src_ip] = 0;
	}


	request_bridging(dest_ip);


	if (connectToPeer(dest_connection) == -1) { //failed to connect
		response = "Successfully established connection."; }
	else if (response = "0") {
		response = "Failed to establish connection. Please retry again."; }
	else if (response = "2") {
		response = "Client does not exist."; }
}



int request_bridging(string &ipAddress, string &response){
	// Check if client is available
	bool result = detectIPAddress(ipAddress);
	int response;

	if (result == true) { // If client is available
		// Bridge the connection between clients
		response = 1;
	}

	else{ //Else if client is not available
		if (result == false) {
			// Return 0 if client exists but failed to establish connection
			response = 0;
		}
		else{
			// Return 2 if client does not exists
			response = 2;
		}
	}
}

void processIncomingMessage(string message, string &response, string clientAddr){

    regex ws_re("\\s+");
    vector<string> result{
        sregex_token_iterator(message.begin(), message.end(), ws_re, -1), {}
    };
    string code = result[0];
    if (code == "1"){
        request_download(response);
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
	serverAddress.sin_port=htons(P2PPort);
	bind(serverSock,(struct sockaddr *)&serverAddress, sizeof(serverAddress));

    listen(serverSock,1);

    socklen_t sosize  = sizeof(clientAddress);

    while (1){
        cnxnSock = accept(serverSock,(struct sockaddr*)&clientAddress,&sosize);
        cout << "connected: " << inet_ntoa(clientAddress.sin_addr) << endl;
        thread slave(threadHandler, cnxnSock, inet_ntoa(clientAddress.sin_addr));
        slave.detach();
    }

    close(serverSock);
    return 0;
}
