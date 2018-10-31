//README

compile tracker:
g++ -o tracker TCPTracker.cpp P2PDB.cpp -std=c++11 -pthread

compile runner:
g++ -o client TCPClient.cpp P2PClient.cpp -std=c++11
