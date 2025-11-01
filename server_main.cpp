#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <set>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

class ChatServer {
private:
    int server_fd;
    std::set<int> clients;
    std::mutex clients_mutex;
    bool running;

    void broadcast(const std::string& message, int sender_fd) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (int client_fd : clients) {
            if (client_fd != sender_fd) {
                send(client_fd, message.c_str(), message.length(), 0);
            }
        }
    }

public:
    ChatServer() : server_fd(-1), running(true) {
        // Handle SIGINT gracefully
        signal(SIGINT, [](int) {
            std::cout << "\nShutting down server...\n";
            exit(0);
        });
    }

    void handle_client(int client_fd) {
        char buffer[1024];
        std::string client_ip;
        {
            sockaddr_in addr;
            socklen_t len = sizeof(addr);
            getpeername(client_fd, (sockaddr*)&addr, &len);
            client_ip = inet_ntoa(addr.sin_addr);
        }

        std::cout << "New connection from " << client_ip << std::endl;

        while (running) {
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_received <= 0) {
                break;
            }

            std::string message(buffer);
            if (message == "/quit") {
                break;
            }

            std::string broadcast_message = client_ip + ": " + message;
            std::cout << broadcast_message << std::endl;
            broadcast(broadcast_message, client_fd);
        }

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(client_fd);
        }
        close(client_fd);
        std::cout << "Client " << client_ip << " disconnected" << std::endl;
    }

    void start(int port) {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("Failed to bind socket");
        }

        if (listen(server_fd, SOMAXCONN) < 0) {
            throw std::runtime_error("Failed to listen on socket");
        }

        std::cout << "Server listening on port " << port << std::endl;

        std::vector<std::thread> threads;
        while (running) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
            
            if (client_fd < 0) {
                std::cerr << "Failed to accept connection" << std::endl;
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.insert(client_fd);
            }

            threads.emplace_back(&ChatServer::handle_client, this, client_fd);
            threads.back().detach();
        }

        close(server_fd);
    }

    ~ChatServer() {
        running = false;
        if (server_fd >= 0) {
            close(server_fd);
        }
    }
};

int main() {
    try {
        ChatServer server;
        server.start(8080);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
