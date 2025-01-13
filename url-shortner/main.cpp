#include <arpa/inet.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

bool serverRunning {true};
int serverSocket;

void HANDLE_INTERUPT(int signal_) {
    std::cout << "Interupt received: " << signal_ << ". Terminating.\n";
    serverRunning = false;
    close(serverSocket);
}

std::string validatePostBody(std::string &body) {
    constexpr const char* invalidMessage {"Invalid Post Body."};
    int keyCount {0};
    char quote, brace {'\0'};
    std::string key, value;
    for (std::size_t idx {0}; idx < body.size(); idx++) {
        char ch {body.at(idx)};
        if (ch == '{' || ch == '}') {
            if ((!brace && ch == '}') || brace == ch)
                return invalidMessage;
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
                return invalidMessage;
            else if (key.empty())
                key = acc;
            else
                value = acc;
        } else if (ch == ':') {
            keyCount++;
        }
    }

    bool validBody {keyCount == 1 && key == "url" && !value.empty() && brace == '}'};
    return validBody? value: invalidMessage;
}

int main() {
    
    constexpr const char* IP {"127.0.0.1"};
    constexpr int PORT {5000};
    constexpr int SERVER_BACKLOG {10};

    // Init socket
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == -1) {
        std::cerr << "Error creating socket object.\n";
        std::exit(1);
    }

    int opt {-1};
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "Failed to set socket options.\n";
        std::exit(1);
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
        std::cerr << "Unsuccessful in starting a listener.\n";
        std::exit(1);
    }

    std::cout << "Up & running on port: " << PORT << "\n";

    std::signal(SIGINT, HANDLE_INTERUPT);
    while (serverRunning) {
        // Connect a client & send a message
        int clientSocket {accept(serverSocket, nullptr, nullptr)};

        // Recv message from client
        char raw_buffer[1024] = {0};
        recv(clientSocket, raw_buffer, sizeof(raw_buffer), 0);
        std::string buffer {raw_buffer};

        // Accept POST, GET, DELETE
        if (buffer.starts_with("GET")) {
            std::cout << "GET\n";
        } else if (buffer.starts_with("POST")) {

            // Read the first '\r\n\r\n'
            std::size_t pos {buffer.find("\r\n\r\n")};
            if (pos == std::string::npos) { std::cerr << "Invalid HTTP format." << "\n"; }
            std::string postBody {buffer.substr(pos + 4)};
            std::cout << postBody << "\n";

            // Validate post body - only {'url': <long_url>} is supported
            std::string url {validatePostBody(postBody)};
            if (url.empty()) {  std::cerr << "Invalid Post Body." << "\n"; }

            std::cout << "Request url: " << url << "\n";

        } else if (buffer.starts_with("DELETE")) {
            std::cout << "DELETE\n";
        } else {
            std::cout << "UNKNOWN\n";
        }

        std::string fileContents {"hello world!\n"};

        // Craft the response
        std::string response {"HTTP/1.1 "}; 
        response += "200";
        response += "\r\nContent-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(fileContents.size()) + "\r\n\r\n";
        response += fileContents;

        send(clientSocket, response.c_str(), response.size(), 0);
        close(clientSocket);
    }
}
