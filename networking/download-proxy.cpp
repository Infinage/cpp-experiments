/*
 * Usage
 *  ```
    curl 'http://localhost:8080/stream' --get \
      --data-urlencode 'url=https://repo.msys2.org/msys/x86_64/clang-20.1.2-1-x86_64.pkg.tar.zst' \
      --data-urlencode 'output=clang.tar.zst'
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
        long sent;

        // Somewhere in middle, passthrough
        if (!first) sent = client->sendAll(raw);

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
            headers["content-disposition"] = {"attachment; filename=\"" + std::string{download} + "\""};
            std::ostringstream headerRaw;
            headerRaw << "HTTP/1.1 " << 200 << " OK\r\n";
            for (const auto &kv: headers) {
                for (const auto &val: kv.second)
                    headerRaw << kv.first << ": " << val << "\r\n";
            }

            // Send with modified headers
            std::string sendBuffer = headerRaw.str() + "\r\n" + std::string{raw.substr(headerEnd + 4)};
            sent = client->sendAll(sendBuffer); first = false;
            std::println("Updated headers sent, beginning content stream {}:{}", 
                client.ip, client.port);
        }
        return sent > 0;
    });
}

// Function owns lifetime of client socket
void handleRequest(Client client) {
    try {
        auto raw = client.socket.recv();
        if (!net::utils::isCompleteHttpRequest(raw))
            throw std::runtime_error{"Invalid Request"};

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

        // Resolving URL can throw, can be empty as well
        net::URL targetURL{target};
        try { targetURL.resolve(); }
        catch(...) { 
            std::ignore = client->send(BAD_URL); 
            return; 
        }

        std::println("Client {}:{} has requested {}", client.ip, client.port, target);
        pipeDownloadStream(client, targetURL, output);
    }

    catch (std::exception &ex) {
        std::println("HandleRequest Error ({}:{}): {}", client.ip, 
            client.port, ex.what());
    }
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
