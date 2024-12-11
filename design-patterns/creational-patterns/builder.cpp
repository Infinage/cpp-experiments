#include <iostream>
#include <string>

class Building {
    private:
        // Define Fields
        std::string material      {"concrete"};
        int         floors        {0};
        int         rooms         {0};
        int         area          {0};
        bool        swimmingPool  {false};
        bool        garden        {false};

    public:
        // Define Setters - return reference at each step
        Building &setMaterial     (std::string &&m) { material     = m; return *this; }
        Building &setFloors       (int f)           { floors       = f; return *this; }
        Building &setRooms        (int r)           { rooms        = r; return *this; }
        Building &setArea         (int a)           { area         = a; return *this; }
        Building &setSwimmingPool (bool p)          { swimmingPool = p; return *this; }
        Building &setGarden       (bool g)          { garden       = g; return *this; }

        // Display info on the building
        void display() {
            std::cout << "====================================================\n"
                      << "Material:      " << material                    << "\n"
                      << "Floors:        " << floors                      << "\n"
                      << "Rooms:         " << rooms                       << "\n"
                      << "Area (sqft):   " << area                        << "\n"
                      << "Swimming Pool: " << (swimmingPool? "Yes": "No") << "\n"
                      << "Garden :       " << (garden? "Yes": "No")       << "\n"
                      << "====================================================\n";
        }
};

int main() {

    // We construct two buildings
    Building b1, b2;

    // Spacious & luxurious
    b1.setMaterial("Concrete")
      .setFloors(4)
      .setRooms(20)
      .setArea(3600)
      .setSwimmingPool(true)
      .setGarden(true);

    // Compact and cozy
    b2.setMaterial("Wood")
      .setFloors(1)
      .setRooms(4)
      .setArea(900);

    // Display both the buildings
    b1.display();
    b2.display();

    return 0;
}
