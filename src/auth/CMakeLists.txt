# matchforge_auth: register and login. Password hashing (Argon2id via libsodium),
# session tokens, and the AuthService that runs the slow work on worker threads.
#
# It depends on matchforge_store for accounts and sessions, and on matchforge_core for the
# username and password rules. It does not depend on networking.

add_library(matchforge_auth STATIC
  PasswordHasher.cpp
  AuthService.cpp)

target_include_directories(matchforge_auth PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(matchforge_auth
  PUBLIC
    matchforge_store     # AccountStore interface
    matchforge_core      # Elo defaults in AuthResult, and the validation rules
    Boost::asio          # thread_pool for worker threads
  PRIVATE
    unofficial-sodium::sodium  # Argon2id and random bytes
    spdlog::spdlog       # logging of failures (never passwords)
    matchforge_warnings)
