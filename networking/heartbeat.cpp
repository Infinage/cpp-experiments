#include <atomic>
#include <csignal>
#include <iostream>

#include "net.hpp"

// Static atomic boolean to set server status
static std::atomic_bool SERVER_RUNNING {true};

int main(int argc, char **argv) {
    if (argc != 2) std::cout << "Usage: heartbeat <port>\n";
    else try {
        // On interupt set SERVER_RUNNING flag to false
        std::signal(SIGINT, [](int) { SERVER_RUNNING = false; });

        // Constants for the server
        constexpr const char *serverIp {"0.0.0.0"};
        constexpr std::string_view response {"HTTP/1.1 200\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK"}; 
        const std::uint16_t PORT {static_cast<std::uint16_t>(std::stoi(argv[1]))};

        // Initialize a non blocking socket and bind to port
        net::Socket serverSocket;
        serverSocket.setNonBlocking();
        serverSocket.bind(serverIp, PORT);
        serverSocket.listen();

        // Feedback on server
        std::cout << "Server listening on port " << PORT << '\n';

        // Listen for incoming connections
        net::PollManager pm;
        pm.track(std::move(serverSocket));
        while (SERVER_RUNNING) {
            for (auto &[socket, event]: pm.poll()) {
                if ((event == net::PollEventType::Readable) && SERVER_RUNNING) {
                    net::Socket clientSocket {socket.accept()};
                    clientSocket.setNonBlocking();
                    long sentBytes {clientSocket.sendAll(response)};
                    if (static_cast<std::size_t>(sentBytes) < response.size()) 
                        std::cerr << "Partial data was sent to client\n";
                }
            }
        }
    } catch (...) {
        std::cout << "Exiting server..\n";
    }
}
