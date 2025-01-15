#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#include "../include/Server.hpp"

// Init Static Variables
bool Server::serverRunning = true;
std::unordered_map<int, Server::SOCKET_INFO_VALUE_TYPE> Server::socketInfo = {};

Server::Server(const std::string &serverIP, const int serverPort, const int serverBacklog):
    handler(serverIP, serverPort) 
{
    // Init socket
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == -1) exitWithError("Error creating socket object.");

    // Set SO_REUSEADDR option
    int opt {1};
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        exitWithError("Error setting SO_REUSEADDR option.");

    // Set server to nonblocking mode
    int fcntl_flags {fcntl(serverSocket, F_GETFL, 0)};
    if(fcntl_flags == -1 || fcntl(serverSocket, F_SETFL, fcntl_flags | O_NONBLOCK) == -1)
        exitWithError("Error setting socket into non-blocking mode.");

    // Bind socket
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(static_cast<std::uint16_t>(serverPort));
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) == -1)
        exitWithError("Failed to bind to specified port.");

    // Start listening for incoming connections
    if (listen(serverSocket, serverBacklog) == -1)
        exitWithError("Error starting a listener.");

    // Ready to receive data from Server
    socketInfo[serverSocket] = {POLLIN, "", 0};

    // Debug Message to console
    std::cout << "Up & running on port: " << serverPort << "\n";
}

void Server::closeSockets(int) {
    serverRunning = false;
    for (const std::pair<const int, SOCKET_INFO_VALUE_TYPE> &kv: socketInfo)
        close(kv.first);
}

void Server::exitWithError(const std::string &message) {
    std::cerr << message << "\n";
    close(serverSocket);
    std::exit(1);
}

std::vector<pollfd> Server::createPollFDs() {
    std::vector<pollfd> result;
    for (const std::pair<const int, SOCKET_INFO_VALUE_TYPE> &kv: socketInfo)
        result.push_back({kv.first, std::get<0>(kv.second), 0});
    return result;
}

// Read request chunk by chunk - for ASYNC
bool Server::readRequest(int clientSocket, std::string &buffer, long &pendingBytes) {
    char raw_buffer[1024] = {0};
    long recvd = recv(clientSocket, raw_buffer, sizeof(raw_buffer), 0);
    if (recvd <= 0) { buffer = "Error receiving data"; return true; }

    // Append received data into request string
    buffer.append(raw_buffer, static_cast<std::size_t>(recvd));

    // We already have read content-len, ensure that we have read all the pending bytes
    if (pendingBytes != -1) { 
        pendingBytes -= recvd; 
        return pendingBytes == 0; 
    } 

    // We are yet to read the content-len, figure out what that is
    else {
        std::size_t httpSepPos {buffer.find("\r\n\r\n")};

        // We are yet to receive the httpSep
        if (httpSepPos == std::string::npos) return false;

        // We have received http Sep, parse the content length
        else {
            std::size_t contentLenStart {buffer.find("Content-Length")};

            // No content length present => no body left to parse
            if (contentLenStart == std::string::npos) return true;
            
            else { // Parse content length
                contentLenStart += 16;
                std::size_t contentLenEnd {buffer.find("\r\n", contentLenStart)}; 
                long contentLen {std::stol(buffer.substr(contentLenStart, contentLenEnd))};
                std::size_t extraLen {buffer.size() - httpSepPos - 4};
                pendingBytes = contentLen - static_cast<long>(extraLen);
                return pendingBytes == 0;
            }
        }
    }
}

bool Server::sendResponse(int clientSocket, const std::string &response, long &pendingBytes) {
    // Send only the remaining portion of message
    std::size_t startPos {response.size() - static_cast<std::size_t>(pendingBytes)};
    std::string_view remaining(response.data() + startPos, static_cast<std::size_t>(pendingBytes));

    // Send to client and check for errors
    long sent = send(clientSocket, remaining.data(), remaining.size(), 0);
    if (sent == -1) return true;

    // If no errors, check if all data has been sent
    pendingBytes -= sent;
    return pendingBytes == 0;
}

void Server::run() {
    std::signal(SIGINT, closeSockets);
    while (serverRunning) {
        // Create Poll FDs and start polling
        std::vector<pollfd> pollInputs {createPollFDs()};
        int pollResult {poll(pollInputs.data(), pollInputs.size(), -1)};
        if (pollResult == -1) {
            if (serverRunning) {
                std::cerr << "Poll failed.\n"; 
                closeSockets();
            } break;
        }
        
        for (pollfd &pollInp: pollInputs) {
            // Accept an incomming connection
            if ((pollInp.revents & POLLIN) && (pollInp.fd == serverSocket)) {
                sockaddr_in clientAddr;
                socklen_t addr_size {sizeof(clientAddr)};
                int clientSocket {accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addr_size)};
                if (clientSocket != -1 && serverRunning) {
                    // Set client to nonblocking mode
                    int fcntl_flags {fcntl(clientSocket, F_GETFL, 0)};
                    if(fcntl_flags == -1 || fcntl(clientSocket, F_SETFL, fcntl_flags | O_NONBLOCK) == -1) {
                        std::cerr << "Error setting client socket to non blocking mode.\n";
                        close(clientSocket);
                    }
                }

                // Mark as ready to recv - Client
                socketInfo[clientSocket] = {POLLIN, "", -1};
            }

            // Recv data from client
            else if ((pollInp.revents & POLLIN) && pollInp.fd != serverSocket) {
                int clientSocket {pollInp.fd};
                std::string &request {std::get<1>(socketInfo[clientSocket])};
                long &pendingBytes {std::get<2>(socketInfo[clientSocket])};
                bool readStatus {readRequest(clientSocket, request, pendingBytes)};
                if (readStatus) {
                    std::string response {handler.processRequest(request)};
                    socketInfo[clientSocket] = {POLLOUT, response, response.size()}; 
                }
            }

            // Send data to client
            else if ((pollInp.revents & POLLOUT) && pollInp.fd != serverSocket) {
                int clientSocket {pollInp.fd};
                std::string &response {std::get<1>(socketInfo[clientSocket])};
                long &pendingBytes {std::get<2>(socketInfo[clientSocket])};
                bool sendStatus {sendResponse(clientSocket, response, pendingBytes)};
                if (sendStatus) {
                    close(clientSocket);
                    socketInfo.erase(clientSocket);
                }
            }
        }
    }
}
