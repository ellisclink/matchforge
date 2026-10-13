# matchforge_net: the TCP layer. It turns a byte stream into messages, owns each
# connection (a Session), and accepts new connections (a Server).
#
# It depends on Boost.Asio for sockets, on matchforge_proto for the message types,
# and on matchforge_auth for register and login. It does not depend on matchforge_core
# or on storage directly.

add_library(matchforge_net STATIC
  Framing.cpp
  Session.cpp
  Server.cpp)

# PUBLIC: anything that uses the network layer gets these include paths and libraries.
target_include_directories(matchforge_net PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(matchforge_net
  PUBLIC
    Boost::asio          # sockets, timers, io_context
    matchforge_proto     # Envelope and the other message types
    matchforge_auth      # register and login
    matchforge_lobby     # the matchmaking queue
    Threads::Threads     # pthreads, needed by Asio on Linux
  PRIVATE
    matchforge_warnings) # our strict warning flags
