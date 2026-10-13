// Version.cpp: the code behind version() declared in Version.h.
#include "core/Version.h"  // Pulls in the declaration so the compiler checks this definition matches it.

namespace matchforge::core {

std::string_view version() {
    // MATCHFORGE_VERSION is a macro that CMake defines on the matchforge_core
    // target (see src/core/CMakeLists.txt). The compiler replaces it with the
    // string literal, for example "0.1.0", before compiling this line.
    return MATCHFORGE_VERSION;
}

}  // namespace matchforge::core
