#pragma once

#include "sqlite3.h"

#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <variant>

namespace sqlite {
    // SQLite column data type
    enum class dtype {null, integer, real, text, blob};

    // Expose underlying ints for OPEN factory func
    inline constexpr int OPEN_READONLY  = SQLITE_OPEN_READONLY;
    inline constexpr int OPEN_READWRITE = SQLITE_OPEN_READWRITE;
    inline constexpr int OPEN_CREATE    = SQLITE_OPEN_CREATE;
    inline constexpr int OPEN_MEMORY    = SQLITE_OPEN_MEMORY;
    inline constexpr int OPEN_URI       = SQLITE_OPEN_URI;
    inline constexpr int OPEN_RW_CREATE = OPEN_READWRITE | OPEN_CREATE;

    // Header wide representation for blobs
    using BlobType = std::span<const std::byte>;

    // Concept to check if a input type is blob convertible
    template<typename T>
    concept IsBlobConvertible = requires(T value) {
        typename T::value_type;
        requires std::is_trivially_copyable_v<typename T::value_type>;
        { value.size() } -> std::convertible_to<std::size_t>;
        { value.data() } -> std::convertible_to<const void*>;
    };

    // Concept to constrain the bind function
    template<dtype BT, typename VT>
    concept IsValidBind =
        (BT == dtype::null    && std::same_as<std::remove_cvref_t<VT>, std::nullptr_t>) ||
        (BT == dtype::integer && std::integral<std::remove_cvref_t<VT>>) ||
        (BT == dtype::real    && std::floating_point<std::remove_cvref_t<VT>>) ||
        (BT == dtype::text    && std::convertible_to<VT, std::string_view>) ||
        (BT == dtype::blob    && IsBlobConvertible<std::remove_cvref_t<VT>>);

    struct NonCopyable {
        NonCopyable() = default;
        NonCopyable(const NonCopyable&) = delete;
        std::size_t size() const;
        const void *data();
        using value_type = int;
    };

    static_assert(IsBlobConvertible<NonCopyable>);

    class Statement {
        private:
            // Private CTOR, use prepare() factory fn
            Statement() = default;

            // RAII wrapper for sqlite3_stmt
            using stmt_handle = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
            stmt_handle handle {nullptr, sqlite3_finalize};

            // Non owning pointer to DB
            sqlite3 *dbPtr {};

        public:
            // Generic column result type
            using ColRType = std::variant<
                std::monostate,
                std::int64_t,
                double,
                std::string_view,
                BlobType 
            >;

            // Iterator over statement rows
            struct Row {
                using iterator_category = std::input_iterator_tag;
                using value_type = Statement;

                Statement *stmt;
                bool done = false;

                Row &operator*() { return *this; }
                Statement *operator->() { return stmt; }

                Row &operator++() {
                    if (!stmt) { done = true; return *this; }
                    auto res = stmt->step();
                    done = !res || !res.value();
                    return *this;
                }

                bool operator==(Row other) const {
                    return done == other.done;
                }

                // Access column by index (0 based)
                [[nodiscard]] ColRType operator[](std::size_t index) { 
                    if (!stmt || done) return std::monostate{}; 
                    return stmt->column(static_cast<int>(index)); 
                }
            };

            // Iterator support
            Row end() { return Row {nullptr, true}; }
            Row begin() {
                if (!reset(false)) return Row{this, false};
                auto res = step();
                return Row{this, !res || !res.value()};
            }

            // Factory function: prepare statement from SQL query
            static std::expected<Statement, std::string> prepare(sqlite3 *dbPtr, std::string_view query) { 
                sqlite3_stmt *stmt {};
                if (!dbPtr || sqlite3_prepare_v2(dbPtr, query.data(), -1, &stmt, nullptr) != SQLITE_OK)
                    return std::unexpected{sqlite3_errmsg(dbPtr)};
                Statement statement;
                statement.handle.reset(stmt);
                statement.dbPtr = dbPtr;
                return statement;
            }

            // Bind value to parameter by index (1 based)
            template<dtype BT, typename VT> requires(IsValidBind<BT, VT>)
            std::expected<void, std::string> bind(int index1, VT value) {
                int rc {}; auto *stmt = handle.get();
                if constexpr (BT == dtype::null) {
                    rc = sqlite3_bind_null(stmt, index1);
                } else if constexpr (BT == dtype::integer) {
                    rc = sqlite3_bind_int64(stmt, index1, static_cast<sqlite3_int64>(value));
                } else if constexpr (BT == dtype::real) {
                    rc = sqlite3_bind_double(stmt, index1, static_cast<double>(value));
                } else if constexpr (BT == dtype::text) {
                    rc = sqlite3_bind_text(stmt, index1, value.data(), value.size(), SQLITE_TRANSIENT);
                } else if constexpr (BT == dtype::blob) {
                    using ValueType = std::remove_reference_t<decltype(value)>::value_type;
                    auto len = sizeof(ValueType) * value.size();
                    rc = sqlite3_bind_blob(stmt, index1, value.data(), len, SQLITE_TRANSIENT);
                }

                if (rc != SQLITE_OK)
                    return std::unexpected(sqlite3_errmsg(dbPtr));
                return {};
            }

            // Bind by named parameter
            template<dtype BT, typename VT> requires(IsValidBind<BT, VT>)
            std::expected<void, std::string> bind(std::string_view idxStr, VT value) {
                int idx = sqlite3_bind_parameter_index(handle.get(), idxStr.data());
                if (idx == 0) return std::unexpected{"No matching param: " + std::string(idxStr)};
                return bind<BT>(idx, value);
            }

            // Number of columns in result set
            [[nodiscard]] std::size_t columns() {
                auto cols = sqlite3_column_count(handle.get());
                return static_cast<std::size_t>(cols);
            }

            std::expected<dtype, std::string> columnType(int index) {
                switch (sqlite3_column_type(handle.get(), index)) {
                    case SQLITE_NULL: return dtype::null;
                    case SQLITE_INTEGER: return dtype::integer; 
                    case SQLITE_FLOAT: return dtype::real;
                    case SQLITE_TEXT: return dtype::text;
                    case SQLITE_BLOB: return dtype::blob;
                }
                return std::unexpected{"unknown col type #" + 
                    std::to_string(index)};
            }

            [[nodiscard]] std::string_view columnName(int index) {
                return sqlite3_column_name(handle.get(), index);
            }

            // Get column value by type, forces conversions in case of type mismatch
            template<dtype T> [[nodiscard]] auto column(int index) {
                auto *stmt = handle.get();
                if constexpr (T == dtype::null) {
                    return sqlite3_column_type(stmt, index) == SQLITE_NULL;
                } else if constexpr (T == dtype::integer) {
                    return static_cast<std::uint64_t>(sqlite3_column_int64(stmt, index));
                } else if constexpr (T == dtype::real) {
                    return sqlite3_column_double(stmt, index);
                } else if constexpr (T == dtype::text) {
                    auto* txt = sqlite3_column_text(stmt, index);
                    auto len  = static_cast<std::size_t>(sqlite3_column_bytes(stmt, index));
                    return std::string_view{reinterpret_cast<const char*>(txt), len};
                } else if constexpr (T == dtype::blob) {
                    auto* ptr = sqlite3_column_blob(stmt, index);
                    auto len  = static_cast<std::size_t>(sqlite3_column_bytes(stmt, index));
                    return BlobType{static_cast<const std::byte*>(ptr), len};
                }
            }

            // Variant-based generic column accessor, return type determined at runtime
            [[nodiscard]] ColRType column(int index) {
                auto *stmt = handle.get();
                switch (sqlite3_column_type(stmt, index)) {
                    case SQLITE_NULL:
                        return std::monostate{};
                    case SQLITE_INTEGER:
                        return sqlite3_column_int64(stmt, index);
                    case SQLITE_FLOAT:
                        return sqlite3_column_double(stmt, index);
                    case SQLITE_TEXT: {
                        auto* txt = sqlite3_column_text(stmt, index);
                        auto len  = static_cast<std::size_t>(sqlite3_column_bytes(stmt, index));
                        return std::string_view{reinterpret_cast<const char*>(txt), len};
                    }
                    case SQLITE_BLOB: {
                        auto* ptr = sqlite3_column_blob(stmt, index);
                        auto len  = static_cast<std::size_t>(sqlite3_column_bytes(stmt, index));
                        return BlobType{static_cast<const std::byte*>(ptr), len};
                    }
                }
                return std::monostate{};    
            }

            // Returns one of (true/false/error) and moves the iterator forward
            [[nodiscard]] std::expected<bool, std::string> step() {
                int rc = sqlite3_step(handle.get());
                if (rc == SQLITE_ROW) return true;
                else if (rc == SQLITE_DONE) return false;
                else {
                    auto emsg = dbPtr? sqlite3_errmsg(dbPtr): "unknown sqlite3 error";
                    return std::unexpected{emsg};
                }
            }

            // Reset statement, optionally clear bound parameters
            std::expected<void, std::string> reset(bool clearBinds = false) { 
                if (!handle) return std::unexpected{"invalid statement"};
                if (sqlite3_reset(handle.get()) != SQLITE_OK)
                    return std::unexpected{sqlite3_errmsg(dbPtr)};
                if (clearBinds && sqlite3_clear_bindings(handle.get()) != SQLITE_OK)
                    return std::unexpected{sqlite3_errmsg(dbPtr)};
                return {};
            }
    };

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

            std::expected<void, std::string> enableloadExtension(bool onOff = true) {
                auto dbPtr = handle.get();
                if (sqlite3_enable_load_extension(dbPtr, onOff) != SQLITE_OK)
                    return std::unexpected{sqlite3_errmsg(dbPtr)};
                return {};
            }

            // Static factory function
            static std::expected<DB, std::string> 
            open(std::string_view path, int oflags = OPEN_RW_CREATE) {
                sqlite3 *raw {};
                if (sqlite3_open_v2(path.data(), &raw, oflags, nullptr) != SQLITE_OK) {
                    auto emsg = raw? sqlite3_errmsg(raw): "sqlite3_open_failed";
                    if (raw) sqlite3_close(raw);
                    return std::unexpected{emsg};
                }

                DB database;
                database.handle.reset(raw);
                return database;
            }

            /**
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

            std::expected<Statement, std::string> query(std::string_view queryStr) {
                return Statement::prepare(handle.get(), queryStr);
            }
    };

    // Wrapper over DB::open
    inline auto open(std::string_view path, int oflags = OPEN_RW_CREATE) 
    { return DB::open(path, oflags); }
}
