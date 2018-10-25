all:	tracker client P2PClient.o P2PDB.o TCPClient.o TCPTracker.o

tracker: P2PDB.o TCPTracker.o 
	g++ -o tracker P2PDB.o TCPTracker.o -std=c++11 -pthread

client: P2PClient.o TCPClient.o
	g++ -o client P2PClient.o TCPClient.o -std=c++11 -pthread

P2PClient.o: P2PClient.cpp
	g++ -c P2PClient.cpp -std=c++11 -pthread

P2PDB.o: P2PDB.cpp
	g++ -c P2PDB.cpp -std=c++11 -pthread

TCPClient.o: TCPClient.cpp
	g++ -c TCPClient.cpp -std=c++11 -pthread

TCPTracker.o: TCPTracker.cpp
	g++ -c TCPTracker.cpp -std=c++11 -pthread

clean:
	rm -f client tracker P2PClient.o P2PDB.o TCPClient.o TCPTracker.o

