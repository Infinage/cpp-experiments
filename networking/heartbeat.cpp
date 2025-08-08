#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// Static variables to enable access from interupt handler
static int clientSocket {-1}, serverSocket {-1};

// Interupt handler function
void closeConnection(int) {
    close(clientSocket);
    close(serverSocket);
    serverSocket = -1;
}

void exitWithError(std::string_view msg) {
    std::cerr << "Error: " << msg << '\n';
    closeConnection(0);
    std::exit(1);
}

int main(int argc, char **argv) {
    if (argc != 2) std::cout << "Usage: heartbeat <port>\n";
    else {
        constexpr int SOCKET_BACKLOG {10}; 
        constexpr const char *serverIp {"0.0.0.0"};
        constexpr std::string_view response {"HTTP/1.1 200\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK"}; 
        const std::uint16_t PORT {static_cast<std::uint16_t>(std::stoi(argv[1]))};

        // Init socket
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == -1) exitWithError("Failed to initialize socket");

        // Set SO_REUSEADDR option to reuse socket post shutdown immediately
        int opt = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
            exitWithError("Failed to set SO_REUSEADDR.");

        // Create sockaddr & bind it
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, serverIp, &serverAddr.sin_addr);
        serverAddr.sin_port = htons(PORT);
        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) == -1)
            exitWithError("Failed to bind to specified port.");

        // Listen for connections
        if (listen(serverSocket, SOCKET_BACKLOG) == -1)
            exitWithError("Unsuccessful in starting a listener.");

        // Feedback on server
        std::cout << "Server listening on port " << PORT << '\n';

        // Listen for incoming connections, terminate server on interupt
        std::signal(SIGINT, closeConnection);
        while (serverSocket != -1) {
            clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket != -1 && serverSocket != -1)
                send(clientSocket, response.data(), response.size(), 0);
            close(clientSocket);
        }
        
        // Close server socket
        close(serverSocket);
        std::cout << '\n';
    }
}
