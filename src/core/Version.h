// Version.h: declares the one function that reports the project version.
// Only the declaration goes here. The code is in Version.cpp.
#pragma once  // Include this file once per compile, even if several files include it.

#include <string_view>  // A lightweight read-only view of a string. It does not copy the text.

namespace matchforge::core {  // Keeps our names separate from other libraries' names.

// Returns the project version, for example "0.1.0". The value comes from
// project() in the top-level CMakeLists.txt, so it is set at build time.
// The caller gets a view of a string that lives for the whole program, so it
// is safe to keep the result.
std::string_view version();

}  // namespace matchforge::core
