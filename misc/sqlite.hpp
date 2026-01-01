#pragma once

#include "sqlite3.h"

#include <expected>
#include <functional>
#include <memory>

namespace sqlite {
    class DB {
        private:
            using db_handle = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
            db_handle handle {nullptr, sqlite3_close};

            // Can only create an instance via factory function provided
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

            // Static factory function
            [[nodiscard]] static std::expected<DB, std::string> 
            open(std::string_view path) { 
                sqlite3 *raw {};
                if (sqlite3_open(path.data(), &raw) != SQLITE_OK) {
                    auto emsg = raw? sqlite3_errmsg(raw): "sqlite3_open_failed";
                    if (raw) sqlite3_close(raw);
                    return std::unexpected{emsg};
                }

                DB database;
                database.handle.reset(raw);
                return database;
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
                auto *sqliteCB = cb? trampoline: nullptr; auto *userFn = cb? &cb: nullptr;
                if (sqlite3_exec(handle.get(), query.data(), sqliteCB, userFn, &emsg) != SQLITE_OK) {
                    std::string emsgStr = emsg? emsg: "unknown sqlite error";
                    sqlite3_free(emsg);
                    return std::unexpected{emsgStr};
                }
                return {};
            }
    };

    // Wrapper over DB::open
    inline auto open(std::string_view path) { return DB::open(path); }
}
