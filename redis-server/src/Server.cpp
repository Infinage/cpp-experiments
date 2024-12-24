#include <arpa/inet.h>
#include <atomic>
#include <charconv>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/poll.h>

#include "../include/Server.hpp"
#include "../include/Utils.hpp"

// Init static variables for the server
std::atomic<bool> Redis::Server::serverRunning {true};
std::unordered_map<int, Redis::Server::SOCKET_INFO_VALUE_TYPE> Redis::Server::socketInfo{};

namespace Redis {

    /* --------------- SERVER CLASS METHOD IMPLEMENTATIONS --------------- */

    // Recv request from client return true / false based on status
    bool Server::readRequest(int client_fd, std::string &request) {
        const char *recvError = "-Error receiving data\r\n", *invalidInp = "-Invalid input data\r\n";

        // Read one portion at a time
        char buffer[1024] = {0};
        long recvd {recv(client_fd, buffer, sizeof(buffer), 0)};
        if (recvd <= 0) {
            request = recvError;
            return true;
        }

        // Append received data into request string
        request.append(buffer, static_cast<std::size_t>(recvd));

        // Try to check if received data is 'complete', return false otherwise
        char ch {request[0]};
        if (ch == '+' || ch == '-' || ch == ':')
            return request.find("\r\n") != std::string::npos;
        else if (ch == '$') {
            std::size_t lenTokEnd {request.find("\r\n")};
            if (lenTokEnd == std::string::npos) return false;
            else if (request[1] == '-' && request.size() < 5) 
                return false;
            else if (request[1] == '-' && request == "$-1\r\n")
                return true;
            else if (request[1] == '-') {
                request = invalidInp; return true;
            } else {
                // Parse the length using from_chars
                std::size_t strLength;
                std::from_chars_result parseResult {std::from_chars(request.c_str() + 1, request.c_str() + lenTokEnd, strLength)};
                if (parseResult.ec != std::errc()) { request = invalidInp; return true; }
                else return lenTokEnd + 2 + strLength + 2 < request.size();
            }
        } else if (ch == '*') {
            std::size_t lenTokEnd {request.find("\r\n")};
            if (lenTokEnd == std::string::npos) return false;
            else if (request[1] == '-' && request.size() < 5) 
                return false;
            else if (request[1] == '-' && request == "*-1\r\n")
                return true;
            else if (request[1] == '-') {
                request = invalidInp; return true;
            } else {
                // Parse the length using from_chars
                std::size_t arrLength;
                std::from_chars_result parseResult {std::from_chars(request.c_str() + 1, request.c_str() + lenTokEnd, arrLength)};
                if (parseResult.ec != std::errc()) { request = invalidInp; return true; }
                std::size_t totalDelims {Redis::countSubstring(request, "\r\n")}, 
                            totalStrs {Redis::countSubstring(request, "$")},
                            expectedDelims {(2 * arrLength) + 1};

                if (totalDelims > expectedDelims || totalStrs > arrLength) {
                    request = invalidInp; return true;
                } else if (totalDelims < expectedDelims || totalStrs < arrLength) {
                    return false;
                } else { 
                    return true;
                }

            }
        } else {
            request = invalidInp;
            return true;
        }
    }

    // Send response to client return true / false based on status
    bool Server::sendResponse(int client_fd, std::string &response, std::size_t &currPos) {
        std::string_view remaining(response.data() + (response.size() - currPos), currPos);
        long sent = send(client_fd, remaining.data(), remaining.size(), 0);
        if (sent == -1) {
            std::cerr << "Sending response to client failed.\n";
            response = ""; currPos = 0;
            return true;
        }

        currPos -= static_cast<std::size_t>(sent);
        return currPos == 0;
    }

    int Server::initServer(const char* serverIP, const int serverPort, int serverBacklog) {
        // Create sockaddr struct for port + IP binding
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);
        serverAddr.sin_port = htons(serverPort);

        // Create server socket
        int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_fd == -1) {
            std::cerr << "Socket could not be initialized.\n";
            return -1;
        }

        // Set SO_REUSEADDR option
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            std::cerr << "Error setting SO_REUSEADDR option.\n";
            return -1;
        }

        // Set server to nonblocking mode
        int fcntl_flags {fcntl(server_fd, F_GETFL, 0)};
        if(fcntl_flags == -1 || fcntl(server_fd, F_SETFL, fcntl_flags | O_NONBLOCK) == -1) {
            std::cerr << "Socket could not be set to nonblocking mode.\n";
            close(server_fd);
            return -1;
        }

        // Bind socket to IP and Port
        if (bind(server_fd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) == -1) {
            std::cerr << "Error binding socket to port.\n";
            return -1;
        }

        // Listen for incomming connections
        if (listen(server_fd, serverBacklog) == -1) {
            std::cerr << "Error listening to bound socket.\n";
            return -1;
        }

        return server_fd;
    }

    int Server::getServerFD() const {
        return server_fd;
    }

    std::vector<pollfd> Server::createPollInput() {
        std::vector<pollfd> result;
        for (const std::pair<const int, SOCKET_INFO_VALUE_TYPE> &p: socketInfo)
            result.push_back({p.first, std::get<0>(p.second), 0});
        return result;
    }
    
    Server::Server(const char* serverIP, const int serverPort, int serverBacklog, const char* dbFP): 
        handler(dbFP) {

        // Initialize server socket and start listening
        server_fd = initServer(serverIP, serverPort, serverBacklog);

        // Mark this as ready to recv
        socketInfo[server_fd] = {POLLIN, "", 0, std::stack<std::tuple<char, std::size_t>>{}};
    }

    void Server::closeSockets(int) {
        serverRunning = false;
        for (const std::pair<const int, SOCKET_INFO_VALUE_TYPE> &kv: socketInfo)
            close(kv.first);
    }

    void Server::run() {

        // Loop until interupt
        std::signal(SIGINT, closeSockets);
        while (serverRunning) {

            // Poll for connections ready to recv from or send to
            std::vector<pollfd> pollInput {createPollInput()};
            int pollResult {poll(pollInput.data(), pollInput.size(), -1)};
            if (pollResult == -1) {
                if (serverRunning) {
                    std::cerr << "Poll failed.\n"; 
                    closeSockets(0);
                } break;
            } 

            for (std::size_t i{0}; i < pollInput.size(); i++) {
                // Accept incomming connection
                if ((pollInput[i].revents & POLLIN) && (pollInput[i].fd == server_fd)) {
                    sockaddr_in clientAddr;
                    socklen_t addr_size {sizeof(clientAddr)};
                    int client_fd {accept(server_fd, reinterpret_cast<sockaddr*>(&clientAddr), &addr_size)};
                    if (serverRunning && client_fd != -1) {
                        // Set client to nonblocking mode
                        int fcntl_flags {fcntl(client_fd, F_GETFL, 0)};
                        if(fcntl_flags == -1 || fcntl(client_fd, F_SETFL, fcntl_flags | O_NONBLOCK) == -1) {
                            std::cerr << "Socket could not be set to nonblocking mode.\n";
                            close(client_fd);
                        }

                        // Add new client to socketinfo
                        socketInfo[client_fd] = {POLLIN, "", 0, {}};
                    }
                } 

                // Recv from client and process it
                else if ((pollInput[i].revents & POLLIN) && (pollInput[i].fd != server_fd)) {
                    // Extract variables from socketInfo
                    int client_fd {pollInput[i].fd};
                    std::string &request {std::get<1>(socketInfo[client_fd])};
                    std::size_t &currPos {std::get<2>(socketInfo[client_fd])};
                    std::stack<std::tuple<char, std::size_t>> &stk {std::get<3>(socketInfo[client_fd])};

                    // Read request from client
                    bool status {(Redis::Server::readRequest(client_fd, request))};
                    if (!status) {
                        // Read pending data
                        socketInfo[client_fd] = {POLLIN, request, currPos, stk};
                    } else {
                        if (request == "-Error receiving data\r\n") {
                            // Close and erase the socket
                            close(client_fd); socketInfo.erase(client_fd); 
                        } else {
                            // Process the request and prepare to send data to client
                            std::string serializedResponse {handler.handleRequest(request)};
                            socketInfo[client_fd] = {POLLOUT, serializedResponse, serializedResponse.size(), {}};
                        }
                    }
                }

                // Sent to client and reset its state back to POLLIN
                else if((pollInput[i].revents & POLLOUT) && (pollInput[i].fd != server_fd)) {
                    // Extract variables from socketInfo
                    int client_fd {pollInput[i].fd};
                    std::string &response {std::get<1>(socketInfo[client_fd])};
                    std::size_t &currPos {std::get<2>(socketInfo[client_fd])};

                    // Send pending data or reset
                    if (!Redis::Server::sendResponse(client_fd, response, currPos)) socketInfo[client_fd] = {POLLOUT, response, currPos, {}};
                    else socketInfo[client_fd] = {POLLIN, "", 0, {}};
                }
            }
        }

        // Cleanup
        close(server_fd);

    }
}
