/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Server.cpp
 * Author: root
 *
 * Created on February 22, 2017, 4:31 PM
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

#define SIZE_HEADER sizeof(uint32_t)

int nSocketClient; //thread create n socket client

bool setTimeout(int nSocketClient)
{
    struct timeval tvTimeout;
    tvTimeout.tv_sec = 60;
    tvTimeout.tv_usec = 0;
    
    int8_t uTimeout = setsockopt(nSocketClient, SOL_SOCKET, SO_RCVTIMEO, (char *)&tvTimeout,sizeof(struct timeval));
    if(uTimeout == -1)
        return false;
    return true;
}

bool recvFileInfor(int nSocketClient, string& strMessageFromClient, int32_t& uLenData)
{
    strMessageFromClient.clear();
    uLenData = 0;
    int32_t uBytes = 0, uLenFileName = 0;
    int32_t uRecvMessage[6] = {0};
    
    //Recv size file name
    if(recv(nSocketClient, &uLenFileName, SIZE_HEADER, 0) != SIZE_HEADER)
    {
        cout << "Receive header failed" << endl;
        return false;
    }
    uLenFileName = ntohl(uLenFileName);
    if(!uLenFileName)
    {
        std::cout << "Header failed" << endl;
        return false;
    }

    while (uLenFileName > 0)
    {
        uBytes = sizeof(uRecvMessage) - 1;
        if(uLenFileName < uBytes)
            uBytes = uLenFileName;

        memset(uRecvMessage, 0, sizeof(uRecvMessage));
        if(recv(nSocketClient , (char*)uRecvMessage , uBytes, 0) != uBytes)
        {
            cout << "Receive data failed" << endl;
            break;
        }
        strMessageFromClient = strMessageFromClient + (char*)uRecvMessage;
        uLenFileName = uLenFileName - uBytes;
    }
    //get data length
    if(recv(nSocketClient , &uLenData , sizeof(uint32_t), 0) != sizeof(uint32_t));
    uLenData = ntohl(uLenData);
    if(!uLenData)
    {
        std::cout << "Recv Data length fail" << endl;
        return false;
    }
    return true;
}

bool sendMessage(string const& strMessageToClient)
{
    uint8_t szBuffer[100] = {0};
    uint32_t uLenData = 0, uLenSend = 0;
    uLenData = strMessageToClient.length();
    uLenData = htonl(uLenData);
    memcpy(szBuffer, &uLenData, SIZE_HEADER);
    memcpy(szBuffer + SIZE_HEADER, strMessageToClient.c_str(), strMessageToClient.length());
    uLenSend = SIZE_HEADER + strMessageToClient.length();
    //Send some data
    if (send(nSocketClient, szBuffer, uLenSend, 0) !=  uLenSend) 
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
    if(!socketListen)
        return NULL;
    //Get the socket descriptor
    nSocketClient = reinterpret_cast<std::uintptr_t>(socketListen);
    if(setTimeout(nSocketClient) == false)
        return NULL;
    while( true )
    {   //Recv file info
        if(recvFileInfor(nSocketClient, strMessageFromClient, uLenData) == false)
            break;

        string strMessageToClient = "Accepted file Infor.";
        if(sendMessage(strMessageToClient) == false)
            break;

        //Create file
        FILE *fWritePtr = fopen(strMessageFromClient.c_str(),"ab"); //ghi binary vao cuoi file
        if(fWritePtr == NULL)
        {
            std::cout << "Opening file is error"<<endl;
            break;
        }
        cout << "Created file" << endl;

        // Recv Data
        int32_t uBytes = 0;
        int8_t uRecvMessage[6] = {0};
        while (uLenData > 0)
        {
            uBytes = sizeof(uRecvMessage) - 1;
            if(uLenData < uBytes)
                uBytes = uLenData;

            memset(uRecvMessage, 0, sizeof(uRecvMessage));
            if(recv(nSocketClient , (char*)uRecvMessage , uBytes, 0) != uBytes)
            {
                cout << "Receive data failed" << endl;
                break;
            }
            fwrite((char*)uRecvMessage, 1, uBytes, fWritePtr);
            uLenData = uLenData - uBytes;
        }

        fclose(fWritePtr);
        strMessageToClient = "Written";
        if(sendMessage(strMessageToClient) == false)
            break;
        strMessageToClient.clear();
    }
    close(nSocketClient);
    return 0;
}

int32_t createSocket()
{
    int skListen = socket(AF_INET , SOCK_STREAM , 0); //create server socket if error is returned -1
    if (skListen == -1)
    {
        cout << "Could not create socket" << endl;
        return -1;
    }
    cout << "Socket created" << endl;
    return skListen;
}

bool bindSocket(int32_t skListen)
{
    struct sockaddr_in saServer , saClient;
    saServer.sin_family = AF_INET;
    saServer.sin_addr.s_addr = INADDR_ANY;
    saServer.sin_port = htons( 8880 );
     
    //Bind: server require port for socket if < 0 return error
    if( bind(skListen,(struct sockaddr *)&saServer , sizeof(saServer)) < 0) //
    {
        //print the error message
        perror("bind failed. Error");
        return false;
    }
    return true;
}

bool listenSocket(int32_t skListen)
{
    if(listen(skListen , 3) < 0)
    {
        close(skListen);
        return false;
    }
    cout << "Waiting for incoming connections..." << endl;
    return true;
}

bool acceptSocket(int32_t skListen)
{
    struct sockaddr_in saClient;
    int32_t skClient = 0;
    int32_t uSize = sizeof(struct sockaddr_in);
    
    while( (skClient = accept(skListen, (struct sockaddr *)&saClient, (socklen_t*)&uSize)) >= 0)
    {
        cout << "New connection accepted" << endl;
        pthread_t sniffer_thread;
        if( pthread_create( &sniffer_thread, NULL,  ConnectionHandler, (void*) skClient) < 0)
        {
            perror("could not create thread");
            break ;
        }
    }
    if(skClient < 0)
    {
        perror("No connection accept");
        return false;
    }
    return true;
}

int main(int argc, char** argv) 
{
    int skListen;
    skListen = createSocket();
    if(skListen == -1)
        return 0;
    if(bindSocket(skListen) == false)
        return 0;
    if(listenSocket(skListen) == false)
        return 0;
    if(acceptSocket(skListen) == false)
        return 0;
    close(skListen); 
    return 0;
}
