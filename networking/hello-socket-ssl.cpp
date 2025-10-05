/*
 * A simple Socket Program, can work as both server & the client based on input args
 * Similar to 'hello-socket.cpp' except that this one uses SSL / TLS
 *
 * To create custom certificate do
 ```
 openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout key.pem -out cert.pem -days 365 \
    -subj "/C=IN/ST=TN/L=Chennai/O=MyOrg/OU=Dev/CN=localhost"
 ```
 */

#include "net.hpp"

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <semaphore>
#include <string>
#include <thread>

// Flag to keep the server running
std::atomic<bool> serverRunning {true};

int main(int argc, char **argv) try {
    if (argc != 4 || (strcmp(argv[1], "server") != 0 && strcmp(argv[1], "client") != 0))
        std::cout << "Usage: hello-socket-ssl <server/client> <IPv4> <port>\n";
    else {
        std::string ip {argv[2]};
        std::uint16_t port {static_cast<std::uint16_t>(std::stoi(argv[3]))};

        if (strcmp(argv[1], "server") == 0) {
            static net::SSLSocket server {true, "cert.pem", "key.pem"};
            server.bind(ip, port);
            server.listen();

            std::signal(SIGINT, [](int) { serverRunning = false; server.close(); });
            std::signal(SIGPIPE, SIG_IGN);

            std::cout << "Server is up and listening on port " << port << ".\n";
            std::vector<std::jthread> threads;
            std::counting_semaphore semaphore(8);

            while (serverRunning) {
                std::string clientIP; std::uint16_t clientPort;
                net::SSLSocket client {server.accept(clientIP, clientPort)};
                semaphore.acquire();
                threads.push_back(std::jthread{[client = std::move(client), &semaphore]() mutable {
                    std::cout << "Connected to Client # " << client.fd() << std::endl;
                    while (serverRunning) try {
                        std::string message {client.recv()};
                        if (!message.empty() && message != "quit") {
                            std::cout << "Received from Client #" << client.fd() << ": " << message << std::endl;
                        } else if (client.ok()) {
                            std::cout << "Disconnecting client #" << client.fd() << std::endl;
                            client.sendAll("Bye Socket!"); 
                            break;
                        }
                    } catch (...) { break; }

                    // Release for consumption by other sockets
                    semaphore.release();
                }});
            }
        }

        else {
            net::SSLSocket client {false, "cert.pem"};
            client.connect(ip, port);
            std::cout << "Type and <Enter> to send to server. Enter 'quit' to exit.\n";
            std::string message;
            while (message != "quit") {
                std::getline(std::cin, message);
                client.sendAll(message);
            }
            std::cout << "Received from Server: " << client.recv() << "\n";
        }
    }
} catch(std::exception&) {
    std::cout << "Exiting..\n"; 
}
