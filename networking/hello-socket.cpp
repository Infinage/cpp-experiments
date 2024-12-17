/*
 * A very simple Socket Program, can work as both server & the client based on input args
 *
 * Server Steps:
 * 1. Create a socket: socket()
 * 2. Bind the socket: bind()
 * 3. Listen on the socket: listen()
 * 4. Accept a connection: accept()
 * 5. Send and receive the data: recv(), send(), recvfrom(), sendto()
 * 6. Disconnect the socket: close()
 *
 * -----------------------------
 *
 * Client Steps:
 * 1. Create a socket: socket()
 * 2. Connect to a socket: connect()
 * 3. Send and receive the data: recv(), send(), recvfrom(), sendto()
 * 4. Disconnect the socket: closesocket()
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include <unistd.h>
#include <vector>

class Socket {
    protected:
        // Static to allow access inside interupt handlers
        // Beware that if your main has both server & client code
        // a same instance is maintained
        static int socket_;

        // Create a sockaddr_in object & cast to sockaddr
        static std::unique_ptr<sockaddr> getSockAddr(const std::string &ip, std::uint16_t port) {
            sockaddr_in *sockaddr_ {new sockaddr_in()};
            sockaddr_->sin_family = AF_INET;
            inet_pton(AF_INET, ip.c_str(), &sockaddr_->sin_addr);
            sockaddr_->sin_port = htons(port);
            return std::unique_ptr<sockaddr>(reinterpret_cast<sockaddr*>(sockaddr_));
        }

        // Create a Socket object
        void initSocket() {
            socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (socket_ == -1) {
                std::cerr << "Error creating socket object.\n";
                std::exit(1);
            }

            // Set SO_REUSEADDR option
            int opt = 1;
            if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
                std::cerr << "Error setting SO_REUSEADDR.\n";
                std::exit(1);
            }
        }
};

class SocketServer: public Socket {
    private:
        static std::atomic<bool> serverRunning;
        
        // Bind Socket to IP & Port
        void bindSocket(const std::string &serverIp, std::uint16_t port) {
            std::unique_ptr<sockaddr> serverAddr {getSockAddr(serverIp, port)};
            int bindStatus {bind(socket_, serverAddr.get(), sizeof(sockaddr_in))};
            if (bindStatus == -1) {
                std::cerr << "Error binding to the socket.\n";
                std::exit(1);
            } 
        }

        // Listen for Connections
        void listenSocket(int socket_, unsigned short connections = 1) {
            int listenStatus {listen(socket_, connections)};
            if (listenStatus == -1) {
                std::cerr << "Error listening on the socket.\n";
                std::exit(1);
            } 
        }

        // Handle a client
        void handleClient(int clientSocket) {
            // Receive data from client
            char buffer [1024] = {0};
            recv(clientSocket, buffer, sizeof(buffer), 0);
            std::cout << "Received from Client: " << buffer << "\n";

            // Send a response back to the client
            std::string message = "Bye Socket!";
            send(clientSocket, message.c_str(), message.size(), 0);

            // Close client socket
            close(clientSocket);
        }

    public:
        void bindAndListen(const std::string &serverIp, std::uint16_t port, unsigned short connections = 3) {
            // Init Socket
            initSocket();

            // Binding socket to IP + PORT
            bindSocket(serverIp, port);

            // Listen for incomming connections
            listenSocket(socket_, connections);

            // Feedback on server
            std::cout << "Server is up and listening on port " << port << ".\n";

            // Handle Clients, stop on Ctrl + C
            std::signal(SIGINT, [](int signal) { serverRunning = false; close(socket_); });
            std::vector<std::thread> clientThreads;
            while (serverRunning) {
                // Accept an incomming connection
                int clientSocket {accept(socket_, nullptr, nullptr)};
                if (clientSocket == -1 || !serverRunning) {
                    close(clientSocket);
                    continue;
                }

                clientThreads.push_back(std::thread([this, clientSocket]() { handleClient(clientSocket); }));
            }

            // Wait for threads to complete before closing
            for (std::thread &t: clientThreads)
                t.join();

            // Cleanup
            close(socket_);
        }
};

class SocketClient: public Socket {
    public:
        void connectAndSend(const std::string &serverIp, std::uint16_t port) {
            // Init Socket
            initSocket();

            // Specifying the address and port of server to connect
            std::unique_ptr<sockaddr> serverAddr {getSockAddr(serverIp, port)};
            if (connect(socket_, serverAddr.get(), sizeof(sockaddr_in)) == -1) {
                std::cerr << "Error connecting to server.\n";
                std::exit(1);
            }

            // Ctrl + C -> ensure that the socket is closed
            std::signal(SIGINT, [](int signal) { close(socket_); });

            // Send data to server
            std::string message = "Hello Socket!";
            send(socket_, message.c_str(), message.size(), 0);

            // Sleep for sometime
            std::this_thread::sleep_for(std::chrono::seconds(5)); 

            // Receive data from server
            char buffer [1024] = {0};
            recv(socket_, buffer, sizeof(buffer), 0);
            std::cout << "Received from Server: " << buffer << "\n";

            // Cleanup
            close(socket_);
        }
};

// Static Variable initialzations
int Socket::socket_ {-1};
std::atomic<bool> SocketServer::serverRunning {true};

int main(int argc, char **argv) {
    if (argc != 4 || (strcmp(argv[1], "server") != 0 && strcmp(argv[1], "client") != 0))
        std::cout << "Usage: hello-socket <server/client> <IPv4> <port>\n";
    else {
        std::string ip {argv[2]};
        std::uint16_t port {static_cast<std::uint16_t>(std::stoi(argv[3]))};

        if (strcmp(argv[1], "server") == 0) {
            SocketServer server;
            server.bindAndListen(ip, port);
        } 

        else {
            SocketClient client;
            client.connectAndSend(ip, port);
        }
    }

    return 0;
}
