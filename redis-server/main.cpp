#include "include/Server.hpp"
#include <charconv>
#include <cstring>
#include <iostream>

int main(int argc, char **argv) {
    if (argc >= 3) {
        std::cerr << "Usage: ./server <port>\n";
        return 1;
    } 

    int PORT;
    if (argc != 2) PORT = 6379;
    else {
        std::from_chars_result parseResult {std::from_chars(argv[1], argv[1] + std::strlen(argv[1]), PORT)};
        if (parseResult.ec != std::errc() || PORT <= 0) {
            std::cerr << "Not a valid port.\n";
            return 1;
        }
    } 

    constexpr const char* SERVER_IP {"0.0.0.0"};
    constexpr const char* DB_SAVE_FP {"dump.rdb"};
    constexpr int SOCKET_BACKLOG {10};

    // Init redis server and start loop
    Redis::Server server(SERVER_IP, PORT, SOCKET_BACKLOG, DB_SAVE_FP);
    server.run();

    return 0;
}
