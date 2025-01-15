#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "include/HashUtils.hpp"

constexpr const char* IP {"127.0.0.1"};
constexpr int PORT {8080};
constexpr int SERVER_BACKLOG {10};

using SOCKET_INFO_VALUE_TYPE = std::tuple<short, std::string, long>;

bool serverRunning {true};
int serverSocket;

std::unordered_map<int, SOCKET_INFO_VALUE_TYPE> socketInfo;
std::unordered_map<std::string, std::string> cache, revCache;

void closeSockets(int = 0) {
    serverRunning = false;
    for (const std::pair<const int, SOCKET_INFO_VALUE_TYPE> &kv: socketInfo)
        close(kv.first);
}

std::pair<unsigned short, std::string> validatePostBody(std::string &body) {
    int keyCount {0};
    char quote, brace {'\0'};
    std::string key, value;
    for (std::size_t idx {0}; idx < body.size(); idx++) {
        char ch {body.at(idx)};
        if (ch == '{' || ch == '}') {
            if ((!brace && ch == '}') || brace == ch)
                return {400, "Invalid JSON"};
            else
                brace = ch;
        } else if (ch == '\'' || ch == '"') {
            quote = ch;
            std::string acc;
            while (++idx < body.size() && body[idx] != quote) {
                acc += body.at(idx);
                if (body.at(idx) == '\\')
                    acc += body.at(++idx);
            }

            if (acc.empty() || !value.empty()) 
                return {400, "Empty String"};
            else if (key.empty())
                key = acc;
            else
                value = acc;
        } else if (ch == ':') {
            keyCount++;
        }
    }

    if (keyCount != 1) 
        return {400, "Only 1 key allowed: 'url'"};
    else if (key != "url") 
        return {400,  "Only 1 key allowed: 'url'"};
    else if (value.empty()) 
        return {400, "Empty URL"};
    else if (brace != '}') 
        return {400, "Invalid JSON"};
    else 
        return {200, value};
}

std::string shortenURL(std::string &longURL) {
    if (cache.find(longURL) == cache.end()) {
        std::size_t idx {cache.size()};
        std::vector<std::uint8_t> encryptedBytes {encryptSizeT(idx, "secret")};
        std::string shortURL {base62EncodeBytes(encryptedBytes)};
        revCache[shortURL] = longURL;
        cache[longURL] = shortURL;
    }

    return cache[longURL];
}

std::string extractRequestURL(const std::string &buffer, const std::string &requestTypeStr) {
    std::size_t reqTypeLen {requestTypeStr.size()};
    std::size_t URLStart {reqTypeLen + 2}, URLEnd {buffer.find(' ', reqTypeLen + 2)};
    return URLEnd == std::string::npos? "": buffer.substr(URLStart, URLEnd - URLStart);
}

std::vector<pollfd> createPollFDs() {
    std::vector<pollfd> result;
    for (const std::pair<const int, SOCKET_INFO_VALUE_TYPE> &kv: socketInfo)
        result.push_back({kv.first, std::get<0>(kv.second), 0});
    return result;
}

// Read request chunk by chunk - for ASYNC
bool readRequest(int clientSocket, std::string &buffer, long &pendingBytes) {
    char raw_buffer[1024] = {0};
    long recvd = recv(clientSocket, raw_buffer, sizeof(raw_buffer), 0);
    if (recvd <= 0) { buffer = "Error receiving data"; return true; }
    buffer += raw_buffer;

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

bool sendResponse(int clientSocket, const std::string &response, long &pendingBytes) {
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

std::string processRequest(const std::string &buffer) {
    // Prepare string for response
    std::string responseHeaders {"\r\nContent-Type: application/json\r\n"};
    unsigned short responseCode {200};
    std::string responseBody;

    // Accept POST, GET, DELETE
    std::size_t pos {buffer.find("\r\n\r\n")};
    if (pos == std::string::npos) {
        responseCode = 400;
        responseBody = "Invalid HTTP format.";
    } else if (buffer.starts_with("GET")) {
        std::string shortURL {extractRequestURL(buffer, "GET")};
        if (shortURL.empty()) {
            responseCode = 400;
            responseBody = "Invalid request";
        } else if (revCache.find(shortURL) != revCache.end()) {
            responseCode = 302;
            responseHeaders += "location: " + revCache[shortURL] + "\r\n";
        } else {
            responseCode = 404;
            responseBody = "URL not found";
        }
    } else if (buffer.starts_with("POST")) {
        // Read the first '\r\n\r\n'
        std::string postBody {buffer.substr(pos + 4)};

        // Validate post body - only {'url': <long_url>} is supported
        std::pair<unsigned short, std::string> parsedPostBody {validatePostBody(postBody)};
        responseCode = parsedPostBody.first;
        if (responseCode == 200) {
            std::string shortURL {shortenURL(parsedPostBody.second)};
            responseBody = std::format(
                R"({{"key": "{}", "long_url": "{}", "short_url": "localhost:{}/{}"}})", 
                shortURL, parsedPostBody.second, PORT, shortURL
            );
        } else {
            responseBody = parsedPostBody.second;
        }
    } else if (buffer.starts_with("DELETE")) {
        std::string shortURL {extractRequestURL(buffer, "DELETE")};
        if (shortURL.empty()) {
            responseCode = 400;
            responseBody = "Invalid request";
        } else if (revCache.find(shortURL) != revCache.end()) {
            std::string longURL {revCache[shortURL]};
            cache.erase(longURL);
            revCache.erase(shortURL);
            responseCode = 200;
        } else {
            responseCode = 404;
            responseBody = "URL Not found";
        }
    } else {
        responseCode = 405;
        responseBody = "Request method unknown or is not supported";
    }

    // Craft the response
    responseBody += "\r\n";
    std::string response {"HTTP/1.1 "}; 
    response += std::to_string(responseCode);
    response += responseHeaders;
    response += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n\r\n";
    response += responseBody;

    // Return the response as string
    return response;
}

int main() {
    
    // Init socket
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == -1) {
        std::cerr << "Error creating socket object.\n";
        std::exit(1);
    }

    // Set SO_REUSEADDR option
    int opt {1};
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "Error setting SO_REUSEADDR option.\n";
        std::exit(1);
    }

    // Set server to nonblocking mode
    int fcntl_flags {fcntl(serverSocket, F_GETFL, 0)};
    if(fcntl_flags == -1 || fcntl(serverSocket, F_SETFL, fcntl_flags | O_NONBLOCK) == -1) {
        std::cerr << "Error setting socket into non-blocking mode.\n";
        close(serverSocket);
        return -1;
    }

    // Bind socket
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, IP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) == -1) {
        std::cerr << "Failed to bind to specified port.\n";
        std::exit(1);
    }

    // Start listening for incoming connections
    if (listen(serverSocket, SERVER_BACKLOG) == -1) {
        std::cerr << "Error starting a listener.\n";
        std::exit(1);
    }

    // Ready to receive data from Server
    socketInfo[serverSocket] = {POLLIN, "", 0};

    std::cout << "Up & running on port: " << PORT << "\n";

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
                    std::string response {processRequest(request)};
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
