#include <iostream>
#include <thread>
#include "socket_wrapper.h"

void send_Messages(SocketWrapper& socket) {
    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        if (line == "/quit") {
            socket.send(line);
            break;
        }
        try {
            socket.send(line);
        } catch (const std::exception& e) {
            std::cerr << "Error sending message: " << e.what() << std::endl;
            break;
        }
    }
}

void receive_Messages(SocketWrapper& socket) {
    while (true) {
        try {
            std::string message = socket.receive();
            std::cout << "\nReceived: " << message << std::endl;
            if (message == "/quit") break;
        } catch (const std::exception& e) {
            std::cerr << "Error receiving message: " << e.what() << std::endl;
            break;
        }
    }
}

int main() {
    try {
        SocketWrapper socket;
        std::cout << "Connecting to server..." << std::endl;
        socket.connect("127.0.0.1", 8080);
        std::cout << "Connected to server!" << std::endl;

        // Create threads for sending and receiving
        std::thread send_thread(send_Messages, std::ref(socket));
        std::thread receive_thread(receive_Messages, std::ref(socket));

        // Wait for threads to finish
        send_thread.join();
        receive_thread.join();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
