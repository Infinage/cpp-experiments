#include <algorithm>
#include <arpa/inet.h>
#include <cstdint>
#include <fcntl.h>
#include <memory>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <type_traits>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include <openssl/ssl.h>

namespace net {
    [[nodiscard]] inline std::string resolveHostname(std::string_view hostname, const char *service = nullptr) {
        addrinfo hints{}, *res {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostname.data(), service, &hints, &res) != 0)
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
            bool _blocking {false};

            [[nodiscard]] static sockaddr_in SockaddrIn(std::string_view ip, std::uint16_t port) {
                in_addr addr {};
                inet_pton(AF_INET, ip.data(), &addr);
                return {
                    .sin_family = AF_INET, 
                    .sin_port = htons(port), 
                    .sin_addr = addr,
                    .sin_zero = {}
                };
            }

        public:
            struct SocketArgs { int domain; int type; int protocol; bool reuseSockAddr; };

            // This ctor is to wrap a socket created from accept into RAII
            explicit Socket(int _fd): _fd{_fd}, _blocking{false} {}
            ~Socket() { if (_fd != -1) { ::close(_fd); _fd = -1; } }

            int fd() const { return _fd; }
            bool isBlocking() const { return _blocking; }

            // Disable copy ctor and assignment
            Socket(const Socket&) = delete;
            Socket &operator=(const Socket&) = delete;

            // Swap function for move assignment / ctors
            void swap(Socket &other) noexcept {
                using std::swap; swap(_fd, other._fd); 
                swap(_blocking, other._blocking);
            }

            // Implement move constructor
            Socket(Socket &&other) noexcept: 
                _fd{std::exchange(other._fd, -1)}, 
                _blocking{std::exchange(other._blocking, false)} 
            {}

            // Implement move assignment operator
            Socket &operator=(Socket &&other) noexcept {
                Socket{std::move(other)}.swap(*this);
                return *this;
            }

            // Default ctor
            Socket(SocketArgs args = { .domain = PF_INET, .type = SOCK_STREAM, .protocol = IPPROTO_TCP, .reuseSockAddr = true }) {
                _fd = socket(args.domain, args.type, args.protocol);
                if (_fd == -1) throw std::runtime_error("Error creating socket object");

                // Reuse socket address
                if (args.reuseSockAddr) {
                    int opt {1};
                    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
                        throw std::runtime_error("Error setting opt 'SO_REUSEADDDR'");
                }
            }

            // Set socket as blocking / nonblocking
            void setNonBlocking(bool enable = true) {
                int fcntl_flags {fcntl(_fd, F_GETFL, 0)}; // Get currently set flags
                int fcntl_flags_modified = enable? fcntl_flags | O_NONBLOCK: fcntl_flags & ~O_NONBLOCK;
                if(fcntl_flags == -1 || fcntl(_fd, F_SETFL, fcntl_flags_modified) == -1)
                    throw "Error setting client socket to non blocking mode to: " + std::to_string(enable);
                _blocking = enable;
            }

            void bind(std::string_view serverIp, std::uint16_t port) {
                sockaddr_in serverAddr {SockaddrIn(serverIp, port)};
                int bindStatus {::bind(_fd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr))};
                if (bindStatus == -1) throw std::runtime_error("Error binding to the socket");
            }

            void listen(unsigned short backlog = 5) {
                int listenStatus {::listen(_fd, backlog)};
                if (listenStatus == -1) throw std::runtime_error("Error listening on socket");
            }

            [[nodiscard]] Socket accept() {
                int clientSocket {::accept(_fd, nullptr, nullptr)};
                if (clientSocket == -1) throw std::runtime_error("Failed to accept an incomming connection");
                return Socket{clientSocket};
            }

            [[nodiscard]] Socket accept(std::string &host, std::uint16_t &port) {
                sockaddr_in hostAddr {}; socklen_t hostAddrLen{sizeof(hostAddr)};
                int clientSocket {::accept(_fd, reinterpret_cast<sockaddr*>(&hostAddr), &hostAddrLen)};
                if (clientSocket == -1) throw std::runtime_error("Failed to accept an incomming connection");

                port = ntohs(hostAddr.sin_port);
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &hostAddr.sin_addr, ipStr, sizeof(ipStr));
                host = ipStr;

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

            [[nodiscard]] std::string recv() {
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

            [[nodiscard]] std::string recvFrom(std::string &host, std::uint16_t &port, std::size_t maxBytes = 2048) {
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

    class SSLSocket {
        public:
            SSLSocket(): socket{}, ctx{SSL_CTX_new(TLS_client_method())}, ssl{SSL_new(ctx)} {
                SSL_set_fd(ssl, socket.fd());
            }

            ~SSLSocket() {
                if (socket.fd() != -1) { 
                    SSL_shutdown(ssl);
                    SSL_free(ssl);
                    SSL_CTX_free(ctx);
                }
            }

            // Swap function for move assignment / ctors
            void swap(SSLSocket &other) noexcept {
                using std::swap; swap(socket, other.socket); 
                swap(ctx, other.ctx); swap(ssl, other.ssl);
            }

            // Implement move constructor
            SSLSocket(SSLSocket &&other) noexcept: 
                socket {std::move(other.socket)},
                ctx {std::exchange(other.ctx, nullptr)},
                ssl {std::exchange(other.ssl, nullptr)}
            {}

            // Implement move assignment operator
            SSLSocket &operator=(SSLSocket &&other) noexcept {
                SSLSocket{std::move(other)}.swap(*this);
                return *this;
            }

            void connect(std::string_view serverIp, std::uint16_t port) {
                socket.connect(serverIp, port);
                if (SSL_connect(ssl) <= 0) 
                    throw std::runtime_error("Failed to establish TLS connection");
            }

            void send(std::string_view message) {
                while (!message.empty()) {
                    long sentBytes {SSL_write(ssl, message.data(), static_cast<int>(message.size()))};
                    if (sentBytes <= 0) throw std::runtime_error("Failed to send");
                    message.remove_prefix(static_cast<std::size_t>(sentBytes));
                }
            }

            [[nodiscard]] std::string recv() {
                std::string message;
                char buf[2048]; long recvBytes;
                while ((recvBytes = SSL_read(ssl, buf, sizeof(buf))) > 0)
                    message.append(buf, static_cast<std::size_t>(recvBytes));
                return message;
            }

        private:
            Socket socket; 
            SSL_CTX *ctx; SSL* ssl; 
    };

    class HttpRequest {
        private:
            std::vector<std::pair<std::string, std::string>> urlParams, headers {{"Content-Type", "application/json"}, {"Connection", "close"}};
            std::string path, method, body;

            static std::string getPathWithParams(const std::string &path, const std::vector<std::pair<std::string, std::string>> &params) {
                if (params.empty()) return path;
                std::ostringstream oss; oss << path << '?';
                for (const auto &kv: params) oss << kv.first << "=" << kv.second << '&';
                std::string res {oss.str()}; res.pop_back();
                return res;
            }

        public:
            HttpRequest(const std::string &path = "/", const std::string &method = "GET"): 
                path{path}, method{method} {}

            void setHeader(const std::string &key, const std::string &value) { headers.push_back({key, value}); }
            void setParam(const std::string &key, const std::string &value) { urlParams.push_back({key, value}); }
            void setBody(std::string body) { this->body = std::move(body); }

            std::string serialize() const {
                std::ostringstream oss;
                oss << method << ' ' << getPathWithParams(path, urlParams) << " HTTP/1.1\r\n";
                for (const auto &kv: headers) oss << kv.first << ": " << kv.second << "\r\n";
                if (!body.empty()) oss << "Content-Length: " << body.size() << "\r\n\r\n" << body;
                else oss << "\r\n";
                return oss.str();
            }

            // TODO: Implement and return a HttpResponse object
            [[nodiscard]] std::string execute(std::string_view hostname, bool enableSSL = true, long timeoutSec = 5) {
                if (std::ranges::find(headers, "Host", &std::pair<std::string,std::string>::first) == headers.end())
                    setHeader("Host", std::string{hostname});
                if (enableSSL) return _executeSSL(resolveHostname(hostname), 443);
                return _execute(resolveHostname(hostname), 80, timeoutSec);
            }

            // TODO: Implement and return a HttpResponse object
            std::string _execute(std::string_view ipAddr, std::uint16_t port, long timeoutSec) {
                    struct timeval timeout {.tv_sec = timeoutSec, .tv_usec = 0};
                    net::Socket socket;
                    socket.connect(ipAddr, port);
                    if (::setsockopt(socket.fd(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 || 
                        ::setsockopt(socket.fd(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
                        throw std::runtime_error("Failed to set socket timeouts");
                    socket.send(serialize());
                    return socket.recv();
            }

            std::string _executeSSL(std::string_view ipAddr, std::uint16_t port) {
                net::SSLSocket socket;
                socket.connect(ipAddr, port);
                socket.send(serialize());
                return socket.recv();
            }
    };

    class PollManager {
        public:
            enum class EventType { Unknown=0, Readable=1, Writable=2, Closed=4, Error=8 };

            friend EventType operator|=(EventType &e1, EventType e2) { return e1 = e1 | e2; }
            friend EventType operator|(EventType e1, EventType e2) {
                using T = std::underlying_type_t<EventType>;
                return static_cast<EventType>(static_cast<T>(e1) | static_cast<T>(e2)); 
            }

            friend bool operator&(EventType e1, EventType e2) {
                using T = std::underlying_type_t<EventType>;
                return static_cast<bool>((static_cast<T>(e1) & static_cast<T>(e2)) == 1); 
            }

            void untrack(int fd) {
                auto it {sockets.find(fd)};
                if (it != sockets.end()) {
                    sockets.erase(fd);
                    auto newEnd {std::ranges::remove(pollFds, fd, &pollfd::fd)};
                    pollFds.erase(newEnd.begin(), newEnd.end());
                }
            }

            void track(Socket &&socket) {
                int fd {socket.fd()};
                sockets.emplace(fd, std::move(socket));
                pollFds.emplace_back(fd, POLLIN | POLLOUT | POLLHUP, 0);
            }

            // To prevent ambiguity, lets delete this one
            void poll(bool) = delete;

            // Warning: While safe to `untrack` while iterating, Socket& can become dangling
            [[nodiscard]] std::vector<std::pair<Socket&, EventType>> poll(int timeout = -1, bool raiseError = true) {
                // If poll failed return empty
                if (::poll(pollFds.data(), pollFds.size(), timeout) == -1) {
                    if (raiseError) throw std::runtime_error("Poll failed");
                    return {};
                }

                // Store the result into events
                std::vector<std::pair<Socket&, EventType>> result;
                for (auto &pollFd: pollFds) {
                    EventType event {EventType::Unknown};

                    if (pollFd.revents & POLLIN) 
                        event |= EventType::Readable;
                    if (pollFd.revents & POLLOUT)
                        event |= EventType::Writable;
                    if (pollFd.revents & POLLHUP)
                        event |= EventType::Closed;
                    if (pollFd.revents & POLLERR || pollFd.revents & POLLNVAL)
                        event |= EventType::Error;

                    if (event != EventType::Unknown)
                        result.emplace_back(getSocket(pollFd.fd), event);
                }

                return result;
            }

            [[nodiscard]] Socket &getSocket(int fd) {
                auto it {sockets.find(fd)};
                if (it == sockets.end()) 
                    throw std::runtime_error("No such file descriptor: " + std::to_string(fd));
                return it->second;
            }

        private:
            std::vector<pollfd> pollFds;
            std::unordered_map<int, Socket> sockets;
    };

    using PollEventType = PollManager::EventType;
}
