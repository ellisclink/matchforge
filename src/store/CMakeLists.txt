# matchforge_store: where accounts and sessions are kept.
#   InMemoryAccountStore  keeps everything in RAM (tests, local runs)
#   RedisAccountStore     keeps everything in Redis (the real server)

add_library(matchforge_store STATIC
  InMemoryAccountStore.cpp
  RedisAccountStore.cpp
  BoostRedisSource.cpp)  # compiles Boost.Redis's implementation (must be exactly once)

target_include_directories(matchforge_store PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(matchforge_store
  PUBLIC
    Boost::asio          # io_context used by the Redis connection
    Boost::redis         # Redis client
    Threads::Threads     # the Redis I/O thread
    OpenSSL::SSL         # headers Boost.Redis includes (no TLS is used)
    OpenSSL::Crypto
  PRIVATE
    matchforge_warnings)
