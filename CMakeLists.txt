# Top-level build script for matchforge. CMake reads this file first, then the
# CMakeLists.txt files in each subfolder (through add_subdirectory below).
#
# You do not run this file directly. Run a preset instead, for example:
#   cmake --preset debug
cmake_minimum_required(VERSION 3.21)  # Refuse to run on CMake older than 3.21 (the presets need it).

# Declares the project name, its version (used by the code in src/core), a
# one-line description, and the language. LANGUAGES CXX means "C++ only", so
# CMake does not look for a C compiler.
project(matchforge
  VERSION 0.1.0
  DESCRIPTION "Multiplayer lobby and Elo matchmaking server in C++"
  LANGUAGES CXX)

# Use C++20 for every target. EXTENSIONS OFF turns off compiler-specific extras
# (-std=gnu++20 becomes -std=c++20), so the code is standard C++ and portable.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # Fail the build instead of quietly falling back to an older standard.
set(CMAKE_CXX_EXTENSIONS OFF)

# Writes build/<preset>/compile_commands.json, a list of every compile command.
# Editors (clangd, VS Code) and clang-tidy read it to understand the code.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# A user-settable switch, stored in the build cache. The debug preset turns it
# on. OFF by default, so release builds are not slowed down by the checks.
option(MATCHFORGE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# A "library" with no source files. Its only job is to carry compiler flags:
# any target that links to matchforge_warnings gets these flags applied to it.
#   -Wall -Wextra -Wpedantic   turn on the most useful compiler warnings
#   -Werror                    treat every warning as an error, so it cannot be ignored
add_library(matchforge_warnings INTERFACE)
target_compile_options(matchforge_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)

# Debug-only memory and bug checks. These are slower, so they are opt-in.
#   -fsanitize=address         catches out-of-bounds reads/writes and use-after-free at runtime
#   -fsanitize=undefined       catches undefined behavior such as signed overflow
#   -fno-omit-frame-pointer    keeps stack traces readable when a sanitizer reports a bug
# The -fsanitize flag must also be given at link time, so it is set as a link option too.
if(MATCHFORGE_SANITIZERS)
  target_compile_options(matchforge_warnings INTERFACE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(matchforge_warnings INTERFACE -fsanitize=address,undefined)
endif()

# Finds the libraries vcpkg installed (listed in vcpkg.json). CONFIG means CMake
# reads the config file each library ships with. REQUIRED stops the configure
# step with an error if one is missing.
find_package(spdlog CONFIG REQUIRED)  # logging, used by the server and bots
find_package(GTest CONFIG REQUIRED)   # the test framework, used by tests/

# Makes the ctest command available. Without this, the test targets exist but
# ctest finds nothing to run.
enable_testing()

# Each add_subdirectory reads that folder's CMakeLists.txt and defines the
# targets in it. Order does not matter here, because CMake resolves
# dependencies between targets after reading every file.
add_subdirectory(proto)        # matchforge_proto: generated Protobuf code
add_subdirectory(src/core)     # matchforge_core: shared logic
add_subdirectory(src/server)   # matchforge_server: the server program
add_subdirectory(src/bots)     # matchforge_bots: the load-test program
add_subdirectory(tests)        # matchforge_tests: the GoogleTest suite
