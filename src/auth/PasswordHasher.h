// PasswordHasher.h: password hashing and session token generation, using libsodium.
#pragma once

#include <string>
#include <string_view>

namespace matchforge::auth {

// Hashes a password with Argon2id, a slow hash designed to resist cracking.
// The result is a single string that holds the algorithm settings, a random salt, and the
// hash, so verifying later needs nothing but this string.
// Throws std::runtime_error if libsodium cannot allocate the memory the hash needs.
// CPU-heavy (about 50 to 100 ms): call it from a worker thread, never an I/O thread.
std::string hashPassword(std::string_view password);

// True if password matches a hash made by hashPassword. Takes the same time for right and wrong
// passwords.
bool verifyPassword(const std::string& hash, std::string_view password);

// A new random session token: 32 random bytes encoded as URL-safe base64 with no padding (43
// characters). The random source is libsodium's, which is safe for secrets.
std::string newSessionToken();

}  // namespace matchforge::auth
