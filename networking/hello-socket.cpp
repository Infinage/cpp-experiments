/*
 * A simple Socket Program, can work as both server & the client based on input args
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

#include "net.hpp"

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>

// Flag to keep the server running
std::atomic<bool> serverRunning {true};

int main(int argc, char **argv) try {
    if (argc != 4 || (strcmp(argv[1], "server") != 0 && strcmp(argv[1], "client") != 0))
        std::cout << "Usage: hello-socket <server/client> <IPv4> <port>\n";
    else {
        std::string ip {argv[2]};
        std::uint16_t port {static_cast<std::uint16_t>(std::stoi(argv[3]))};

        if (strcmp(argv[1], "server") == 0) {
            std::signal(SIGINT, [](int) { serverRunning = false; });

            net::PollManager manager;
            int serverFD {-1};

            {
                // Scoping server variable to prevent use after move
                net::Socket server;
                server.setNonBlocking();
                server.bind(ip, port);
                server.listen();
                serverFD = server.fd();
                manager.track(std::move(server), net::PollEventType::Readable);
            }

            std::cout << "Server is up and listening on port " << port << ".\n";

            while (serverRunning) {
                for (auto &[socket, event]: manager.poll()) {
                    if (!serverRunning) break;
                    else if (event == net::PollEventType::Closed || event == net::PollEventType::Error) {
                        if (socket.fd() == serverFD) throw std::runtime_error("Server Socket poll failed");
                        manager.untrack(socket.fd());
                    } else if (event == net::PollEventType::Readable && socket.fd() == serverFD) {
                        net::Socket client {socket.accept()};
                        std::cout << "Connected to Client # " << client.fd() << ".\n";
                        client.setNonBlocking();
                        manager.track(std::move(client), net::PollEventType::Readable);
                    } else if (event == net::PollEventType::Readable) {
                        std::string message {socket.recvAll()};
                        if (message != "quit") {
                            std::cout << "Received from Client #" << socket.fd() << ": " << message << "\n";
                        } else {
                            std::cout << "Disconnecting client #" << socket.fd() << '\n';
                            socket.sendAll("Bye Socket!"); 
                            manager.untrack(socket.fd()); 
                        }
                    }
                }
            }
        } 

        else {
            net::Socket client;
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
