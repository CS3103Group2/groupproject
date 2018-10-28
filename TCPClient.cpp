#include "TCPClient.h"

TCPClient::TCPClient()
{
    sock = -1;
    port = 0;
    address = "";
}
/**
    Connect to a host on a certain port number
*/
int TCPClient::connectTo(string address , int port)
{
    //create socket if it is not already created
    if(sock == -1)
    {
        //Create socket
        sock = socket(AF_INET , SOCK_STREAM , 0);
        if (sock == -1)
        {
            perror("Could not create socket");
            return -1;
        }

        cout << "Socket created\n";
    }


    //setup address structure
    if(inet_addr(address.c_str()) == -1)
    {
        struct hostent *he;
        struct in_addr **addr_list;

        //resolve the hostname, its not an ip address
        if ( (he = gethostbyname( address.c_str() ) ) == NULL)
        {
            //gethostbyname failed
            herror("gethostbyname");
            cout << "Failed to resolve hostname\n";

            return -1;
        }

        //Cast the h_addr_list to in_addr , since h_addr_list also has the ip address in long format only
        addr_list = (struct in_addr **) he->h_addr_list;

        for(int i = 0; addr_list[i] != NULL; i++)
        {
            //strcpy(ip , inet_ntoa(*addr_list[i]) );
            server.sin_addr = *addr_list[i];

            cout<<address<<" resolved to "<<inet_ntoa(*addr_list[i])<<endl;

            break;
        }
    }

    //plain ip address
    else {
        server.sin_addr.s_addr = inet_addr( address.c_str() );
    }

    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    //Connect to remote server
    if (connect(sock, (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        return -1;
    }

    cout<< "Connected to " + address;
    return sock;
}

/**
    Send data to the connected host
*/
bool TCPClient::send_data(string data)
{
    //Send some data
    if(send(sock , data.c_str() , strlen(data.c_str()) , 0) < 0)
    {
        perror("Send failed : ");
        return false;
    }
    cout<<"Data send\n";

    return true;
}

/**
    Receive data from the connected host
*/
int TCPClient::receiveAndWriteToFile(string filepath)
{
    ofstream myfile;
    myfile.open(filepath.c_str());

    char buffer[1] = {};
    char buff0[1] = {'a'}, buff1[1] = {'a'}, buff2[1] = {'a'};
  	string reply;
  	while ( buff0[0] != '\0' && buff1[0] != '\0' && buff2[0] != '\n' && buffer[0] != '\r') {
    		if(recv(sock , buffer , sizeof(buffer) , 0) < 0){
                cout << "receive failed!" << endl;
                return -1;
    		}
        buff0[0] = buff1[0];
        buff1[0] = buff2[0];
        buff2[0] = buffer[0];
		reply += buffer[0];
	}

    cout << reply <<endl;
    myfile << reply;
    myfile.close();
    return 1;
}

string TCPClient::read()
{
  	char buffer[1] = {};
  	string reply;
  	while (buffer[0] != '\n') {
    		if(recv(sock , buffer , sizeof(buffer) , 0) < 0){
                cout << "receive failed!" << endl;
			    return "";
    		}
		reply += buffer[0];
	}
	return reply;
}

void TCPClient::exit()
{
    close(sock);
}
