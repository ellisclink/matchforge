// PasswordHasher.cpp: Argon2id hashing, password checks and session tokens, via libsodium.
#include "auth/PasswordHasher.h"

#include <sodium.h>

#include <array>
#include <cstddef>
#include <stdexcept>

namespace matchforge::auth {

namespace {

// Session tokens are 256 random bits. Guessing one is not feasible.
constexpr std::size_t kTokenBytes = 32;

// sodium_init sets up libsodium's random number generator and CPU detection. It must run
// once before any other call. Calling it again is safe, so this is only a convenience.
void ensureSodiumReady() {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium failed to initialize");
    }
}

}  // namespace

std::string hashPassword(std::string_view password) {
    ensureSodiumReady();

    // crypto_pwhash_STRBYTES is the size of the output buffer. The output string is
    // NUL-terminated, which is why we keep the full buffer.
    std::array<char, crypto_pwhash_STRBYTES> hash{};

    // The INTERACTIVE limits give a hash in roughly 50 to 100 ms with 64 MB of memory.
    // That is slow enough to make guessing expensive, and fast enough for a login.
    const int result =
        crypto_pwhash_str(hash.data(), password.data(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE);
    if (result != 0) {
        // Almost always out of memory. The message says nothing about the password.
        throw std::runtime_error("password hashing failed");
    }
    return std::string(hash.data());
}

bool verifyPassword(const std::string& hash, std::string_view password) {
    ensureSodiumReady();

    // Returns 0 when the password matches. Any other value means no match.
    return crypto_pwhash_str_verify(hash.c_str(), password.data(), password.size()) == 0;
}

std::string newSessionToken() {
    ensureSodiumReady();

    std::array<unsigned char, kTokenBytes> raw{};
    randombytes_buf(raw.data(), raw.size());

    // Base64 with URL-safe characters and no padding, so the token can go in a URL or header
    // unchanged.
    std::array<char,
               sodium_base64_ENCODED_LEN(kTokenBytes, sodium_base64_VARIANT_URLSAFE_NO_PADDING)>
        encoded{};
    sodium_bin2base64(encoded.data(), encoded.size(), raw.data(), raw.size(),
                      sodium_base64_VARIANT_URLSAFE_NO_PADDING);
    return std::string(encoded.data());
}

}  // namespace matchforge::auth
