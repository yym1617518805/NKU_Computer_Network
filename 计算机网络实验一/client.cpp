#include <iostream>
#include <thread>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

#define PORT 8888
#define SERVER_IP "127.0.0.1"
using namespace std;
void receive_messages(int sockfd) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            std::cerr << "Connection closed by server.\n";
            close(sockfd);
            exit(0);
        }
        std::cout << buffer<<endl;
    }
}

void send_messages(int sockfd) {
    std::string message;
    while (true) {
        std::getline(std::cin, message);
        if(message.find("overover") != std::string::npos){
            cout<<"Clinet stop!";
            close(sockfd);
            exit(0);
            return ;
        }
        send(sockfd, message.c_str(), message.length(), 0);
    }
}

int main() {
    int sockfd;
    sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cerr << "Socket creation failed\n";
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed\n";
        return -1;
    }

    std::cout << "Connected to server. Start chatting!\n";

    std::thread receive_thread(receive_messages, sockfd);
    std::thread send_thread(send_messages, sockfd);

    receive_thread.join();
    send_thread.join();

    close(sockfd);
    return 0;
}
