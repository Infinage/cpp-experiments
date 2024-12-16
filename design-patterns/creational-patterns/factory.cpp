#include <iostream>
#include <memory>
#include <string>

// Base class from which other children are derived
class Vehicle {
    public:
        /*
         * With the virtual keyword, C++ uses dynamic polymorphism -
         * where the method to invoke is determined at runtime based on 
         * the actual type of the object being pointed to (not the pointer type).
         *
         * Without virtual, the method resolution uses static polymorphism - 
         * meaning the base class's method is always invoked, regardless of the object's actual type.
         *
         * The below function is "pure virtual" having no function definition "virtual void func() = 0;"
        */

        virtual std::string getType() = 0;
        virtual ~Vehicle() = default;
};

class Bike: public Vehicle {
    public: std::string  getType() override { return "Bike"; }
};

class Car: public Vehicle {
    public: std::string  getType() override { return "Car"; }
};

class Truck: public Vehicle {
    public: std::string  getType() override { return "Truck"; }
};

class VehicleFactory {
    public:
        enum VehicleType {BIKE=1, CAR=2, TRUCK=3};

        // Easily extensible, simply add to the types above and
        // add a method to the function below
        std::unique_ptr<Vehicle> create(VehicleType typeID) {
            if      (typeID == VehicleType::BIKE)  return std::make_unique<Bike> (); 
            else if (typeID == VehicleType::CAR)   return std::make_unique<Car>  ();
            else if (typeID == VehicleType::TRUCK) return std::make_unique<Truck>();
            else                                   return nullptr;
        }
};

int main() {
    VehicleFactory factory;    

    std::unique_ptr<Vehicle> bike  {factory.create(VehicleFactory::BIKE)};
    std::cout << "This is a " << bike->getType()  << "\n";

    std::unique_ptr<Vehicle> car   {factory.create(VehicleFactory::CAR)};
    std::cout << "This is a " << car->getType()   << "\n";

    std::unique_ptr<Vehicle> truck {factory.create(VehicleFactory::TRUCK)};
    std::cout << "This is a " << truck->getType() << "\n";
}
