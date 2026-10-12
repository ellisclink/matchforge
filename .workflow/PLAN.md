# Plan: v0 - project setup

Each phase below starts with a plain-English note on what you will see when it is done.

## Glossary (plain words for the tools)
- **C++**: the programming language. It is faster than Python or JavaScript and makes you manage memory yourself, which is why the rules say to use `unique_ptr` and RAII.
- **WSL2 (Windows Subsystem for Linux)**: a Linux system running inside Windows. We build the project there because the tools work best on Linux.
- **Ubuntu**: a popular Linux system. The "NVIDIA-Workbench" distro is the Ubuntu install where you put the tools.
- **Terminal / shell**: the text window where you type commands.
- **Repo (repository)**: the project folder plus its full change history. Yours is on GitHub.
- **Git and GitHub**: Git records changes. GitHub stores the repo online. A **branch** is a separate copy of the work so `main` stays safe. A **commit** is one saved set of changes. A **push** uploads commits to GitHub.
- **CMake**: a tool that reads a description of the project and makes the build instructions for the compiler. You never write those instructions by hand.
- **Preset (CMakePresets.json)**: a saved set of CMake settings with a name, such as `debug`, so you can type `--preset debug` instead of long flags.
- **Target**: one thing CMake builds, such as a program or a library. A **library** is code other programs use. An **executable** is a program you run.
- **Build**: turning source code into a runnable program. **Debug** builds include extra checks and are easier to debug. **Release** builds are optimised for speed.
- **vcpkg**: a package manager. It downloads and builds the outside libraries we need (listed in `vcpkg.json`). **Manifest mode** means the list lives in a file in the project, so anyone who clones the repo gets the same libraries.
- **Boost.Asio**: a library for networking: accepting connections and sending data without waiting on each one.
- **Protobuf (Protocol Buffers)**: a format for describing messages in a `.proto` file. A tool turns that file into C++ code, so the server and players agree on message shapes. This is called **code generation**.
- **Envelope with a oneof**: one wrapper message that can hold exactly one of several message types. The server reads the wrapper, then knows which message it got.
- **Redis**: a very fast database that keeps data in memory. Used later for accounts and ratings.
- **GoogleTest and ctest**: GoogleTest is the library for writing tests. ctest is the tool that runs all the tests at once.
- **spdlog**: a logging library. It prints messages like "server started" with timestamps.
- **nlohmann-json**: reads and writes JSON config files.
- **libsodium**: a security library. Used later to scramble passwords so they are never stored in plain text.
- **prometheus-cpp**: exports numbers (like players online) for monitoring. Used from Milestone 9.
- **clang-format**: a tool that automatically tidies code layout so it looks the same everywhere.
- **Docker, Docker Compose**: Docker packages a program with everything it needs into a **container**. Compose starts several containers together from one file.
- **Multi-stage Dockerfile**: builds the program in one large image, then copies only the finished program into a small image to run. This keeps the final image small and free of build tools.
- **CI (GitHub Actions)**: GitHub automatically builds and tests the project each time you push, so breakages show up quickly.

## Phase 1: toolchain check and skeleton
Plain English: the folders and settings files are created. Nothing is compiled yet.
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
- Status: done (checked 2026-10-12, see EVIDENCE.md)
## Phase 2: code and targets
Plain English: the actual code files are written and the build is taught about them. At the end, one command builds the server, the bots, the core library, the message definitions, and the tests.
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
Plain English: we build the project in both modes and run the test. If it all passes, the foundation is proven to work on your machine.
Runs: debug and release builds and tests pass in WSL
### Task 3.1: Build and test both presets
- Serves: AC-1, AC-2, AC-4
- Files: .workflow/EVIDENCE.md
- Depends on: 2.5
- Wave: 6
- Verify: `cmake --build --preset debug && ctest --preset debug && cmake --build --preset release`
- Status: todo
## Phase 4: CI and containers
Plain English: GitHub will build and test the project on every push, and Docker will package the server with Redis so it can be started with one command.
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
