/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Server.cpp
 * Author: root
 *
 * Created on February 24, 2017, 4:31 PM
 */

#include <cstdlib>
#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<pthread.h> //for threading , link with lpthread
#include <iostream>

using namespace std;

#define SIZE_HEADER_FILE_NAME sizeof(uint32_t)
int nSocketClient; //thread create n socket client

bool recvFileInfor(int nSocketClient, string& strMessageFromClient, int32_t& uLenData) 
{
    strMessageFromClient.clear();
    uLenData = 0;
    int32_t uBytes = 0, uLenFileName = 0;
    int32_t uRecvMessage[6] = {0};

    //Recv length file name
    if (recv(nSocketClient, &uLenFileName, SIZE_HEADER_FILE_NAME, 0) != SIZE_HEADER_FILE_NAME) 
    {
        cout << "Receive header failed" << endl;
        return false;
    }
    uLenFileName = ntohl(uLenFileName);
    if (!uLenFileName) 
    {
        std::cout << "Header failed" << endl;
        return false;
    }

    //Recv  length data file
    if (recv(nSocketClient, &uLenData, sizeof (uint32_t), 0) != sizeof (uint32_t)) 
    {
        return false;
    }
    if (!uLenData) 
    {
        std::cout << "Recv Data length fail" << endl;
        return false;
    }
    uLenData = ntohl(uLenData);

    //Recv file name
    while (uLenFileName > 0) 
    {
        uBytes = sizeof (uRecvMessage) - 1;
        if (uLenFileName < uBytes)
            uBytes = uLenFileName;

        memset(uRecvMessage, 0, sizeof (uRecvMessage));
        if (recv(nSocketClient, (char*) uRecvMessage, uBytes, 0) != uBytes) 
        {
            cout << "Receive data failed" << endl;
            break;
        }
        strMessageFromClient = strMessageFromClient + (char*) uRecvMessage;
        uLenFileName = uLenFileName - uBytes;
    }
    return true;
}

bool sendMessage(string const& strMessageToClient) 
{
    uint8_t szBuffer[100] = {0};
    uint32_t uLenData = 0, uLenSend = 0;
    uLenData = strMessageToClient.length();
    uLenData = htonl(uLenData);
    memcpy(szBuffer, &uLenData, SIZE_HEADER_FILE_NAME);
    memcpy(szBuffer + SIZE_HEADER_FILE_NAME, strMessageToClient.c_str(), strMessageToClient.length());
    uLenSend = SIZE_HEADER_FILE_NAME + strMessageToClient.length();
    //Send some data
    if (send(nSocketClient, szBuffer, uLenSend, 0) != uLenSend) 
    {
        std::cout << "Send failed" << endl;
        return false;
    }
    return true;
}

void *ConnectionHandler(void *socketListen) 
{
    string strMessageFromClient = "";
    int32_t uLenData = 0;
    if (!socketListen)
        return NULL;
    //Get the socket descriptor
    nSocketClient = reinterpret_cast<std::uintptr_t> (socketListen);

    //set timeout
    struct timeval tvTimeout;
    tvTimeout.tv_sec = 60;
    tvTimeout.tv_usec = 0;

    int8_t uTimeout = setsockopt(nSocketClient, SOL_SOCKET, SO_RCVTIMEO, (char *) &tvTimeout, sizeof (struct timeval));
    if (uTimeout == -1)
    {
        close(nSocketClient);
        return NULL;
    }
    
    //Recv file info
    while (true) 
    { 
        if (recvFileInfor(nSocketClient, strMessageFromClient, uLenData) == false)
            break;

        //Create file
        FILE *fWritePtr = fopen(strMessageFromClient.c_str(), "wb"); //ghi binary vao cuoi file
        if (fWritePtr == NULL) 
        {
            std::cout << "Opening file is error" << endl;
            break;
        }
        cout << "Created file" << endl;

        // Recv Data
        int32_t uMaxRecBytes = 0;
        int32_t uRecBytes = 0;
        int8_t uRecvMessage[6] = {0};
        while (uLenData > 0) 
        {
            uMaxRecBytes = sizeof (uRecvMessage) - 1;
            if (uLenData < uMaxRecBytes)
                uMaxRecBytes = uLenData;

            memset(uRecvMessage, 0, sizeof (uRecvMessage));
            
            uRecBytes = recv(nSocketClient, (char*) uRecvMessage, uMaxRecBytes, 0);
            if(uRecBytes < 1)
            {
                cout << "Receive data failed" << endl;
                break;
            }
            
            if(fwrite((char*) uRecvMessage, 1, uRecBytes, fWritePtr) != uRecBytes)
            {
                cout << "Write data failed" << endl;
                break;
            }
            uLenData = uLenData - uRecBytes;
        }
        fclose(fWritePtr);

        string strMessageToClient = "Write successfully!";
        if (sendMessage(strMessageToClient) == false)
            break;
        
        strMessageToClient.clear();
    }
    
    close(nSocketClient);
    return NULL;
}

int32_t ConnectSocket()
{
    // create socket
    int32_t skListen = socket(AF_INET, SOCK_STREAM, 0); //create server socket if error is returned -1
    if (skListen == -1) 
    {
        cout << "Could not create socket" << endl;
        return -1;
    }
    cout << "Socket created" << endl;
    
    struct sockaddr_in saServer, saClient;
    saServer.sin_family = AF_INET;
    saServer.sin_addr.s_addr = INADDR_ANY;
    saServer.sin_port = htons(8880);

    //Bind: server require port for socket if < 0 return error
    if (bind(skListen, (struct sockaddr *) &saServer, sizeof (saServer)) < 0) //
    {
        close(skListen);
        perror("bind failed. Error");
        return -1;
    }
    
    //Socket listen
    if (listen(skListen, 3) < 0) 
    {
        close(skListen);
        return -1;
    }
    cout << "Waiting for incoming connections..." << endl; 
    return skListen;
}

bool AcceptSocket(int32_t skListen) 
{
    struct sockaddr_in saClient;
    int32_t skClient = 0;
    int32_t uSize = sizeof (struct sockaddr_in);

    while ((skClient = accept(skListen, (struct sockaddr *) &saClient, (socklen_t*) & uSize)) >= 0) 
    {
        cout << "New connection accepted" << endl;
        pthread_t sniffer_thread;
        if (pthread_create(&sniffer_thread, NULL, ConnectionHandler, (void*) skClient) < 0) 
        {
            perror("could not create thread");
            break;
        }
    }
    if (skClient < 0) 
    {
        perror("No connection accept");
        return false;
    }
    return true;
}

int main(int argc, char** argv) 
{
    uint32_t skListen = ConnectSocket();
    if(skListen < 0)
        return 0;
    
    if (AcceptSocket(skListen) == false)
    {
        close(skListen);
        return 0;
    }
    close(skListen);
    return 0;
}

