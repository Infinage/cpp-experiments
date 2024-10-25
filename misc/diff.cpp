#include <algorithm>
#include <iostream>
#include <vector>

class Diff {
    public:
        std::string longestCommonSubsequence(std::string &word1, std::string &word2) {
            // Length of both strings
            std::size_t N1 = word1.size(), N2 = word2.size();

            // Compute the LCS length
            std::vector<std::vector<int>> dp(N1 + 1, std::vector<int>(N2 + 1, 0));
            for (std::size_t i = 0; i < N1; i++) {
                for (std::size_t j = 0; j < N2; j++) {
                    if (word1[i] == word2[j])
                        dp[i + 1][j + 1] = 1 + dp[i][j];
                    else
                        dp[i + 1][j + 1] = std::max(dp[i][j + 1], dp[i + 1][j]);
                }
            }

            // Compute LCS string
            std::string LCS {""};
            std::size_t i = N1, j = N2;
            while (i > 0 && j > 0) {
                if (word1[i - 1] == word2[j - 1]) {
                    LCS += word1[i - 1];
                    i--; j--;
                } else if (dp[i - 1][j] > dp[i][j - 1]) i--;
                else j--;
            }

            // Reverse the LCS obtained
            std::ranges::reverse(LCS);

            return LCS;
        }
};

int main() {
    // Read two strings & return the LCS
    Diff diff;
    std::string word1, word2;
    std::cin >> word1 >> word2;
    std::cout << diff.longestCommonSubsequence(word1, word2) << "\n";
    return 0;
}
