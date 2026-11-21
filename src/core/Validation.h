// Validation.h: rules for usernames and passwords. Pure functions with no
// networking or storage, so they are tested on their own.
#pragma once

#include <cstddef>
#include <string_view>

namespace matchforge::core {

// Length limits. Short usernames are hard to read in a lobby; very long ones fill the screen.
inline constexpr std::size_t kMinUsernameLength = 3;
inline constexpr std::size_t kMaxUsernameLength = 20;

// Passwords: at least 8 characters, to resist guessing. The upper bound stops someone from
// sending megabytes of text that the hashing step would then spend time on.
inline constexpr std::size_t kMinPasswordLength = 8;
inline constexpr std::size_t kMaxPasswordLength = 128;

// True if the username is 3 to 20 characters, using only letters, digits, and underscore.
// Other characters are rejected so that names cannot be confused with commands or
// spoofed with lookalike symbols.
bool isValidUsername(std::string_view username);

// True if the password is 8 to 128 bytes long. Content is not restricted: a passphrase
// with spaces is allowed.
bool isValidPassword(std::string_view password);

}  // namespace matchforge::core
