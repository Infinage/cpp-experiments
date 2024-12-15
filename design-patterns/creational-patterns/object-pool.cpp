#include <iostream>
#include <memory>
#include <vector>

class Truck {};

class Warehouse {
    private:
        static std::shared_ptr<Warehouse> instance;
        std::vector<std::unique_ptr<Truck>> trucks;
        Warehouse() {}

    public:
        static std::shared_ptr<Warehouse> getInstance() {
            if (instance == nullptr) 
                instance = std::shared_ptr<Warehouse>(new Warehouse());
            return instance;
        }

        std::unique_ptr<Truck> getResource() {
            if (trucks.empty()) {
                std::cout << "Purchasing a new Truck.\n"; 
                return std::make_unique<Truck>();
            } else {
                std::cout << "Reusing an available truck.\n";
                std::unique_ptr<Truck> truck {std::move(trucks.back())};
                trucks.pop_back();
                return truck;
            }
        }

        void returnResource(std::unique_ptr<Truck> truck) {
            trucks.push_back(std::move(truck));
        }
};

// Assign the global instance for our singleton
std::shared_ptr<Warehouse> Warehouse::instance {nullptr};

// ---------------------------- Sample Code ----------------------------

int main() {
    std::shared_ptr<Warehouse> warehouse {Warehouse::getInstance()};

    // Create new resource
    std::unique_ptr<Truck> truck1{warehouse->getResource()};
    std::unique_ptr<Truck> truck2{warehouse->getResource()};
    std::unique_ptr<Truck> truck3{warehouse->getResource()};

    // Return resources
    warehouse->returnResource(std::move(truck1));
    warehouse->returnResource(std::move(truck2));

    // Reuse existing resources
    std::unique_ptr<Truck> truck4{warehouse->getResource()};
    std::unique_ptr<Truck> truck5{warehouse->getResource()};

    // Create new resource
    std::unique_ptr<Truck> truck6{warehouse->getResource()};

    return 0;
}
