# Decisions
## D-001: Build environment is the NVIDIA-Workbench WSL distro (Ubuntu 22.04)
- Date: 2026-10-12
- Decision: Build and test in WSL distro NVIDIA-Workbench (Ubuntu 22.04.4, user workbench). It has g++ 11.4, cmake 3.22.1, ninja 1.10.1, clang-format 14, gh 2.4, and vcpkg at ~/vcpkg.
- Reason: The user installed the toolchain there. The "Ubuntu" distro (24.04) has none of it.
- Rejected alternatives: Installing tools in the "Ubuntu" distro (duplicates work); building on Windows (CLAUDE.md requires WSL2).
- Consequence: Presets need CMake >= 3.21 (3.22 is fine). If a dependency needs a newer GCC, revisit.
## D-002: Repo location and branch
- Date: 2026-10-12
- Decision: Clone at ~/src/matchforge (WSL home, not /mnt/c). Work on branch milestone-0-setup, one Conventional Commit per task, PR at milestone close.
- Reason: The Linux filesystem is much faster for builds. The branch keeps main clean.
- Rejected alternatives: Committing straight to main.
## D-003: Planning files pushed before code
- Date: 2026-10-12
- Decision: CLAUDE.md and .workflow/ are committed and pushed to milestone-0-setup before any code.
- Reason: User request; gives a reviewable record of the plan.
## D-004: 4-space indent (ACCEPTED 2026-10-12)
- Decision: .clang-format and .editorconfig use 4 spaces.
- Reason: Readability in a multi-module project. Google default is 2.
- Rejected alternatives: Google default of 2 spaces.
## D-005: Pin vcpkg builtin-baseline (ACCEPTED 2026-10-12)
- Decision: vcpkg.json sets a builtin-baseline commit so every build uses the same library versions.
- Reason: Reproducible builds and effective binary caching in CI.
- Rejected alternatives: Floating latest versions.
## D-006: Strict warnings; ASan/UBSan in the debug preset (ACCEPTED 2026-10-12)
- Decision: -Wall -Wextra -Werror everywhere. Debug preset also enables AddressSanitizer and UndefinedBehaviorSanitizer.
- Reason: Catch bugs early; standard C++ practice.
- Rejected alternatives: Default warnings only.
## D-007: .gitkeep in empty folders; pinned Docker base images (ACCEPTED 2026-10-12)
- Decision: Empty layout folders get a .gitkeep file. Dockerfile uses pinned versions (ubuntu:24.04, debian:bookworm-slim), never latest.
- Reason: Git does not track empty folders; "latest" tags make builds non-reproducible.
- Rejected alternatives: Leaving folders empty; using latest tags.
## D-009: Call protoc directly instead of protobuf_generate (2026-10-12)
- Decision: proto/CMakeLists.txt uses add_custom_command to run protoc. It does not use the protobuf_generate helper.
- Reason: The helper's TARGET form needs CMake 3.23. This machine has CMake 3.22.1.
- Rejected alternatives: Upgrading CMake (extra install, and the user did not ask for it).
## D-011 (ACCEPTED 2026-10-13): AC-5 means both containers start
- Decision: the server container starts, logs its version, and exits 0. Redis stays running and healthy. This is how AC-5 is read.
- Reason: AC-3 requires the server to exit after logging. A long-running server belongs to a later milestone.
- User decision: accepted by the user on 2026-10-13.
## D-014 (ACCEPTED 2026-10-13): Second test for Protobuf round-trip
- Decision: tests/proto/EnvelopeTest.cpp adds a test that sets a Heartbeat in an Envelope, serializes it, parses it back, and checks the payload and timestamp. AC-4 now means two tests.
- Reason: Compiling the generated code does not prove it works at runtime. This test does.
- User decision: accepted by the user on 2026-10-13.
- Rejected alternatives: keeping one test (does not prove the Protobuf code runs).
## D-010 (REVISED 2026-10-13): Runtime image is debian:bookworm-slim, as in D-007
- Decision: The build stage uses ubuntu:24.04. The runtime stage uses debian:bookworm-slim.
- Reason: First I believed bookworm's older libstdc++ might not run the GCC 13 binary. I tested it: the binary built in the image runs on debian:bookworm-slim and exits 0. The server also runs as the non-root user matchforge there. Bookworm is smaller and matches the option accepted in D-007, so no deviation is needed.
- Risk to watch: the binary is built with GCC 13 and runs on bookworm's GCC 12 libstdc++. This works today. If a later milestone breaks it, switch the runtime stage to a newer Debian or Ubuntu image.
- Rejected alternatives: ubuntu:24.04 for runtime (works, but larger than needed).
## D-011 (INTERPRETATION, needs your OK): AC-5 means both containers start
- Decision: compose sets restart: "no" on the server. The server prints its version and exits, so its container ends with exit 0. Redis stays running and healthy.
- Reason: AC-3 requires the server to exit after logging. "Starts both containers" can only mean the server starts and exits cleanly. A long-running server arrives in a later milestone.
## D-012: Strict warnings are an INTERFACE target (2026-10-12)
- Decision: matchforge_warnings carries -Wall -Wextra -Wpedantic -Werror. Our targets link to it. Generated protobuf code is excluded.
- Reason: One place to change the rules; generated code is outside our control.
## D-013: Fix for libsodium build (2026-10-12)
- Decision: autoconf-archive is added to the apt installs in CI and the Dockerfile. It was missing and libsodium's build fails without it.
## D-008: Spec and plan approved (2026-10-12)
- Decision: SPEC.md (7 acceptance criteria) and PLAN.md (4 phases) approved as written. Phase 1 may begin.
- Reason: User review, including the plain-English summary.
