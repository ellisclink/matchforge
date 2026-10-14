// main.cpp for the server: the program's entry point. The operating system
// calls main() first when the program starts.
#include <spdlog/spdlog.h>  // Logging library. Provides spdlog::info and friends.

#include "core/Version.h"  // Gives us matchforge::core::version().

// Milestone 0: the server only reports its version and exits. Networking and
// the accept loop arrive in later milestones.
int main() {
    // Writes one line like "[2026-10-12 20:00:00.000] [info] matchforge server 0.1.0".
    // The {} is a placeholder that spdlog fills with the version string.
    spdlog::info("matchforge server {}", matchforge::core::version());

    // 0 tells the operating system the program finished successfully.
    // Docker and CI read this to decide whether the run passed.
    return 0;
}
