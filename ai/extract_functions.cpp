// g++ extract-functions.cpp -ltree-sitter -ltree-sitter-cpp -o fextract -std=c++23 
// find ../ \( -name "*.hpp" -or -name "*.cpp" \) -exec ./fextract code.csv {} + 

#include <fstream>
#include <iostream>
#include <print>
#include <string>

#include "../misc/CSVUtil.hpp"

extern "C" {
#include "tree_sitter/api.h"
}

extern "C" const TSLanguage *tree_sitter_cpp();

// Given a node, extract its offset from the code
std::string_view extractNodeText(std::string_view code, TSNode node) {
    uint32_t startPos = ts_node_start_byte(node);
    uint32_t endPos = ts_node_end_byte(node);
    return code.substr(startPos, endPos - startPos);
}

// Get first parent matching given input type
TSNode parentOfType(TSNode node, std::string_view typeName) {
    TSNode p {ts_node_parent(node)};
    while (!ts_node_is_null(p)) {
        if (std::string_view(ts_node_type(p)) == typeName.data()) return p;
        p = ts_node_parent(p);
    }
    return TSNode{};
}

// Extract first parent matching type but return its field name
std::string_view parentOfType_Name(std::string_view code, TSNode node, std::string_view typeName) {
    TSNode ns {parentOfType(node, typeName)};
    if (ts_node_is_null(ns)) return "";

    TSNode name {ts_node_child_by_field_name(ns, "name", 4)};
    if (ts_node_is_null(name)) return "";

    return extractNodeText(code, name);
}

// Extracts and returns CSV in the following format:
// filepath,namespaceName,className,functionName,startPos,endPos,<funcBody>
std::string extractFunctionInfo(std::string_view code, TSQueryMatch match) {
    TSNode fnBodyNode = match.captures[0].node; // @func.def
    TSNode fnNameNode = match.captures[1].node; // @func.name

    auto ns {parentOfType_Name(code, fnNameNode, "namespace_definition")};
    auto cls {parentOfType_Name(code, fnNameNode, "class_specifier")};
    auto name {extractNodeText(code, fnNameNode)};
    auto body {extractNodeText(code, fnBodyNode)};
    uint32_t startPos = ts_node_start_byte(fnBodyNode);
    uint32_t endPos = ts_node_end_byte(fnBodyNode);

    return std::format("{},{},{},{},{},{}", CSVUtil::writeCSVField(ns), 
        CSVUtil::writeCSVField(cls), CSVUtil::writeCSVField(name), 
        startPos, endPos, CSVUtil::writeCSVField(body));
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::println("Usage: ts-fextract <outputFile> <file1> [<file2> [<file3>]]");
        return 1;
    }

    // Initialize the tree sitter parser
    TSParser *parser = ts_parser_new();
    if (!ts_parser_set_language(parser, tree_sitter_cpp()))
        throw std::runtime_error{"Language version mismatch"};

    // Parse and create the query object
    constexpr const char queryStr[] = R"(
    (
      function_definition
        declarator: (function_declarator
          declarator: [
            (identifier) @func.name
            (qualified_identifier) @func.name
          ]
        )
    ) @func.def
    )";
    uint32_t errOffset; TSQueryError errType;
    TSQuery *query {ts_query_new(tree_sitter_cpp(), queryStr, sizeof(queryStr) - 1, &errOffset, &errType)}; 
    if (!query) throw std::runtime_error{"Query error type at pos " + std::to_string(errOffset)};

    // Create a file to write the CSV
    std::ofstream ofs {argv[1]};
    if (!ofs) throw std::runtime_error("Unable to open file for writing output");

    // Read all of the input files
    ofs << "file,namespace,class,function,start,end,body\n";
    for (int i {2}; i < argc; ++i) {
        const char *fileName {argv[i]};
        // Attempt to load the file
        std::ifstream ifs {fileName};
        if (!ifs) { std::println(std::cerr, "File {} is missing or cannot be read", fileName); continue; }
        std::string code {std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};

        // Parse the file once into a treesitter tree
        TSTree *tree {ts_parser_parse_string(parser, nullptr, code.c_str(), static_cast<uint32_t>(code.size()))};
        if (!tree) { std::println(std::cerr, "Failed to parse file {}", argv[i]); continue; }

        // Execute the query and extract the matches
        TSQueryCursor *cursor {ts_query_cursor_new()};
        ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));
        TSQueryMatch match;
        while (ts_query_cursor_next_match(cursor, &match)) {
            ofs << std::format("{},{}", CSVUtil::writeCSVField(fileName), extractFunctionInfo(code, match)) << '\n';
        }

        // Free the cursor and query object
        ts_query_cursor_delete(cursor);
        ts_tree_delete(tree);
    }

    // Clean up the resources
    ts_query_delete(query);
    ts_parser_delete(parser);
}
