#include <cctype>
#include <iostream>
#include <array>
#include <fstream>
#include <format>
#include <string>

std::string rotcipher(std::string& plaintext, int shift) {
    // We have 26 alphabets, max possible shift
    shift = ((shift % 26) + 26) % 26;

    // We map both upper and lowercase
    // would be ignoring special chars
    std::array<char, 26> lowercaseMapping;
    std::array<char, 26> uppercaseMapping;
    for (int i = 0; i < 26; i++) {
        lowercaseMapping[i] = 'a' + ((i + shift) % 26);
        uppercaseMapping[i] = 'A' + ((i + shift) % 26);
    }

    std::string ciphertext {""};
    ciphertext.reserve(plaintext.size());
    for (char ch: plaintext) {
        if (std::islower(ch))
            ciphertext += lowercaseMapping[ch - 'a'];
        else if (std::isupper(ch))
            ciphertext += uppercaseMapping[ch - 'A'];
        else
            ciphertext += ch;
    }

    return ciphertext;
}

int main() {
    std::string ifilename, ofilename, buffer;
    int op, shift;

    std::cout << "Enter input filename: ";
    std::cin >> ifilename;

    std::cout << "Enter output filename: ";
    std::cin >> ofilename;

    std::ifstream ifs({ifilename});
    std::ofstream ofs({ofilename});

    if (!ifs || !ofs) {

        std::cerr << std::format("IO Error, provided input: '{}', output: '{}'\n", ifilename, ofilename);
        return 1;

    } else {

        // Read entire contents
        std::string temp;
        while(std::getline(ifs, temp))
            buffer += temp + "\n";
        buffer.pop_back();

        std::cout << "File read successful.\nWhat do you wish to do (1,2)?\n1. Encrypt\n2. Decrypt\n>> ";
        std::cin >> op;

        switch (op) {
            case 1:
                std::cout << "\nEnter shift: ";
                std::cin >> shift;
                temp = rotcipher(buffer, shift);
                std::cout << std::format("\nPlaintext ----> \n{}\n\nEncrypted ----> \n{}\n", buffer, temp);
                ofs << temp << "\n";
                break;

            case 2:
                for (shift = 1; shift < 26; shift++) {
                    temp = std::format("\nDecrypted (Shift: {}) ----> \n{}\n", shift, rotcipher(buffer, shift));
                    std::cout << temp;
                    ofs << temp;
                }
                break;

            default:
                std::cout << "Invalid operation selected. Exiting.\n";
        }

        return 0;
    }
}
