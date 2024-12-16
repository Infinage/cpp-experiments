#include <iostream>
#include <memory>
#include <mutex>

// A sample singleton class
class Database {
    private:
        // Ensure that lambda func is called only once - THREAD SAFE
        static std::once_flag flag;                                   

        // Globally only one variable exists
        static std::shared_ptr<Database> db;

        // Mark constructor as private
        Database() { std::cout << "Created a Database instance.\n"; }

    public:
        static std::shared_ptr<Database> getInstance() {
            std::call_once(flag, [](){ db = std::shared_ptr<Database>(new Database()); }); 
            return db;
        }
};

// Assign static variable values
std::shared_ptr<Database> Database::db = nullptr;
std::once_flag Database::flag;

// ----------------------- SAMPLE PROGRAM ----------------------- //

int main() {
    // Try creating multiple instances
    std::shared_ptr<Database> conn1 {Database::getInstance()};
    std::shared_ptr<Database> conn2 {Database::getInstance()};

    // Print the memory address of both the instances
    std::cout << "Connection Obj1 addr#: " << conn1.get() << "\n";
    std::cout << "Connection Obj2 addr#: " << conn2.get() << "\n";

    return 0;
}
