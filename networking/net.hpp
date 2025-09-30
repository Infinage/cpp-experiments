#include <arpa/inet.h>
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace net {
    [[nodiscard]] inline std::string resolveHostname(std::string_view hostname) {
        addrinfo hints{}, *res {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostname.data(), nullptr, &hints, &res) != 0)
            throw std::runtime_error("Failed to resolve hostname");

        // Ensure res is freed always by wrapping inside a smart pointer
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resGaurd {res, freeaddrinfo};

        char ipStr[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr, ipStr, sizeof(ipStr)) == nullptr)
            throw std::runtime_error("Failed to convert address to string");

        return ipStr;
    }

    class Socket {
        protected:
            int _fd {-1};

            [[nodiscard]] static sockaddr_in SockaddrIn(std::string_view ip, std::uint16_t port) {
                sockaddr_in sockAddr;
                sockAddr.sin_family = AF_INET;
                inet_pton(AF_INET, ip.data(), &sockAddr.sin_addr);
                sockAddr.sin_port = htons(port);
                return sockAddr;
            }

        public:
            struct SocketArgs { int domain; int type; int protocol; bool reuseSockAddr; };

            // This ctor is to wrap a socket created from accept into RAII
            explicit Socket(int _fd): _fd{_fd} {}
            virtual ~Socket() { if (_fd != -1) { ::close(_fd); _fd = -1; } }

            int fd() const { return _fd; }

            // Disable copy ctor and assignment
            Socket(const Socket&) = delete;
            Socket &operator=(const Socket&) = delete;

            // Swap function for move assignment / ctors
            void swap(Socket &other) noexcept {
                using std::swap; swap(_fd, other._fd);
            }

            // Implement move assignment and ctor
            Socket(Socket &&other) noexcept: _fd{std::exchange(other._fd, -1)} {}
            Socket &operator=(Socket &&other) noexcept {
                Socket{std::move(other)}.swap(*this);
                return *this;
            }

            // Default ctor
            Socket(SocketArgs args = { .domain = AF_INET, .type = SOCK_STREAM, .protocol = IPPROTO_TCP, .reuseSockAddr = true }) 
            {
                _fd = socket(args.domain, args.type, args.protocol);
                if (_fd == -1) throw std::runtime_error("Error creating socket object");

                // Reuse socket address
                if (args.reuseSockAddr) {
                    int opt {1};
                    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
                        throw std::runtime_error("Error setting opt 'SO_REUSEADDDR'");
                }
            }

            void bind(std::string_view serverIp, std::uint16_t port) {
                sockaddr_in serverAddr {SockaddrIn(serverIp, port)};
                int bindStatus {::bind(_fd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr))};
                if (bindStatus == -1) throw std::runtime_error("Error binding to the socket");
            }

            void listen(unsigned short connections = 1) {
                int listenStatus {::listen(_fd, connections)};
                if (listenStatus == -1) throw std::runtime_error("Error listening on socket");
            }

            [[nodiscard]] Socket accept() {
                int clientSocket {::accept(_fd, nullptr, nullptr)};
                if (clientSocket == -1) throw std::runtime_error("Failed to accept an incomming connection");
                return Socket{clientSocket};
            }

            void connect(std::string_view serverIp, std::uint16_t port) {
                sockaddr_in serverAddr {SockaddrIn(serverIp, port)};
                int connectStatus {::connect(_fd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr))};
                if (connectStatus == -1) throw std::runtime_error("Error connecting to server");
            }

            void send(std::string_view message) {
                while (!message.empty()) {
                    long sentBytes {::send(_fd, message.data(), message.size(), 0)};
                    if (sentBytes <= 0) throw std::runtime_error("Failed to send");
                    message.remove_prefix(static_cast<std::size_t>(sentBytes));
                }
            }

            std::string recv() {
                std::string message;
                char buffer[2048] {}; long recvBytes;
                while ((recvBytes = ::recv(_fd, buffer, sizeof(buffer), 0)) > 0)
                    message.append(buffer, static_cast<std::size_t>(recvBytes));
                return message;
            }

            void sendTo(std::string_view message, std::string_view host, std::uint16_t port) {
                sockaddr_in hostAddr {SockaddrIn(host, port)};
                long sentBytes {::sendto(_fd, message.data(), message.size(), 0, reinterpret_cast<sockaddr*>(&hostAddr), sizeof(hostAddr))};
                if (sentBytes <= 0) throw std::runtime_error("Failed to send");
            }

            std::string recvFrom(std::string &host, std::uint16_t &port, std::size_t maxBytes = 2048) {
                sockaddr_in hostAddr {}; socklen_t hostAddrLen {sizeof(hostAddr)};
                std::vector<char> buffer(maxBytes); 
                long recvBytes {::recvfrom(_fd, buffer.data(), maxBytes, 0, reinterpret_cast<sockaddr*>(&hostAddr), &hostAddrLen)};
                if (recvBytes <= 0) throw std::runtime_error("Failed to recv");

                port = ntohs(hostAddr.sin_port);
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &hostAddr.sin_addr, ipStr, sizeof(ipStr));
                host = ipStr;

                return {buffer.data(), static_cast<std::size_t>(recvBytes)};
            }
    };
}
