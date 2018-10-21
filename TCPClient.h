#include<iostream>    //cout
#include<stdio.h> //printf
#include<string.h>    //strlen
#include<string>  //string
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr
#include<netdb.h> //hostent


#define PORT 15000;
#define SIZE 4096;

using namespace std;


class TCPClient
{
private:
  int sock;
  std::string address;
  int port;
  struct sockaddr_in server;

public:
  TCPClient();
  bool setup(string address, int port);
  bool Send(string data);
  string receive();
  string read();
  void exit();
};

#endif
