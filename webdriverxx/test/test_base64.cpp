#include "base64.hpp"

int main() {
    // Encode test
    int encStatus {1};
    encStatus &= Base64::base64Encode("Hey") == "SGV5";
    encStatus &= Base64::base64Encode("Hello") == "SGVsbG8=";
    encStatus &= Base64::base64Encode("Uranium") == "VXJhbml1bQ==";
    encStatus &= Base64::base64Encode("The quick brown fox jumped over the lazy dog") == "VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wZWQgb3ZlciB0aGUgbGF6eSBkb2c=";

    // Decode test
    int decStatus {1};
    decStatus &= Base64::base64Decode("SGV5") == "Hey";
    decStatus &= Base64::base64Decode("SGVsbG8=") == "Hello";
    decStatus &= Base64::base64Decode("VXJhbml1bQ==") == "Uranium";
    decStatus &= Base64::base64Decode("VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wZWQgb3ZlciB0aGUgbGF6eSBkb2c=") == "The quick brown fox jumped over the lazy dog";

    return !(encStatus && decStatus);
}
