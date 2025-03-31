#pragma once

#include <iomanip>
#include <iostream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>
#include <charconv>

namespace argparse {
    enum ARGTYPE { POSITIONAL, NAMED, BOTH };

    using VALUE_TYPE = std::variant<
        bool, short, int, long, float, long long, double, 
        std::string, std::vector<std::string>>;

    template<typename T>
    concept ValidValueType = 
    std::same_as<T,   bool> || std::same_as<T,       short> ||
    std::same_as<T,    int> || std::same_as<T,        long> ||
    std::same_as<T,  float> || std::same_as<T,   long long> ||
    std::same_as<T, double> || std::same_as<T, std::string> ||
    std::same_as<T, std::vector<std::string>>;

    // Helper to print variant directly to outputstream
    inline std::ostream &operator<<(std::ostream &oss, const VALUE_TYPE &val) {
        std::visit([&oss](const auto &arg) {
            if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, std::vector<std::string>>) {
                oss << '{';
                for (size_t i = 0; i < arg.size(); ++i) {
                    if (i > 0) oss << ',';
                    oss << arg[i];
                }
                oss << '}';
            } else {
                oss << arg;
            }
        }, val);
        return oss;
    }

    class Argument {
        private:
            const std::string _name; 
            const ARGTYPE _type;
            bool _required {false}, _valueSet {false}; 
            bool _typeSet {false}, _defaultValueSet {false};
            std::string _alias, _helpStr;
            VALUE_TYPE _value;
            std::optional<VALUE_TYPE> _default, _implicit;

        public:
            Argument(const std::string &name, const ARGTYPE &type = ARGTYPE::BOTH): 
                _name(name), _type(type), _value("")
            {
                // Check parameter name validity
                if (name.empty()) { 
                    throw std::runtime_error("Argparse Error: Argument name cannot be empty");
                } else if (name.starts_with('-')) {
                    throw std::runtime_error(
                        "Argparse Error: Parameter names must not start with a hypen, "
                        "consider explicitly setting the argtype instead."
                    );
                } else {
                    for (const char &ch: name) {
                        if (ch == '=')
                            throw std::runtime_error("Argparse Error: Invalid parameter name: " + name);
                    }
                }
            }

            bool           ok() const { return !_required || _valueSet || _defaultValueSet; }
            bool   isOptional() const { return !_required || _defaultValueSet; }

            bool   isValueSet() const { return _valueSet; }
            bool isDefaultSet() const { return _defaultValueSet; }

            std::string getAlias() const { return _alias; }
            std::string getName() const { return _name; }

            ARGTYPE getArgType() const { return _type; }

            template<typename T>
            T get() const {
                if (!_valueSet && !_defaultValueSet) 
                    throw std::runtime_error("Argparse Error: Argument '" + _name + "' was not set");
                else if (!std::holds_alternative<T>(_value))
                    throw std::runtime_error("Argparse Error: Type mismatch (get): " + _name);
                else
                    return std::get<T>(_value);
            }

            std::string getHelp(const int width = 15) const { 
                std::ostringstream oss, part;

                // Handle the name portion with padding
                part << "--" << _name;
                if (!_alias.empty()) 
                    part << ", -" << _alias;

                // Insert name along with the actual help str
                oss << std::left << std::setw(width) << part.str() 
                    << "\t" << _helpStr;

                if (_required) oss << " (REQUIRED)";
                if (_implicit.has_value()) oss << " (implicit=" << *_implicit << ")";
                if (_defaultValueSet) oss << " (default=" << *_default << ")";

                return oss.str(); 
            }

            std::string getTypeName() const {
                return std::visit([](const auto& v) -> std::string {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, bool>) return "bool";
                    else if constexpr (std::is_same_v<T, short>) return "short";
                    else if constexpr (std::is_same_v<T, int>) return "int";
                    else if constexpr (std::is_same_v<T, long>) return "long";
                    else if constexpr (std::is_same_v<T, long long>) return "long long";
                    else if constexpr (std::is_same_v<T, float>) return "float";
                    else if constexpr (std::is_same_v<T, double>) return "double";
                    else if constexpr (std::is_same_v<T, std::string>) return "string";
                    else if constexpr (std::is_same_v<T, std::vector<std::string>>) return "list[str]";
                    else return "unknown";
                }, _value);
            }

            Argument &alias(const std::string &name) { 
                if (_type == ARGTYPE::POSITIONAL)
                    throw std::runtime_error("Argparse Error: Alias being set for a positional argument: " + _name);
                _alias = name; return *this;
            }

            Argument &required() { 
                _required = true; return *this; 
            }

            Argument &help(const std::string &msg) { 
                _helpStr = msg; return *this; 
            }

            Argument &set() {
                if (!_implicit.has_value())
                    throw std::runtime_error("Argparse Error: No implicit value set: " + _name);
                _value = *_implicit; _valueSet = true; return *this;
            }

            Argument &set(const std::string &val) {
                std::visit([&](auto &arg) {
                    using T = std::decay_t<decltype(arg)>;
                    arg = parse<T>(val);
                }, _value);
                _typeSet = true; _valueSet = true; return *this;
            }

            template <typename T>
            Argument &scan() { 
                if (_typeSet && !std::holds_alternative<T>(_value))
                    throw std::runtime_error("Argparse Error: Type mismatch (scan): " + _name);
                _typeSet = true; _value = T{}; return *this;
            }

            template<typename T>
            Argument &defaultValue(const T &val) {
                if (_typeSet && !std::holds_alternative<T>(_value))
                    throw std::runtime_error("Argparse Error: Type mismatch (default): " + _name);
                _defaultValueSet = true; _typeSet = true; 
                _default = _value = val; return *this;
            }

            template<typename T>
            Argument &implicitValue(const T &val) {
                if (_typeSet && !std::holds_alternative<T>(_value))
                    throw std::runtime_error("Argparse Error: Type mismatch (implicit): " + _name);
                _typeSet = true; _implicit = val;  _value = T{};
                return *this;
            }

            // Auto cast char* to std::string
            Argument  &defaultValue(const char *val) { return  defaultValue<std::string>(val); }
            Argument &implicitValue(const char *val) { return implicitValue<std::string>(val); }

            template<typename T>
            T parse(const std::string &arg) {
                if constexpr (std::is_same_v<T, bool>) {
                    return arg != "" && arg != "0" && arg != "false";
                }

                else if constexpr (std::is_same_v<T, std::string>) {
                    return arg;
                }

                else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                    std::vector<std::string> placeholder;
                    std::string acc; char insideQuote {0}, prevCh {0};
                    for (const char &ch: arg) {
                        if (ch == ',' && !insideQuote) {
                            placeholder.emplace_back(acc); 
                            acc.clear();
                        } else if (insideQuote == ch && prevCh != '\\') {
                            insideQuote = 0;
                        } else if (!insideQuote && (ch == '\'' || ch == '"')) {
                            insideQuote = ch;
                        } else {
                            if (prevCh == '\\' && ch == insideQuote) 
                                acc.pop_back();
                            acc += ch;
                        }
                        prevCh = ch;
                    }
                    if (insideQuote) 
                        throw std::runtime_error("Argparse Error: Invalid value passed to '" + _name + "': " + arg);
                    placeholder.emplace_back(acc);
                    return placeholder;
                } 

                else {
                    T placeholder;
                    std::from_chars_result parseResult {std::from_chars(arg.c_str(), arg.c_str() + arg.size(), placeholder)};
                    if (parseResult.ec != std::errc() || parseResult.ptr != arg.c_str() + arg.size())
                        throw std::runtime_error("Argparse Error: Invalid value passed to '" + _name + "': " + arg);
                    return placeholder;
                }
            }
    };

    class ArgumentParser {
        private:
            int maxArgLen{15}, maxSubCmdLen{15};
            const std::string name, helpArgName, helpAliasName;
            std::optional<std::string> _description, _epilog;
            std::unordered_map<std::string, Argument> allArgs;
            std::unordered_map<std::size_t, Argument&> positionalArgs;
            std::unordered_map<std::string, Argument&> namedArgs, aliasedArgs;
            std::unordered_map<std::string, ArgumentParser> subcommands;

            // Check if the parser was called at all
            // Esp required in the context of subcommands
            bool touched {false}; 

            // Variable to update references (update only once)
            bool refUpdated {false};

            // Static utility to split based on '=' sign (named args with '--')
            static std::pair<std::string, std::string> splitArg(const std::string &arg) {
                std::size_t pos {arg.find('=')};
                if (pos == std::string::npos)
                    return {arg, ""};
                else {
                    char prevCh {0}, insideQuote {0};
                    std::string value;
                    for (std::size_t i {pos + 1}; i < arg.size(); i++) {
                        const char &ch {arg[i]};
                        if (!insideQuote && ch == '=') {
                            insideQuote = '='; break;
                        } else if (!insideQuote && (ch == '\'' || ch == '"')) {
                            insideQuote = ch;
                        } else if (insideQuote == ch && prevCh != '\\') {
                            insideQuote = 0;
                        } else {
                            if (prevCh == '\\' && ch == insideQuote) 
                                value.pop_back();
                            value += ch;
                        }
                        prevCh = ch;
                    }

                    if (insideQuote)
                        throw std::runtime_error("Argparse Error: Invalid argument passed: " + arg);

                    return {arg.substr(0, pos), value};
                }
            }

            // Once all set and done we recursively update the references 
            // aliasedArgs, positionalArgs, namedArgs
            void updateReferences() {
                if (refUpdated) return;
                else {
                    for (const auto &[argName, arg]: allArgs) {
                        // Update positional args and named args
                        if (arg.getArgType() == ARGTYPE::POSITIONAL || arg.getArgType() == ARGTYPE::BOTH)
                            positionalArgs.emplace(positionalArgs.size(), allArgs.at(argName));
                        if (arg.getArgType() == ARGTYPE::NAMED || arg.getArgType() == ARGTYPE::BOTH)
                            namedArgs.emplace(argName, allArgs.at(argName));

                        // Updated alias map, error if alias already exists
                        if (!arg.getAlias().empty()) {
                            auto [it, inserted] = aliasedArgs.try_emplace(arg.getAlias(), allArgs.at(argName));
                            if (!inserted)
                                throw std::runtime_error("Argparse Error: Duplicate argument with alias: " + arg.getAlias());
                        }

                        // Update maxArgLen for prettified help message
                        maxArgLen = std::max(maxArgLen, static_cast<int>(argName.size() + 10));
                    }

                    // Recurse into subcommands
                    for (std::pair<const std::string, ArgumentParser> &kv: subcommands) {
                        maxSubCmdLen = std::max(maxSubCmdLen, static_cast<int>(kv.first.size() + 10));
                        kv.second.updateReferences();
                    }

                    refUpdated = true;
                }
            }

        public:
            ArgumentParser(
                const std::string &name, 
                const std::string &helpArgName = "help", 
                const std::string &helpAliasName = ""
            ): 
                name(name), 
                helpArgName(helpArgName), 
                helpAliasName(helpAliasName)
            {
                // Help is mandatory to short circuit checks
                if (helpArgName.empty())
                    throw std::runtime_error("Argparse Error: Help Argument name cannot be empty");
                Argument help {
                    Argument(helpArgName, ARGTYPE::NAMED)
                    .help("Display this help text and exit")
                    .implicitValue(true)
                    .defaultValue(false)
                };
                if (!helpAliasName.empty()) 
                    help.alias(helpAliasName);
                addArgument(std::move(help));
            }

            // Check if all arguments contained inside the parser are satisifed
            // If any arg is not satisifed, returns its name for logging
            // DOES NOT CHECK THE CHILD PARSERS
            std::string check() const {
                for (const auto &[_, arg]: allArgs)
                    if (!arg.ok())
                        return arg.getName();
                return "";
            }

            // Conveneince function over `check()`
            bool ok() const { return touched && check().empty(); }

            ArgumentParser &description(const std::string &message) {
                _description = message; return *this;
            }

            ArgumentParser &epilog(const std::string &message) {
                _epilog = message; return *this;
            }

            // Template specialization to get the arg value
            template<ValidValueType T=std::string>
            T get(const std::string &key) const {
                auto it {allArgs.find(key)};
                if (it == allArgs.end())
                    throw std::runtime_error("Argparse Error: Argument with name '" + key + "' does not exist");
                return it->second.get<T>();
            }

            // Command to retreive child parsers aka subcommands
            ArgumentParser& getChildParser(const std::string &key) {
                auto it {subcommands.find(key)};
                if (it == subcommands.end())
                    throw std::runtime_error("Argparse Error: Subcommand with name '" + key + "' does not exist");
                return it->second;
            }

            // Return true/false based on whether a key is retreivable
            bool exists(const std::string &key) const noexcept {
                auto it {allArgs.find(key)};
                if (it == allArgs.end()) return false;
                else return it->second.isValueSet() || it->second.isDefaultSet();
            }

            // Bread and butter, recursively call subcommands if found
            // Skip parsing parent and validating parent if subcommand has been found
            void parseArgs(int argc, char** argv, std::size_t parseStartIdx = 0) {
                // Update references if not already done
                updateReferences();

                // If this func was called, set touched as true
                touched = true;

                // Convert to strings for ease of parsing
                const std::vector<std::string> argVec {argv, argv+argc};

                // Parse the args
                std::size_t position {0}; bool explicitPositionalArgMarker {false};
                for (std::size_t i {parseStartIdx + 1}; i < argVec.size(); i++) {
                    std::string arg {argVec[i]};

                    // Marks the start of positional argument start (optional)
                    if (arg == "--") {
                        explicitPositionalArgMarker = true;
                    }

                    // Named arg, eg: "--age=12" (or) "--age 12"
                    else if (!explicitPositionalArgMarker && arg.starts_with("--")) {
                        arg = arg.substr(2); std::string value;
                        std::tie(arg, value) = splitArg(arg);

                        auto it {namedArgs.find(arg)};
                        if (it == namedArgs.end())
                            throw std::runtime_error("Argparse Error: Unknown named argument passed: " + arg);
                        else if (!value.empty())
                            it->second.set(value);
                        else if (i == argVec.size() - 1 || argVec[i + 1].starts_with('-'))
                            it->second.set();
                        else
                            it->second.set(argVec[++i]);
                    }

                    // Aliased arg, eg: "-a 20"
                    else if (!explicitPositionalArgMarker && arg.starts_with('-')) {
                        arg = arg.substr(1);
                        auto it {aliasedArgs.find(arg)};
                        if (it == aliasedArgs.end())
                            throw std::runtime_error("Argparse Error: Unknown aliased argument passed: " + arg);

                        if (i == argVec.size() - 1 || argVec[i + 1].starts_with('-'))
                            it->second.set();
                        else
                            it->second.set(argVec[++i]);
                    }

                    // Check if the arg is command, doing this check here allows us to processes
                    // no subcommands uptil this point if we wanted to
                    // Recursive call - skip validating the parent parser; hence the return
                    else if (!explicitPositionalArgMarker && subcommands.find(arg) != subcommands.end()) {
                        subcommands.at(arg).parseArgs(argc, argv, i);
                        return;
                    }

                    // None of the options suit us, maybe this is a positional argument
                    else {
                        // Once positional arg starts, no turning back
                        explicitPositionalArgMarker = true;

                        // If already set, move on
                        while (position < positionalArgs.size() && positionalArgs.at(position).isValueSet()) 
                            position++;

                        // If no positional args, left throw error
                        if (position >= positionalArgs.size())
                            throw std::runtime_error("Argparse Error: Unknown positional argument passed: " + arg);

                        positionalArgs.at(position++).set(arg);
                    }

                    // Check if help parameter has been set
                    // If yes, stop the parsing & exit early
                    if (allArgs.at(helpArgName).get<bool>()) {
                        std::cout << getHelp() << '\n';
                        std::exit(1);
                    }
                }

                // Check if all args are satisified
                const std::string missingArg {check()};
                if (!missingArg.empty())
                    throw std::runtime_error("Argparse Error: Missing value for argument: " + missingArg);
            }

            // Add a new argument for the parser, no duplicates
            ArgumentParser &addArgument(const Argument& arg) {
                const std::string argName {arg.getName()};

                // Ensure no duplicates
                if (allArgs.find(argName) != allArgs.end())
                    throw std::runtime_error("Argparse Error: Duplicate argument with name: " + argName);
                if (subcommands.find(argName) != subcommands.end())
                    throw std::runtime_error("Argparse Error: Argument name conflicts with subcommand: " + argName);

                allArgs.emplace(argName, std::move(arg));
                return *this;
            }

            // Overload addArgument to directly take in two strings
            Argument &addArgument(const std::string &name, const ARGTYPE &type = ARGTYPE::BOTH) {
                addArgument(Argument{name, type});
                return allArgs.at(name);
            }

            // Add subcommand into the parser, check if name is not already taken
            ArgumentParser &addSubcommand(
                const std::string &name, 
                const std::string &helpArgName = "help", 
                const std::string &helpAliasName = "") 
            {
                if (allArgs.find(name) != allArgs.end())
                    throw std::runtime_error("Argparse Error: Subcommand conflict with argument: " + name);
                if (subcommands.find(name) != subcommands.end())
                    throw std::runtime_error("Argparse Error: Duplicate subcommand with name: " + name);

                subcommands.emplace(name, ArgumentParser{name, helpArgName, helpAliasName});
                return subcommands.at(name);
            }

            // Processes and returns the help message
            std::string getHelp() const {
                std::ostringstream oss;
                oss << "Usage: " << name << " [OPTIONS] ";

                // Print out any subcommands if present
                std::ostringstream subcommandsHelp;
                std::string subcommandsAvailable;
                for (const auto& [_, command]: subcommands) {
                    subcommandsAvailable += command.name + ',';
                    std::string commandDesc {command._description? *command._description: "The '" + command.name + "' subcommand"};
                    subcommandsHelp << ' ' << std::left << std::setw(maxSubCmdLen) 
                                    << command.name << "\t" << commandDesc 
                                    << '\n';
                }
                if (!subcommands.empty()) {
                    subcommandsAvailable.pop_back();
                    oss << '{' << subcommandsAvailable << "} ";
                }

                // Print out positional args
                for (std::size_t i {0}; i < positionalArgs.size(); i++) {
                    std::string argName {positionalArgs.at(i).getName()};
                    if (positionalArgs.at(i).isOptional()) 
                        argName = '[' + argName + ']';
                    oss << argName << " ";
                }

                if (_description) 
                    oss << "\n\n" << *_description;

                if (!subcommands.empty()) {
                    std::string temp {subcommandsHelp.str()};
                    temp.pop_back();
                    oss << "\n\nSubcommands:\n" << temp;
                }

                // Print out all the args with details
                oss << "\n\nArguments:\n";
                for (const auto &[_, arg]: allArgs)
                    oss << " " << arg.getHelp(maxArgLen) << '\n';

                if (_epilog) 
                    oss << "\n" << *_epilog << '\n';

                return oss.str();
            }
    };
}
