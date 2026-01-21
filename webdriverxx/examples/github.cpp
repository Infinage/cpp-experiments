#include "webdriverxx.hpp"

#include <iostream>

using namespace webdriverxx;

// Specify Browser type, Location and driver port
auto browserType = BROWSERS::CHROME;
auto browserPath = "C:/Program Files/Google/Chrome/Application/chrome.exe";
auto driverPort = "1000";

int main() {
    // Create a new webdriver
    auto caps = Capabilities(browserType, browserPath).startMaximized(true).headless(false);
    Driver driver{caps, driverPort};

    // Navigate to URL
    driver.navigateTo("https://www.github.com");

    // Click on 'Open Source'
    driver.findElement(XPATH, "//button[contains(text(), 'Open Source')]").click();

    // Click on 'Trending'
    driver.findElement(CSS, "li a[href*='trending']").click();

    // Wait until loaded
    waitUntil([&]{ return driver.getTitle().starts_with("Trending repositories on GitHub"); });

    // Click on languages
    driver.findElement(CSS, "#select-menu-language .btn-link").click();

    // Type in 'C++' into the input
    driver.findElement(CSS, "input[placeholder='Filter languages']").sendKeys("C++");

    // Wait until 'C++' is visible and click
    auto cppLangFilter = driver.findElement(CSS, "div[data-filter-list] a[href*='c++']");
    waitUntil([&]{ return cppLangFilter.getElementAttribute("hidden") == "false"; }, 1500);
    cppLangFilter.click();

    // Wait until page loaded
    waitUntil([&]{ return driver.getTitle().starts_with("Trending C++ repositories on GitHub"); });

    // Extract the top 3 repos
    std::vector<std::string> repoLinks;
    auto repos = driver.findElements(CSS, "article.Box-row h2.h3 a.Link");
    for (std::size_t i {0}; i < 3; i++) 
        repoLinks.emplace_back(repos[i].getElementAttribute("href"));

    // Print header
    std::cout << std::left << std::setw(35) << "Repository"
              << std::setw(10) << "Stars"
              << std::setw(10) << "Forks"
              << std::setw(10) << "Commits"
              << std::setw(10) << "Issues"
              << std::setw(10) << "PRs" << "\n"
              << std::string(85, '-') << "\n";

    // Visit the url and scrape
    for (auto url: repoLinks) {

        // Wait until page loads
        driver.navigateTo("https://github.com" + url);
        waitUntil([&]{ return driver.getCurrentURL().ends_with(url); });

        // Scrape useful info - Stars, Forks, Commits, Issues, PRs
        auto stars = driver.findElement(CSS, "#repo-stars-counter-star").getElementText();
        auto forks = driver.findElement(CSS, "#repo-network-counter").getElementText();
        auto commits = driver.findElement(XPATH, "//span[contains(text(), 'Commits')]").getElementText();
        auto issues = driver.findElement(CSS, "#issues-tab #issues-repo-tab-count").getElementText();
        auto prs = driver.findElement(CSS, "#pull-requests-tab #pull-requests-repo-tab-count").getElementText();

        // Print the info
        std::cout << std::left << std::setw(35) << url
                  << std::setw(10) << stars
                  << std::setw(10) << forks
                  << std::setw(10) << commits.substr(0, commits.size() - 8)
                  << std::setw(10) << issues
                  << std::setw(10) << prs << "\n";
    }
}
