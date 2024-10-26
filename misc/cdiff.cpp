#include <algorithm>
#include <bits/ranges_algo.h>
#include <cstring>
#include <deque>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

class Diff {
    private:
        // Compute the LCS DP Grid
        static std::vector<std::vector<int>> computeLCSGrid(
                std::size_t N1, std::size_t N2,
                std::vector<std::string> &sentences1,
                std::vector<std::string> &sentences2
            ) {
            std::vector<std::vector<int>> dp(N1 + 1, std::vector<int>(N2 + 1, 0));
            for (std::size_t i = 0; i < N1; i++) {
                for (std::size_t j = 0; j < N2; j++) {
                    if (sentences1[i] == sentences2[j])
                        dp[i + 1][j + 1] = 1 + dp[i][j];
                    else
                        dp[i + 1][j + 1] = std::max(dp[i][j + 1], dp[i + 1][j]);
                }
            }
            return dp;
        }

        static inline std::string defaultPatchText(
                std::size_t i, std::size_t j,
                std::deque<std::string> &f1Patch,
                std::deque<std::string> &f2Patch
            ) {

            std::string result{""};

            // Patch info ----------------------
            result += std::to_string(f1Patch.empty()? i: i + 1); // Lines from first file
            if (f1Patch.size() > 1)
                result += "," + std::to_string(i + f1Patch.size());
            result += f1Patch.size() == 0? "a": f2Patch.size() == 0? "d": "c"; 
            result += std::to_string(f2Patch.empty()? j: j + 1); // Lines from second file
            if (f2Patch.size() > 1)
                result += "," + std::to_string(j + f2Patch.size());

            result += "\n";

            // Lines from both files ----------------------
            result += std::ranges::fold_left(f1Patch, std::string{}, [](auto &&acc, auto &t) { return acc + "< " + t + "\n"; });
            if (!f1Patch.empty() && !f2Patch.empty()) result += "---\n";
            result += std::ranges::fold_left(f2Patch, std::string{}, [](auto &&acc, auto &t) { return acc + "> " + t + "\n"; });

            result.pop_back();
            return result;
        }

        /* We already have everything we need, we need to fold the outputs */
        static std::string unifiedPatchText(const std::deque<std::tuple<int, int, std::string>> &deltas, std::size_t context = 3) {
            std::size_t N {deltas.size()}, changesWithinContext {0};
            std::vector<bool> toDisplay(N, true);

            // We do a sliding window approach, compute for the 1st window
            std::for_each(
                    deltas.cbegin(), deltas.cbegin() + (int) std::min(context, deltas.size()),
                    [&changesWithinContext] (const std::tuple<int, int, std::string> &tp) {
                        if (std::get<2>(tp)[0] != ' ') changesWithinContext++;
                    }
            );

            // If we have atleast one change inside window we would display it
            for (std::size_t i = 0; i < deltas.size(); i++) {
                if (i + context < N && std::get<2>(deltas[i + context])[0] != ' ') changesWithinContext++;
                if (i > context && std::get<2>(deltas[i - context - 1])[0] != ' ') changesWithinContext--;
                if (!changesWithinContext) toDisplay[i] = false;
            }

            std::size_t start;
            std::string result {""}, acc {""};
            for (std::size_t i {0}; i < N;) {
                while (i < N && !toDisplay[i]) i++;

                start = i;
                while (i < N && toDisplay[i])
                    acc += std::get<2>(deltas[i++]) + "\n";

                int istart {std::get<0>(deltas[start])}, jstart {std::get<1>(deltas[start])};
                int ilen {std::get<0>(deltas[i - 1]) - istart + 1}, jlen {std::get<1>(deltas[i - 1]) - jstart + 1};

                // For the last iteration only include the indices for len computation
                if (i == N && toDisplay[N - 1]) {
                    ilen--; jlen--;
                    acc.pop_back(); acc.pop_back();
                }

                if (!acc.empty())
                    result += std::format("@@ -{},{} +{},{} @@\n{}", istart + 1, ilen, jstart + 1, jlen, acc);

                acc.clear();
            }

            result.pop_back();
            return result;
        }

    public:
        static std::string unifiedDiff(std::vector<std::string> &sentences1, std::vector<std::string> &sentences2) {
            // Length of both strings
            std::size_t N1 = sentences1.size(), N2 = sentences2.size();

            // Compute DP Grid
            std::vector<std::vector<int>> dp {computeLCSGrid(N1, N2, sentences1, sentences2)};

            // Compute difference between two strings
            std::size_t i = N1, j = N2;
            std::deque<std::tuple<int, int, std::string>> deltas{{N1, N2, " "}};
            while (i > 0 || j > 0) {
                if (i > 0 && j > 0 && sentences1[i - 1] == sentences2[j - 1]) {
                    i--; j--;
                    deltas.push_front({i, j, " " + sentences1[i]});
                } else if (j <= 0 || (i > 0 && dp[i - 1][j] > dp[i][j - 1])) {
                    i--;
                    deltas.push_front({i, j, "-" + sentences1[i]});
                }
                else {
                    j--;
                    deltas.push_front({i, j, "+" + sentences2[j]});
                }
            }

            // Convert to string
            std::string result {unifiedPatchText(deltas)};

            return result;
        }

        static std::string defaultDiff(std::vector<std::string> &sentences1, std::vector<std::string> &sentences2) {
            // Length of both strings
            std::size_t N1 = sentences1.size(), N2 = sentences2.size();

            // Compute DP Grid
            std::vector<std::vector<int>> dp {computeLCSGrid(N1, N2, sentences1, sentences2)};

            // Compute difference between two strings
            std::deque<std::string> deltas;
            std::deque<std::string> f1Patch, f2Patch;
            std::size_t i = N1, j = N2;
            while (i > 0 || j > 0) {
                if (i > 0 && j > 0 && sentences1[i - 1] == sentences2[j - 1]) {
                    if (!f1Patch.empty() || !f2Patch.empty())
                        deltas.push_back(defaultPatchText(i, j, f1Patch, f2Patch));
                    f1Patch.clear();
                    f2Patch.clear();
                    i--; j--;
                } else if (j <= 0 || (i > 0 && dp[i - 1][j] > dp[i][j - 1])) {
                    f1Patch.push_front(sentences1[--i]);
                }
                else {
                    f2Patch.push_front(sentences2[--j]);
                }
            }

            // Missing pieces until start of both files
            if (!f1Patch.empty() || !f2Patch.empty())
                deltas.push_back(defaultPatchText(i, j, f1Patch, f2Patch));

            // Reverse the deltas obtained
            std::ranges::reverse(deltas);

            // Convert to string
            std::string result {std::ranges::fold_left(deltas, std::string{}, [](auto &&acc, auto &s) { return acc + s + "\n"; })};
            result.pop_back();

            return result;
        }
};

int main(int argc, char **argv) {
    if (argc < 3 || (argc == 4 && std::strcmp(argv[1], "-u") != 0) || (argc > 4))
        std::cout << "Usage: cdiff [-u] <file1> <file2>\n";

    else {
        auto readSentences = [] (std::string &&fname) -> std::vector<std::string> {
            std::ifstream ifs {fname};
            std::string buffer;
            std::vector<std::string> sentences;
            while (std::getline(ifs, buffer))
                sentences.push_back(buffer);
            return sentences;
        };

        std::vector<std::string> sentences1 {readSentences(argc == 3? argv[1]: argv[2])};
        std::vector<std::string> sentences2 {readSentences(argc == 3? argv[2]: argv[3])};
        std::string deltas {argc == 4? Diff::unifiedDiff(sentences1, sentences2): Diff::defaultDiff(sentences1, sentences2)};

        // Print out the deltas
        std::cout << deltas << "\n";
    }

    return 0;
}
