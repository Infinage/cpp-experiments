#pragma once

#include <mutex>
#include <print>

namespace Logging {
    enum class Level { ERROR = 1, WARN, INFO, DEBUG };

    constexpr std::string_view LevelStr(Level level) {
        switch (level) {
            case Level::ERROR: return "ERROR";
            case Level::WARN:  return  "WARN";
            case Level::INFO:  return  "INFO";
            case Level::DEBUG: return "DEBUG";
        }
        return "UNKNOWN";
    }
}

namespace Logging::Static {

    #ifdef BUILD_LOG_LEVEL_VAL
        static_assert(BUILD_LOG_LEVEL_VAL >= 1 && BUILD_LOG_LEVEL_VAL <= 4);
        constexpr Level BuildLogLevel = static_cast<Level>(BUILD_LOG_LEVEL_VAL);
    #else
        constexpr Level BuildLogLevel = Level::INFO;
    #endif

    class Logger {
        private:
            mutable std::mutex mutex; 
            Logger() {}

        public:
            [[nodiscard]] static Logger &instance() { 
                static Logger logger;
                return logger;
            }

            template<Level lvl, typename ...Args>
            void log(std::format_string<Args...> fmt, Args &&...args) const {
                using LevelT = std::underlying_type_t<Level>;
                if constexpr (static_cast<LevelT>(lvl) <= static_cast<LevelT>(BuildLogLevel)) {
                    auto msg = std::format(fmt, std::forward<Args>(args)...);
                    std::lock_guard lock {mutex};
                    std::println("[{}] {}", LevelStr(lvl), msg);
                }
            }
    };

    template<typename ...Args>
    inline void Error(std::format_string<Args...> fmt, Args &&...args) {
        Logger::instance().log<Level::ERROR>(fmt, std::forward<Args>(args)...);
    }

    template<typename ...Args>
    inline void Warn(std::format_string<Args...> fmt, Args &&...args) {
        Logger::instance().log<Level::WARN>(fmt, std::forward<Args>(args)...);
    }

    template<typename ...Args>
    inline void Info(std::format_string<Args...> fmt, Args &&...args) {
        Logger::instance().log<Level::INFO>(fmt, std::forward<Args>(args)...);
    }

    template<typename ...Args>
    inline void Debug(std::format_string<Args...> fmt, Args &&...args) {
        Logger::instance().log<Level::DEBUG>(fmt, std::forward<Args>(args)...);
    }
}

namespace Logging::Dynamic {
    class Logger {
        private:
            Level logLevel; 
            mutable std::mutex mutex;

            Logger(): logLevel{Level::INFO} {}

        public:
            inline void setLevel(Level level) { this->logLevel = level; }

            [[nodiscard]] static Logger &instance() { 
                static Logger logger {};
                return logger;
            }

            template<typename ...Args>
            void log(Level level, std::format_string<Args...> fmt, Args &&...args) const {
                using levelT = std::underlying_type_t<Level>;
                if (static_cast<levelT>(this->logLevel) >= static_cast<levelT>(level)) {
                    auto msg = std::format(fmt, std::forward<Args>(args)...);
                    std::lock_guard lock {mutex};
                    std::println("[{}] {}", LevelStr(level), msg);
                }
            }
    };

    inline void setLogLevel(Level level) { Logger::instance().setLevel(level); }

    template<typename ...Args>
    inline void Log(Level level, std::format_string<Args...> fmt, Args &&...args) {
        Logger::instance().log(level, fmt, std::forward<Args>(args)...);
    }
}
