#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>    //cout
#include <string>  //string
#include <arpa/inet.h> //inet_addr
#include <fstream>


#define PORT 15000
#define SIZE 4096

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
  int connectTo(string address, int port);
  bool send_data(string data);
  int receiveAndWriteToFile(string filepath, int filesize);
  string readn();
  string read();
  string readAllFiles(int);
  void exit();
};
