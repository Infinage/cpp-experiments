#include <iostream>
#include <string>
#include <vector>

class InsuranceVisitor;

class Client {
    protected:
        std::string name, address;

    public:
        virtual ~Client() = default;
        Client(std::string &&name, std::string &&address): name(name), address(address) {}
        std::string getName() const { return name; }
        virtual void accept(InsuranceVisitor *visitor) const = 0;
};

class Bank: public Client {
    public:
        using Client::Client;
        void accept(InsuranceVisitor *visitor) const override;
};

class Company: public Client {
    public:
        using Client::Client;
        void accept(InsuranceVisitor *visitor) const override;
};

class Resident: public Client {
    public:
        using Client::Client;
        void accept(InsuranceVisitor *visitor) const override;
};

class InsuranceVisitor {
    private:
        std::vector<Client*> clients;

    public:
        InsuranceVisitor(std::vector<Client*> &&clients): clients(clients) {}
        ~InsuranceVisitor() { for (Client *client: clients) delete client; }

        void visitBank(const Bank *client) { 
            std::cout << "Sharing details regarding Theft Insurance to " << client->getName() << "..\n"; 
        }

        void visitCompany(const Company *client) { 
            std::cout << "Sharing details regarding Equipment Insurance to " << client->getName() << "..\n"; 
        }

        void visitResident(const Resident *client) { 
            std::cout << "Sharing details regarding Medical Insurance to " << client->getName() << "..\n"; 
        }

        void visitClients() {
            for (Client *client: clients)
                client->accept(this);
        }
};

void     Bank::accept(InsuranceVisitor *visitor) const {     visitor->visitBank(this); }
void  Company::accept(InsuranceVisitor *visitor) const {  visitor->visitCompany(this); }
void Resident::accept(InsuranceVisitor *visitor) const { visitor->visitResident(this); }

int main() {
    InsuranceVisitor visitor{{
        new Bank{"International Trust Bank", "123 Elm Street, Springfield, IL 62701"},
        new Company{"XYZ Corp", "45 Silicon Valley Road, Bengaluru, Karnataka 560100"},
        new Resident{"John Doe", "78 Maple Avenue, Greenfield, NY 12866"},
    }};

    visitor.visitClients();

    return 0;
}
