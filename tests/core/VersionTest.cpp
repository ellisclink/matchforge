// VersionTest.cpp: tests for src/core/Version.cpp.
#include <gtest/gtest.h>  // GoogleTest: provides TEST(), EXPECT_EQ, EXPECT_FALSE and so on.

#include <string_view>

#include "core/Version.h"  // The code under test.

// The one Milestone 0 test. It proves the test pipeline works end to end:
// CMake builds the test, GoogleTest runs it, and ctest reports it.
// TEST(SuiteName, TestName) defines one test. The name shows up in ctest's output.
TEST(VersionTest, ReportsTheVersionCMakeConfigured) {
    // EXPECT_* records a failure but keeps running, so one run can show several problems.
    // Checks that version() did not return an empty string.
    EXPECT_FALSE(matchforge::core::version().empty());

    // Checks that version() returns exactly the value CMake defined in MATCHFORGE_VERSION.
    // The std::string_view{...} wrapper makes both sides the same type for the comparison.
    EXPECT_EQ(matchforge::core::version(), std::string_view{MATCHFORGE_VERSION});
}
