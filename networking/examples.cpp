#include "net.hpp"
#include <print>
#include <thread>

int main() try {
    // Resolve IPV6 domain and get resp (curl -6 https://api6.ipify.org?format=json)
    {
        const char *domain {"api6.ipify.org"};
        std::string ipAddr {net::resolveHostname(domain, nullptr, net::SOCKTYPE::TCP, net::IP::V6)};
        std::println("Resolving IPV6 Domain (): {}", ipAddr);
        net::HttpRequest req; req.setParam("format", "json");
        std::println("{}", req.serialize());
        net::HttpResponse resp {req._executeSSL(ipAddr, 443, domain)};
        std::println("{}", resp.json().str());
    }

    // Send a https get request to API (i.e with SSL support)
    {
        net::HttpRequest req {"/users/1"};
        net::HttpResponse resp {req.execute("jsonplaceholder.typicode.com")};
        JSON::JSONHandle json {resp.json()};
        std::println(
            "Status Code: {}\nContent Type: {}\n\n"
            "Name: {}\nusername: {}\n"
            "User Company Name: {}\n",
            resp.statusCode, 
            resp.headers["content-type"], 
            json["name"].str(),
            json["username"].str(), 
            json["company"]["name"].str()
        );
    }

    // Send a http get request to API (i.e without SSL support)
    // Github forces redirect to https
    {
        net::HttpRequest req;
        net::HttpResponse resp {req.execute("github.com", false)};
        std::println("Raw response: {}", resp.raw);
    }

    // Send UDP packets to `nc -l -u 4444`, ensure running
    {
        net::Socket client {net::SOCKTYPE::UDP};
        client.connect("0.0.0.0", 4444);
        for (std::size_t i {}; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            long _ {client.send("Message: " + std::to_string(i) + '\n')};
        }
    }
} catch (std::exception &ex) {
    std::println("{}", ex.what());
}
