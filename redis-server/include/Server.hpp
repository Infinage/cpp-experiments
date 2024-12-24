#pragma once

#include <atomic>
#include <stack>

#include "CommandHandler.hpp"

namespace Redis {

    /* --------------- SERVER CLASS METHOD IMPLEMENTATIONS --------------- */

    class Server {
        private:
            using SOCKET_INFO_VALUE_TYPE = std::tuple<short, std::string, std::size_t, std::stack<std::tuple<char, std::size_t>>>;

            // Variables for doing stuff
            CommandHandler handler;
            int server_fd;
            static std::unordered_map<int, SOCKET_INFO_VALUE_TYPE> socketInfo;
            static std::atomic<bool> serverRunning;

            // Helpers
            static bool readRequest(int client_fd, std::string &request);
            static bool sendResponse(int client_fd, std::string &response, std::size_t &currPos);
            static void closeSockets(int);

            // Init Server socket
            int initServer(const char* serverIP, const int serverPort, int serverBacklog);

            // Create poll request from socketInfo
            std::vector<pollfd> createPollInput();

        public:
            Server(const char* serverIP, const int serverPort, int serverBacklog, const char* dbFP);
            int getServerFD() const;
            void run();
    };
}
