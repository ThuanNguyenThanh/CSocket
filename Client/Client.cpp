/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.cpp
 * Author: root
 *
 * Created on February 2, 2017, 4:16 PM
 */

#include <cstdlib>
#include <unistd.h>
#include <stdio.h> //printf
#include <string.h>    //strlen
#include <sys/socket.h>    //socket
#include <arpa/inet.h> //inet_addr
#include <iostream>
#include <typeinfo>

using namespace std;
#define SIZE_HEADER_FILE_NAME  sizeof(uint32_t)
#define SIZE_HEADER_FILE_SIZE  sizeof(uint32_t)

int32_t ConnectSocket()
{
    int32_t skClient = socket(AF_INET, SOCK_STREAM, 0);
    if (skClient < 0) 
    {
        std::cout << "Could not create socket" << endl;
        return -1;
    }
    
    struct sockaddr_in saServer;
    saServer.sin_addr.s_addr = inet_addr("127.0.0.1");
    saServer.sin_family = AF_INET;
    saServer.sin_port = htons(8880);
    if (connect(skClient, (struct sockaddr *) &saServer, sizeof (saServer)) < 0)  // correct is returned 0 else return -1
    {
        close(skClient);
        perror("connect failed. Error");
        return -1;
    }
    std::cout << "Connected" << endl;
    
    //set timeout
    struct timeval tvTimeout;
    tvTimeout.tv_sec = 60;
    tvTimeout.tv_usec = 0;
    
    if(setsockopt(skClient, SOL_SOCKET, SO_RCVTIMEO, (char *)&tvTimeout,sizeof(struct timeval)) < 0)
    {
        close(skClient);
        perror("set socket option failed!");
        return -1;
    }
    
    return skClient;
}

string GetFileName()
{
    string strFileName = "";
    std::cout << "Enter File Name: ";
    std::getline(std::cin, strFileName);
    if (!strFileName.compare("stop"))
        return "stop";
    return strFileName;
}
    
int32_t GetFileSize(const string& strNameFile)
{
    FILE * file = fopen(strNameFile.c_str(),"rb");
    if(!file) 
    {
        cout<<"File Empty";
        return -1;
    }
    
    fseek(file,0,SEEK_END);
    int32_t uSize = ftell(file);
    fseek(file,SEEK_CUR,SEEK_SET);
    fclose(file);
    return uSize;
}

//send message Download file to server 
bool SendMessege(int32_t skClient)
{
    uint8_t uBuffer[100] = {0};
    uint32_t uLenFileName = 0;
    uint32_t uLenSend = 0;
    string strFileName = GetFileName();
    if(!strFileName.compare("stop"))
        return false;
    uLenFileName = strFileName.length();
    uLenFileName = htonl(uLenFileName);
    memcpy(uBuffer, &uLenFileName, SIZE_HEADER_FILE_NAME);
    memcpy(uBuffer + SIZE_HEADER_FILE_NAME, strFileName.c_str(), strFileName.length());
    uLenSend = SIZE_HEADER_FILE_NAME + strFileName.length();
    
    if(send(skClient, (char*)uBuffer, uLenSend, 0) != uLenSend)
        return false;
}



bool SendFileInfor(int32_t skClient, const string& strFileName, uint32_t uFileSize)
{
    uint8_t szBuffer[2048] = {0};
    uint32_t uLenSend = 0; // sum of uSizeHeader and uLenData
    uint32_t uLenFileName = 0; 
    
    uLenFileName = strFileName.length();
    uLenFileName = htonl(uLenFileName);
    uFileSize = htonl(uFileSize);
    
    memcpy(szBuffer, &uLenFileName, SIZE_HEADER_FILE_NAME); //length file name
    memcpy(szBuffer + SIZE_HEADER_FILE_NAME, &uFileSize, SIZE_HEADER_FILE_SIZE); // length data
    memcpy(szBuffer + SIZE_HEADER_FILE_NAME + SIZE_HEADER_FILE_SIZE, strFileName.c_str(), strFileName.length()); // File name
    uLenSend = SIZE_HEADER_FILE_NAME + SIZE_HEADER_FILE_SIZE + strFileName.length(); 
    
    //Send Header
    if (send(skClient, (char*)szBuffer, uLenSend, 0) !=  uLenSend) 
    {
        std::cout << "Send failed" << endl;
        return false;
    }
    return true;
}

bool SendFileData(int32_t skClient)
{
    //get file name
    string strFileName = GetFileName();
    if (!strFileName.compare("stop"))
        return false;
    
    //get file size
    uint32_t uFileSize = GetFileSize(strFileName);
    if(uFileSize == -1)
        return false;
    
    //send file info include: file name, size of file
    if(SendFileInfor(skClient, strFileName, uFileSize) == false)
        return false;
            
    FILE * file = fopen(strFileName.c_str(),"rb");
    if(!file)
        return false;
    
    char buf[5] = {0};
    int bufSize = sizeof(buf);
    
    int nSizeSendBytes = 0;

    while(uFileSize > 0)
    {
        if(bufSize < uFileSize)
            nSizeSendBytes = bufSize;
        else
            nSizeSendBytes = uFileSize;
        
        uint32_t uByteRead = fread(buf, 1, nSizeSendBytes,file);
        if(uByteRead != nSizeSendBytes)
        {
            fclose(file);
            std::cout << "Read file failed" << endl;
            return false;
        }
        
        if (send(skClient, buf, uByteRead, 0) !=  uByteRead) 
        {
            fclose(file);
            std::cout << "Send failed" << endl;
            return false;
        }
        
        memset(buf, 0, sizeof(buf));
        uFileSize = uFileSize - uByteRead;
    }
    
    fclose(file);
    return true;
}

bool CheckRecv(int32_t skClient)
{
    uint32_t uLenData = 0; // sum of uSizeHeader and uLenData
    string strRecvFromServer = "";
    char uServerReply[50] = {0}; //server replay ok?
    //Recv header
    if(recv(skClient, &uLenData, SIZE_HEADER_FILE_NAME, 0) != SIZE_HEADER_FILE_NAME)
    {
        std::cout << "Receive header failed" << endl;
        return false;
    }
    uLenData = ntohl(uLenData);
    if(!uLenData)
    {
        std::cout << "Header failed" << endl;
        return false;
    }
    
    // data Recv from the server
    memset(uServerReply, 0, strlen(uServerReply));
    if(recv(skClient, uServerReply, uLenData, 0) != uLenData)
    {
        std::cout << "recv failed" << endl;
        return false;
    }
    strRecvFromServer = std::string(uServerReply);
    cout << "Server replay: " << strRecvFromServer << endl;
    return true;
}

int main(int argc, char** argv) 
{
    //Create socket
    int32_t skClient = ConnectSocket();
    if(skClient < 0)
        return 0;
    
    //keep communicating with server
    while (true) 
    {
        if(SendFileData(skClient) == false) //Send File Data
            break;
        if(CheckRecv(skClient) == false)  // check Recv from server
            break;
    }
    
    close(skClient); 
    return 0;
}