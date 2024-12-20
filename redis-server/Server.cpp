#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <stack>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <fcntl.h>
#include <poll.h>
#include <vector>

#include "include/Node.hpp"
#include "include/Cache.hpp"

bool serverRunning {true};

using SOCKET_INFO_VALUE_TYPE = std::tuple<short, std::string, std::size_t, std::stack<std::tuple<char, std::size_t>>>;
std::unordered_map<int, SOCKET_INFO_VALUE_TYPE> socketInfo;

void closeSockets(int signal_) {
    serverRunning = false;
    for (const std::pair<const int, SOCKET_INFO_VALUE_TYPE> &p: socketInfo)
        close(p.first);
}

// Recv request from client return true / false based on status
bool readRequest(int client_fd, std::string &request, std::size_t &currPos, std::stack<std::tuple<char, std::size_t>> &stk) {
    // Dummy message to return on error
    const std::string errorMessage = "-Error receiving data\r\n";

    // Read one portion at a time
    char buffer[1024] = {0};
    if (recv(client_fd, buffer, sizeof(buffer), 0) == -1) {
        request = errorMessage;
        return true;
    }

    // Append received data into request string
    request += buffer;

    // Try to check if received data is 'complete', return false otherwise
    while (currPos < request.size()) {
        // We check for req end usually by counting '\r\n' except
        // when we are parsing a bulk string, in which case we count chars
        if (stk.empty() || std::get<0>(stk.top()) != '$') {
            std::size_t tokEnd {request.find("\r\n", currPos)};
            if (tokEnd == std::string::npos) { return false; } 
            else {
                if (request.at(currPos) == '$' || request.at(currPos) == '*') {
                    std::size_t aggLength {
                        request.at(currPos + 1) == '-'? 0: 
                        std::stoull(request.substr(currPos + 1, tokEnd - currPos - 1))
                    };
                    stk.push({request.at(currPos), aggLength});
                } else { stk.push({request.at(currPos), 0}); }

                currPos = tokEnd + 2;
            }
        } 

        // We are currently parsing a bulk string
        else {
             std::size_t bulkStrAddLength {std::min(std::get<1>(stk.top()), request.size() - currPos)};
             stk.top() = {'$', std::get<1>(stk.top()) - bulkStrAddLength};
             currPos += bulkStrAddLength;

             // If complete, update currPos to include '\r\n'
             if (std::get<1>(stk.top()) == 0) currPos += 2;
             else return false;
        }

        // Pop the top most if we can and add to prev guy
        while (!stk.empty() && std::get<1>(stk.top()) == 0) {
             stk.pop();
             if (stk.empty()) return true;
             else stk.top() = {'*', std::get<1>(stk.top()) - 1};
        }
    }

    return false;
}

// Send response to client return true / false based on status
bool sendResponse(int client_fd, std::string &response, std::size_t &currPos) {
    long sent {send(client_fd, response.c_str(), currPos, 0)};
    if (sent == -1) {
        std::cerr << "Sending response to client failed.\n";
        response = ""; currPos = 0;
        return true;
    }

    response = response.substr(static_cast<std::size_t>(sent));
    currPos -= static_cast<std::size_t>(sent);
    return currPos == 0;
}

std::string handleCommandPing(std::shared_ptr<Redis::AggregateRedisNode> &args) {
    if (args->size() == 0) {
        return Redis::PlainRedisNode("PONG").serialize();
    } else if (args->size() == 1) {
        return args->front()->cast<Redis::VariantRedisNode>()->serialize();
    } else {
        return Redis::PlainRedisNode("Wrong number of arguments for 'ping' command", false).serialize();
    }
}

std::string handleCommandEcho(std::shared_ptr<Redis::AggregateRedisNode> &args) {
    if (args->size() == 1) {
        return args->front()->cast<Redis::VariantRedisNode>()->serialize();
    } else {
        return Redis::PlainRedisNode("Wrong number of arguments for 'echo' command", false).serialize();
    }
}

std::string handleCommandSet(std::shared_ptr<Redis::AggregateRedisNode> &args, Redis::Cache &cache) {
    if (args->size() == 2) {
        std::string key {std::get<std::string>((*args)[0]->cast<Redis::VariantRedisNode>()->getValue())};
        std::string value {std::get<std::string>((*args)[1]->cast<Redis::VariantRedisNode>()->getValue())};
        cache[key] = std::make_shared<Redis::PlainRedisNode>(value);
        return Redis::PlainRedisNode("OK").serialize();
    } else {
        return Redis::PlainRedisNode("Wrong number of arguments for 'set' command", false).serialize();
    }
}

std::string handleCommandGet(std::shared_ptr<Redis::AggregateRedisNode> &args, Redis::Cache &cache) {
    if (args->size() == 1) {
        std::string key {std::get<std::string>((*args)[0]->cast<Redis::VariantRedisNode>()->getValue())};
        return cache.exists(key)? cache[key]->serialize(): Redis::VariantRedisNode(nullptr).serialize();
    } else {
        return Redis::PlainRedisNode("Wrong number of arguments for 'get' command", false).serialize();
    }
}

std::string handleRequest(std::string &request, Redis::Cache &cache) {
    // Process the request
    std::shared_ptr<Redis::RedisNode> reqNode {Redis::RedisNode::deserialize(request)};
    std::string command;
    std::shared_ptr<Redis::AggregateRedisNode> args;
    if (reqNode->getType() == Redis::NODE_TYPE::PLAIN) {
        std::shared_ptr<Redis::PlainRedisNode> plainNode {reqNode->cast<Redis::PlainRedisNode>()};
        command = plainNode->getMessage();
        args = std::make_shared<Redis::AggregateRedisNode>();
    } else if (reqNode->getType() == Redis::NODE_TYPE::VARIANT) {
        std::shared_ptr<Redis::VariantRedisNode> varNode = reqNode->cast<Redis::VariantRedisNode>();
        command = std::get<std::string>(varNode->getValue());
        args = std::make_shared<Redis::AggregateRedisNode>();
    } else {
        std::shared_ptr<Redis::AggregateRedisNode> aggNode{reqNode->cast<Redis::AggregateRedisNode>()};
        std::shared_ptr<Redis::VariantRedisNode>   varNode{aggNode->front()->cast<Redis::VariantRedisNode>()};
        aggNode->pop_front();
        command = std::get<std::string>(varNode->getValue());
        args = aggNode;
    }

    // Convert to lower case
    std::transform(command.begin(), command.end(), command.begin(), [](const char ch) { 
        return std::tolower(ch); 
    });

    // Prepare a suitable response
    std::string serializedResponse;
    if (command == "ping")
        serializedResponse = handleCommandPing(args);
    else if (command == "echo")
        serializedResponse = handleCommandEcho(args);
    else if (command == "set")
        serializedResponse = handleCommandSet(args, cache);
    else if (command == "get")
        serializedResponse = handleCommandGet(args, cache);
    else
        serializedResponse = Redis::PlainRedisNode("Not supported", false).serialize();

    // Return serialized response
    return serializedResponse;
}

std::vector<pollfd> createPollInput() {
    std::vector<pollfd> result;
    for (const std::pair<const int, SOCKET_INFO_VALUE_TYPE> &p: socketInfo)
        result.push_back({p.first, std::get<0>(p.second), 0});
    return result;
}

int main() {

    constexpr const char* SERVER_IP {"0.0.0.0"};
    const std::uint16_t PORT {static_cast<std::uint16_t>(6379)};
    constexpr int SOCKET_BACKLOG {10};

    // Create an instance of Redis Cache
    Redis::Cache cache;

    // Create sockaddr struct for port + IP binding
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);

    // Create server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == -1) {
        std::cerr << "Socket could not be initialized.\n";
        std::exit(1);
    }

    // Set SO_REUSEADDR option
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "Error setting SO_REUSEADDR option.\n";
        std::exit(1);
    }

    // Set server to nonblocking mode
    int fcntl_flags {fcntl(server_fd, F_GETFL, 0)};
    if(fcntl_flags == -1 || fcntl(server_fd, F_SETFL, fcntl_flags | O_NONBLOCK) == -1) {
        std::cerr << "Socket could not be set to nonblocking mode.\n";
        close(server_fd);
        std::exit(1);
    }

    // Bind socket to IP and Port
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) == -1) {
        std::cerr << "Error binding socket to port.\n";
        std::exit(1);
    }

    // Listen for incomming connections
    if (listen(server_fd, SOCKET_BACKLOG) == -1) {
        std::cerr << "Error listening to bound socket.\n";
        std::exit(1);
    }

    // Add server_fd to list of sockets to monitor and maintain
    socketInfo[server_fd] = {POLLIN, "", 0, std::stack<std::tuple<char, std::size_t>>{}};

    std::signal(SIGINT, closeSockets);
    while (serverRunning) {
        std::vector<pollfd> pollInput {createPollInput()};
        int pollResult {poll(pollInput.data(), pollInput.size(), -1)};
        if (pollResult == -1) {
            std::cerr << "Poll failed.\n";
            closeSockets(0); break;
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
                bool status {(readRequest(client_fd, request, currPos, stk))};
                if (!status) {
                    // Read pending data
                    socketInfo[client_fd] = {POLLIN, request, currPos, stk};
                } else {
                    // Process the request and prepare to send data to client
                    std::string serializedResponse {handleRequest(request, cache)};
                    socketInfo[client_fd] = {POLLOUT, serializedResponse, serializedResponse.size(), {}};
                }
            }

            // Sent to client and reset its state back to POLLIN
            else if((pollInput[i].revents & POLLOUT) && (pollInput[i].fd != server_fd)) {
                // Extract variables from socketInfo
                int client_fd {pollInput[i].fd};
                std::string &response {std::get<1>(socketInfo[client_fd])};
                std::size_t &currPos {std::get<2>(socketInfo[client_fd])};

                // Send pending data or reset
                if (!sendResponse(client_fd, response, currPos)) socketInfo[client_fd] = {POLLOUT, response, currPos, {}};
                else socketInfo[client_fd] = {POLLIN, "", 0, {}};
            }
        }
    }

    close(server_fd);
    return 0;
}
