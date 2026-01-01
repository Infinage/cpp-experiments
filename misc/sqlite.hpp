#pragma once

#include "sqlite3.h"

#include <expected>
#include <functional>
#include <format>
#include <utility>

namespace sqlite {
    class DB {
        private:
            sqlite3 *db {};
            DB() = default;

            /*
             * SQLite is a C library and accepts only plain C function pointers as callbacks.
             * To allow C++ callables (lambdas, std::function, etc.), sqlite3_exec provides
             * a `void*` user-data parameter that is passed through to the callback unchanged.
             *
             * This static "trampoline" function acts as a bridge:
             *  - SQLite calls it with the raw C callback signature
             *  - We recover the original C++ callable from `userFn`
             *  - The callable is invoked with the row data
             *
             * The callback object must remain alive for the duration of sqlite3_exec,
             * which is safe here because sqlite3_exec is synchronous.
             */
            static int trampoline(void *userFn, int argc, char **values, char **columns) {
                auto &cb = *static_cast<RowCallback*>(userFn);
                return cb(argc, values, columns)? 0: 1;
            }

        public:
            using RowCallback = std::function<bool(int colC, char **values, char **names)>;

            // Disable copy semantics
            DB(const DB&) = delete;
            DB &operator=(const DB&) = delete;

            // Allow move semantics
            DB(DB &&other) noexcept: db{std::exchange(other.db, nullptr)} {}
            DB &operator=(DB &&other) noexcept {
                DB{std::move(other)}.swap(*this);
                return *this;
            }

            // Swap member function for swap idiom
            void swap(DB &other) noexcept { 
                using std::swap; 
                swap(other.db, db); 
            }

            // Close connection on exit
            ~DB() { if (db) sqlite3_close(db); }

            // Static factory function
            [[nodiscard]] static std::expected<DB, std::string> 
            open(std::string_view path) { 
                DB db;
                if (sqlite3_open(path.data(), &db.db) != SQLITE_OK) {
                    auto emsg = std::format("Failed to init: {}", sqlite3_errmsg(db.db));
                    return std::unexpected{emsg};
                }
                return db;
            }

            /**
             * @brief Execute an SQL statement.
             *
             * Executes the given SQL query using sqlite3_exec.
             *
             * If a callback is provided, it is invoked once per result row produced
             * by the query. The callback receives:
             *   - the number of columns in the row
             *   - an array of column values (as null-terminated strings)
             *   - an array of column names
             *
             * The callback may return `false` to stop further execution early.
             *
             * If no callback is provided, the query is executed in fire-and-forget
             * mode (useful for INSERT, UPDATE, DELETE, DDL, etc).
             *
             * @param query SQL statement to execute.
             * @param cb Optional row callback invoked for each result row.
             *
             * @return std::expected<void, std::string>
             *         - success: empty expected
             *         - failure: SQLite error message
             */
            std::expected<void, std::string> exec(std::string_view query, RowCallback cb = nullptr) {
                char *emsg {}; 
                void *userFn = cb == nullptr? nullptr: &cb;
                if (sqlite3_exec(db, query.data(), trampoline, userFn, &emsg) != SQLITE_OK) {
                    std::string emsgStr = emsg? emsg: "unknown sqlite error";
                    sqlite3_free(emsg);
                    return std::unexpected(emsgStr);
                }
                return {};
            }
    };

    // Wrapper over DB::open
    inline auto open(std::string_view path) { return DB::open(path); }
}
