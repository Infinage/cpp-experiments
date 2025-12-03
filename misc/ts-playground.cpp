/*
 * How to use:
 * 1. Install tree-sitter: sudo pacman -S tree-sitter
 * 2. Install tree-sitter-cpp to LDPATH
 *  ```
 *  git clone https://github.com/tree-sitter/tree-sitter-cpp
 *  make
 *  sudo cp tree-sitter-cpp.so /usr/lib
 *  sudo ldconfig
 *  g++ treesitter-playground.cpp -o tsquery -ltree-sitter -ltree-sitter-cpp
 *  ```
 * 3. ./tsquery <file> # enter query to see it extracting code fragments
 */

#include <fstream>
#include <iostream>
#include <print>
#include <string>

extern "C" {
#include "tree_sitter/api.h"
}

extern "C" const TSLanguage *tree_sitter_cpp();

std::string_view extractNodeText(std::string_view code, TSNode node) {
    uint32_t startPos = ts_node_start_byte(node);
    uint32_t endPos = ts_node_end_byte(node);
    return code.substr(startPos, endPos - startPos);
}

void execQuery(std::string_view code, TSTree *tree, TSQuery *query) {
    // Execute the query
    TSQueryCursor *cursor {ts_query_cursor_new()};
    ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));

    // Iterate through the matching results
    TSQueryMatch match; std::size_t matchCount {};
    while (ts_query_cursor_next_match(cursor, &match)) {
        std::print("Found Match #{}, ", matchCount++);
        for (std::size_t i {}; i < match.capture_count; ++i) {
            TSQueryCapture capture {match.captures[i]};
            auto fragment {extractNodeText(code, capture.node)};
            std::println("Capture #{}\n{}\n", i, fragment);
        }
    }

    // Free the cursor and query object
    ts_query_cursor_delete(cursor);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::println("Usage: tsquery <file>");
        return 1;
    }

    // Attempt loading the file
    std::ifstream ifs {argv[1]};
    if (!ifs) throw std::runtime_error{"File is missing or cannot be read"};

    // Initialize the tree sitter parser
    TSParser *parser = ts_parser_new();
    if (!ts_parser_set_language(parser, tree_sitter_cpp()))
        throw std::runtime_error{"Language version mismatch"};

    // Read the file
    std::string code {std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};

    // Parse the file once into a treesitter tree
    TSTree *tree {ts_parser_parse_string(parser, nullptr, code.c_str(), 
        static_cast<uint32_t>(code.size()))};
    if (!tree) throw std::runtime_error("Parse failed");

    std::string queryStr {"*"};
    while (!queryStr.empty()) {
        std::getline(std::cin, queryStr);
        uint32_t errOffset; TSQueryError errType;
        TSQuery *query {ts_query_new(tree_sitter_cpp(), queryStr.c_str(), 
                static_cast<uint32_t>(queryStr.size()), &errOffset, &errType)};
        if (!query) std::println("Query error type at pos {}", errOffset);
        else {
            execQuery(code, tree, query);
            ts_query_delete(query);
        }
    }

    // Clean up the resources
    ts_tree_delete(tree);
    ts_parser_delete(parser);
}
