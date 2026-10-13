# matchforge_server: the server program. Milestone 0 only prints its version.

# Builds the executable matchforge_server from main.cpp.
add_executable(matchforge_server main.cpp)

# Libraries this program needs. PRIVATE means they are used only to build this
# program. Nothing else links to the server, so nothing else inherits them.
target_link_libraries(matchforge_server
  PRIVATE
    matchforge_core     # our version() function
    spdlog::spdlog      # logging (spdlog::info)
    matchforge_warnings)  # strict warning flags for our own code
