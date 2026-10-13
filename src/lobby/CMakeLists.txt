# matchforge_lobby: the matchmaking queue as a network-facing service. It owns the
# core Matchmaker on one strand, announces matches, and applies match results to ratings.
#
# It knows nothing about sockets: it talks to players through the LobbyClient interface,
# which Session implements.

add_library(matchforge_lobby STATIC LobbyService.cpp)

target_include_directories(matchforge_lobby PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(matchforge_lobby
  PUBLIC
    matchforge_core      # Matchmaker and Elo
    matchforge_store     # ratings
    Boost::asio          # strand, timer, thread_pool
  PRIVATE
    matchforge_proto     # the messages sent to players
    matchforge_auth      # newSessionToken() for random match ids
    spdlog::spdlog
    matchforge_warnings)
