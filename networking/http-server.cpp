#include <arpa/inet.h>
#include <atomic>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_set>

#include "../misc/ThreadPool.hpp"

// HTTP Server Thread running?
std::atomic<bool> httpServerRunning {true};

// Limit concurrent threads & active socket connections
constexpr int N_THREADS {10};
std::counting_semaphore<N_THREADS> semaphore(N_THREADS);

// Mark the client sockets for closure
std::unordered_set<int> activeClientSockets;
std::mutex clientSocketMutex;

// Global variable to access via signal handler
int serverSocket = -1;
    
// Helper to close socket_fd, display an error message and quit
void exitWithError (int socket_, const std::string &message) {
    close(socket_);
    if (!message.empty())
        std::cerr << message;
    std::exit(1);
}

// Helper to handle interupts
void handleInterrupt(int) {
    std::cout << "Keyboard interrupt received, exiting.\n";
    httpServerRunning = false; close(serverSocket); 

    {
        // Shutdown all client sockets
        std::lock_guard<std::mutex> lock(clientSocketMutex);
        for (int clientSock: activeClientSockets) {
            shutdown(clientSock, SHUT_RDWR);
            close(clientSock);
        }
        activeClientSockets.clear();
    }
}

// Accept connection from a client and respond back
void handleClient(int clientSocket, const std::string clientIp, const std::filesystem::path &serveDirectory) {
    {
        // Add into list of active clients - RAII
        std::lock_guard<std::mutex> lock(clientSocketMutex);
        activeClientSockets.insert(clientSocket);
    }

    // Read the request from client
    char raw_buffer[512] = {0};
    recv(clientSocket, raw_buffer, sizeof(raw_buffer), 0);

    // Ensure that it is a get request & read the query
    std::string buffer{raw_buffer};
    std::size_t reqTypeEndPos {buffer.find(' ')};
    std::string reqType {buffer.substr(0, reqTypeEndPos)}, reqPathRaw {
        buffer.size() < reqTypeEndPos + 2 || buffer[reqTypeEndPos + 2] == ' ' ? "": 
            buffer.substr(reqTypeEndPos + 2, buffer.find(' ', reqTypeEndPos + 2) - (reqTypeEndPos + 2))
    };

    // Check if file exists and is within our serve directory, strip out query params
    std::filesystem::path reqPath {serveDirectory / reqPathRaw.substr(0, reqPathRaw.find('?'))};
    bool fileExists {std::filesystem::exists(reqPath)}, validFile {false};

    if (reqType == "GET" && fileExists) {
        std::string relativePath {std::filesystem::relative(reqPath, serveDirectory)};
        validFile = relativePath.size() == 1 || relativePath.substr(0, 2) != "..";
    }

    // Set the respones code
    std::string responseCode;
    if (validFile) responseCode = "200 OK";
    else if (reqType == "GET") responseCode = "404 Not Found";
    else responseCode = "405 Method Not Allowed";

    // Log the request
    if (!buffer.empty())
        std::cout << clientIp << ": " << reqType << " /" << reqPathRaw << " [" << responseCode << "]\n";

    // Read the file requested - if exists and req is valid
    std::string fileContents{""};
    if (validFile) {
        std::ifstream ifs {reqPath};
        std::string ipBuffer;
        while (std::getline(ifs, ipBuffer))
            fileContents += ipBuffer + "\n";
    }

    // Craft the response
    std::string response {"HTTP/1.1 "}; 
    response += responseCode;
    response += "\r\nContent-Type: text/plain\r\n";
    response += "Content-Length: " + std::to_string(fileContents.size()) + "\r\n\r\n";
    response += fileContents;

    // Send response to connected client
    send(clientSocket, response.c_str(), response.size(), 0);

    // Cleanup - remove client socket from activeSockets set
    close(clientSocket);
    {
        std::lock_guard<std::mutex> lock(clientSocketMutex);
        activeClientSockets.erase(clientSocket);
    }
    semaphore.release();
}

int main(int argc, char **argv) {

    if (argc != 3) {
        std::cout << "Usage: ./http-server <port> <path>\n";
        return 0;
    }

    constexpr int SOCKET_BACKLOG {10}; 
    constexpr const char *serverIp {"0.0.0.0"};
    const std::uint16_t PORT {static_cast<std::uint16_t>(std::stoi(argv[1]))};

    // Get the path to serve from
    const std::filesystem::path serveDirectory {strcmp(argv[2], ".") == 0? 
        std::filesystem::current_path(): 
        std::filesystem::path(argv[2])
    };

    // Fail if serve dir doesn't exist
    if (!std::filesystem::exists(serveDirectory)) {
        std::cerr << serveDirectory << " doesn't exist, server failed to start.\n";
        std::exit(1);
    }

    // Init socket - Global Variable
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == -1) 
        exitWithError(serverSocket, "Failed to initialize socket.\n");

    // Set SO_REUSEADDR option to reuse socket post shutdown immediately
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        exitWithError(serverSocket, "Failed to set SO_REUSEADDR.\n");

    // Create sockaddr & bind it
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIp, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) == -1)
        exitWithError(serverSocket, "Failed to bind to specified port.\n");

    // Listen for connections
    if (listen(serverSocket, SOCKET_BACKLOG) == -1)
        exitWithError(serverSocket, "Unsuccessful in starting a listener.\n");

    // Feedback on server startup
    std::cout << "Serving HTTP on port " << PORT << " (http://" <<  serverIp << ":" << PORT 
              << "/) \nDirectory: " << serveDirectory << "\n\n";

    // Accept connections
    ThreadPool<std::function<void()>> pool(N_THREADS);
    std::signal(SIGINT, handleInterrupt);
    while (httpServerRunning) {
        // Accept a connection, store the client addr
        struct sockaddr_in clientAddr;
        socklen_t addr_size {sizeof (clientAddr)};
        int clientSocket {accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addr_size)};

        // Parse the IP from client sock
        struct in_addr ipAddr = clientAddr.sin_addr;
        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, clientIp, INET_ADDRSTRLEN);

        if (httpServerRunning && clientSocket != -1) {
            semaphore.acquire();
            pool.enqueue([clientSocket, clientIp, &serveDirectory]() {
                handleClient(clientSocket, std::string{clientIp}, serveDirectory); 
            });
        }
    }

    // Cleanup
    close(serverSocket);

    return 0;
}
