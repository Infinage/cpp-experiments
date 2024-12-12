#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

// Base Class that all other classes would inherit from
class Container {
    protected:
        const long id;

    public:
        explicit Container(long id): id(id) {}
        virtual ~Container() = default;

        // Defining ID for easy deletion
        long getId() const { return id; }
        virtual double calculatePrice() const = 0;
};

// Class impl that could potential contain other Containers inside it
class Products: public Container {
    protected:
        std::vector<std::unique_ptr<Container>> items;

    public:
        explicit Products(long id, std::vector<std::unique_ptr<Container>> &&items): Container(id), items(std::move(items)) {}

        void addContainer(std::unique_ptr<Container> ptr) { items.push_back(std::move(ptr)); }
        void removeContainer(long id) {
            for (std::vector<std::unique_ptr<Container>>::iterator it {items.begin()}; it != items.end();) {
                if ((*it)->getId() == id) it = items.erase(it);
                else it++;
            }
        }

        double calculatePrice() const override {
            return std::accumulate(items.begin(), items.end(), 0., [](double acc, const std::unique_ptr<Container> &c) { 
                return acc + c->calculatePrice(); 
            });
        }
};

// Base class for out actual product (leaf)
class Product: public Container {
    protected:
        const std::string title;
        const double price;

    public:
        Product(long id, std::string &title, double price): Container(id), title(title), price(price) {}
        virtual double calculatePrice() const override = 0;
        virtual ~Product() = default;
};

// Leaf node
class Book: public Product {
    public:
        Book(long id, std::string &&title, double price): Product(id, title, price) {}
        double calculatePrice() const override { return price; }
};

// Leaf node
class VideoGame: public Product {
    public:
        VideoGame(long id, std::string &&title, double price): Product(id, title, price) {}
        double calculatePrice() const override { return price; }
};

// Leaf node
class Stationary: public Product {
    public:
        Stationary(long id, std::string &&title, double price): Product(id, title, price) {}
        double calculatePrice() const override { return price; }
};

// https://godbolt.org/z/9nrxfWP6Y
// To init a vector of unique_ptr
template<typename T, typename ...Ptrs>
std::vector<std::unique_ptr<T>> make_vector(Ptrs&& ... ptrs) {
    std::vector<std::unique_ptr<T>> result;
    ( result.push_back( std::forward<Ptrs>(ptrs) ), ... );
    return result;
}

int main() {
    std::unique_ptr<Container> books {std::make_unique<Products>(1L, make_vector<Container>(
        std::make_unique<Book>(1L, "Book1", 200.), 
        std::make_unique<Book>(2L, "Book2", 400.)
    ))};

    std::unique_ptr<Container> games {std::make_unique<Products>(1L, make_vector<Container>(
        std::make_unique<Book>(1L, "Game1", 300.), 
        std::make_unique<Book>(2L, "Game2", 1000.),
        std::make_unique<Stationary>(3L, "Misplaced Stionary 3", 10.)
    ))};

    std::unique_ptr<Container> stationaries {std::make_unique<Products>(3L, make_vector<Container>(
        std::make_unique<Stationary>(1L, "Stationary 1", 30.), 
        std::make_unique<Stationary>(2L, "Stationary 2", 5.),
        std::make_unique<Stationary>(3L, "Stationary 4", 10.),
        std::make_unique<Stationary>(3L, "Stationary 5", 12.),
        std::make_unique<Stationary>(3L, "Stationary 6", 90.),
        std::make_unique<Stationary>(3L, "Stationary 7", 8.)
    ))};

    Products store {0L, make_vector<Container>(std::move(books), std::move(games), std::move(stationaries))};
    std::cout << "Total worth of items in Store: " << store.calculatePrice() << "\n";

    return 0;
}
