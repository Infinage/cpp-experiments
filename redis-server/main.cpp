/*
   redis-cli set abc 123123adasdasdsadsadsadasdasdasdasdsaddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddasdasdasdasdasdasdsadasdsadsadsadsadadsadasdsadsadasdsadsadsadasdasdqweqwedasassadasdweqwqwdwdasdasfweqwdasdsadqwqwfadsdasasfqweqwdadasddasdasdasdasddqwewqihuasdsadasdlsaldhsaldhlasdhlsakhdksalxlklqlqwjdlwdlqwlnqwdlqwjmwqkwopqjowhdpwhdpqwdqwmdwqpodkqwpdqwhpdqwdjwqowqpowqdowkdqwdoqkdwqjdpqwopqwohpoqwfnwqonopfpwnwmdoqpdokwqdpwqdonwdqwodqwodkqwpdhqwpdhqwpdomwqdpqdqopwdopwqmdomqwodpqwopdhwqodwqndowqdomqwodwqoqwopqwojuwqprupqwfoqwfmowmdwomdpqndpowqnqwohqwojdqwpwqodpqwoqwmqwpmmwq,xqwpojqwpfhqhqwipdhqwdnqwxwoqmdqowmxqwndqwidhqwopdnqwmomxqwowq,fmnqwodnqwopjqwojdqowndoqwmopwqmndpoqwhdjwqpmoqwpmxpqwxwqodjqwoxwpqnpqwhjqwopmqwpoxmqwopoqpowhfownqowocmqwpo,qwop,xqwqpwonqasdadasdasdasdasdsadasdasdsadasasdasdasasdasdasdasdsadasdasdasdsadsadasdasdasdsadasdasa

   Fix this
 */
#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <charconv>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <stack>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <vector>

#include "include/Node.hpp"
#include "include/Server.hpp"
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
bool readRequest(int client_fd, std::string &request) {
    const char *recvError = "-Error receiving data\r\n", *invalidInp = "-Invalid input data\r\n";

    // Read one portion at a time
    char buffer[1024] = {0};
    if (recv(client_fd, buffer, sizeof(buffer), 0) <= 0) {
        request = recvError;
        return true;
    }

    // Append received data into request string
    request += buffer;

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
bool sendResponse(int client_fd, std::string &response, std::size_t &currPos) {
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

std::string handleCommandPing(std::vector<std::string> &args) {
    if (args.size() == 1) {
        return Redis::PlainRedisNode("PONG").serialize();
    } else if (args.size() == 2) {
        return Redis::VariantRedisNode(args.back()).serialize();
    } else {
        return Redis::PlainRedisNode("Wrong number of arguments for 'ping' command", false).serialize();
    }
}

std::string handleCommandEcho(std::vector<std::string> &args) {
    if (args.size() == 2) {
        return Redis::VariantRedisNode(args.back()).serialize();
    } else {
        return Redis::PlainRedisNode("Wrong number of arguments for 'echo' command", false).serialize();
    }
}

std::string handleCommandSet(std::vector<std::string> &args, Redis::Cache &cache) {
    if (args.size() >= 3) {
        // Store the value at specified key
        std::string &key {args[1]};
        std::string &value {args[2]};
        cache.setValue(key, std::make_shared<Redis::VariantRedisNode>(value));

        // Set the expiry
        if (args.size() >= 5) {
            for (std::size_t i {3}; i < args.size() - 1; i++) {
                // Extract this and the next
                std::string &expiryCode {args[i]};
                std::string &expiry {args[i + 1]};

                // To lower case
                Redis::lower(expiryCode);

                // Check if code is valid and expiry code
                bool isValidCode {expiryCode == "ex" || expiryCode == "exat" || expiryCode == "px" || expiryCode == "pxat"}, 
                     isValidExpiry {isValidCode && Redis::allDigitsUnsigned(expiry.cbegin(), expiry.cend())};

                if (!isValidCode)        { continue; }
                else if (!isValidExpiry) { return Redis::PlainRedisNode("Invalid syntax", false).serialize(); }
                else if (expiryCode ==   "ex") {    cache.setTTLS(key, std::stoul(expiry)); break; }
                else if (expiryCode ==   "px") {   cache.setTTLMS(key, std::stoul(expiry)); break; }
                else if (expiryCode == "exat") {  cache.setTTLSAt(key, std::stoul(expiry)); break; }
                else if (expiryCode == "pxat") { cache.setTTLMSAt(key, std::stoul(expiry)); break; }
            }
        }

        // Send response
        return Redis::PlainRedisNode("OK").serialize();

    } else {

        return Redis::PlainRedisNode("Wrong number of arguments for 'set' command", false).serialize();
    }
}

std::string handleCommandGet(std::vector<std::string> &args, Redis::Cache &cache) {
    if (args.size() == 2) {
        std::string &key {args[1]};
        return cache.getValue(key)->serialize();
    } else {
        return Redis::PlainRedisNode("Wrong number of arguments for 'get' command", false).serialize();
    }
}

std::string handleCommandExists(std::vector<std::string> &args, Redis::Cache &cache) {
    long result {0};
    for (std::size_t i{1}; i < args.size(); i++) {
        std::string &arg {args[i]};
        if (cache.exists(arg)) {
            result++;
        }
    }
    return Redis::VariantRedisNode(result).serialize();
}

std::string handleCommandDel(std::vector<std::string> &args, Redis::Cache &cache) {
    long result {0};
    for (std::size_t i{1}; i < args.size(); i++) {
        std::string &arg {args[i]};
        if (cache.exists(arg)) {
            result += !cache.expired(arg);
            cache.erase(arg); 
        }
    }
    return Redis::VariantRedisNode(result).serialize();
}

std::string handleCommandLAdd(std::vector<std::string> &args, Redis::Cache &cache, long by) {
    if (args.size() == 2) {
        std::string &key {args[1]};
        if (!cache.exists(key) || cache.expired(key)) {
            cache.setValue(key, std::make_shared<Redis::VariantRedisNode>(std::to_string(by)));
            return cache.getValue(key)->serialize();
        } else {
            const std::string &value {cache.getValue(key)->cast<Redis::VariantRedisNode>()->str()};
            if (Redis::allDigitsSigned(value.cbegin(), value.cend())) {
                cache.setValue(key, std::make_shared<Redis::VariantRedisNode>(std::to_string(std::stol(value) + by)));
                return cache.getValue(key)->serialize();
            } else {
                return Redis::PlainRedisNode("value is not an integer or out of range", false).serialize();
            }
        }
    } else {
        return Redis::PlainRedisNode("Wrong number of arguments for 'incr' command", false).serialize();
    }
}

std::string handleCommandTTL(std::vector<std::string> &args, Redis::Cache &cache) {
   if (args.size() == 2) {
        std::string &key {args[1]};
        long ttlVal {cache.getTTL(key)};
        return Redis::VariantRedisNode(ttlVal > 0? ttlVal / 1000: ttlVal).serialize();
   } else {
        return Redis::PlainRedisNode("Wrong number of arguments for 'ttl' command", false).serialize();
   }
}

std::string handleCommandLRange(std::vector<std::string> &args, Redis::Cache &cache) {
    if (args.size() == 4) {
        std::string &key {args[1]};
        if (!Redis::allDigitsSigned(args[2].begin(), args[2].end()) || !Redis::allDigitsSigned(args[3].begin(), args[3].end())) {
            return Redis::PlainRedisNode("Value is not an integer or out of range", false).serialize();
        } else if (!cache.exists(key) || cache.expired(key)) {
            return Redis::AggregateRedisNode().serialize();
        } else if (cache.getValue(key)->getType() != Redis::NODE_TYPE::AGGREGATE) {
            return Redis::PlainRedisNode("WRONGTYPE Operation against a key holding the wrong kind of value", false).serialize();
        } else {
            std::shared_ptr<Redis::AggregateRedisNode> value {cache.getValue(key)->cast<Redis::AggregateRedisNode>()};
            Redis::AggregateRedisNode result;
            long N{static_cast<long>(value->size())}, left {std::stol(args[2])}, right {std::stol(args[3])};
            if (left < 0) left = N + left;
            if (right < 0) right = N + right;
            left = std::max(left, 0L); right = std::min(right, N - 1);
            for (long curr {left}; curr <= right; curr++)
                result.push_back((*value)[curr]);
            return result.serialize();
        }
    } else {
        return Redis::PlainRedisNode("ERR wrong number of arguments for command", false).serialize();
    }
}

std::string handleCommandPush(std::vector<std::string> &args, Redis::Cache &cache, bool pushBack) {
    if (args.size() >= 3) {
        std::string &key {args[1]};
        bool exists {cache.exists(key) && !cache.expired(key)}, 
             isAggNode {exists && cache.getValue(key)->getType() == Redis::NODE_TYPE::AGGREGATE};

        // Exists but of wrong data type
        if (exists && !isAggNode)
            return Redis::PlainRedisNode("WRONGTYPE Operation against a key holding the wrong kind of value", false).serialize();

        // If it doesn't exist, create new
        if (!exists)
            cache.setValue(key, std::make_shared<Redis::AggregateRedisNode>());

        // Start inserting into the aggNode
        long result {0};
        std::shared_ptr<Redis::AggregateRedisNode> aggNode {cache.getValue(key)->cast<Redis::AggregateRedisNode>()};
        for (std::size_t i{2}; i < args.size(); i++) {
            if (pushBack) aggNode->push_back(std::make_shared<Redis::VariantRedisNode>(args[i])); 
            else aggNode->push_front(std::make_shared<Redis::VariantRedisNode>(args[i])); 
            result++;
        }
        return Redis::VariantRedisNode(result).serialize();

    } else {
        return Redis::PlainRedisNode("ERR wrong number of arguments for command", false).serialize();
    }
}

std::string handleCommandLLen(std::vector<std::string> &args, Redis::Cache &cache) {
    if (args.size() == 2) {
        std::string &key {args[1]};
        if (!cache.exists(key) || cache.expired(key)) {
            return Redis::VariantRedisNode(0).serialize();
        } else if (cache.getValue(key)->getType() != Redis::NODE_TYPE::AGGREGATE) {
            return Redis::PlainRedisNode("WRONGTYPE Operation against a key holding the wrong kind of value", false).serialize();
        } else {
            std::size_t N {cache.getValue(key)->cast<Redis::AggregateRedisNode>()->size()};
            return Redis::VariantRedisNode(static_cast<long>(N)).serialize();
        }
    } else {
        return Redis::PlainRedisNode("ERR wrong number of arguments for command", false).serialize();
    }
}

std::string handleCommandSave(std::vector<std::string> &args, Redis::Cache &cache, bool background = false) {
    if (args.size() != 1) {
        return Redis::PlainRedisNode("ERR wrong number of arguments for command", false).serialize();
    } else if (background) {
        int pid {fork()};
        if (pid == -1) {
            return Redis::PlainRedisNode("Save failed", false).serialize();
        } else if (pid == 0) {
            bool status {cache.save("dump.rdb")}; std::exit(!status);
        } else {
            return Redis::PlainRedisNode("OK").serialize();
        }
    } else {
        if (cache.save("dump.rdb")) return Redis::PlainRedisNode("OK").serialize();
        else return Redis::PlainRedisNode("Save failed", false).serialize();
    }
}

std::string handleRequest(std::string &request, Redis::Cache &cache) {
    // Process the request
    std::shared_ptr<Redis::RedisNode> reqNode {Redis::RedisNode::deserialize(request)};
    std::vector<std::string> args; std::string command;
    if (reqNode->getType() != Redis::NODE_TYPE::AGGREGATE) {
        command = "missing";
    } else {
        args = reqNode->cast<Redis::AggregateRedisNode>()->vector();
        command = args[0];
    }

    // Convert to lower case
    Redis::lower(command);

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
    else if (command == "exists")
        serializedResponse = handleCommandExists(args, cache);
    else if (command == "del")
        serializedResponse = handleCommandDel(args, cache);
    else if (command == "incr")
        serializedResponse = handleCommandLAdd(args, cache, 1);
    else if (command == "decr")
        serializedResponse = handleCommandLAdd(args, cache, -1);
    else if (command == "ttl")
        serializedResponse = handleCommandTTL(args, cache);
    else if (command == "lrange")
        serializedResponse = handleCommandLRange(args, cache);
    else if (command == "lpush")
        serializedResponse = handleCommandPush(args, cache, false);
    else if (command == "rpush")
        serializedResponse = handleCommandPush(args, cache, true);
    else if (command == "llen")
        serializedResponse = handleCommandLLen(args, cache);
    else if (command == "save")
        serializedResponse = handleCommandSave(args, cache);
    else if (command == "bgsave")
        serializedResponse = handleCommandSave(args, cache, true);
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

    std::string dumpName {"dump.rdb"};
    if (!std::filesystem::exists(dumpName))
        std::cout << "No existing save found. Creating a new instance.\n";
    else if (cache.load(dumpName))
        std::cout << "Load successful.\n";
    else
        std::cout << "Restore failed. Creating a new instance.\n";

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
                bool status {(readRequest(client_fd, request))};
                if (!status) {
                    // Read pending data
                    socketInfo[client_fd] = {POLLIN, request, currPos, stk};
                } else {
                    if (request == "-Error receiving data\r\n") {
                        // Close and erase the socket
                        close(client_fd); socketInfo.erase(client_fd); 
                    } else {
                        // Process the request and prepare to send data to client
                        std::string serializedResponse {handleRequest(request, cache)};
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
                if (!sendResponse(client_fd, response, currPos)) socketInfo[client_fd] = {POLLOUT, response, currPos, {}};
                else socketInfo[client_fd] = {POLLIN, "", 0, {}};
            }
        }
    }

    close(server_fd);
    return 0;
}
