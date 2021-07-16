#include <fstream>
#include <iostream>
#include <vector>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include "json-develop/single_include/nlohmann/json.hpp"

char sentence[4085];
char buffer[2047];
using namespace std;
using json = nlohmann::json;

void receive_cmd_from_server(int sockfd)
{
    bzero(buffer,2047);
    if(read(sockfd , buffer , 2047) < 0)
        write(2,"Error reading\n",14);

    sprintf(sentence,"%s",buffer);
    puts(sentence);
}

vector<string> removeDupWord(string str) 
{ 
    vector<string> ans;
    string word = ""; 
    for (auto x : str)  
    { 
        if (x == ' ') 
        { 
            ans.push_back(word);
            word = ""; 
        } 
        else { 
            if(x!= '\n')
                word = word + x; 
        } 
    } 

    ans.push_back(word);
    return ans;
} 

void handle_commands(int sockfd,int sockfd_data, char* input)
{
    vector<string> splitted_input = removeDupWord(input);
    string command = splitted_input[0];

    if(command == "user" || command == "pass" || command == "pwd" || command == "mkd" || command == "dele" 
    || command == "cwd" || command == "rename" || command == "quit" || command == "help")   
        receive_cmd_from_server(sockfd);
    
    else if(command == "ls" || command == "retr"){
        receive_cmd_from_server(sockfd_data);
        receive_cmd_from_server(sockfd);
    }
    else
        receive_cmd_from_server(sockfd);
}

int main(){
    int sockfd ,sockfd_data, portNo ,portNo_data,n;
    struct sockaddr_in serv_addr;
    struct sockaddr_in serv_addr_data;
    struct hostent *server;

    ifstream i("config.json");
    json j;
    i >> j;
    portNo = j["commandChannelPort"];
    portNo_data = j["dataChannelPort"];

    sockfd = socket(AF_INET , SOCK_STREAM , 0);
    if (sockfd < 0) 
        write(2,"Error opening socket\n",21);
    
    sockfd_data = socket(AF_INET , SOCK_STREAM , 0);
    if (sockfd_data < 0) 
        write(2,"Error opening socket\n",21);

    char server_addr[10 + sizeof(char)];
    sprintf(server_addr, "%s", "127.0.0.1");    

    server = gethostbyname(server_addr);

    bzero((char *)&serv_addr , sizeof(serv_addr));
    bzero((char *)&serv_addr_data , sizeof(serv_addr_data));

    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr , (char *) &serv_addr.sin_addr.s_addr , server->h_length);
    serv_addr.sin_port = htons(portNo);

    serv_addr_data.sin_family = AF_INET;
    bcopy((char *)server->h_addr , (char *) &serv_addr_data.sin_addr.s_addr , server->h_length);
    serv_addr_data.sin_port = htons(portNo_data);

    if(connect(sockfd , (struct sockaddr *) &serv_addr , sizeof(serv_addr)) < 0)
        write(2,"Connection failed\n",18);
    
    if(connect(sockfd_data , (struct sockaddr *) &serv_addr_data , sizeof(serv_addr_data)) < 0)
        write(2,"Connection failed\n",18);
    
    char curr_socket_data_port[2047];
    bzero(curr_socket_data_port,2047);
    if(read(sockfd , curr_socket_data_port , 2047)<0)
        write(2,"Error reading\n",14);

    sprintf(sentence,"server - your data channel port socket in sever is: %s",curr_socket_data_port);
    puts(sentence);

    while(1){
        bzero(buffer,2047);
        read(0,buffer,2047);
        if(write(sockfd,&buffer,2047) < 0)
            write(2,"Error writing\n",14);
        if(write(sockfd,&curr_socket_data_port,2047) < 0)
            write(2,"Error writing\n",14);
        handle_commands(sockfd,sockfd_data,buffer);
    }

    return 0;
}