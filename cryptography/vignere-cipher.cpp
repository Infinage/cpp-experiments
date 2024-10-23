#include <cctype>
#include <format>
#include <iostream>

class Vignere {
public:
    enum MODE { DECRYPT, ENCRYPT };

    std::string vignerecipher(std::string& original, std::string& key, int mode) {
        std::string result {""};
        std::size_t keyIdx {0};
        for (char ch: original) {

            int shift = 0;
            if (std::isalpha(ch)) {
                int textOrd = std::tolower(ch) - 'a';
                int keyOrd = key[keyIdx] - 'a';
                shift = mode == ENCRYPT? (textOrd + keyOrd) % 26: (textOrd - keyOrd + 26) % 26;
                keyIdx = (keyIdx + 1) % key.size();
            }

            if (std::islower(ch))
                result += 'a' + shift;
            else if (std::isupper(ch))
                result += 'A' + shift;
            else
                result += ch;

        }
        return result;
    }
};


int main() {

    Vignere algo;
    std::string plaintext {"The quick brown fox jumped over the lazy dog."};
    std::string key {"secret"};
    std::string encrypted = algo.vignerecipher(plaintext, key, algo.ENCRYPT);
    std::string decrypted = algo.vignerecipher(encrypted, key, algo.DECRYPT);
    std::cout << std::format("Original: {}\nEncrypted: {}\nDecrypted: {}", plaintext, encrypted, decrypted) << "\n";

    return 0;
}
