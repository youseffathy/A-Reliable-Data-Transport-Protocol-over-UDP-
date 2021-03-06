//client.cpp
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <string>
#include <thread>
#include <ctime>
#include <bits/stdc++.h>

#define TIMEOUT 10


struct packet
{
    uint16_t len;
    uint32_t seqno;
    char data [500];
};

struct ack_packet
{
    uint16_t cksum;
    uint16_t len;
    uint32_t ackno;
};

using namespace std;

packet create_packet(const string& file_name);
void send_request(char* buf);
void extract_data(struct packet *data_packet);
void send_ack(uint32_t seqno);


map<uint32_t, vector<char>> packets;
int sockfd;
struct sockaddr_in serv_addr;
int port_number;
socklen_t addr_len = sizeof(struct sockaddr);

int main()
{
    memset(&serv_addr, 0, sizeof(serv_addr));
    // reading client.in
    ifstream fin;
    ofstream fout;
    string file_name;
    string line;
    port_number = 8080;
    file_name = "input.txt";
    // server info
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_number);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(serv_addr.sin_zero), '\0', 8);

    // create client socket
    if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
    {
        cerr << "Can't create a socket! Quitting" << endl;
        exit(EXIT_FAILURE);
    }

    // send a packet with the filename
    struct packet p = create_packet(file_name);

    char* buf = new char[600];
    memset(buf,0, 600);
    memcpy(buf, &p, sizeof(p));

    send_request(buf);

    //receive the data
    int i = 0;
    while(true)
    {
        int sret;
        fd_set readfds;
        struct timeval timeout {};
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;
        sret = select(sockfd+1, &readfds, nullptr, nullptr, &timeout);

        if(sret == 0)
        {
            cout << "No response from the server!!. XxXxXxTerminatingxXxXxX" << endl;
            break;
        }
        else if(sret == -1)
        {
            perror("error in select");
            exit(1);
        }

        memset(buf,0, 600);
        int bytesReceived = recvfrom(sockfd, buf, 600, 0, (struct sockaddr*)&serv_addr,  &addr_len);
        if (bytesReceived == -1)
        {
            perror("Error while receivng packet");
            exit(EXIT_FAILURE);
        }

        cout <<"The Packet number "<<i<<" is received " << packets.size() << endl;

        auto* data_packet = (struct packet*) buf;
        extract_data(data_packet);
        send_ack(data_packet->seqno);
        i++;
    }
    fout.open(file_name);
    cout <<"file size: "<< packets.size() << " packets" << endl;
    map<uint32_t, vector<char>> :: iterator it;
    for (it=packets.begin() ; it!=packets.end() ; it++){
        for(char c : (*it).second){
            fout << c;
        }
    }
    fout.close();
}
void send_ack(uint32_t seqno)
{
    struct ack_packet ack;
    ack.cksum = 0;
    ack.len = sizeof(ack);
    ack.ackno = seqno;

    char* buf = new char[600];
    memset(buf,0, 600);
    memcpy(buf, &ack, sizeof(ack));

    int bytesSent = sendto(sockfd, buf, 600, 0,(struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    if (bytesSent == -1)
    {
        perror("couldn't send the ack");
        exit(1);
    }
}

void extract_data(struct packet *data_packet)
{
    uint32_t seqno = data_packet->seqno;
    vector<char> data;

    int size = data_packet->len - sizeof(seqno) - sizeof(data_packet->len);
    cout <<"data size = " << size << endl;
    data.reserve(size);

    for (int i = 0; i < size; ++i)
    {
        data.push_back(data_packet->data[i]);
    }

    packets.insert(make_pair(seqno, data));
}

packet create_packet(const string& data)
{
    struct packet p {};
    p.seqno = 0;
    p.len = data.length() + sizeof(p.len) + sizeof(p.seqno);
    strcpy(p.data, data.c_str());
    return p;
}


void send_request(char* buf)
{
    int sret;
    fd_set readfds;
    struct timeval timeout {};

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    int bytesSent = sendto(sockfd, buf, 600, 0,(struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    if (bytesSent == -1)
    {
        perror("couldn't send the packet");
    }

    sret = select(sockfd+1, &readfds, nullptr, nullptr, &timeout);

    if(sret == 0)
    {
        cout << "No response from the server! \n Resending packet..." << endl;
        send_request(buf);
    }
    else if(sret == -1)
    {
        perror("error in select");
        exit(1);
    }
}
