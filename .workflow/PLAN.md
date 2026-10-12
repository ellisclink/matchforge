# Plan: v0 - project setup
## Phase 1: toolchain check and skeleton
Runs: cmake configures an empty project in ~/src/matchforge
### Task 1.1: Verify toolchain and vcpkg in NVIDIA-Workbench distro
- Serves: AC-1, AC-2
- Files: none
- Depends on: none
- Wave: 1
- Verify: `cmake --version; ninja --version; $VCPKG_ROOT/vcpkg version`
- Status: done (checked 2026-10-12)
### Task 1.2: Folder layout, .gitignore, .editorconfig, .clang-format, .gitkeep
- Serves: AC-7
- Files: layout dirs, .gitignore, .editorconfig, .clang-format
- Depends on: 1.1
- Wave: 1
- Verify: `clang-format --version; find . -maxdepth 2 -type d | sort`
- Status: todo
## Phase 2: code and targets
Runs: `cmake --build --preset debug` builds all five targets
### Task 2.1: vcpkg.json with 8 deps and builtin-baseline
- Serves: AC-1
- Files: vcpkg.json
- Depends on: 1.2
- Wave: 2
- Verify: `$VCPKG_ROOT/vcpkg x-update-baseline --dry-run` shows baseline set
- Status: todo
### Task 2.2: proto/matchforge.proto and matchforge_proto library
- Serves: AC-1
- Files: proto/matchforge.proto, CMakeLists.txt
- Depends on: 2.1
- Wave: 3
- Verify: `cmake --build --preset debug --target matchforge_proto`
- Status: todo
### Task 2.3: matchforge_core library and server/bots mains
- Serves: AC-1, AC-3
- Files: src/core/*, src/server/main.cpp, src/bots/main.cpp, CMakeLists.txt
- Depends on: 2.1
- Wave: 3
- Verify: `./build/debug/matchforge_server` prints version, exit 0
- Status: todo
### Task 2.4: CMakeLists.txt and CMakePresets.json
- Serves: AC-1, AC-2
- Files: CMakeLists.txt, CMakePresets.json
- Depends on: 2.2, 2.3
- Wave: 4
- Verify: `cmake --preset debug && cmake --preset release`
- Status: todo
### Task 2.5: matchforge_tests with one GoogleTest, registered with ctest
- Serves: AC-4
- Files: tests/CMakeLists.txt, tests/core/*.cpp
- Depends on: 2.4
- Wave: 5
- Verify: `ctest --preset debug --output-on-failure`
- Status: todo
## Phase 3: verify locally
Runs: debug and release builds and tests pass in WSL
### Task 3.1: Build and test both presets
- Serves: AC-1, AC-2, AC-4
- Files: .workflow/EVIDENCE.md
- Depends on: 2.5
- Wave: 6
- Verify: `cmake --build --preset debug && ctest --preset debug && cmake --build --preset release`
- Status: todo
## Phase 4: CI and containers
Runs: CI YAML valid; compose builds and starts
### Task 4.1: .github/workflows/ci.yml with vcpkg binary caching
- Serves: AC-6
- Files: .github/workflows/ci.yml
- Depends on: 3.1
- Wave: 7
- Verify: `actionlint .github/workflows/ci.yml`
- Status: todo
### Task 4.2: deploy/Dockerfile (multi-stage) and deploy/docker-compose.yml
- Serves: AC-5
- Files: deploy/Dockerfile, deploy/docker-compose.yml
- Depends on: 3.1
- Wave: 7
- Verify: `docker compose -f deploy/docker-compose.yml up --build -d && docker compose -f deploy/docker-compose.yml ps`
- Status: todo
### Task 4.3: README stub
- Serves: none (docs)
- Files: README.md
- Depends on: 3.1
- Wave: 8
- Verify: README shows name, description, build commands
- Status: todo
