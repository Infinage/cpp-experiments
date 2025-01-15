#include <regex>
#include <string>
#include <unordered_map>

class RequestHandler {
    private:
        // Variables
        std::unordered_map<std::string, std::string> cache, revCache;
        const std::string serverIP;
        const int serverPort;
        static const std::regex URLRegex;
        static const std::regex ProtocolRegex;
        static std::size_t Counter;

        // Static methods
        static std::pair<unsigned short, std::string> validatePostBody(std::string &body);
        static bool validateURL(std::string &url);
        static std::string readFile(const std::string &fpath);

        // Methods
        std::string shortenURL(std::string &longURL);
        std::string extractRequestURL(const std::string &buffer, const std::string &requestTypeStr);

    public:
        RequestHandler(const std::string &serverIP, const int serverPort);
        std::string processRequest(const std::string &buffer);
};
