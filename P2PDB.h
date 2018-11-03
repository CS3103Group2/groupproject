#pragma once
#include <iostream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <math.h>
#include <stdlib.h>
#include <mutex>

using namespace std;

const int chunk_size = 1024;

typedef string FILE_NAME;
typedef int CHUNK_ID;
typedef string PEER_IP;
typedef vector<PEER_IP> PEER_IP_LIST;
typedef unordered_map<CHUNK_ID, PEER_IP_LIST> CHUNK_IP_MAP;
typedef unordered_map<FILE_NAME, CHUNK_IP_MAP> FILE_KNOWLEDGE_BASE;

typedef unordered_set<CHUNK_ID> CHUNK_ID_LIST;
typedef unordered_map<FILE_NAME, CHUNK_ID_LIST> FILE_CHUNK_MAP;
typedef unordered_map<PEER_IP, FILE_CHUNK_MAP> PEER_KNOWLEDGE_BASE;

typedef unordered_map<PEER_IP, PEER_IP> IP_MAPPING;

typedef int FILE_SIZE;

struct FileInfo {
    FILE_NAME fileName;
    FILE_SIZE fileSize;
    PEER_IP initialSeeder;
};
typedef unordered_map<FILE_NAME, FileInfo> FILE_DETAILS_MAP;



class IP_MAP{
    private:
    IP_MAPPING ord_to_new;
    IP_MAPPING new_to_ord;

    public:
    PEER_IP getOTNMapping(PEER_IP oldIP);
    PEER_IP getNTOMapping(PEER_IP newIP);
    void updatePeerIP(PEER_IP oldIP, PEER_IP newIP);
    void createPeer(PEER_IP IP);
    PEER_IP removePeer(PEER_IP IP);
    bool peerExists(PEER_IP oldIP);
}

class Knowledge_Base{
    private:
    FILE_KNOWLEDGE_BASE File_List;
    PEER_KNOWLEDGE_BASE Peer_List;
    FILE_DETAILS_MAP fdm;
    IP_MAP IpMapping;

    int readcount = 0;
    int writecount = 0;
    mutex rmutex, wmutex, readTry, resource; //(initial value = 1)


    void removeFileFromFileList(FILE_NAME fileName);
    void removeFileFromFDM(FILE_NAME fileName);
    void removePeerFromFileList(FILE_NAME fileName, CHUNK_ID_LIST chunkIDList, PEER_IP peerIP);
    void readerLock();
    void readerUnlock();
    void writerLock();
    void writerUnlock();

    public:
   
    void removePeer(string ipAddr);

    string listAllFiles();
    string getFileInfo(string fileName);
    string downloadFile(string fileName);    
    string getPeerForChunks(string fn, vector<int> chunkIDList);
    void uploadNewFile(string ipAddr, string fileName, int fileSize);   
    void updatePeerFileChunkStatus(string ipAddr, string fileName, vector<int> chunkIDList);
    void updatePeerIP(string oldIP, string newIP);

    void printEverything();   

    bool isEmpty();
    bool containsFile(string fileName);
};