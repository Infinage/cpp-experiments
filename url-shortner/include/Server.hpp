#include <string>
#include <poll.h>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "RequestHandler.hpp"

class Server {
    private:
        // POLLIN/POLLOUT, Request/Response, pendingBytes
        using SOCKET_INFO_VALUE_TYPE = std::tuple<short, std::string, long>;

        // Static Variables, functions
        static bool serverRunning;
        static std::unordered_map<int, SOCKET_INFO_VALUE_TYPE> socketInfo;
        static void closeSockets(int = 0);
        void exitWithError(const std::string &message);

        // Private members
        int serverSocket;
        RequestHandler handler;

    public:
        Server(const std::string &serverIP, const int serverPort, const int serverBacklog);
        std::vector<pollfd> createPollFDs();
        bool readRequest(int clientSocket, std::string &buffer, long &pendingBytes);
        bool sendResponse(int clientSocket, const std::string &response, long &pendingBytes);
        void run();
};
