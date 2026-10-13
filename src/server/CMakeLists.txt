# matchforge_server: the server program. It listens on a TCP port and answers
# heartbeats (Milestone 1).

# Builds the executable matchforge_server from main.cpp.
add_executable(matchforge_server main.cpp)

# Libraries this program needs. PRIVATE means they are used only to build this
# program. Nothing else links to the server, so nothing else inherits them.
target_link_libraries(matchforge_server
  PRIVATE
    matchforge_core      # our version() function
    matchforge_net       # the TCP server, framing and sessions
    spdlog::spdlog       # logging (spdlog::info)
    Boost::asio          # io_context and signal handling
    matchforge_warnings) # strict warning flags for our own code
