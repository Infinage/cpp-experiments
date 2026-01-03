#include "../misc/sqlite.hpp"
#include "../json-parser/json.hpp"
#include "../networking/net.hpp"

#include <iostream>
#include <print>
#include <string>

// Constants
constexpr std::size_t EMBED_MAX_CHARS = 300; 
constexpr std::size_t EMBED_DIM = 384; 

// Embedding type for convenience
using EMBEDDING = std::array<float, EMBED_DIM>;

// Struct to store the retreived vector search match
struct SimilarityResult {
    std::size_t eid, fid;
    double dist;
    std::string file, ns, cls, fn, body;

    SimilarityResult(sqlite::Statement::Row &row):
        eid {row->column<sqlite::dtype::integer>(0)},
        fid {row->column<sqlite::dtype::integer>(1)},
        dist {row->column<sqlite::dtype::real>(2)},
        file {std::string{row->column<sqlite::dtype::text>(3)}},
        ns {std::string{row->column<sqlite::dtype::text>(4)}},
        cls {std::string{row->column<sqlite::dtype::text>(5)}},
        fn {std::string{row->column<sqlite::dtype::text>(6)}},
        body {std::string{row->column<sqlite::dtype::text>(7)}}
    {}
};

// Struct to store the context to feed into the prompt
struct Context {
    std::string file, fn, body;
    Context(sqlite::Statement::Row &row):
        file {std::string{row->column<sqlite::dtype::text>(0)}},
        fn {std::string{row->column<sqlite::dtype::text>(1)}},
        body {std::string{row->column<sqlite::dtype::text>(2)}}
    {}
};

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

std::string preparePrompt(const std::vector<Context> &context, std::string_view input) {
    std::ostringstream oss; 
    oss << "You are a coding assistant.\n\n"
        << "Use the following context to answer the question.\n"
        << "If the answer is not in the context, say so clearly.\n\n"
        << "[CONTEXT]\n";
    for (auto &ctx: context) {
        oss << "File: "     << ctx.file << "\n";
        oss << "Function: " << ctx.fn   << "\n";
        oss << "Code:\n"    << ctx.body << "\n";
        oss << "--- \n ---";
    }
    oss << "\n\n[QUESTION]\n" << input;
    return oss.str();
}

void queryModelWithPrompt(std::string_view prompt) {
    // Send a HTTP request to process embeddings
    net::HttpRequest req {"http://localhost:11434/api/generate", "POST"};
    req.setHeader("Accept", "application/json");
    req.setBody(R"({"model":"phi3:mini","stream": true, "prompt":")" 
        + JSON::helper::jsonEscape(prompt) + R"("})");

    // Execute query & throw in case of errors
    req.stream([](const auto &resp) -> bool {
        if (!resp.ok()) throw std::runtime_error{resp.body};
        auto data = resp.json();
        auto output = data["response"].template to<std::string>();
        std::print("{}", JSON::helper::jsonUnescape(output)); 
        std::fflush(stdout);
        return !data["done"].template to<bool>();
    }, 30);
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
    auto fetchContextQ = db->query(R"(
        SELECT e.id, f.id, v.distance, f.file, f.namespace, f.class, f.function, e.body
        from embeddings as e JOIN functions as f ON e.fid = f.id
        JOIN vector_quantize_scan('embeddings', 'embedding', ?, 50) as v
        ON e.id = v.rowid
    )");
    if (!fetchContextQ) throw std::runtime_error{"Q prep: " + res.error()};

    // Prepare query to fetch the complete function
    auto fetchCompleteFunctionQ = db->query(R"(
        SELECT file, function, group_concat(body, '') AS full_body
        FROM (
            SELECT e.fid, f.file, f.function, e.chunk, e.body 
            FROM embeddings e
            JOIN functions f ON e.fid = f.id
            WHERE e.fid IN (?, ?, ?)
            ORDER BY e.fid, e.chunk
        )
        GROUP BY fid;
    )");

    // REPL style
    bool debugCtx = false; std::string input;
    while (std::print(">> "), std::getline(std::cin, input)) {
        if (input.empty()) continue;
        else if (input == "/bye") break;
        else if (input == "/debug") { 
            debugCtx = !debugCtx; 
            std::println("DEBUG MODE: {}", debugCtx);
            continue; 
        }

        res = fetchContextQ->reset(true);
        if (!res) std::println("Unbind: {}", res.error());

        auto embeddings = extractEmbeddings(input);
        res = fetchContextQ->bind<sqlite::dtype::blob>(1, embeddings);
        if (!res) std::println("Vector search: {}", res.error());

        // Retrieve all matches and score at a function level
        std::unordered_map<std::size_t, std::pair<std::size_t, double>> counter;
        for (SimilarityResult ctx: *fetchContextQ) {
            auto &[count, dist] = counter[ctx.fid];
            ++count; dist += ctx.dist;
        }

        // Compute mean of the scores
        auto transformed = counter | std::views::transform(
            [](auto &pr) -> std::pair<std::size_t, double> { 
                auto [count, dist] = pr.second;
                return {pr.first, dist / (double) count}; 
            }
        );

        // Store the top five hits
        std::vector<std::pair<std::size_t, double>> topHits(3);
        std::ranges::partial_sort_copy(transformed, topHits);

        res = fetchCompleteFunctionQ->reset(true);
        if (!res) std::println("Unbind: {}", res.error());

        // Fetch the relevant functions from DB
        for (std::size_t i {}; i < 3; ++i) {
            res = fetchCompleteFunctionQ->bind<sqlite::dtype::integer>(
                static_cast<int>(i + 1), topHits[i].first);
            if (!res) throw std::runtime_error{res.error()};
        }

        // Pull the result from the query
        std::vector<Context> functions;
        for (auto row: *fetchCompleteFunctionQ) functions.emplace_back(row);

        // Only print out the context that would be used for prompt
        if (debugCtx) { 
            for (auto &ctx: functions) 
                std::println("File={}\nFunction={}\nbody={}\n", 
                    ctx.file, ctx.fn, ctx.body);
        }

        // Prep the prompt and query the model
        else {
            auto prompt = preparePrompt(functions, input);
            queryModelWithPrompt(prompt); std::println();
        }
    }

    std::println("bye!");
}
