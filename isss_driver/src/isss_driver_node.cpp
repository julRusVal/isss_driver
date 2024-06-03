// ISSSRec.cpp : This file contains the 'main' function. Program execution begins and ends there.
////#include <winsock2.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>//#include <ws2tcpip.h>
#include <iostream>
#include <fstream>

#include <ros/ros.h>
#include <std_msgs/String.h>

//#pragma comment(lib, "ws2_32.lib") // Link with Winsock librarytypedef unsigned int U32;

typedef unsigned short U16;
typedef unsigned char U8;

using namespace std;/////////////////////////

// Received Packet
/////////////////////////
    // Header
        // 0xFE
        // 0x05
        // size packet[15:8]  // 3007
        // size packet[ 7:0]
        // Type ISSS=0xE1
        // time[31:2]  [ms]
        // time[23:1]  [ms]
        // time[15:8]  [ms]
        // time[ 7:0]  [ms]
        // 0x01            // FREQ id
        // decimation_i[15:8]
        // decimation_i[ 7:0]    // 1000 loop
        // Channel 0
        // phase[15:8]
        // phase[ 7:0]
        // RSS        // Channel 1
        // phase[15:8]
        // phase[ 7:0]
        // RSS
    // End loopclass 

class ISSSonar
{
public:    

    ISSSonar()
    {
        string_pub_ = nh.advertise<std_msgs::String>("/isss_stick/isss/raw", 100);
        param<std::string>(("save_path"), save_path_, "/home/nacho/workspaces/DV/");
    }

    void SetupSonar()
    {        
        int decimation = int(float(m_range) / num_samples / 750 * (12.5e6f / 3));        // 0x10 Decimation
        SetReg_V4(0x10, decimation);
        // 0x11 Num samples per channel def:1000
        SetReg_V4(0x11, num_samples);
        // 0x12 Num Phases  def: 2 , 0 off    
    }    
    
    void SetReg_V4(int reg, int value)
    {
        U8 txBuff[4];
        txBuff[0] = 0xFE;   // Start of setup packet        txBuff[1] = U8(reg & 0xFF);
        txBuff[2] = U8((value >> 8) & 0xFF);
        txBuff[3] = U8((value) & 0xFF);
        //send(sock, (const char*)txBuff, 4, 0);
// /    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);       
        sendto(sock, (const char*)txBuff, 4, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
    }    
    
    int StartRec(const string& serverIP, const int port)
    {        
        // Initialize Winsock
        // if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            // std::cerr << "Failed to initialize Winsock!" << std::endl;
            // return;
        // }        // Create a socket
        // if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            // std::cerr << "Failed to create socket!" << std::endl;
            // WSACleanup();
            // return;
        // }        // Server address setup
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);        // Connect to the server
        ROS_INFO("Server address is set up");
        
        // if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        //     std::cerr << "Failed to connect!" << std::endl;
        //     closesocket(clientSocket);
        //     WSACleanup();
        //     return;
        // }    
        int connectRes = connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
        if (connectRes == -1)
        {
            ROS_ERROR("Connect() error: %s", strerror(errno));
            return 1;
        }    

        unsigned timeout_in_seconds = 2;
        struct timeval tv;
        tv.tv_sec = timeout_in_seconds;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);        
        cout << "TV " << tv.tv_usec << endl;        
        
        char buffer[12+2*3000];        // Setup the sonar
        
        SetupSonar();
        
        int bytesReceived = recv(sock, buffer, 2, 0); // Read 2 0xFF        cout << "bytesReceived " << bytesReceived << endl;        // Receive data
        int i = 0;
        // uint data;
        uint8_t data;        
        ofstream os(save_path_, ios::binary);
        int dx = 0;
        // for(;;) {
        while(ros::ok())
        {
        //while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
            // bytesReceived = recv(sock, buffer, sizeof(buffer), 0);         
            int bytesReceived = recv(sock, (char*)buffer, sizeof(buffer), MSG_WAITALL);
            // int bytesReceived = recv(sock, (char*)buffer, sizeof(buffer), 0);
            os.write(buffer, bytesReceived);        
            cout << "bytesReceived1 " << bytesReceived << endl;
            
            std_msgs::String msg_string;
            msg_string.data = buffer;
            string_pub_.publish(msg_string);

            // for (int j=0; j<12; ++j){        
            //     cout << "buffer << j << " << uint(buffer[j]) << endl;
            // }
            //if (status == -1) {
            //    cout <<"Failed to get first part of message even though enabled" <<endl;;
            //}
            
            // for (int i=0; i<100; ++i){
            //     data = buffer[12+i];            
            //     cout << i <<  " : "<< unsigned(data) <<endl;            
            // }
            ros::spinOnce();
            ros::Duration(0.001).sleep();  
        }       
         // Cleanup
        //closesocket(clientSocket);
        //WSACleanup();        
        close(sock);    
    }    
    
    //WSADATA wsaData;
    //SOCKET clientSocket;
    int sock = socket(AF_INET, SOCK_STREAM, 0);    
    struct sockaddr_in serverAddr;    
    int m_range = 20;
    int num_samples = 1000;
    ros::Publisher string_pub_;
    ros::NodeHandle nh;
    std::string save_path_;
};


int main(int argc, char* argv[])
{
    ros::init(argc, argv, "isss_driver_node");

    std::cout << "ISS Sonar starting" << std::endl;
    ISSSonar sonar;
    sonar.StartRec("192.168.2.62", 0xE005);

    return 0;
}


