#pragma once

#include "../../cli/argparse.hpp"

class CommandHandler {
    public:
        CommandHandler();

        // Parse the CMD inputs and execute the appropriate function
        void handleArgs(int argc, char **argv);

    private:
        argparse::ArgumentParser argparser;

        // Initialize the parser with all the gory details
        static argparse::ArgumentParser initParser();
};
