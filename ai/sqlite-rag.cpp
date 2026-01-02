#include "../networking/net.hpp"
#include "../json-parser/json.hpp"
#include "../misc/CSVUtil.hpp"
#include "../misc/sqlite.hpp"

#include <exception>
#include <print>

// Constants
constexpr std::size_t BATCH_SIZE = 32, EMBED_DIM = 384; 

// CSV data structure for code.csv
using REC_TYPE = std::tuple<
    std::string, // file
    std::string, // namespace
    std::string, // class
    std::string, // function
    std::size_t, // start
    std::size_t, // end
    std::string  // body
>;

// Embedding type for convenience
using EMBEDDING = std::array<float, EMBED_DIM>;

// Helper to remove all double quotes and newlines from string; fine for embeddings
constexpr std::string removeDoubleQuotes(std::string_view str) {
    return std::ranges::filter_view(str, [](char ch){ return ch != '"' && ch != '\n'; })
        | std::ranges::to<std::string>();
}

// Process and send out REST API request to ollama embedding url
std::array<EMBEDDING, BATCH_SIZE> 
extractEmbeddings(const std::array<REC_TYPE, BATCH_SIZE> &batch) {
    // Prepare a list of strings (in json string)
    std::string text(1, '[');
    text.reserve(BATCH_SIZE * std::get<6>(batch[0]).size());
    for (auto &rec: batch)
        text += '"' + removeDoubleQuotes(std::get<6>(rec)) + "\",";
    text.pop_back(); text += ']';

    // Send a HTTP request to process embeddings
    net::HttpRequest req {"http://localhost:11434/api/embed", "POST"};
    req.setHeader("Accept", "application/json");
    req.setBody(R"({"model":"all-minilm:l6-v2","input":)" + text + R"(})");
    auto resp = req.execute();

    // Process the HTTP response (unchunk if required)
    auto data = JSON::Parser::loads(resp.header("Transfer-Encoding") == "chunked"? 
            resp.unchunk(): resp.body);

    // Write to result
    std::array<EMBEDDING, BATCH_SIZE> result;
    for (auto [i, embedding_]: std::ranges::enumerate_view(data["embeddings"])) {
        JSON::JSONHandle embedding {embedding_};
        for (auto [j, val_]: std::ranges::enumerate_view(embedding)) {
            JSON::JSONHandle val {val_};
            result[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] 
                = static_cast<float>(val.to<double>());
        }
    }

    return result;
}

int main() try {
    // Helper specific to this program to bind single record to query
    auto bindRowToQuery =
    [] (sqlite::Statement &query, REC_TYPE &csvRow, EMBEDDING &embedding) {
        auto res = query.bind<sqlite::dtype::text>(":file", std::get<0>(csvRow));
        if (!res) throw std::runtime_error{res.error()};

        res = query.bind<sqlite::dtype::text>(":namespace", std::get<1>(csvRow));
        if (!res) throw std::runtime_error{res.error()};

        res = query.bind<sqlite::dtype::text>(":class", std::get<2>(csvRow));
        if (!res) throw std::runtime_error{res.error()};

        res = query.bind<sqlite::dtype::text>(":function", std::get<3>(csvRow));
        if (!res) throw std::runtime_error{res.error()};

        res = query.bind<sqlite::dtype::integer>(":start", std::get<4>(csvRow));
        if (!res) throw std::runtime_error{res.error()};

        res = query.bind<sqlite::dtype::integer>(":end", std::get<5>(csvRow));
        if (!res) throw std::runtime_error{res.error()};

        res = query.bind<sqlite::dtype::text>(":body", std::get<6>(csvRow));
        if (!res) throw std::runtime_error{res.error()};

        res = query.bind<sqlite::dtype::blob>(":embedding", embedding);
        if (!res) throw std::runtime_error{res.error()};
    };

    // Create sqlite db and init tables
    auto db = sqlite::open(".embeddings");
    if (!db) throw std::runtime_error{db.error()};

    auto initQuery = R"(
        CREATE TABLE IF NOT EXISTS embeddings (
            id INTEGER PRIMARY KEY,
            file TEXT,
            namespace TEXT,
            class TEXT,
            function TEXT,
            start INTEGER,
            end INTEGER,
            body text NOT NULL,
            embedding BLOB NOT NULL
        );
    )";

    auto res = db->exec(initQuery);
    if (!res) throw std::runtime_error{res.error()};

    // Prepare the insert query
    auto q1 = db->query(R"(
        insert into embeddings (file, namespace, class, function, start, end, body, embedding) 
        values (:file, :namespace, :class, :function, :start, :end, :body, :embedding)
    )");
    if (!q1) throw std::runtime_error{q1.error()};

    auto txRes = db->exec("BEGIN TRANSACTION");
    if (!txRes) throw std::runtime_error{txRes.error()};

    // Read from csv file and create embeddings out of them
    std::array<REC_TYPE, BATCH_SIZE> batch;
    std::size_t curr = 0;
    for (auto row: CSVUtil::CSVReader("code.csv", 0, 1)) {
        // file,namespace,class,function,start,end,body
        row.unpack(batch[curr++]);
        if (curr == BATCH_SIZE) {
            // Fetch the embeddings for batch
            auto embeddings = extractEmbeddings(batch);

            // Bind & insert records one at a time
            for (auto [csvRow, embedding]: std::ranges::zip_view(batch, embeddings)) {
                bindRowToQuery(q1.value(), csvRow, embedding);
                auto stepRes = q1->step();
                if (!stepRes) throw std::runtime_error{stepRes.error()};
                auto resetRes = q1->reset(true);
                if (!resetRes) throw std::runtime_error{stepRes.error()};
            }

            // Reset batch counter
            curr = 0;
        }
    }

    txRes = db->exec("COMMIT");
    if (!txRes) throw std::runtime_error{txRes.error()};

    // Reloading the data
    //auto query = db.query("select * from embeddings where id = 1");
    //auto blob = query->column<sqlite::dtype::blob>(2);
    //std::array<float, EMBED_DIM> embeddingsCpy(blob.size() / sizeof(float));
    //std::memcpy(embeddingsCpy.data(), blob.data(), blob.size());
} 

catch(std::exception &ex) {
    std::println("Error: {}", ex.what());
}
