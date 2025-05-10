// https://queensgame.vercel.app/level/1
// g++ queens.cpp -o queens -std=c++23 -I/usr/include/opencv4 -lopencv_core -lopencv_imgcodecs -lopencv_imgproc -I/home/kael/cpplib/include -Wno-deprecated-enum-enum-conversion

#include <algorithm>
#include <ranges>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "crow/app.h"

std::string htmlMarkup {R"*(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Queens Puzzle Solver</title>
    <style>
        body {
            background-color: #111;
            color: #eee;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding-top: 2em;
            height: 100vh;
            margin: 0;
        }

        .container {
            text-align: center;
            max-width: 90vw;
        }

        h1 {
            margin-bottom: 1em;
        }

        input[type="file"] {
            margin: 0.5em 0;
            padding: 0.3em;
            color: #ccc;
        }

        input::file-selector-button,
        button {
            margin: 0.5em;
            padding: 0.6em 1.2em;
            background-color: #333;
            color: #fff;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
        }

        input::file-selector-button:hover,
        button:hover {
            background-color: #555;
        }

        img {
            margin-top: 1em;
            max-width: 100%;
            max-height: 65vh;
            border: 2px solid #444;
            border-radius: 4px;
            box-shadow: 0 0 12px rgba(0,0,0,0.6);
            display: none;
        }

        #pasteNote {
            font-size: 0.9em;
            color: #aaa;
            margin-top: 0.5em;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Queens Puzzle Solver</h1>
        <div id="pasteNote">Paste from Clipboard or upload an image.</div>
        <div style="display: flex; align-items: center; justify-content: center; gap: 0.5em;">
            <input type="file" id="fileInput" accept="image/*"/>
            <span id="clearBtn" title="Clear" onclick="clearImage()" style="display:none; cursor:pointer; font-size: 1.5em;">❌</span>
        </div>
        <img id="outputImg" />
    </div>

    <script>
        async function submitImage(imageBlob = null) {
            const file = imageBlob || document.getElementById('fileInput').files[0];
            if (!file) return;
            const arrayBuffer = await file.arrayBuffer();
            const response = await fetch("/solve", {
                method: "POST",
                body: arrayBuffer,
                headers: { "Content-Type": "application/octet-stream" }
            });

            if (response.ok) {
                const blob = await response.blob();
                const url = URL.createObjectURL(blob);
                const imgEl = document.getElementById("outputImg");
                imgEl.src = url;
                imgEl.style.display = 'block';
                document.getElementById("clearBtn").style.display = 'inline';
            } else {
                alert("Failed to process image.");
            }
        }

        function clearImage() {
            document.getElementById('fileInput').value = "";
            const imgEl = document.getElementById("outputImg");
            imgEl.src = "";
            imgEl.style.display = 'none';
            document.getElementById("clearBtn").style.display = 'none';
        }

        document.getElementById('fileInput').addEventListener('change', () => {
            submitImage();
        });

        document.addEventListener('paste', (e) => {
            const items = e.clipboardData.items;
            for (const item of items) {
                if (item.type.startsWith("image/")) {
                    const blob = item.getAsFile();
                    submitImage(blob);
                    break;
                }
            }
        });
    </script>
</body>
</html>
)*"};

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

        static cv::Mat readImage(const std::string &data) {
            std::vector<uchar> blob {data.begin(), data.end()};
            cv::Mat img {cv::imdecode(blob, cv::IMREAD_COLOR)};
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
            auto contoursView {
                rawContours 
                    | std::ranges::views::filter(filterContFunc) 
                    | std::ranges::views::transform([](const std::vector<cv::Point> &cont) { return cv::boundingRect(cont); })
            };
            std::vector<cv::Rect> contours {contoursView.begin(), contoursView.end()};

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
                cv::putText(image, std::to_string(ch), bbox.tl() + cv::Point(2, 12), 
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 0), 1);
            }
            return grid;
        }

    public:
        QueensSolver(const std::string &blobData): 
            image(readImage(blobData)), contours(getCells(image)),
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
        cv::Mat getImage() const {
            cv::Mat solImg {image.clone()};
            for (std::size_t i {0}; i < contours.size(); ++i) {
                std::size_t row {i / nQueens}, col {i % nQueens};
                const cv::Rect &bbox {contours[i]};
                if (solution[row][col]) drawX(solImg, bbox);
            }
            return solImg;
        }
};

static crow::response solvePuzzleReq(const crow::request &req) {
    // Read input and solve it
    QueensSolver solver{req.body};
    solver.solve();
    cv::Mat sol {solver.getImage()};

    // Get solution image for embedding into resp
    std::vector<uchar> blob;
    cv::imencode(".png", sol, blob);
    std::string data {blob.begin(), blob.end()};

    // Construct and return resp
    crow::response resp{data};
    resp.set_header("Content-Type", "image/png");
    return resp;
}

static crow::response homePage() {
    crow::response resp{htmlMarkup};
    resp.set_header("Content-Type", "text/html");
    return resp;
}

int main() {
    crow::SimpleApp app;
    CROW_ROUTE(app, "/").methods(crow::HTTPMethod::GET)(homePage);
    CROW_ROUTE(app, "/solve").methods(crow::HTTPMethod::POST)(solvePuzzleReq);
    app.port(8080).run();
}
