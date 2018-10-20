#pragma once
#include <iostream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <math.h>
#include <stdlib.h>

using namespace std;

const int chunk_size = 131072;

typedef string FILE_NAME;
typedef int CHUNK_ID;
typedef string PEER_IP;
typedef vector<PEER_IP> PEER_IP_LIST;
typedef unordered_map<CHUNK_ID, PEER_IP_LIST> CHUNK_IP_MAP;
typedef unordered_map<FILE_NAME, CHUNK_IP_MAP> FILE_KNOWLEDGE_BASE;

typedef unordered_set<CHUNK_ID> CHUNK_ID_LIST;
typedef unordered_map<FILE_NAME, CHUNK_ID_LIST> FILE_CHUNK_MAP;
typedef unordered_map<PEER_IP, FILE_CHUNK_MAP> PEER_KNOWLEDGE_BASE; 

typedef int FILE_SIZE;

struct FileInfo {
    FILE_NAME fileName;
    FILE_SIZE fileSize;
    PEER_IP initialSeeder;
};
typedef unordered_map<FILE_NAME, FileInfo> FILE_DETAILS_MAP;

class Knowledge_Base{
    private:
    FILE_KNOWLEDGE_BASE File_List;
    PEER_KNOWLEDGE_BASE Peer_List;
    FILE_DETAILS_MAP fdm;

    void removeFileFromFileList(FILE_NAME fileName);
    void removeFileFromFDM(FILE_NAME fileName);
    void removePeerFromFileList(FILE_NAME fileName, CHUNK_ID_LIST chunkIDList, PEER_IP peerIP);

    public:
    void createNewPeer(string ipAddr);
    void removePeer(string ipAddr);

    string listAllFiles();
    string getFileInfo(string fileName);
    string downloadFile(string fileName);    
    void uploadNewFile(string ipAddr, string fileName, int fileSize);   
    void updatePeerFileChunkStatus(string ipAddr, string fileName, vector<int> chunkIDList);

    void printEverything();   

    bool isEmpty();
    bool containsFile(string fileName);
};