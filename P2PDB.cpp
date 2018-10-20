#include "P2PDB.h"

void Knowledge_Base::createNewPeer(string ipAddr){
    return;
}

void Knowledge_Base::removeFileFromFileList(FILE_NAME fileName){
    File_List.erase(fileName);
}

void Knowledge_Base::removeFileFromFDM(FILE_NAME fileName){
    fdm.erase(fileName);
}

void Knowledge_Base::removePeerFromFileList(FILE_NAME fileName, CHUNK_ID_LIST chunkIDList, PEER_IP peerIP){
    CHUNK_IP_MAP &toRemoveFrom = File_List[fileName];
    for (auto chunkID: chunkIDList){
        PEER_IP_LIST &peerIPList = toRemoveFrom[chunkID];
        for (int i=0; i<peerIPList.size(); i++){ // search and remove peer
            if (peerIPList[i] == peerIP){
                peerIPList.erase(peerIPList.begin()+i);
                break;
            }
        }
        if (peerIPList.empty()){ // the only peer holding a chunk has left, remove the whole file
            removeFileFromFileList(fileName);
            removeFileFromFDM(fileName);
            return;
        }
    }
}

void Knowledge_Base::removePeer(string ipAddr){
    // user information from peer list to remove in file list
    PEER_KNOWLEDGE_BASE::const_iterator itr = Peer_List.find(ipAddr);
    if (itr != Peer_List.end()){
        FILE_CHUNK_MAP fileInfoToRemove = itr->second;
        for (auto it = fileInfoToRemove.begin(); it != fileInfoToRemove.end(); ++it){
            FILE_NAME fileName = it->first;
            CHUNK_ID_LIST chunkIdLst = it->second;
            removePeerFromFileList(fileName, chunkIdLst, ipAddr);
        }
    }
    // remove user from peer list
    Peer_List.erase(ipAddr);
}

string Knowledge_Base::listAllFiles(){
    string returnString;
    returnString += "     File Name      File Size      Initial Seeder\n";
    int i = 1;
    for(auto &itr: fdm){
        returnString += to_string(i) + ".   ";
        returnString += itr.first + "   ";
        FileInfo &fi = itr.second;
        returnString += to_string(fi.fileSize) + "  ";
        returnString += fi.initialSeeder + "    \n";
        i++;
    }
    returnString += "\r\n";
    return returnString;
}

string Knowledge_Base::downloadFile(string fileName){
    string returnStr;

    returnStr += fileName + " "; // file name
    returnStr += to_string(fdm[fileName].fileSize) + " "; // file size
    returnStr += to_string(File_List[fileName].size()) + " "; // num of chunk
    int randomNo;
    int i=1;
    
    srand (time(NULL));
    for (auto itr: File_List[fileName]){
        returnStr += to_string(itr.first) + " ";    //chunk id
        randomNo = rand() % itr.second.size();
        returnStr += itr.second[randomNo] + " "; // random peer IP
    }
    returnStr += "\r\n";
    return returnStr;
}

string Knowledge_Base::getFileInfo(FILE_NAME fileName){
    return "";
}
void Knowledge_Base::uploadNewFile(string ipAddr, string fileName, int fileSize){
    // update peer list
    FILE_CHUNK_MAP &userFileChunkMap = Peer_List[ipAddr];
    CHUNK_ID_LIST fullChunkIDList;
    int num_chunks = ceil((double)fileSize / (double)chunk_size);
    for (int i=1; i<= num_chunks; i++){
        fullChunkIDList.insert(i);
    }
    userFileChunkMap[fileName] = fullChunkIDList;

    // update file list
    FILE_KNOWLEDGE_BASE::const_iterator itr = File_List.find(fileName);
    if (itr == File_List.end()){
        PEER_IP_LIST peerIPList;
        peerIPList.push_back(ipAddr);
        CHUNK_IP_MAP chunkIPMap;
        for (int i=1; i<= num_chunks; i++){
            chunkIPMap[i] = peerIPList;
        }
        File_List[fileName] = chunkIPMap;        
    }

    // update fdm
    FILE_DETAILS_MAP::const_iterator fdmItr = fdm.find(fileName);
    if (fdmItr == fdm.end()){
        FileInfo fi;
        fi.fileName = fileName;
        fi.fileSize = fileSize;
        fi.initialSeeder = ipAddr;
        fdm[fileName] = fi;
    }    
    
}

void Knowledge_Base::printEverything(){
    cout << "file list size = " << File_List.size() << endl;
    for (auto &itr: File_List){
        cout << "file name: " << itr.first << endl;
        cout << "num of chunks: " << itr.second.size() << endl;
        for (auto &item: itr.second){
            cout << "chunk id: " << item.first << " ";
            cout << "ip addresses: "<< endl;
            for (auto t: item.second){
                cout << t << " " << endl;
            }
        }
        cout << endl;
    }
}

void Knowledge_Base::updatePeerFileChunkStatus(string ipAddr, string fileName, vector<int> chunkIDListToAdd){
    // update Peer List
    CHUNK_ID_LIST &chunkIDList = Peer_List[ipAddr][fileName];
    for (auto i: chunkIDListToAdd){
        chunkIDList.insert(i);
    }

    //update File List
    CHUNK_IP_MAP &chunkIPMap = File_List[fileName];
    for (auto i: chunkIDListToAdd){
        chunkIPMap[i].push_back(ipAddr);
    }
}

bool Knowledge_Base::isEmpty(){
    return fdm.empty();
}

bool Knowledge_Base::containsFile(string fileName){
    FILE_DETAILS_MAP::const_iterator itr = fdm.find(fileName);
    return itr != fdm.end();
}

/*
int main(int argc, char const *argv[])
{
    Knowledge_Base KB;
    cout << KB.listAllFiles();
    KB.uploadNewFile("1.1.1.1", "Test.txt", 1000000);
    cout << KB.listAllFiles();
    cout << KB.downloadFile("Test.txt") << endl;
    KB.uploadNewFile("129.52.31.221", "anotherText.xee", 9020);
    cout << KB.downloadFile("anotherText.xee") << endl;
    cout << KB.listAllFiles();
    vector<int> temp;
    temp.push_back(2);
    temp.push_back(5);
    temp.push_back(8);
    
    KB.updatePeerFileChunkStatus("129.52.31.221", "Test.txt", temp);
    cout << KB.downloadFile("Test.txt") << endl;
    //KB.printEverything();
    cout << KB.downloadFile("anotherText.xee") << endl;
    temp.clear();
    temp.push_back(1);
    KB.updatePeerFileChunkStatus("1.1.1.1", "anotherText.xee", temp);
    cout << KB.downloadFile("anotherText.xee") << endl;
cout << KB.listAllFiles();
KB.printEverything();
    KB.removePeer("129.52.31.221");
    cout << KB.downloadFile("anotherText.xee") << endl;
    KB.printEverything();
    cout << KB.listAllFiles();
    KB.removePeer("1.1.1.1");
    cout << KB.listAllFiles();
    KB.printEverything();
    return 0;
}
*/