# matchforge_server: the server program. It listens on a TCP port, answers heartbeats,
# and handles register and login with accounts stored in Redis.

# Builds the executable matchforge_server from main.cpp.
add_executable(matchforge_server main.cpp)

# Libraries this program needs. PRIVATE means they are used only to build this
# program. Nothing else links to the server, so nothing else inherits them.
target_link_libraries(matchforge_server
  PRIVATE
    matchforge_core      # our version() function
    matchforge_net       # the TCP server, framing and sessions
    matchforge_auth      # register and login
    matchforge_lobby     # the matchmaking queue
    matchforge_config    # the JSON settings file
    matchforge_metrics   # Prometheus metrics
    prometheus-cpp::pull # the HTTP endpoint Prometheus scrapes
    matchforge_store     # Redis account storage
    spdlog::spdlog       # logging (spdlog::info)
    Boost::asio          # io_context, thread_pool and signal handling
    matchforge_warnings) # strict warning flags for our own code
