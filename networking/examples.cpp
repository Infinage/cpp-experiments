#include "net.hpp"
#include <print>
#include <thread>

int main() try {
    // Resolve just a domain without caring about its protocol
    {
        std::println("\ngoogle.com resolved to: {}\n", net::utils::resolveHostname("google.com"));
    }

    // Resolve IPV6 domain and get resp (curl -6 https://api6.ipify.org?format=json)
    // We need to explicitly resolve URL, but HttpRequest auto resolves for us
    {
        net::URL url {"https://api6.ipify.org", net::IP::V6};
        url.setParam("format", "json");
        net::HttpRequest req {url, "GET"};
        net::HttpResponse resp {req.execute()};
        std::println("Resolved IPV6 Addr: {}\nResponse: {}\n", url.ipAddr, resp.body);
    }

    // Send a https get request to API (i.e with SSL support)
    {
        net::HttpRequest req {"https://jsonplaceholder.typicode.com/users/1"};
        net::HttpResponse resp {req.execute()};
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
        net::HttpRequest req {"http://github.com"};
        net::HttpResponse resp {req.execute()};
        std::println("Response Status: {}\nRequest URL redirected to: {}\n", 
            resp.statusCode, resp.location);
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
