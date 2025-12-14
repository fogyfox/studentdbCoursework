#include <iostream>
#include <string>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>
#include <openssl/crypto.h>

std::string bytesToHex(const unsigned char* bytes, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
    return oss.str();
}

std::string hashPassword(const std::string& password) {
    const int saltLen = 16;
    const int hashLen = 32;
    const int iterations = 100000;

    unsigned char salt[saltLen];
    RAND_bytes(salt, saltLen);

    unsigned char hash[hashLen];
    PKCS5_PBKDF2_HMAC(
        password.c_str(),
        password.length(),
        salt,
        saltLen,
        iterations,
        EVP_sha256(),
        hashLen,
        hash
    );

    return bytesToHex(salt, saltLen) + ":" + bytesToHex(hash, hashLen);
}

int main() {
    std::string pass = "admin";
    std::cout << hashPassword(pass) << std::endl;
}
