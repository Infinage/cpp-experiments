/*
 * Usage
 *  ```
    curl 'http://localhost:8080/stream' --get \
      --data-urlencode 'target=https://repo.msys2.org/msys/x86_64/clang-20.1.2-1-x86_64.pkg.tar.zst' \
      --data-urlencode 'filename=clang.tar.zst'
      --data-urlencode 'token=SECRET'
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

#include "../misc/threadPool.hpp"
#include "net.hpp"

#include <csignal>
#include <cstdlib>
#include <print>

constexpr const char *BAD_URL = 
"HTTP/1.1 400\r\n"
"Connection: close\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 7\r\n\r\n"
"Bad URL";

constexpr const char *UNAUTHORISED = 
"HTTP/1.1 401\r\n"
"Connection: close\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 12\r\n\r\n"
"Unauthorised";

constexpr const char *NOT_FOUND = 
"HTTP/1.1 404\r\n"
"Connection: close\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 9\r\n\r\n"
"Not Found";

void pipeDownloadStream(net::Socket &client, const std::string &ip, const std::uint16_t port, 
    const net::URL &target, std::string_view filename) 
{
    net::HttpRequest req{target};
    bool first = true; std::string acc;
    req.stream([&](std::string_view raw) {
        long sent;

        // Somewhere in middle, passthrough
        if (!first) sent = client.sendAll(raw);

        // Incomplete headers, wait for more
        else {
            acc += raw;
            auto headerStart = acc.find("\r\n");
            if (first && headerStart == std::string::npos) return true;
            auto headerEnd = acc.find("\r\n\r\n", headerStart + 2);
            if (first && headerEnd == std::string::npos) return true;

            // Headers done, send everything but modify headers
            auto headers = net::utils::parseHeadersFromString(acc.substr(headerStart + 2, headerEnd - headerStart));
            headers.erase("server");
            headers["connection"] = {"close"};
            headers["x-content-type-options"] = {"nosniff"};
            headers["content-disposition"] = {"attachment; filename=\"" + std::string{filename} + "\""};
            std::ostringstream headerRaw;
            headerRaw << "HTTP/1.1 " << 200 << " OK\r\n";
            for (const auto &kv: headers) {
                for (const auto &val: kv.second)
                    headerRaw << kv.first << ": " << val << "\r\n";
            }

            // Send with modified headers
            std::string sendBuffer = headerRaw.str() + "\r\n" + std::string{raw.substr(headerEnd + 4)};
            sent = client.sendAll(sendBuffer); first = false;
            std::println("Updated headers sent, beginning content stream {}:{}", ip, port);
        }
        return sent > 0;
    });
}

// Function owns lifetime of client socket
void handleRequest(net::Socket &&client, std::string ip, std::uint16_t port, const std::string &TOKEN) {
    try {
        auto raw = client.recv();
        if (!net::utils::isCompleteHttpRequest(raw))
            throw std::runtime_error{"Invalid Request"};

        auto req = net::HttpRequest::fromString(raw);
        if (req.getURL().path != "/stream") {
            std::ignore = client.send(NOT_FOUND);
            return;
        }

        bool validToken = false;
        std::string target, filename {"download"};
        for (auto [key_, val_]: req.getURL().params) {
            auto key = net::URL::decode(key_);
            auto val = net::URL::decode(val_);
            if (key == "target") target = val;
            else if (key == "filename") filename = val;
            else if (key == "token") validToken = val == TOKEN;
        }

        // Add a simple string based authentication
        if (!validToken) { std::ignore = client.send(UNAUTHORISED); return; }

        // Resolving URL can throw, can be empty as well
        net::URL targetURL{target};
        try { targetURL.resolve(); }
        catch(...) { std::ignore = client.send(BAD_URL); return; }

        std::println("Client {}:{} has requested {}", ip, port, target);
        pipeDownloadStream(client, ip, port, targetURL, filename);
    }

    catch (std::exception &ex) {
        std::println("HandleRequest Error ({}:{}): {}", ip, port, ex.what());
    }
}

int main() try {
    const std::string SERVERIP = "0.0.0.0";
    const std::uint16_t SERVERPORT = 8080;
    const std::string TOKEN = [] {
        auto *envTok = std::getenv("TOKEN");
        return envTok == nullptr? "SECRET": envTok;
    }();

    static net::Socket server;
    server.bind(SERVERIP, SERVERPORT); server.listen();
    std::println("Up and listening on {}:{}", SERVERIP, SERVERPORT);

    std::signal(SIGINT, [](int) { server.close(); });
    async::ThreadPool pool {4, async::ThreadPool::ExitPolicy::CURRENT_TASK_COMPLETE};
    while (server.ok()) {
        std::string clientIP; std::uint16_t clientPort;
        auto sock = server.accept(clientIP, clientPort);    
        std::println("Connection from {}:{}", clientIP, clientPort);
        pool.enqueue([client = std::move(sock), clientIP, clientPort, &TOKEN] mutable {
            handleRequest(std::move(client), clientIP, clientPort, TOKEN); 
        });
    }
}

catch (std::exception &ex) { 
    std::println("Fatal Error: {}", ex.what()); 
}
