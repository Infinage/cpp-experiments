#include "../cli/argparse.hpp"

int main(int argc, char **argv) {
    try {
        // Main argument parser
        argparse::ArgumentParser program{"fittrack"};
        program.addArgument(argparse::Argument("user").help("User's name").alias("u").required());
        program.addArgument(argparse::Argument("age").help("User's age").defaultValue(18).alias("a"));
        program.addArgument(argparse::Argument("weight").help("Current weight (kg)").alias("w").required());
        program.addArgument(argparse::Argument("goal").help("Fitness goal (e.g., weight loss, muscle gain)").alias("g"));

        program.description("A command-line fitness tracker to log workouts and track progress.");

        // Subcommand: Log workout
        argparse::ArgumentParser logWorkout{"log"};
        logWorkout.addArgument(argparse::Argument("exercise").help("Type of workout").required());
        logWorkout.addArgument(argparse::Argument("duration").help("Duration in minutes").scan<int>().required());
        logWorkout.addArgument(argparse::Argument("calories").help("Calories burned").scan<int>().defaultValue(0));

        logWorkout.description("Log a new workout session.");

        // Subcommand: View progress
        argparse::ArgumentParser progress{"progress"};
        progress.addArgument(argparse::Argument("days").help("Show logs for last N days").scan<int>().defaultValue(7));

        progress.description("View workout logs for a given number of days.");

        // Subcommand: Sync data
        argparse::ArgumentParser sync{"sync"};
        sync.description("Sync workout data with cloud storage.");

        // Add subcommands
        program.addSubcommand(logWorkout);
        program.addSubcommand(progress);
        program.addSubcommand(sync);

        // Parse arguments
        program.parseArgs(argc, argv);

        // Handle subcommands
        if (logWorkout.ok()) {
            std::cout << "Logging Workout:\n";
            std::cout << "Exercise: " << logWorkout.get<std::string>("exercise") << '\n';
            std::cout << "Duration: " << logWorkout.get<int>("duration") << " minutes\n";
            std::cout << "Calories burned: " << logWorkout.get<int>("calories") << '\n';
        } 

        else if (progress.ok()) {
            std::cout << "Fetched last " << progress.get<int>("days") << " days of workout logs.\n";
        } 

        else if (sync.ok()) {
            std::cout << "Syncing workout data to the cloud...\n";
        } 

        else {
            // Display user info
            std::cout << "User: " << program.get<std::string>("user") << '\n';
            std::cout << "Age: " << program.get<int>("age") << '\n';
            std::cout << "Weight: " << program.get<std::string>("weight") << " kg\n";
            if (program.exists("goal"))
                std::cout << "Goal: " << program.get<std::string>("goal") << '\n';
        }

    } catch (const std::exception &ex) {
        std::cerr << ex.what() << '\n';
    }

    return 0;
}
