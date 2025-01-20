#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <mutex>
#include <algorithm>

#define PORT 8888
#define MAX_CLIENTS 10
using namespace std;
std::vector<int> clients;
std::mutex clients_mutex;

void broadcast_message(const std::string &message, int sender_fd) {
    std::lock_guard<std::mutex> guard(clients_mutex);
    for (int client_fd : clients) {
        if (client_fd != sender_fd) {
            send(client_fd, message.c_str(), message.length(), 0);
        }
    }
}

void handle_client(int client_fd) {
    char buffer[1024];
    std::string welcome_msg = "Welcome to the chat room!\n";
    send(client_fd, welcome_msg.c_str(), welcome_msg.length(), 0);

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes_received <= 0) {
            std::lock_guard<std::mutex> guard(clients_mutex);
            clients.erase(std::remove(clients.begin(), clients.end(), client_fd), clients.end());
            close(client_fd);
            std::cout << "Client disconnected: " << client_fd << std::endl;
            break;
        }

        std::string message = "Client " + std::to_string(client_fd) + ": " + buffer;
        std::cout << message<<endl;
        broadcast_message(message, client_fd);
    }
}

int main() {
    int server_fd, client_fd;
    sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Socket creation failed\n";
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed\n";
        return -1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        std::cerr << "Listen failed\n";
        return -1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            std::cerr << "Accept failed\n";
            continue;
        }

        std::lock_guard<std::mutex> guard(clients_mutex);
        clients.push_back(client_fd);
        std::cout << "Client connected: " << client_fd << std::endl;
        std::thread(handle_client, client_fd).detach();
    }

    close(server_fd);
    return 0;
}
