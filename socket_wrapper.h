// this is the socket wrapper
#ifndef SOCKET_WRAPPER_H
#define SOCKET_WRAPPER_H

#include <iostream>
#include <string>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
#endif

class SocketWrapper {
public:
    SocketWrapper() : sock(INVALID_SOCKET) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            throw std::runtime_error("Failed to create socket");
        }
    }

    ~SocketWrapper() {
        if (sock != INVALID_SOCKET) {
#ifdef _WIN32
            closesocket(sock);
#else
            close(sock);
#endif
        }
    }

    void connect(const std::string& host, int port) {
        struct sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        
        if (inet_pton(AF_INET, host.c_str(), &server.sin_addr) <= 0) {
            throw std::runtime_error("Invalid address/Address not supported");
        }

        if (::connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
            throw std::runtime_error("Connection failed");
        }
    }

    void send(const std::string& message) {
        if (::send(sock, message.c_str(), message.length(), 0) < 0) {
            throw std::runtime_error("Send failed");
        }
    }

    std::string receive(int buffer_size = 1024) {
        char buffer[buffer_size];
        int bytes_received = recv(sock, buffer, buffer_size - 1, 0);
        
        if (bytes_received < 0) {
            throw std::runtime_error("Receive failed");
        }
        
        buffer[bytes_received] = '\0';
        return std::string(buffer);
    }

    SOCKET getSocket() const {
        return sock;
    }

private:
    SOCKET sock;
};

#endif // SOCKET_WRAPPER_H