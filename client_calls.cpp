//Let's start by including WIN32 specific headers

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#endif

//I'll configure now the socket connection for the client side on Linux/MacOS systems

#include <iostream>
#include "socket_wrapper.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//Now that I'm done I will start using the socket APIs to start actually making it work

int main(){
     int sock = socket(AF_INET6, SOCK_STREAM, 0);
     if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return -1;
    }
    std::cout << "Successfully created socket, Welcome to TChat" << std::endl;


}