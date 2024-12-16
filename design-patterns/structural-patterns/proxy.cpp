#include <iostream>
#include <random>
#include <string>
#include <unordered_map>

// Common interface for our 3rd Party & Proxy
class ThirdPartyYouTubeLib {
    public:
        virtual std::string downloadVideo(long id) = 0; 
        virtual ~ThirdPartyYouTubeLib() = default;
};

// 3rd Party Service that is too expensive to use
class ThirdPartyYouTubeOriginal: public ThirdPartyYouTubeLib {
    private:
        std::mt19937 gen {std::random_device {}()};

    public:
        std::string downloadVideo(long id) override {
            std::cout << "Fetching blob from YouTube.com..\n";
            std::uniform_int_distribution<char> charDist('a', 'z');
            std::string hash{};
            for (int i{0}; i < 30; i++)
                hash += charDist(gen);
            return hash;
        }
};

// Proxy example here sits between Client & the actual 3rd party lib caching results
class ThirdPartyYouTubeProxy: public ThirdPartyYouTubeLib {
    private:
        mutable std::unordered_map<long, std::string> cache;
        ThirdPartyYouTubeLib &service;

    public:
        ThirdPartyYouTubeProxy(ThirdPartyYouTubeLib &service): service(service) {}
        std::string downloadVideo(long id) override {
            if (cache.find(id) == cache.end())
                return cache[id] = service.downloadVideo(id);
            else {
                std::cout << "Fetching blob from Cache..\n";
                return cache[id];
            }
        }
};

// Sample Client Code
int main() {
    ThirdPartyYouTubeOriginal originalService {ThirdPartyYouTubeOriginal{}};
    ThirdPartyYouTubeProxy proxyService {ThirdPartyYouTubeProxy{originalService}};
    std::cout << proxyService.downloadVideo(1L) << "\n\n";
    std::cout << proxyService.downloadVideo(1L) << "\n\n";
    std::cout << proxyService.downloadVideo(2L) << "\n";
    return 0;
}
