# Milestone v0 summary: project setup (closed 2026-10-13)

## What was built
- A C++20 project built with CMake presets (`debug`, `release`) and vcpkg in manifest mode with a pinned baseline.
- `matchforge_proto`: generated Protobuf code from `proto/matchforge.proto` (package `matchforge.v1`, `Envelope` with a oneof of `Heartbeat` and `ErrorResponse`).
- `matchforge_core`: `version()`, with no networking.
- `matchforge_server` and `matchforge_bots`: print the version via spdlog and exit 0.
- `matchforge_tests`: two GoogleTests, both passing under ctest.
- CI on GitHub Actions: formatting check, then configure, build, and test for both presets. Dependabot for Actions and Docker images.
- Docker: multi-stage build (Ubuntu 24.04 build stage, Debian bookworm-slim runtime), and Compose with Redis (health-checked) and the server.

## Decisions made (see DECISIONS.md)
- D-001 to D-003: WSL distro "NVIDIA-Workbench", repo location, planning files first.
- D-004 to D-008: 4-space indent, pinned vcpkg baseline, strict warnings and sanitizers, .gitkeep and pinned images, spec approval.
- D-009: protoc is called directly because CMake 3.22 has no protobuf_generate(TARGET).
- D-010: runtime image is debian:bookworm-slim, tested with the binary.
- D-011: AC-5 means the server starts, logs, and exits 0 while Redis stays up. Accepted.
- D-012: strict warnings are an INTERFACE target.
- D-013: autoconf-archive added to CI and Docker for libsodium.
- D-014: AC-4 means two tests, including a Protobuf round-trip. Accepted.

## Verification (see EVIDENCE.md)
- AC-1 to AC-4: debug and release build, ctest passes 2 of 2 on both presets, after a clean rebuild.
- AC-3: both programs print the version and exit 0.
- AC-5: full --no-cache Docker build, then Compose; Redis healthy, server exits 0.
- AC-6: actionlint passes on the workflow.
- AC-7: clang-format 14.0.6 passes on every .cpp and .h file.
- CI: latest run is green on both presets, including the formatting step.

## Known gaps carried forward (see MILESTONES.md and BACKLOG.md)
- The server does not use Redis or listen on the network yet.
- The bot program does nothing yet.
- Local clang-format (14.0.0) and CI's (14.0.6) differ by a patch version.
- The main branch has no protection rules yet.

## Notes
- The milestone's branch is `milestone-0-setup`. It has not been merged to main. A PR is the next step, if you want one.
