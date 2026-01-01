#include "../networking/net.hpp"
#include "../json-parser/json.hpp"
#include "../misc/sqlite.hpp"

#include <exception>
#include <print>

int main() try {
    std::size_t id = 1;
    std::string text = R"(The quick brown fox jumps over the lazy dog)";
    
    // Compute the embeddings
    net::HttpRequest req {"http://localhost:11434/api/embeddings", "POST"};
    req.setHeader("Accept", "application/json");
    req.setBody(R"({"model":"all-minilm:l6-v2","prompt":")" + text + R"("})");
    auto resp = req.execute();
    auto data = JSON::Parser::loads(resp.header("Transfer-Encoding") == "chunked"? 
            resp.unchunk(): resp.body);
    std::vector<float> embeddings;
    for (auto [i, val_]: std::ranges::enumerate_view(data["embedding"])) {
        JSON::JSONHandle val = val_;
        embeddings.push_back(static_cast<float>(val.to<double>()));
    }

    // Sqlite stuff to create the table storing the embeddings
    auto db = sqlite::open(".embeddings");
    if (!db) throw std::runtime_error{db.error()};

    auto initQuery = R"(
        CREATE TABLE IF NOT EXISTS embeddings ( id INTEGER PRIMARY KEY,
            text TEXT NOT NULL,
            embedding BLOB NOT NULL
        );
    )";

    auto res = db->exec(initQuery);
    if (!res) throw std::runtime_error{res.error()};

    // Prepare the insert query
    auto q1 = db->query("insert into embeddings (id, text, embedding) values (?, ?, ?)");
    if (!q1) throw std::runtime_error{q1.error()};

    // Bind the values
    res = q1->bind<sqlite::dtype::integer>(1, id);
    if (!res) throw std::runtime_error{res.error()};
    res = q1->bind<sqlite::dtype::text>(2, text);
    if (!res) throw std::runtime_error{res.error()};
    res = q1->bind<sqlite::dtype::blob>(3, embeddings);
    if (!res) throw std::runtime_error{res.error()};

    // Insert the embedding
    auto stepRes = q1->step();
    if (!stepRes) throw std::runtime_error{stepRes.error()};

    // Try reloading back to see if the embeddings have been inserted correctly
    auto q2 = db->query("select * from embeddings where id = 1");
    if (!q2) throw std::runtime_error{q2.error()};

    // Execute query
    stepRes = q2->step();
    if (!stepRes) throw std::runtime_error{stepRes.error()};

    // Reload and compare
    auto blob = q2->column<sqlite::dtype::blob>(2);
    std::vector<float> embeddingsCpy(blob.size() / sizeof(float));
    std::memcpy(embeddingsCpy.data(), blob.data(), blob.size());
    auto allEq = std::ranges::equal(embeddings, embeddingsCpy);
    std::println("Loaded and reloaded embeddings, pass: {}", allEq);
} 

catch(std::exception &ex) {
    std::println("Error: {}", ex.what());
}
