#include "../networking/net.hpp"
#include "../json-parser/json.hpp"
#include "../misc/CSVUtil.hpp"
#include "../misc/sqlite.hpp"

#include <exception>
#include <print>

// Constants
constexpr std::size_t EMBED_BATCH_SIZE =  64; 
constexpr std::size_t  EMBED_MAX_CHARS = 300; 
constexpr std::size_t        EMBED_DIM = 384; 

// CSV data structure for code.csv
struct CSVRow {
    std::string file;
    std::string ns;
    std::string cls;
    std::string function;
    std::size_t start;
    std::size_t end;
    std::string body;

    CSVRow(CSVUtil::CSVRecord &rec) {
        rec.unpack(file, ns, cls, function, start, end, body);
    }
};

// Embedding type for convenience
using EMBEDDING = std::array<float, EMBED_DIM>;

// Process and send out REST API request to ollama embedding url
std::vector<EMBEDDING>
extractEmbeddings(const std::vector<std::tuple<int, int, std::string>> &batch) {
    std::vector<EMBEDDING> result(batch.size());

    // Prepare a list of strings (in json string)
    std::string text(1, '[');
    text.reserve(batch.size() * EMBED_MAX_CHARS);
    for (auto &[_, __, body]: batch)
        text += '"' + JSON::helper::jsonEscape(body) + "\",";
    text.pop_back(); text += ']';

    // Send a HTTP request to process embeddings
    net::HttpRequest req {"http://localhost:11434/api/embed", "POST"};
    req.setHeader("Accept", "application/json");
    req.setBody(R"({"model":"all-minilm:l6-v2","input":)" + text + R"(})");
    auto resp = req.execute();

    // Log in case of errors
    if (!resp.ok()) std::println("Batch failed with message: {}", resp.body);

    // Process the HTTP response (unchunk if required)
    else {
        auto data = JSON::Parser::loads(resp.header("Transfer-Encoding") == "chunked"? 
                resp.unchunk(): resp.body);

        for (auto [i, embedding_]: std::ranges::enumerate_view(data["embeddings"])) {
            JSON::JSONHandle embedding {embedding_};
            for (auto [j, val_]: std::ranges::enumerate_view(embedding)) {
                JSON::JSONHandle val {val_};
                result[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] 
                    = static_cast<float>(val.to<double>());
            }
        }
    }

    return result;
}

// Bind a single record for inserting to functions table
void bindToFunctionsInsertQuery(sqlite::Statement &query, const CSVRow &csvRow) {
    auto res = query.bind<sqlite::dtype::text>(":file", csvRow.file);
    if (!res) throw std::runtime_error{res.error()};

    res = query.bind<sqlite::dtype::text>(":namespace", csvRow.ns);
    if (!res) throw std::runtime_error{res.error()};

    res = query.bind<sqlite::dtype::text>(":class", csvRow.cls);
    if (!res) throw std::runtime_error{res.error()};

    res = query.bind<sqlite::dtype::text>(":function", csvRow.function);
    if (!res) throw std::runtime_error{res.error()};

    res = query.bind<sqlite::dtype::integer>(":start", csvRow.start);
    if (!res) throw std::runtime_error{res.error()};

    res = query.bind<sqlite::dtype::integer>(":end", csvRow.end);
    if (!res) throw std::runtime_error{res.error()};

    auto numChunks = (csvRow.body.size() + EMBED_MAX_CHARS - 1) / EMBED_MAX_CHARS;
    res = query.bind<sqlite::dtype::integer>(":chunks", numChunks);
    if (!res) throw std::runtime_error{res.error()};
}

// Bind a single record for inserting to embeddings table
void bindToEmbeddingsInsertQuery(sqlite::Statement &query, int fid, 
    int chunkId, std::string_view body, const EMBEDDING &embedding) 
{
    auto res = query.bind<sqlite::dtype::integer>(":fid", fid);
    if (!res) throw std::runtime_error{res.error()};

    res = query.bind<sqlite::dtype::integer>(":chunk", chunkId);
    if (!res) throw std::runtime_error{res.error()};

    res = query.bind<sqlite::dtype::text>(":body", body);
    if (!res) throw std::runtime_error{res.error()};

    res = query.bind<sqlite::dtype::blob>(":embedding", embedding);
    if (!res) throw std::runtime_error{res.error()};
}

// Helper to process a batch of inputs to compute embeddings and 
// insert into embeddings table
void processEmbeddingBatch(sqlite::Statement &query, 
    const std::vector<std::tuple<int, int, std::string>> &batch) 
{
    auto embeddings = extractEmbeddings(batch);

    // Bind & insert records one at a time
    for (const auto &[batchRow, embedding]: std::ranges::zip_view(batch, embeddings)) {
        const auto &[fid, cid, body] = batchRow;
        auto metaEnd = body.find("]\n"); // Remove the meta we added
        auto fnStart = metaEnd != std::string::npos? static_cast<long>(metaEnd) + 2: 0;
        fnStart = std::min(fnStart, static_cast<long>(body.size()));
        std::string_view bodyWithoutMeta {body.begin() + fnStart, body.end()};
        bindToEmbeddingsInsertQuery(query, fid, cid, bodyWithoutMeta, embedding);
        auto stepRes = query.step();
        if (!stepRes) throw std::runtime_error{stepRes.error()};
        auto resetRes = query.reset(true);
        if (!resetRes) throw std::runtime_error{stepRes.error()};
    }
}

int main() try {

    // Create sqlite db and init tables
    auto db = sqlite::open(".codebase");
    if (!db) throw std::runtime_error{db.error()};

    auto initQuery = R"(
        CREATE TABLE IF NOT EXISTS functions (
            id INTEGER PRIMARY KEY,
            file TEXT,
            namespace TEXT,
            class TEXT,
            function TEXT,
            start INTEGER,
            end INTEGER,
            chunks INTEGER
        );

        CREATE TABLE IF NOT EXISTS embeddings (
            id INTEGER PRIMARY KEY,
            fid INTEGER,
            chunk INTEGER,
            body TEXT,
            embedding BLOB,
            FOREIGN KEY (fid) REFERENCES functions(id)
        );
    )";

    // Execute table creation query
    auto res = db->exec(initQuery);
    if (!res) throw std::runtime_error{res.error()};

    // Prepare the insert query for functions table
    auto funcQ = db->query(R"(
        insert into functions (file, namespace, class, function, start, end, chunks) 
        values (:file, :namespace, :class, :function, :start, :end, :chunks)
    )");
    if (!funcQ) throw std::runtime_error{funcQ.error()};

    // Prepare the insert query for embeddings table
    auto embedQ = db->query(R"(
        insert into embeddings (fid, chunk, body, embedding) 
        values (:fid, :chunk, :body, :embedding)
    )");
    if (!embedQ) throw std::runtime_error{embedQ.error()};

    // Write as a transaction for speed
    auto txRes = db->exec("BEGIN TRANSACTION");
    if (!txRes) throw std::runtime_error{txRes.error()};

    // Read from csv file and create embeddings out of them
    std::size_t fid {1};
    std::vector<std::tuple<int, int, std::string>> batch;
    batch.reserve(EMBED_BATCH_SIZE);
    for (CSVRow row: CSVUtil::CSVReader("code.csv", 0, 1)) {
        // Insert record into functions table
        bindToFunctionsInsertQuery(funcQ.value(), row);
        auto stepRes = funcQ->step();
        if (!stepRes) throw std::runtime_error{stepRes.error()};
        auto resetRes = funcQ->reset(true);
        if (!resetRes) throw std::runtime_error{stepRes.error()};
        
        // Process the file meta along side the function for better retreival
        auto meta = std::format("[file={} namespace={} class={} function={}]\n", 
                row.file, row.ns, row.cls, row.function);

        // Split the function into chunks as required
        if (meta.size() >= EMBED_MAX_CHARS) throw std::runtime_error{"EMBED_MAX_CHARS too low"};
        std::size_t actualChunkSize {EMBED_MAX_CHARS - meta.size()};
        std::size_t numChunks {(row.body.size() + actualChunkSize - 1) / actualChunkSize};
        for (std::size_t cid {}; cid < numChunks; ++cid) {
            auto start = cid * actualChunkSize;
            auto end = std::min(start + actualChunkSize, row.body.size());
            auto toEmbed = meta + row.body.substr(start, end - start);
            batch.emplace_back(fid, cid, toEmbed); 
        }

        // Compute and write the embeddings to DB
        if (batch.size() >= EMBED_BATCH_SIZE) {
            processEmbeddingBatch(embedQ.value(), batch);
            std::println("Writing batch, current func counter: {}", fid);
            batch.clear();
        }

        // For each row we have processed, one new function encountered
        // Assuming a monotonically increasing ID counter for functions table
        ++fid;
    }
        
    // Handle any leftovers
    if (!batch.empty()) processEmbeddingBatch(embedQ.value(), batch);

    txRes = db->exec("COMMIT");
    if (!txRes) throw std::runtime_error{txRes.error()};
} 

catch(std::exception &ex) {
    std::println("Error: {}", ex.what());
}
