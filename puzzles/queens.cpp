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

#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"

class QueensSolver {
    private:
        cv::Mat image;
        const std::vector<cv::Rect> contours;
        const std::size_t nQueens;
        const std::vector<std::vector<unsigned>> grid; 
        std::unordered_set<std::size_t> cols;
        std::unordered_set<unsigned> regions; 
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

        static void drawX(const cv::Mat &img, const cv::Rect &bbox) {
            int dx {bbox.width / 10}; 
            int dy {bbox.height / 10};

            cv::Point p1 {bbox.x + dx, bbox.y + dy};
            cv::Point p2 {bbox.x + bbox.width - dx, bbox.y + bbox.height - dy};
            cv::Point p3 {bbox.x + bbox.width - dx, bbox.y + dy};
            cv::Point p4 {bbox.x + dx, bbox.y + bbox.height - dy};

            cv::line(img, p1, p2, cv::Scalar(0, 0, 255), 2);
            cv::line(img, p3, p4, cv::Scalar(0, 0, 255), 2);
        }

        static cv::Mat readImage(const std::string &fname) {
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
            return img;
        }

        static std::vector<cv::Rect> getCells(const cv::Mat &image) {
            // 1. Detect contours
            cv::Mat gray, thresh;
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
            cv::threshold(gray, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
            std::vector<std::vector<cv::Point>> rawContours;
            cv::findContours(thresh, rawContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            if (rawContours.empty()) throw std::runtime_error("No cell contours found.");

            // 2. Filter out contours with areas too large or too small
            std::ranges::nth_element(
                rawContours, rawContours.begin() + rawContours.size() / 2, std::ranges::less{}, 
                [](const std::vector<cv::Point> &cont) { return cv::contourArea(cont); }
            );
            auto filterContFunc {[mA = cv::contourArea(rawContours[rawContours.size() / 2])]
                (const std::vector<cv::Point> &cont) {
                    return (std::fabs(cv::contourArea(cont) - mA) / mA) < 0.3;
                }
            };
            std::vector<cv::Rect> contours {
                rawContours 
                    | std::ranges::views::filter(filterContFunc) 
                    | std::ranges::views::transform([](const std::vector<cv::Point> &cont) { return cv::boundingRect(cont); })
                    | std::ranges::to<std::vector>()
            };

            // 3. Sort the contours
            std::ranges::sort(contours, [](const cv::Rect &rp1, const cv::Rect &rp2) -> bool {
                return std::abs(rp1.y - rp2.y) > (rp1.height / 2)? 
                    rp1.y < rp2.y: // Different rows
                    rp1.x < rp2.x; // Different cols
            });

            // Ensure that we have a perfect square of cells
            unsigned long nQueens {static_cast<unsigned long>(std::sqrt(contours.size()))};
            if (nQueens * nQueens != contours.size())
                throw std::runtime_error("Invalid grid, detected cells is not a square no: " 
                        + std::to_string(contours.size()));

            return contours;
        }

        std::vector<std::vector<unsigned>> constructGrid() {
            // Convert to grid of characters
            unsigned region {0};
            std::vector<std::vector<unsigned>> grid(nQueens, std::vector<unsigned>(nQueens, 0));
            std::unordered_map<double, unsigned> colorMapping;
            for (std::size_t i {0}; i < contours.size(); ++i) {
                const cv::Rect &bbox {contours[i]};
                cv::Point center {bbox.x + bbox.width / 2, bbox.y + bbox.height / 2};
                cv::Vec3b bgr {image.at<cv::Vec3b>(center)};
                double avgColor = 0.114 * bgr[0] + 0.587 * bgr[1] + 0.299 * bgr[2];
                if (colorMapping.find(avgColor) == colorMapping.end())
                    colorMapping[avgColor] = region++;
                unsigned ch {colorMapping[avgColor]};
                grid[i / nQueens][i % nQueens] = ch;
                cv::rectangle(image, bbox, cv::Scalar(0, 255, 0), 2);
                cv::putText(image, std::to_string(ch), bbox.tl() + cv::Point(2, 12), 
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 255), 1);
            }
            return grid;
        }

    public:
        QueensSolver(const std::string &fname): 
            image(readImage(fname)), 
            contours(getCells(image)),
            nQueens(static_cast<unsigned long>(std::sqrt(contours.size()))), 
            grid(constructGrid())
        {
            // Check - 1
            if (grid.empty() || grid.size() != grid[0].size())
                throw std::runtime_error("Invalid Grid dimensions");

            // Cols is the set of still unfilled columns
            for (std::size_t i {0}; i < nQueens; ++i) 
                cols.insert(i);

            // Obtain the list of regions unfilled
            for (const std::vector<unsigned> &row: grid)
                for (const unsigned& ch: row)
                    regions.insert(ch);

            // Check - 2
            if (regions.size() != nQueens) 
                throw std::runtime_error("Regions must equal the grid dimensions");

            // Placeholder to hold the text rep solution
            solution = std::vector<std::vector<bool>>(nQueens, std::vector<bool>(nQueens, false));
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

        // Write solution to image file
        void save(const std::string &fname) const {
            cv::Mat solImg {image.clone()};
            for (std::size_t i {0}; i < contours.size(); ++i) {
                std::size_t row {i / nQueens}, col {i % nQueens};
                const cv::Rect &bbox {contours[i]};
                if (solution[row][col]) drawX(solImg, bbox);
            }
            cv::imwrite(fname, solImg);
        }
};

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage queens <input_image> <output_image>\n";
        return 1;
    } else {
        QueensSolver solver(argv[1]);
        if (!solver.solve())
            throw std::runtime_error("No solution exists");
        solver.save(argv[2]);
        return 0;
    }
}
