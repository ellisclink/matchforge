# Spec: matchforge v0 - project setup
## Goal
A buildable, tested, containerised C++20 skeleton for the matchforge matchmaking server. No game or networking logic yet.
## Users and main flows
- The author builds, tests, and runs the skeleton locally in WSL2 and in Docker.
## In scope
- Folder layout from CLAUDE.md; vcpkg manifest; CMake targets; CMakePresets (debug, release); proto package matchforge.v1 with Envelope (Heartbeat, ErrorResponse); server and bots mains that log the version; one GoogleTest test; .clang-format, .gitignore, .editorconfig; CI workflow; Dockerfile and compose (server + Redis); README stub.
## Non-goals
- Protobuf framing on the wire, Asio networking, Redis calls, matchmaking/Elo logic, Prometheus/Grafana (Milestone 9).
## Acceptance criteria
- [ ] AC-1: `cmake --preset debug && cmake --build --preset debug && ctest --preset debug` passes in WSL2 (NVIDIA-Workbench distro)
- [ ] AC-2: `cmake --preset release` configures and builds
- [ ] AC-3: `matchforge_server` and `matchforge_bots` each print the version via spdlog and exit 0
- [ ] AC-4: One GoogleTest test is discovered and passes under ctest
- [ ] AC-5: `docker compose -f deploy/docker-compose.yml up --build` builds and starts the server and Redis containers
- [ ] AC-6: `.github/workflows/ci.yml` validates with `actionlint`
- [ ] AC-7: Sources are formatted: `clang-format --dry-run --Werror` passes
## Assumptions (see DECISIONS.md)
- D-001, D-002, D-003
