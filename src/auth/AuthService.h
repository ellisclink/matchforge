// AuthService.h: register and login. Runs the slow work (hashing, storage) on worker threads.
#pragma once

#include <boost/asio/thread_pool.hpp>
#include <cstdint>
#include <functional>
#include <string>

#include "store/AccountStore.h"

namespace matchforge::auth {

// The outcome of a register or login attempt.
struct AuthResult {
    bool ok = false;
    std::string sessionToken;  // Set when ok is true.
    std::string username;  // Set when ok is true; the lowercase name the account is stored under.
    std::int32_t errorCode = 0;  // Set when ok is false. Uses the same codes as ErrorResponse.
    std::string errorMessage;    // Safe to show to the client. Never contains the password.
};

// Called once with the outcome. It runs on a worker thread, not the thread that made the request.
using AuthCallback = std::function<void(AuthResult)>;

class AuthService {
public:
    // The store must outlive the service. Work runs on the pool, which must also outlive the
    // service.
    AuthService(store::AccountStore& store, boost::asio::thread_pool& workers);

    // Starts an account registration and returns at once. The callback is called later.
    void registerAccount(std::string username, std::string password, AuthCallback callback);

    // Starts a login and returns at once. The callback is called later.
    void login(std::string username, std::string password, AuthCallback callback);

private:
    store::AccountStore& m_store;
    boost::asio::thread_pool& m_workers;
};

}  // namespace matchforge::auth
