#include <fstream>
#include <print>
#include <stdexcept>
#include <vector>
#include <string>
#include <cassert>

#include "llama.h"
#include "common.h"

class LLAMA_INIT {
    private:
        LLAMA_INIT() {
            auto noop = [](ggml_log_level, const char*, void*){};
            llama_log_set(noop, nullptr);
            llama_backend_init();
            llama_numa_init(GGML_NUMA_STRATEGY_DISABLED);
        }

    public:
        ~LLAMA_INIT() { llama_backend_free(); }

        // Singleton: disallow every other access
        LLAMA_INIT(LLAMA_INIT&&) = delete;
        LLAMA_INIT& operator=(LLAMA_INIT&&) = delete;
        LLAMA_INIT(const LLAMA_INIT&) = delete;
        LLAMA_INIT& operator=(const LLAMA_INIT&) = delete;

        [[nodiscard("RAII class to init llama cpp")]]
        static LLAMA_INIT &scoped() {
            static LLAMA_INIT lm{};
            return lm;
        }
};

class EmbeddingModel {
    private:
        llama_context *ctx;
        llama_model *model;
        llama_model_params mparams {llama_model_default_params()};
        llama_context_params cparams = llama_context_default_params();

    public:
        EmbeddingModel(
            const std::string &modelPath, uint32_t ctxLen, 
            enum llama_pooling_type ptype = LLAMA_POOLING_TYPE_MEAN
        ) {
            model = llama_model_load_from_file(modelPath.c_str(), llama_model_default_params()); 
            if (!model) throw std::runtime_error{"failed to load model"};

            cparams.n_ctx = ctxLen; cparams.embeddings = true;
            cparams.pooling_type = ptype;

            ctx = llama_init_from_model(model, cparams);
            if(!ctx) throw std::runtime_error{"failed to create context"};
        }

        [[nodiscard]] std::vector<float> embed(const std::string &text) {
            // ---- tokenize ----
            auto tokens = common_tokenize(ctx, text, true, true);
            if (tokens.size() > cparams.n_ctx) tokens.resize(cparams.n_ctx);

            // ---- create batch ----
            auto n_tokens = static_cast<int32_t>(tokens.size());
            llama_batch batch = llama_batch_init(n_tokens, 0, 1);

            for (int i {}; i < n_tokens; ++i) {
                bool isFirst = i == n_tokens - 1;
                common_batch_add(batch, tokens[(std::size_t) i], i, {0}, isFirst);
            }

            // ---- run model ----
            llama_memory_clear(llama_get_memory(ctx), true);

            if(llama_decode(ctx, batch) < 0) 
                throw std::runtime_error{"llama_decode failed"};

            // ---- get embedding ----
            const float *emb = llama_get_embeddings_seq(ctx, 0);
            if(!emb) throw std::runtime_error{"no embedding produced"};

            const int dim = llama_model_n_embd_out(model);

            // Free batch
            llama_batch_free(batch);
            return {emb, emb + dim};
        }

    ~EmbeddingModel() {
        llama_free(ctx);
        llama_model_free(model);
    }
};

std::string loadf(std::string_view fname) {
    std::ifstream ifs{fname.data()};
    std::ostringstream oss; 
    oss << ifs.rdbuf();
    return oss.str();
}

int main() {
    [[maybe_unused]] auto &llama = LLAMA_INIT::scoped();
    EmbeddingModel embedder{"all-MiniLM-L6-v2-Q5_K_S.gguf", 512};
    auto text = loadf("sample.txt");
    auto embedding = embedder.embed(text);
    std::println("{}", embedding);
}
