# matchforge_bots: the load simulator. Many fake players register, queue and get
# matched, and the program reports how long matching took.

# The reusable parts (options parsing, the client, the report) are a library, so the
# tests can check them without running the program.
add_library(matchforge_botlib STATIC
  BotOptions.cpp
  BotClient.cpp
  BotReport.cpp)
target_include_directories(matchforge_botlib PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(matchforge_botlib
  PUBLIC
    matchforge_proto     # the messages
    matchforge_framing   # length-prefixed frames
    Boost::asio          # sockets
    Threads::Threads
  PRIVATE
    matchforge_warnings)

add_executable(matchforge_bots main.cpp)
target_link_libraries(matchforge_bots
  PRIVATE
    matchforge_botlib
    matchforge_core      # version()
    spdlog::spdlog       # logging the summary
    matchforge_warnings)
