/*
 * Usage
 *  ```
    curl 'http://localhost:8080/stream' --get \
      --data-urlencode 'url=https://repo.msys2.org/msys/x86_64/ctags-6.2.1-2-x86_64.pkg.tar.zst' \
      --data-urlencode 'output=ctar.tar.zst'
 *  ```
 *
 *  Dockerfile:
 *  ```
    # Stage 1: Builder
    FROM alpine:latest AS builder
    
    # Install build tools and dependencies
    RUN apk add --no-cache g++ cmake make git perl linux-headers binutils
    
    # Build and install static OpenSSL
    WORKDIR /tmp/openssl
    RUN git clone https://github.com/openssl/openssl .
    RUN ./Configure -static --openssldir=/etc/ssl && \ 
        make -j8 && make install_sw 
    
    # Build download-proxy
    WORKDIR /app
    RUN git clone https://github.com/infinage/cpp-experiments .
    RUN cd networking && \
        g++ download-proxy.cpp -std=c++23 -I/usr/local/ssl/include \
        -L/usr/local/ssl/lib -static -lssl -lcrypto -o download-proxy
    
    # Stage 2: Minimal runtime
    FROM alpine:latest
    
    # Copy the statically built binary
    WORKDIR /home/app
    COPY --from=builder /app/cpp-experiments/networking/download-proxy .
    
    # Expose port & run server
    EXPOSE 8080
    CMD ["./download-proxy"]
 *  ```
 */

#include "net.hpp"
#include <csignal>
#include <print>

struct Client {
    net::Socket socket; 
    std::string ip; 
    std::uint16_t port; 
    net::Socket *operator->() { return &socket; }
};

constexpr const char *BAD_REQ = 
"HTTP/1.1 400\r\n"
"Connection: close\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 11\r\n\r\n"
"Bad Request";

constexpr const char *BAD_URL = 
"HTTP/1.1 400\r\n"
"Connection: close\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 7\r\n\r\n"
"Bad URL";

constexpr const char *NOT_FOUND = 
"HTTP/1.1 404\r\n"
"Connection: close\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 9\r\n\r\n"
"Not Found";

void pipeDownloadStream(Client &client, const net::URL &target, std::string_view download) {
    net::HttpRequest req{target}; 
    bool first = true; std::string acc;
    req.stream([&first, &acc, &client, &download](std::string_view raw) {
        // Somewhere in middle, passthrough
        if (!first) client->sendAll(raw);
        else {
            // Incomplete headers, wait for more
            acc += raw;
            if (first && !net::utils::isCompleteHttpRequest(acc, false)) 
                return true;

            // Headers done, send everything but modify headers
            auto resp = net::HttpResponse::fromString(acc);
            resp.headers.erase("server");
            resp.headers["connection"] = {"close"};
            resp.headers["x-content-type-options"] = {"nosniff"};
            resp.headers["content-disposition"] = {"attachment; filename=\"" + std::string{download} + "\""};
            client->sendAll(resp.toString());
            first = false;
        }
        return true;
    });
}

void handleRequest(Client &&client) {
    auto raw = client.socket.recv();
    if (!net::utils::isCompleteHttpRequest(raw)) {
        std::println("Invalid req from client ({}: {})", client.ip, client.port);
        std::ignore = client->send(BAD_REQ);
        return;
    }

    auto req = net::HttpRequest::fromString(raw);
    if (req.getURL().path != "/stream") {
        std::ignore = client->send(NOT_FOUND);
        return;
    }

    std::string target, output {"download"};
    for (auto [key_, val_]: req.getURL().params) {
        auto key = net::URL::decode(key_);
        auto val = net::URL::decode(val_);
        if (key == "url") target = val;
        else if (key == "output") output = val;
    }

    net::URL targetURL{target};
    try { targetURL.resolve(); }
    catch(...) { 
        std::ignore = client->send(BAD_URL); 
        return; 
    }

    std::println("Client {}:{} has requested {}", client.ip, client.port, targetURL.path);
    pipeDownloadStream(client, targetURL, output);
}

int main() try {
    const std::string serverIP = "0.0.0.0";
    const std::uint16_t serverPort = 8080;

    static net::Socket server;
    server.bind(serverIP, serverPort);
    server.listen();
    std::println("Up and listening on {}:{}", serverIP, serverPort);

    std::signal(SIGINT, [](int) { server.close(); });
    while (server.ok()) {
        std::string clientIP; std::uint16_t clientPort;
        auto client = server.accept(clientIP, clientPort);    
        std::println("Connection from {}:{}", clientIP, clientPort);
        handleRequest({std::move(client), clientIP, clientPort});
    }
} 

catch (std::exception &ex) { 
    std::println("Error occured: {}", ex.what()); 
}
