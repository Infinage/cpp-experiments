#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <stack>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "Node.hpp"

std::atomic<bool> serverRunning {true};

int temp_socket_id;

void handleInterupt(int signal_) {
    serverRunning = false;
    close(temp_socket_id);
    std::exit(1);
}

// Recv request from client until complete
std::string readRequest(int client_fd) {
    // Variables to store & process request 
    std::string request {};
    std::size_t currPos {0};
    bool reqData {true};
    std::stack<std::tuple<char, std::size_t>> stk;

    // Dummy message to return on error
    std::string errorMessage = "-Error receiving data\r\n";

    // Recv until all of request is read
    while (reqData || currPos < request.size()) {
        char buffer[1024] = {0};
        bool recvOk {!reqData || recv(client_fd, buffer, sizeof(buffer), 0) != -1};
        if (!recvOk) return errorMessage;

        // Append received data into request string
        request += buffer;

        // We check for req end usually by counting '\r\n' except
        // when we are parsing a bulk string, in which case we count chars
        if (stk.empty() || std::get<0>(stk.top()) != '$') {
            std::size_t tokEnd {request.find("\r\n", currPos)};
            if (tokEnd == std::string::npos) { reqData = true; } 
            else {
                if (request.at(currPos) == '$' || request.at(currPos) == '*') {
                    std::size_t aggLength {
                        request.at(currPos + 1) == '-'? 0: 
                        std::stoull(request.substr(currPos + 1, tokEnd - currPos - 1))
                    };
                    stk.push({request.at(currPos), aggLength});
                } else { stk.push({request.at(currPos), 0}); }

                reqData = false;
                currPos = tokEnd + 2;
            }
        } 

        // We are currently parsing a bulk string
        else {
             std::size_t bulkStrAddLength {std::min(std::get<1>(stk.top()), request.size() - currPos)};
             stk.top() = {'$', std::get<1>(stk.top()) - bulkStrAddLength};
             currPos += bulkStrAddLength;

             // If we have parsed the bulk string, update currPos to include '\r\n'
             // Else request more data
             if (std::get<1>(stk.top()) == 0) currPos += 2;
             else reqData = true;
        }

        // Pop the top most if we can and add to prev guy
        while (!stk.empty() && std::get<1>(stk.top()) == 0) {
             stk.pop();
             if (stk.empty()) return request;
             else stk.top() = {'*', std::get<1>(stk.top()) - 1};
        }
    }
    
    return errorMessage;
}

void handleClient(int client_fd, const std::string clientIP) {
    // Read request from client
    std::string serializedRequest {readRequest(client_fd)};
    std::cout << Redis::RedisNode::deserialize(serializedRequest)->serialize() << "\n";
    std::string respMsg {Redis::PlainRedisNode("OK").serialize()};
    send(client_fd, respMsg.c_str(), respMsg.size(), 0);
    close(client_fd);
}

int main() {

    constexpr const char* SERVER_IP {"0.0.0.0"};
    const std::uint16_t PORT {static_cast<std::uint16_t>(6379)};
    constexpr int SOCKET_BACKLOG {10};

    // Create server socket
    int server_fd {socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    if (server_fd == -1) {
        std::cerr << "Socket could not be initialized.\n";
        std::exit(1);
    }

    // DEBUG
    temp_socket_id = server_fd;

    // Set SO_REUSEADDR option
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "Error setting SO_REUSEADDR option.\n";
        std::exit(1);
    }

    // Bind socket to IP and Port
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) == -1) {
        std::cerr << "Error binding socket to port.\n";
        std::exit(1);
    }

    // Listen for incomming connections
    if (listen(server_fd, SOCKET_BACKLOG) == -1) {
        std::cerr << "Error listening to bound socket.\n";
        std::exit(1);
    }

    std::signal(SIGINT, handleInterupt);
    while (serverRunning) {
        // Accept incomming connection
        sockaddr_in clientAddr;
        socklen_t addr_size {sizeof(clientAddr)};
        int client_fd {accept(server_fd, reinterpret_cast<sockaddr*>(&clientAddr), &addr_size)};

        // Parse the IP client address
        in_addr ipAddr {clientAddr.sin_addr};
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, clientIP, INET_ADDRSTRLEN);

        if (serverRunning && client_fd != -1) {
            handleClient(client_fd, std::string{clientIP});;
        }
    }

    close(server_fd);

    return 0;
}
