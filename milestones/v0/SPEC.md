# Spec: matchforge v0 - project setup

## In plain English
matchforge is a program that runs on a server. Players connect to it over the internet, log in, and join a queue to play a game. The server looks at everyone waiting and groups players of similar skill together. Skill is measured with an Elo rating, the same number system chess uses: you gain points when you win and lose points when you lose, and beating a stronger player gives you more points.

This milestone does not build any of that yet. It builds the empty framework that the later work will sit on: the folders, the build settings, two programs that start and print their version, one test, and the setup for running everything in Docker. The goal is that when you type the build command, everything compiles, the test passes, and both programs run.

Milestone 0 creates these parts:
- **server**: the main program. For now it only prints its version and quits.
- **bots**: a second program that will later pretend to be many players, to load-test the server. For now it only prints its version and quits.
- **core**: the code for matching players, kept free of networking so it can be tested alone. Empty for now.
- **proto**: the file that describes the messages players send and receive. It contains a "heartbeat" message (a "still alive" ping) and an "error" message.
- **tests**: two tests. One checks the version. The other sends a message through Protobuf and back, to prove the generated code works.
- **Docker setup**: a way to package the server and start it next to a Redis database (a fast storage system used for player accounts later).

If you are new to these terms, see the glossary at the end of PLAN.md.

## Goal
A buildable, tested, containerised C++20 skeleton for the matchforge matchmaking server. No game or networking logic yet.
## Users and main flows
- The author builds, tests, and runs the skeleton locally in WSL2 and in Docker.
## In scope
- Folder layout from CLAUDE.md; vcpkg manifest; CMake targets; CMakePresets (debug, release); proto package matchforge.v1 with Envelope (Heartbeat, ErrorResponse); server and bots mains that log the version; one GoogleTest test; .clang-format, .gitignore, .editorconfig; CI workflow; Dockerfile and compose (server + Redis); README stub.
## Non-goals
- Protobuf framing on the wire, Asio networking, Redis calls, matchmaking/Elo logic, Prometheus/Grafana (Milestone 9).
## Acceptance criteria
All seven were met on 2026-10-13. Evidence is in `milestones/v0/EVIDENCE.md`.
- [x] AC-1: `cmake --preset debug && cmake --build --preset debug && ctest --preset debug` passes in WSL2 (NVIDIA-Workbench distro)
- [x] AC-2: `cmake --preset release` configures and builds
- [x] AC-3: `matchforge_server` and `matchforge_bots` each print the version via spdlog and exit 0
- [x] AC-4: Two GoogleTest tests (version check and Protobuf round-trip) are discovered and pass under ctest (D-014 changed this from one test to two)
- [x] AC-5: `docker compose -f deploy/docker-compose.yml up --build` builds and starts the server and Redis containers. The server container exits 0 after logging, as D-011 defines.
- [x] AC-6: `.github/workflows/ci.yml` validates with `actionlint`
- [x] AC-7: Sources are formatted: `clang-format --dry-run --Werror` passes
## Assumptions (see DECISIONS.md)
- D-001, D-002, D-003, D-011, D-014
