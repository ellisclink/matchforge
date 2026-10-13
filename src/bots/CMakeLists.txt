# matchforge_bots: the load-test simulator. Milestone 0 only prints its version.

# Builds the executable matchforge_bots from main.cpp.
add_executable(matchforge_bots main.cpp)

# Libraries this program needs. PRIVATE means they are used only to build this
# program, not passed on to anything else.
target_link_libraries(matchforge_bots
  PRIVATE
    matchforge_core     # our version() function
    spdlog::spdlog      # logging (spdlog::info)
    matchforge_warnings)  # strict warning flags for our own code
