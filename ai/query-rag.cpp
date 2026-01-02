#include "../misc/sqlite.hpp"
#include "../json-parser/json.hpp"
#include "../networking/net.hpp"

#include <iostream>
#include <print>
#include <string>

// Constants
constexpr std::size_t EMBED_MAX_CHARS = 300; 
constexpr std::size_t EMBED_DIM = 384; 

// Struct to store the retreived context info
struct Context {
    std::size_t eid;
    double dist;
    std::string file, ns, cls, fn, body;

    Context(sqlite::Statement::Row &row):
        eid {row->column<sqlite::dtype::integer>(0)},
        dist {row->column<sqlite::dtype::real>(1)},
        file {std::string{row->column<sqlite::dtype::text>(2)}},
        ns {std::string{row->column<sqlite::dtype::text>(3)}},
        cls {std::string{row->column<sqlite::dtype::text>(4)}},
        fn {std::string{row->column<sqlite::dtype::text>(5)}},
        body {std::string{row->column<sqlite::dtype::text>(6)}}
    {}
};

// Embedding type for convenience
using EMBEDDING = std::array<float, EMBED_DIM>;

// Helper to chunk given query per embed char limit
std::vector<std::string_view> splitChunks(std::string_view text, std::size_t maxSize) {
    std::vector<std::string_view> chunks;
    auto numChunks = (text.size() + maxSize - 1) / maxSize;
    for (std::size_t cid {}; cid < numChunks; ++cid) {
        auto start = cid * maxSize;
        auto end = std::min(start + maxSize, text.size());
        chunks.push_back(text.substr(start, end - start));
    }
    return chunks;
} 

// Process and send out REST API request to ollama embedding url
EMBEDDING extractEmbeddings(std::string_view query) {
    auto chunks = splitChunks(query, EMBED_MAX_CHARS);

    // Prepare a list of strings (in json string)
    std::string text(1, '[');
    text.reserve(chunks.size() * EMBED_MAX_CHARS);
    for (auto chunk: chunks)
        text += '"' + JSON::helper::jsonEscape(chunk) + "\",";
    text.pop_back(); text += ']';

    // Send a HTTP request to process embeddings
    net::HttpRequest req {"http://localhost:11434/api/embed", "POST"};
    req.setHeader("Accept", "application/json");
    req.setBody(R"({"model":"all-minilm:l6-v2","input":)" + text + R"(})");
    auto resp = req.execute();

    // Throw in case of errors
    if (!resp.ok()) throw std::runtime_error{resp.body};

    // Process the HTTP response (unchunk if required)
    else {
        auto data = JSON::Parser::loads(resp.header("Transfer-Encoding") == "chunked"? 
                resp.unchunk(): resp.body);

        // Retrieve and store the mean embedding
        EMBEDDING result {};
        for (JSON::JSONHandle embedding: data["embeddings"]) {
            for (auto [j, valNode]: std::ranges::enumerate_view(embedding)) {
                auto val = reinterpret_cast<JSON::JSONValueNode*>(valNode.get())->getValue();
                result[static_cast<std::size_t>(j)] += static_cast<float>(
                    std::get<double>(val) / static_cast<double>(chunks.size()));
            }
        }

        return result;
    }
}

std::string preparePrompt(sqlite::Statement &queryRes, std::string_view input) {
    std::ostringstream oss; 
    oss << "You are a coding assistant.\n\n"
        << "Use the following context to answer the question.\n"
        << "If the answer is not in the context, say so clearly.\n\n"
        << "[CONTEXT]\n";
    for (Context ctx: queryRes) {
        oss << "File: " << ctx.file;
        oss << "Function: " << ctx.fn;
        oss << "Code:\n" << ctx.body;
        oss << "--- \n ---";
    }
    oss << "\n\n[QUESTION]\n" << input;
    return oss.str();
}

// TODO: handle stream API
std::string queryModelWithPrompt(std::string_view prompt) {
    // Send a HTTP request to process embeddings
    net::HttpRequest req {"http://localhost:11434/api/generate", "POST"};
    req.setHeader("Accept", "application/json");
    req.setBody(R"({"model":"phi3:mini","stream": false, "prompt":")" 
        + JSON::helper::jsonEscape(prompt) + R"("})");

    // Execute query & throw in case of errors
    auto resp = req.execute(30);
    if (!resp.ok()) throw std::runtime_error{resp.body};

    // Process the HTTP response (unchunk if required)
    else {
        auto data = JSON::Parser::loads(resp.header("Transfer-Encoding") == "chunked"? 
                resp.unchunk(): resp.body);
        return data["response"].to<std::string>();
    }
 }

int main() {
    // Init a DB connection
    auto db = sqlite::open(".codebase");
    if (!db) throw std::runtime_error{"Init: " + db.error()};

    // Enable extension loading
    auto res = db->enableloadExtension(true);
    if (!res) throw std::runtime_error{"Enable ext load: " + res.error()};

    // Vector DB support
    res = db->exec(R"(
        SELECT load_extension('./vector');
        SELECT vector_init('embeddings', 'embedding', 'dimension=384,distance=cosine');
        SELECT vector_quantize('embeddings', 'embedding');
        -- SELECT vector_quantize_preload('embeddings', 'embedding');
    )");
    if (!res) throw std::runtime_error{"Ext load: " + res.error()};

    // Prepare query to fetch context
    auto searchQ = db->query(R"(
        SELECT e.id, v.distance, f.file, f.namespace, f.class, f.function, e.body
        from embeddings as e JOIN functions as f ON e.fid = f.id
        JOIN vector_quantize_scan('embeddings', 'embedding', ?, 5) as v
        ON e.id = v.rowid
    )");
    if (!searchQ) throw std::runtime_error{"Q prep: " + res.error()};

    // REPL style
    std::string input;        
    while (std::print(">> "), std::getline(std::cin, input)) {
        if (input.empty()) continue;
        if (input == "exit" || input == "quit") break;

        res = searchQ->reset(true);
        if (!res) std::println("Unbind: {}", res.error());

        auto embeddings = extractEmbeddings(input);
        res = searchQ->bind<sqlite::dtype::blob>(1, embeddings);
        if (!res) std::println("Vector search: {}", res.error());

        // Prepare the prompt
        auto prompt = preparePrompt(searchQ.value(), input);

        // Get model response
        std::println("{}", queryModelWithPrompt(prompt));
    }

    std::println("bye!");
}
