#include <atomic>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>

// Atomic boolean to track
static std::atomic<bool> SERVER_RUNNING {true};

// Helper to close connections and exit
void exitWithError(std::string_view msg, int server = -1) {
    std::cerr << "Error: " << msg << '\n';
    close(server); std::exit(1);
}

// Helper to set server into non blocking mode
bool setNonBlocking(int server) {
    int fcntl_flags {fcntl(server, F_GETFL, 0)};
    return !(fcntl_flags == -1 || fcntl(server, F_SETFL, fcntl_flags | O_NONBLOCK) == -1);
}

int main(int argc, char **argv) {
    if (argc != 2) std::cout << "Usage: heartbeat <port>\n";
    else {
        // On interupt set SERVER_RUNNING flag to false
        std::signal(SIGINT, [](int) { SERVER_RUNNING = false; });

        // Constants for the server
        constexpr int SOCKET_BACKLOG {10}; 
        constexpr const char *serverIp {"0.0.0.0"};
        constexpr std::string_view response {"HTTP/1.1 200\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK"}; 
        const std::uint16_t PORT {static_cast<std::uint16_t>(std::stoi(argv[1]))};

        // Init socket
        int serverSocket {socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
        if (serverSocket == -1) exitWithError("Failed to initialize socket");

        // Set SO_REUSEADDR option to reuse socket post shutdown immediately
        int opt {1};
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
            exitWithError("Failed to set SO_REUSEADDR.", serverSocket);

        // Set server to nonblocking mode
        if (!setNonBlocking(serverSocket))
            exitWithError("Failed to set socket to non-blocking mode.", serverSocket);

        // Create sockaddr & bind it
        sockaddr_in serverAddr {};
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, serverIp, &serverAddr.sin_addr);
        serverAddr.sin_port = htons(PORT);
        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) == -1)
            exitWithError("Failed to bind to specified port.", serverSocket);

        // Listen for connections
        if (listen(serverSocket, SOCKET_BACKLOG) == -1)
            exitWithError("Unsuccessful in starting a listener.", serverSocket);

        // Feedback on server
        std::cout << "Server listening on port " << PORT << '\n';

        // Listen for incoming connections
        std::vector<pollfd> pollFDs {{serverSocket, POLLIN, 0}};
        while (SERVER_RUNNING) {
            int pollResult {poll(pollFDs.data(), pollFDs.size(), -1)};

            // Poll failed
            if (pollResult == -1) {
                if (errno == EINTR) continue;
                exitWithError("Poll failed.", serverSocket);
            }

            // Accept an incomming connection
            if (pollFDs[0].revents & POLLIN) {
                int clientSocket {accept(serverSocket, nullptr, nullptr)};

                // Skip incomming if server stopped or client is invalid
                if (clientSocket == -1 || !SERVER_RUNNING) close(clientSocket);

                // Set client to nonblocking mode
                else if (!setNonBlocking(clientSocket)) close(clientSocket);

                // Send message to client and close the connection
                else {
                    send(clientSocket, response.data(), response.size(), 0);
                    close(clientSocket);
                }
            }
        }
        
        // Cleanup
        close(serverSocket); std::cout << '\n';
    }
}
