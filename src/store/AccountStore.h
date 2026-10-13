// AccountStore.h: the storage interface for accounts and sessions.
//
// The rest of the server talks to this interface, not to Redis directly. That
// lets the unit tests use an in-memory store, and lets us swap the backend later.
#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace matchforge::store {

class AccountStore {
public:
    virtual ~AccountStore() = default;

    // Creates an account. Returns false if the username is already taken.
    // Throws std::runtime_error if the storage backend fails.
    virtual bool createAccount(std::string_view username, std::string_view passwordHash) = 0;

    // The stored password hash for a username, or nullopt if no such account exists.
    // Throws std::runtime_error if the storage backend fails.
    virtual std::optional<std::string> findPasswordHash(std::string_view username) = 0;

    // Stores a session token that maps to a username. The token expires after ttl.
    // Throws std::runtime_error if the storage backend fails.
    virtual void saveSession(std::string_view token, std::string_view username,
                             std::chrono::seconds ttl) = 0;

    // The username a session token belongs to, or nullopt if the token is unknown or expired.
    // Throws std::runtime_error if the storage backend fails.
    virtual std::optional<std::string> findSessionUser(std::string_view token) = 0;

    // The player's saved Elo rating, or nullopt if none has been saved yet (a new player).
    // Throws std::runtime_error if the storage backend fails.
    virtual std::optional<int> findRating(std::string_view username) = 0;

    // Saves the player's Elo rating, replacing any earlier value.
    // Throws std::runtime_error if the storage backend fails.
    virtual void saveRating(std::string_view username, int rating) = 0;

    // Adds delta to the player's rating in one atomic step and returns the new rating.
    // A player with no saved rating starts from startingRating. Atomic means two match
    // results for the same player, arriving at the same time, are both applied.
    // Throws std::runtime_error if the storage backend fails.
    virtual int adjustRating(std::string_view username, int delta, int startingRating) = 0;
};

}  // namespace matchforge::store
