# matchforge_tests: GoogleTest suites. Each .cpp file under tests/ mirrors a
# folder under src/. Every test is registered with ctest, so `ctest` runs them.

# Loads CMake's GoogleTest helper, which provides gtest_discover_tests() below.
include(GoogleTest)

# The test program. It is built from every test source file in the list.
add_executable(matchforge_tests
  core/VersionTest.cpp
  core/ValidationTest.cpp           # username and password rules
  core/EloTest.cpp                  # Elo formulas against hand-checked values
  core/MatchmakerTest.cpp           # queue and matching rules
  core/RateLimiterTest.cpp          # token bucket
  core/LoginThrottleTest.cpp        # failed-login lockout
  config/ServerConfigTest.cpp       # JSON settings: parsing and validation
  proto/EnvelopeTest.cpp            # Protobuf round-trip: the generated code works at runtime
  auth/PasswordHasherTest.cpp       # Argon2id hashing and session tokens
  auth/AuthServiceTest.cpp          # register and login rules, against the in-memory store
  store/InMemoryAccountStoreTest.cpp
  store/RedisAccountStoreTest.cpp   # real Redis; skipped unless MATCHFORGE_TEST_REDIS_PORT is set
  net/FramingTest.cpp               # frame encoder and decoder (no sockets)
  net/ServerTest.cpp                # integration: a real server on loopback and a client socket
  net/SessionLimitsTest.cpp         # idle timeout and rate limiting
  lobby/LobbyTest.cpp               # queue, matches, results and concurrency, over TCP
  bots/BotOptionsTest.cpp           # bot command-line parsing
  bots/BotReportTest.cpp            # average and percentiles
  bots/BotClientTest.cpp)           # the bot client against a real server               # integration: a real server on loopback and a client socket

# Libraries the tests need.
#   matchforge_core      the code under test (version(), validation)
#   matchforge_proto     the generated Envelope/Heartbeat classes under test
#   matchforge_net       the framing, session and server code under test
#   matchforge_auth      password hashing and the auth service
#   matchforge_store     the in-memory and Redis account stores
#   Boost::asio, Threads the sockets and threads the integration tests use
#   GTest::gtest_main    GoogleTest plus a ready-made main(), so each test file needs no main()
#   matchforge_warnings  the same strict warning flags as the real code
target_link_libraries(matchforge_tests
  PRIVATE
    matchforge_core
    matchforge_proto
    matchforge_net
    matchforge_auth
    matchforge_store
    matchforge_lobby
    matchforge_botlib
    matchforge_config
    Boost::asio
    Threads::Threads
    GTest::gtest_main
    matchforge_warnings)

# After the program is built, CMake runs it with --gtest_list_tests, finds every
# TEST() name, and registers each one with ctest. So a single `ctest` run covers all tests.
# Lets tests include shared helpers such as "net/TestSupport.h".
# Lets tests find files in the repository, such as config/server.json.
target_compile_definitions(matchforge_tests PRIVATE MATCHFORGE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

target_include_directories(matchforge_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# DISCOVERY_TIMEOUT: the test binary is large with sanitizers on; give listing tests time.
gtest_discover_tests(matchforge_tests DISCOVERY_TIMEOUT 60)
