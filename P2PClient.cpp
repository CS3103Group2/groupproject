#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netdb.h>
#include <vector>

using namespace std;

string p2pserver_address;

void displayOptions(){
    cout << "\n*-*-*-*-*-*-*-*-*-* OPTIONS *-*-*-*-*-*-*-*-*-*-*-*" << endl;
    cout << "1. List available files" << endl;
    cout << "2. Query for a specific file" << endl;
    cout << "3. Download a specific file from server" << endl;
    cout << "4. Upload file to P2P server" << endl;
    cout << "5. Exit" << endl;

    cout << "Enter your option: ";
}

/************** Utilities ********************* */

void connectToServer(TCPClient tcp_client){
    int sock = -1;
    int count = 0;
    string response = "y";
    do{

        sock = tcp_client.connect(p2pserver_address, PORT);
        count++;

        if (count == 5) {
            cout << "Unable to connect to P2P Server. Do you want to try again? [y/n]";
            getline(cin, response);
            count = 0;
        }

    } while(sock == -1 && response.compare("y") == 0);

    return sock;

}

string generate_query(){
    
}

/**************** COMMANDS  ***********************
*/

int listFiles(){

}

int searchFile(){

}

int downloadFile(){
    int server_sock;
    TCPClient server_connection;

    if(connectToServer(server_connection) == -1){
        exit(1);
    }



    server_connection.



}

int uploadFile(){

}

int main()
{
    int op;
    cout << "Enter IP address of P2P server: ";
    getline(cin, p2pserver_address);

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
                cout << "Invalid option. Please try again."; << endl;
        }

    } while (op ! = 5);

}
