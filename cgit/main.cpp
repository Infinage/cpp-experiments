#include "include/CommandHandler.hpp"

int main(int argc, char **argv) {
    try {
        CommandHandler handler;    
        handler.handleArgs(argc, argv);
        return 0;
    }

    catch (std::exception &ex) {
        std::cerr << "fatal: " << ex.what() << '\n';
        return 1;
    }
}
