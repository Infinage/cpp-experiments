// https://queensgame.vercel.app/level/1
// g++ queens.cpp -o queens -std=c++23 -I/usr/include/opencv4 -lopencv_core -lopencv_imgcodecs -lopencv_imgproc -lopencv_highgui

#include <algorithm>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

class QueensSolver {
    private:
        const std::vector<std::vector<char>> grid; 
        std::size_t nQueens;
        std::unordered_set<std::size_t> cols;
        std::unordered_set<char> regions; 
        std::vector<std::vector<bool>> solution;

        // Returns true if neighbours of said cell has a queen placed
        bool checkNeighbours(const std::size_t row, const std::size_t col) const {
            for (int dr {-1}; dr <= 1; ++dr) {
                for (int dc {-1}; dc <= 1; ++dc) {
                    std::size_t cRow {row + static_cast<std::size_t>(dr)};
                    std::size_t cCol {col + static_cast<std::size_t>(dc)};
                    if (cRow < nQueens && cCol < nQueens && solution[cRow][cCol])
                        return true;
                }
            }
            return false;
        }

        bool check(const std::size_t row, const std::size_t col) const {
            if (row >= nQueens || col >= nQueens) return false;
            else if (cols.find(col) == cols.end()) return false;
            else if (regions.find(grid[row][col]) == regions.end()) return false;
            else if (checkNeighbours(row, col)) return false;
            else return true;
        }

        void set(const std::size_t row, const std::size_t col) {
            solution[row][col] = true; cols.erase(col); 
            regions.erase(grid[row][col]);
        }

        void unset(const std::size_t row, const std::size_t col) {
            solution[row][col] = false; cols.insert(col); 
            regions.insert(grid[row][col]);
        }

        static std::vector<std::vector<char>> readGrid(const std::string &fname) {
            cv::Mat img {cv::imread(fname)};
            if (img.empty()) throw std::runtime_error("cannot read image");

            // 1. HSV -> saturation
            cv::Mat hsv, sat; std::vector<cv::Mat> ch;
            cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
            cv::split(hsv, ch); sat = ch[1];

            // 2. Blur + threshold
            cv::Mat satBlur, mask;
            cv::GaussianBlur(sat, satBlur, cv::Size(7,7), 0);
            cv::threshold(satBlur, mask, 60, 255, cv::THRESH_BINARY);

            // 3. Morph close to fill gaps, then contours
            cv::Mat kernel {cv::getStructuringElement(cv::MORPH_RECT, cv::Size(25,25))};
            cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
            std::vector<std::vector<cv::Point>> cnts;
            cv::findContours(mask, cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            if (cnts.empty()) throw std::runtime_error("No colorful region");
            std::vector<cv::Point> biggest = *std::max_element(
                cnts.begin(), cnts.end(), [](auto &a, auto &b){ 
                    return cv::contourArea(a) < cv::contourArea(b); 
                }
            );

            // 4. Crop the image around region of interest
            img = img(cv::boundingRect(biggest)).clone(); 

            // 5. Detect contours
            cv::Mat gray, thresh;
            cv::cvtColor(img, gray, cv::COLOR_RGB2GRAY);
            cv::threshold(gray, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
            std::vector<std::vector<cv::Point>> rawContours;
            cv::findContours(thresh, rawContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            if (rawContours.empty()) throw std::runtime_error("No cell contours found.");

            // 6. Filter out contours with areas too large or too small
            std::ranges::nth_element(
                rawContours, rawContours.begin() + rawContours.size() / 2, std::ranges::less{}, 
                [](const std::vector<cv::Point> &cont) { return cv::contourArea(cont); }
            );
            auto filterContFunc {[mA = cv::contourArea(rawContours[rawContours.size() / 2])]
                (const std::vector<cv::Point> &cont) {
                    return std::fabs(cv::contourArea(cont) - mA) < 0.25;
                }
            };
            std::vector<std::vector<cv::Point>> contours {
                rawContours | std::ranges::views::filter(filterContFunc) | std::ranges::to<std::vector>()
            };

            // 7. Sort the filtered contours and apply non max supression
            std::vector<std::vector<cv::Point>> finalBoxes;
            std::ranges::sort(contours, [](const std::vector<cv::Point> &p1, const std::vector<cv::Point> &p2) -> bool {
                cv::Rect rp1 {cv::boundingRect(p1)}, rp2 {cv::boundingRect(p2)};
                return rp1.y < rp2.y || (rp1.y == rp2.y && rp1.x < rp2.x);
            });
            for (const std::vector<cv::Point> &cont: contours) {

            }

            // DEBUG
            std::size_t index {1};
            for (const std::vector<cv::Point> &contour: contours) {
                cv::Rect bbox = cv::boundingRect(contour);
                cv::rectangle(img, bbox, cv::Scalar(0, 255, 0), 2);
                cv::putText(img, std::to_string(index++), bbox.tl() + cv::Point(2, 12),
                            cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 255), 1);
            }
            cv::imshow("debug", img); cv::waitKey(0);

            throw "NOPE";
        }

    public:
        QueensSolver(const std::string &fname): 
            grid(readGrid(fname)), nQueens(this->grid.size()), 
            solution(nQueens, std::vector<bool>(nQueens, false)) 
        {
            // Check - 1
            if (grid.empty() || grid.size() != grid[0].size())
                throw std::runtime_error("Invalid Grid dimensions");

            // Cols is the set of still unfilled columns
            for (std::size_t i {0}; i < nQueens; ++i) 
                cols.insert(i);

            // Obtain the list of regions unfilled
            for (const std::vector<char> &row: grid)
                for (const char& ch: row)
                    regions.insert(ch);

            // Check - 2
            if (regions.size() != nQueens) 
                throw std::runtime_error("Regions must equal the grid dimensions");
        }

        bool solve() {
            std::stack<std::pair<std::size_t, std::size_t>> stk{{{0, 0}}};
            while (!stk.empty()) {
                std::size_t row, col;
                std::tie(row, col) = stk.top();

                // When we were able to move 1 row beyond max
                if (row == nQueens) return true;

                // When we have tried all combinations for a row
                else if (col == nQueens) {
                    stk.pop();
                    if (!stk.empty()) {
                        unset(stk.top().first, stk.top().second);
                        stk.top().second += 1;
                    }
                } 

                // If valid, place row and col onto grid, next row
                else if (check(row, col)) {
                    set(row, col);
                    stk.push({row + 1, 0});
                } 

                // Not valid and col is within bounds, try next col
                else stk.top().second += 1;
            }
            
            return false;
        }

        // Print the board
        std::string getString() const {
            std::ostringstream oss;
            std::string lineSep(nQueens * 2, '-');
            oss << lineSep << '\n';
            for (const std::vector<bool> &row: solution) {
                for (const bool& pos: row)
                    oss << (pos? 'X': ' ') << '|';
                oss << '\n' << lineSep << '\n';
            }
            return oss.str();
        }
};

int main(int argc, char **argv) {
    if (argc != 2) std::cerr << "Usage queens <image>\n";
    else {
        // Solve the grid
        QueensSolver solver(argv[1]);
        if (!solver.solve())
            throw std::runtime_error("No solution exists");

        // Print out the solution
        std::cout << solver.getString();

        return 0;
    }
}
