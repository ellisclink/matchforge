#matchforge_tests : GoogleTest suites.Each.cpp file under tests / mirrors a
#folder under src /.Every test is registered with ctest, so `ctest` runs them.

#Loads CMake's GoogleTest helper, which provides gtest_discover_tests() below.
include(GoogleTest)

#The test program.It is built from every test source file in the list.
add_executable(matchforge_tests
  core/VersionTest.cpp
  proto/EnvelopeTest.cpp   # Proves the generated Protobuf code works at runtime, not only that it compiles
  net/FramingTest.cpp      # Unit tests for the frame encoder and decoder (no sockets)
  net/ServerTest.cpp)      # Integration tests: a real server on loopback and a client socket

#Libraries the tests need.
#matchforge_core the code under test(version())
#matchforge_proto the generated Envelope / Heartbeat classes under test
#matchforge_net the framing, session and server code under test
#Boost::asio, Threads the sockets and threads the integration tests use
#GTest::gtest_main GoogleTest plus a ready - made main(), so each test file needs no main()
#matchforge_warnings the same strict warning flags as the real code
target_link_libraries(matchforge_tests
  PRIVATE
    matchforge_core
    matchforge_proto
    matchforge_net
    Boost::asio
    Threads::Threads
    GTest::gtest_main
    matchforge_warnings)

#After the program is built, CMake runs it with-- gtest_list_tests, finds every
#TEST() name, and registers each one with ctest.So a single `ctest` run covers all tests.
gtest_discover_tests(matchforge_tests)
