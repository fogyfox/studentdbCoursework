#include "crypto.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>
#include <cstring>

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
    PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                      salt, saltLen, iterations,
                      EVP_sha256(), hashLen, hash);

    return bytesToHex(salt, saltLen) + ":" + bytesToHex(hash, hashLen);
}

bool checkPassword(const std::string& password, const std::string& stored) {
    const int saltLen = 16;
    const int hashLen = 32;
    const int iterations = 100000;

    auto pos = stored.find(':');
    if (pos == std::string::npos) return false;

    std::string saltHex = stored.substr(0, pos);
    std::string hashHex = stored.substr(pos + 1);

    unsigned char salt[saltLen];
    unsigned char hash[hashLen];

    for (int i = 0; i < saltLen; ++i)
        salt[i] = std::stoi(saltHex.substr(i * 2, 2), nullptr, 16);
    for (int i = 0; i < hashLen; ++i)
        hash[i] = std::stoi(hashHex.substr(i * 2, 2), nullptr, 16);

    unsigned char testHash[hashLen];
    PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                      salt, saltLen, iterations,
                      EVP_sha256(), hashLen, testHash);

    return CRYPTO_memcmp(hash, testHash, hashLen) == 0;
}