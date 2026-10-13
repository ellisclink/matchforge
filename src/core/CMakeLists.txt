# matchforge_core: game and matchmaking logic. It has no networking and no
# Redis, so its tests can run without any servers.

# A static library (a .a file) built from Version.cpp. Other targets link to it
# and get its compiled code.
add_library(matchforge_core STATIC
  Version.cpp
  Validation.cpp)  # username and password rules

# PUBLIC: anything that links matchforge_core gets this include folder too.
# Adding src/ here lets code write #include "core/Version.h" from any folder.
target_include_directories(matchforge_core PUBLIC "${PROJECT_SOURCE_DIR}/src")

# Defines a preprocessor macro MATCHFORGE_VERSION, set to the project version
# (for example "0.1.0"), before the compiler reads any source file. Version.cpp
# uses it. PUBLIC makes the macro visible to the tests as well.
target_compile_definitions(matchforge_core PUBLIC MATCHFORGE_VERSION="${PROJECT_VERSION}")

# Applies the strict warning flags (from the top-level file) to this library only.
# PRIVATE: the flags are used to compile this library but are not passed on to
# the programs that use it.
target_link_libraries(matchforge_core PRIVATE matchforge_warnings)
