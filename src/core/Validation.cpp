// Validation.cpp: the checks declared in Validation.h.
#include "core/Validation.h"

namespace matchforge::core {

namespace {

// True for characters allowed in a username. Written out by range so the set is clear.
bool isUsernameCharacter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

}  // namespace

bool isValidUsername(std::string_view username) {
    if (username.size() < kMinUsernameLength || username.size() > kMaxUsernameLength) {
        return false;
    }
    for (const char c : username) {
        if (!isUsernameCharacter(c)) {
            return false;
        }
    }
    return true;
}

bool isValidPassword(std::string_view password) {
    return password.size() >= kMinPasswordLength && password.size() <= kMaxPasswordLength;
}

}  // namespace matchforge::core
