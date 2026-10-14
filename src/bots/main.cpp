// main.cpp for the bots: the program's entry point. The operating system
// calls main() first when the program starts.
#include <spdlog/spdlog.h>  // Logging library. Provides spdlog::info and friends.

#include "core/Version.h"  // Gives us matchforge::core::version().

// Milestone 0: the bot simulator only reports its version and exits. Fake
// players that load-test the server are added in a later milestone.
int main() {
    // Writes one log line with the version. The {} is replaced by the version string.
    spdlog::info("matchforge bots {}", matchforge::core::version());

    // 0 means success. A non-zero value would tell the caller that something went wrong.
    return 0;
}
