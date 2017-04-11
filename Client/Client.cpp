/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.cpp
 * Author: root
 *
 * Created on February 22, 2017, 4:16 PM
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
#define uSizeHeader  sizeof(uint32_t)

int32_t CreateSocket()
{
    int32_t skClient = socket(AF_INET, SOCK_STREAM, 0);
    if (skClient == -1) 
    {
        std::cout << "Could not create socket" << endl;
        return -1;
    }
    std::cout << "Socket created" << endl;
    return skClient;
}

bool SetTimeout(int32_t skClient)
{
    struct timeval tvTimeout;
    tvTimeout.tv_sec = 60;
    tvTimeout.tv_usec = 0;
    // set timeout
    if(setsockopt(skClient, SOL_SOCKET, SO_RCVTIMEO, (char *)&tvTimeout,sizeof(struct timeval)) < 0)
    {
        perror("set socket option failed!");
        close(skClient);
        return false;
    }
    return true;
}

bool ConnectSocket(int32_t skClient)
{
    struct sockaddr_in saServer;
    saServer.sin_addr.s_addr = inet_addr("127.0.0.1");
    saServer.sin_family = AF_INET;
    saServer.sin_port = htons(8880);
    
    if (connect(skClient, (struct sockaddr *) &saServer, sizeof (saServer)) < 0)  // correct is returned 0 else return -1
    {
        perror("connect failed. Error");
        close(skClient);
        return false;
    }
    std::cout << "Connected" << endl;
    //set timeout
    if(!SetTimeout(skClient))
        return false;
    return true;
}

// Get file size
int32_t GetFileSize(const string& strNameFile)
{
    FILE * file = fopen(strNameFile.c_str(),"rb");
    if(file == NULL) 
    {
        cout<<"File Empty";
        return -1;
    }
    else 
    {
        fseek(file,0,SEEK_END);
        int32_t uSize = ftell(file);
        fseek(file,SEEK_CUR,SEEK_SET);
        fclose(file);
        return uSize;
    }
}

bool SendFileInfor(int32_t skClient, const string& strFileName)
{
    uint32_t szBuffer[2048] = {0};
    uint32_t uLenSend = 0, uLenFileName = 0; // sum of uSizeHeader and uLenData
    uint32_t uSizeFile = GetFileSize(strFileName);
    
    if(uSizeFile == -1)
        return false;
    
    uLenFileName = strFileName.length();
    uLenFileName = htonl(uLenFileName);
    memcpy(szBuffer, &uLenFileName, uSizeHeader); //length file name
    memcpy(szBuffer + uSizeHeader, strFileName.c_str(), strFileName.length()); // File name
    memcpy(szBuffer + uSizeHeader + strFileName.length(), &uSizeFile, sizeof(int32_t)); // length data
    uLenSend = uSizeHeader + strFileName.length() + sizeof(uSizeFile); 
    //Send some data
    if (send(skClient, szBuffer, uLenSend, 0) !=  uLenSend) 
    {
        std::cout << "Send failed" << endl;
        return false;
    }
    return true;
}

bool SendFileName(int32_t skClient, string& strFileName)
{
    strFileName.clear();
    std::cout << "Enter File Name: ";
    std::getline(std::cin, strFileName);
    if(!strFileName.compare("stop"))
        return false;
    
    if(SendFileInfor(skClient, strFileName) == false)
        return false;
    
    return true;
}

bool SendFileData(int32_t skClient)
{
    string strFileName = "";
    if(SendFileName(skClient, strFileName) == false)
        return false;
    FILE * file = fopen(strFileName.c_str(),"rb");
    if(!file)
        return false;
    
    char buf[5] = {0};
    int bufSize = sizeof(buf);
    while(!feof(file))
    {
        int res = fread(buf,1,bufSize,file);
        //fileConTent(skClient, buf);
        if (send(skClient, buf, res, 0) !=  res) 
        {
            std::cout << "Send failed" << endl;
            return false;
        }
        memset(buf, 0, sizeof(buf));
    }
    fclose(file);
    return true;
}

bool CheckRecv(int32_t skClient)
{
    uint32_t uLenSend = 0, uLenData = 0; // sum of uSizeHeader and uLenData
    string strRecvFromServer = "\0";
    char uServerReply[50] = {0}; //server replay ok?
    //Recv header
    if(recv(skClient, &uLenData, uSizeHeader, 0) != uSizeHeader)
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
    int32_t skClient = CreateSocket();
    if(skClient == -1)
        return 0;
    //Connect to remote server
    if(!ConnectSocket(skClient))
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
