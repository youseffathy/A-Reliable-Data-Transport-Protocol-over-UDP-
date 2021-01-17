#include <utility>
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
#include <sys/stat.h>

#define TIMEOUT 1
#define PLP 0.0

using namespace std;


struct packet
{
    uint16_t len;
    uint32_t seqno;
    char data [500];
};

struct ack_packet
{
    uint16_t len;
    uint32_t ackno;
};


packet create_packet(vector<char> data, uint32_t seqno, int data_len);
void handle_request(int client_fd, sockaddr_in client_addr, socklen_t addr_len, packet *packet);
vector<vector<char>> get_data(string file_name);
void send_data(int client_fd, struct sockaddr_in client_addr, socklen_t addr_len,  vector<vector<char>> data);



int main()
{
    int port_number;
    int server_fd, client_fd;
    struct sockaddr_in server_addr {};
    struct sockaddr_in client_addr {};
    socklen_t addr_len = sizeof(struct sockaddr);
    int broadcast = 1;

    port_number = 8080;

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(server_addr.sin_zero), '\0', 8);


    if ((server_fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
    {
        cerr << "Can't create a socket! Quitting" << endl;
        return -6;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(int)))
    {
        perror("setsockopt server_fd:");
        return 1;
    }

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
    {
        cerr << "Can't bind the socket! Quitting" << endl;
        return -2;
    }

    cout << "Server is start listening on port 8080"<< endl;

    char* buf = new char[600];

    while(true)
    {
        memset(buf,0, 600);

        int bytesReceived = recvfrom(server_fd, buf, 600, 0, (struct sockaddr*)&client_addr,  &addr_len);
        if (bytesReceived == -1)
        {
            perror("Error in recv(). Quitting\n");
            return -5;
        }
        if (bytesReceived == 0)
        {
            perror("Client disconnected !!! \n");
            return-5;
        }

        auto* data_packet = (struct packet*) buf;

        //delegate request to child process
        pid_t pid = fork();
        if (pid == 0)
        {
            //child process
            if ((client_fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
            {
                cerr << "error during creating a child process" << endl;
                return -6;
            }
            if (setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(int)))
            {
                perror("setsockopt server_fd:");
                return 1;
            }
            handle_request(client_fd, client_addr, addr_len, data_packet);
        }
    }
}
void handle_request(int client_fd, sockaddr_in client_addr, socklen_t addr_len, packet *packet)
{
    vector<vector<char>> data;
    int data_length = packet->len - sizeof(packet->seqno) - sizeof(packet->len);
    string file_name = string(packet->data, 0, data_length);
    cout << "file name: "<<string(packet->data, 0, data_length)<<endl;
    data = get_data(file_name);
    send_data(client_fd, client_addr, addr_len, data);
}

vector<vector<char>> get_data(string file_name)
{
    vector<vector<char>> file_data;
    vector<char> row;
    char c;
    ifstream fin;
    fin.open(file_name);
    if(fin)
    {
        int counter = 0;
        while(fin.get(c))
        {
            if(counter < 499)
                row.push_back(c);
            else
            {
                file_data.push_back(row);
                row.clear();
                row.push_back(c);
                counter = 0;
                continue;
            }
            counter++;
        }
        if(counter > 0) file_data.push_back(row);
    }
    else
    {
        perror("An error occurred or no file exist with that name");
        exit(1);
    }
    fin.close();
    return file_data;
}

void send_data(int client_fd, struct sockaddr_in client_addr, socklen_t addr_len,  vector<vector<char>> data)
{
    int seqno ;
    for (int i = 0; i < data.size(); ++i)
    {
        seqno = 500*i;
        struct packet p = create_packet(data.at(i), seqno, data.at(i).size());
        cout <<"data size = " << data.at(i).size()<<endl;

        char* buf = new char[600];
        memset(buf, 0, 600);
        memcpy(buf, &p, sizeof(p));
        double prob = rand() % 100  ;
        cout << prob << "---" << PLP * 100<< endl;
        if( prob > PLP * 100)
        {
            int bytesSent = sendto(client_fd, buf, 600, 0,(struct sockaddr *)&client_addr, sizeof(struct sockaddr));
            if (bytesSent == -1)
            {
                perror("couldn't send the ack");
                exit(1);
            }
        }
        int sret;
        fd_set readfds;
        struct timeval timeout {};

        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);

        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;

        sret = select(client_fd+1, &readfds, nullptr, nullptr, &timeout);

        if(sret == 0)
        {
            cout << "No acks received for packet "<< i << ". Resending" << endl;
            i--;
            continue;

        }
        else if(sret == -1)
        {
            perror("error in select");
            exit(1);
        }
        else
        {
            memset(buf,0, 600);
            int bytesReceived = recvfrom(client_fd, buf, 600, 0, (struct sockaddr*)&client_addr,  &addr_len);
            if (bytesReceived == -1)
            {
                perror("Error in recv(). Quitting");
                exit(1);
            }
            cout <<"ack received for packet "<< i <<endl;
        }
    }
}

packet create_packet(vector<char> data, uint32_t seqno, int data_len)
{
    struct packet p;
    p.len = data_len + sizeof(p.len) + sizeof(p.seqno);
    p.seqno = seqno;
    memset(p.data, 0, 500);
    strcpy(p.data, data.data());
    return p;
}
