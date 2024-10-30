#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class Diff {
    private:
        static std::vector<std::string> readSentences(std::string &fname) {
            std::ifstream ifs {fname};
            std::string buffer;
            std::vector<std::string> sentences;
            if (!ifs) {
                std::cout << "cdiff: " << fname << ": No such file or directory\n"; 
                std::exit(1);
            } else {
                while (std::getline(ifs, buffer))
                    sentences.push_back(buffer);
                return sentences;
            }
        }

        /* Compute the LCS DP Grid */
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

        /* Helper to generate the patch text to display for 'default' mode.
         * Takes in the patches from two files seperately */
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

        /* We already have everything we need, we need to fold the outputs based on the context length. */
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

            if (!result.empty())
                result.pop_back();
            return result;
        }

        /* We already have everything we need, we need to fold the outputs */
        static std::string contextPatchText(
            const std::deque<std::tuple<int, std::string>> &f1Patch,
            const std::deque<std::tuple<int, std::string>> &f2Patch,
            std::size_t context = 3
        ) {

            auto withinContextHelper = [] (const std::deque<std::tuple<int, std::string>> &patches, std::size_t context) {
                std::size_t N {patches.size()}, changesWithinContext {0};
                std::vector<bool> toDisplay(N, true);

                // We do a sliding window approach, compute for the 1st window
                std::for_each(
                        patches.cbegin(), patches.cbegin() + (int) std::min(context, patches.size()),
                        [&changesWithinContext] (const std::tuple<int, std::string> &tp) {
                            if (std::get<1>(tp)[0] != ' ') changesWithinContext++;
                        }
                );

                // If we have atleast one change inside window we would display it
                for (std::size_t i = 0; i < patches.size(); i++) {
                    if (i + context < N && std::get<1>(patches[i + context])[0] != ' ') changesWithinContext++;
                    if (i > context && std::get<1>(patches[i - context - 1])[0] != ' ') changesWithinContext--;
                    if (!changesWithinContext) toDisplay[i] = false;
                }

                return toDisplay;
            };

            std::size_t N1 {f1Patch.size()}, N2 {f2Patch.size()}, i {0}, j {0};
            std::vector<bool> f1Display {withinContextHelper(f1Patch, context)};
            std::vector<bool> f2Display {withinContextHelper(f2Patch, context)};

            std::size_t startf1, startf2;
            std::string result {""}, acc1 {""}, acc2 {""};
            while (i < N1 || j < N2) {
                while (i < N1 && !f1Display[i] && j < N2 && !f2Display[j]) {
                    i++; j++;
                }

                startf1 = i, startf2 = j;
                while ((i < N1 && f1Display[i]) || (j < N2 && f2Display[j]) || (i < N1 && j >= N2) || (i >= N1 && j < N2)) {
                    std::string curr1 {i < N1? std::get<1>(f1Patch[i]): " "}, curr2 {j < N2? std::get<1>(f2Patch[j]): " "};
                    if (curr1[0] == '-') {
                        if (i < N1 && f1Display[i])
                            acc1 += curr1 + "\n"; 
                        i++;
                    } else if (curr2[0] == '+') {
                        if (j < N2 && f2Display[j])
                            acc2 += curr2 + "\n"; 
                        j++;
                    } else {
                        if (i < N1 && f1Display[i])
                            acc1 += curr1 + "\n"; 
                        if (j < N2 && f2Display[j])
                            acc2 += curr2 + "\n"; 
                        i++; j++;
                    }
                }

                result += std::format("***************\n*** {},{} ****\n{}--- {},{} ----\n{}", startf1 + 1, i, acc1, startf2 + 1, j, acc2);
                acc1.clear();
                acc2.clear();

            }

            return result;
        }

        /* From std::fs::file_time_type to string format: 2024-10-09 21:45:57.930538238 +0530 */
        static std::string format_file_time(const std::filesystem::file_time_type& ftime) {
            // Cast to system clock
            std::chrono::time_point sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);

            // Separate into whole seconds and subseconds
            auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(sctp);
            auto subseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(sctp - seconds).count();

            std::stringstream buffer;
            std::time_t time = std::chrono::system_clock::to_time_t(sctp);
            std::tm *lt {std::localtime(&time)};
            buffer << std::put_time(lt, "%F %T");
            buffer << '.' << std::setfill('0') << std::setw(9) << subseconds;
            buffer << std::put_time(lt, " %z");

            return buffer.str();
        }
        
    public:
        /* Helper function to be used when cdiff is being used in context and unified mode */
        static std::string diffFileHeader(std::string &fname1, std::string &fname2, char left, char right) {
            int fnameLength {std::max({(int)fname1.size(), (int)fname2.size(), 20})};
            std::filesystem::file_time_type mdate1 {std::filesystem::last_write_time(fname1)}, mdate2 {std::filesystem::last_write_time(fname2)};
            return std::format(
                    "{} {:<{}} {}\n{} {:<{}} {}", std::string(3, left), fname1, fnameLength,
                    format_file_time(mdate1), std::string(3, right),
                    fname2, fnameLength, format_file_time(mdate2)
            );
        }

        static std::string unifiedDiff(std::string &fpath1, std::string &fpath2) {
            // Read file name into vector
            std::vector<std::string> sentences1 {readSentences(fpath1)}, sentences2 {readSentences(fpath2)};

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
            std::string result {diffFileHeader(fpath1, fpath2, '-', '+') + "\n" + unifiedPatchText(deltas)};

            return result;
        }

        static std::string contextDiff(std::string &fpath1, std::string &fpath2) {
            // Read file into vector
            std::vector<std::string> sentences1 {readSentences(fpath1)}, sentences2 {readSentences(fpath2)};

            // Length of both strings
            std::size_t N1 = sentences1.size(), N2 = sentences2.size();

            // Compute DP Grid
            std::vector<std::vector<int>> dp {computeLCSGrid(N1, N2, sentences1, sentences2)};

            // Compute difference between two strings
            std::size_t i = N1, j = N2;
            std::deque<std::tuple<int, std::string>> f1Patches, f2Patches, f1Patch, f2Patch;

            // Helper to eliminate some code redundancy
            auto accumulatePatch = [] (std::deque<std::tuple<int, std::string>> &patch, std::deque<std::tuple<int, std::string>> &patches, bool otherPatchEmpty) -> void {
                for (std::tuple<int, std::string> &tp: patch) {
                    if (!otherPatchEmpty)
                        std::get<1>(tp) = "!" + std::get<1>(tp).substr(1);
                    patches.push_front(tp);
                }
            };

            while (i > 0 || j > 0) {
                if (i > 0 && j > 0 && sentences1[i - 1] == sentences2[j - 1]) {
                    // Accumulate Patch created so far
                    accumulatePatch(f1Patch, f1Patches, f2Patch.empty());
                    accumulatePatch(f2Patch, f2Patches, f1Patch.empty());

                    // Clear patches before moving forward
                    f1Patch.clear();
                    f2Patch.clear();

                    i--; j--;
                    f1Patches.push_front({i, "  " + sentences1[i]});
                    f2Patches.push_front({j, "  " + sentences2[j]});
                } else if (j <= 0 || (i > 0 && dp[i - 1][j] > dp[i][j - 1])) {
                    i--;
                    f1Patch.push_back({i, "- " + sentences1[i]});
                }
                else {
                    j--;
                    f2Patch.push_back({j, "+ " + sentences2[j]});
                }
            }

            // Accumulate Patch created so far
            accumulatePatch(f1Patch, f1Patches, f2Patch.empty());
            accumulatePatch(f2Patch, f2Patches, f1Patch.empty());

            // Convert to string
            std::string result {diffFileHeader(fpath1, fpath2, '*', '-') + "\n" + contextPatchText(f1Patches, f2Patches)};

            return result;
        }

        static std::string defaultDiff(std::string &fpath1, std::string &fpath2) {
            // Read file into vector
            std::vector<std::string> sentences1 {readSentences(fpath1)}, sentences2 {readSentences(fpath2)};

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
            if (!result.empty() && result.back() == '\n')
                result.pop_back();

            return result;
        }
};

int main(int argc, char **argv) {
    if (argc < 3 || (argc == 4 && std::strcmp(argv[1], "-u") != 0 && std::strcmp(argv[1], "-c") != 0) || (argc > 4))
        std::cout << "Usage: cdiff [-u|-c] <file1> <file2>\n";

    else {

        std::string fpath1 {argc == 3? argv[1]: argv[2]}, fpath2 {argc == 3? argv[2]: argv[3]};

        std::string deltas;
        if (argc == 3)
            deltas = Diff::defaultDiff(fpath1, fpath2);
        else if (std::strcmp(argv[1], "-u") == 0)
            deltas = Diff::unifiedDiff(fpath1, fpath2);
        else
            deltas = Diff::contextDiff(fpath1, fpath2);

        // Print out the deltas
        std::cout << deltas << "\n";
    }

    return 0;
}
