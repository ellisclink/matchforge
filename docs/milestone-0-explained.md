# Milestone 0 explained

This document explains what was built in Milestone 0 (project setup), how the pieces work together, why they were built that way, and questions an interviewer might ask. It is written for someone who has done simple coding projects before.

---

## 1. What was built

The project is a skeleton. Nothing plays a game yet. What exists is the structure that all later work sits on.

| Piece | Plain-language role |
|---|---|
| `CMakeLists.txt` (top level) | The recipe for building the whole project. CMake reads it first. |
| `CMakePresets.json` | Named shortcuts for the build settings: `debug` and `release`. |
| `vcpkg.json` | The shopping list of outside libraries, with a fixed version. |
| `proto/matchforge.proto` | The shape of every message the server and players will send. |
| `src/core/` | Game logic. No networking, so it can be tested alone. Right now it only holds `version()`. |
| `src/server/` | The server program. Prints its version and exits. |
| `src/bots/` | The load-test program. Prints its version and exits. |
| `tests/` | One test that checks the version. It proves the test system works. |
| `.github/workflows/ci.yml` | A robot that builds and tests on GitHub every time you push. |
| `deploy/Dockerfile` | A recipe for packaging the server into a container image. |
| `deploy/docker-compose.yml` | Starts the server and Redis together. |

---

## 2. How it works

### 2.1 The build, from start to finish

When you run `cmake --preset debug`, this happens in order:

1. **CMake reads `CMakePresets.json`.** The `debug` preset says: use Ninja as the build tool, use the vcpkg toolchain, put build files in `build/debug`, turn on sanitizers.
2. **The vcpkg toolchain runs (manifest mode).** vcpkg reads `vcpkg.json`, downloads each library at the pinned version, builds it, and installs it into `vcpkg_installed/`. Because the list lives in a file in the project, anyone who clones the repo gets the same libraries.
3. **CMake reads `CMakeLists.txt`,** which calls `find_package` for spdlog and GoogleTest (so their targets exist), then calls `add_subdirectory` for each folder.
4. **CMake generates the build files** (`build/debug/build.ninja`). Nothing is compiled yet. CMake only writes the instructions.

When you run `cmake --build --preset debug`, Ninja runs those instructions, which compile and link the programs.

### 2.2 CMake targets

A **target** is one thing CMake builds. Milestone 0 has these:

| Target | Kind | What it is |
|---|---|---|
| `matchforge_warnings` | INTERFACE library | Has no code. It carries the compiler flags (`-Wall -Wextra -Werror`, sanitizers). Other targets link to it to get the flags. |
| `matchforge_proto` | Static library | The C++ code generated from `matchforge.proto`. |
| `matchforge_core` | Static library | `Version.cpp`, the shared logic. |
| `matchforge_server` | Executable | The server program. It links to `matchforge_core` and `spdlog`. |
| `matchforge_bots` | Executable | The bot program. Same links as the server. |
| `matchforge_tests` | Executable | The test program. It links to `matchforge_core` and GoogleTest's `gtest_main`. |

Key ideas:

- **Static library** (`.a` file): code that gets copied into the program that uses it at link time.
- **`PRIVATE`, `PUBLIC`, `INTERFACE`** control what gets passed on. `PRIVATE` means "I need this to build myself". `PUBLIC` means "I need this, and so does anyone who uses me". `INTERFACE` means "anyone who uses me needs this, but I do not". For example, `matchforge_core` links `matchforge_warnings` as `PRIVATE`, so the warning flags apply to core but are not forced on the server.
- **Dependencies between targets** are how CMake knows the order. `matchforge_server` depends on `matchforge_core`, so core is built first.

### 2.3 vcpkg manifest mode

vcpkg is a package manager for C++. C++ has no built-in package manager, so libraries are usually a headache to install.

- **Classic mode** means you install libraries by hand on each machine.
- **Manifest mode** means the project file (`vcpkg.json`) lists the libraries. CMake's vcpkg toolchain installs them automatically during configure.
- **`builtin-baseline`** pins the vcpkg registry to one commit. Without it, two people building on different days can get different library versions and get different bugs. This is the single line that makes builds repeatable.

### 2.4 How Protobuf code generation fits into the build

Protobuf (Protocol Buffers) solves this problem: the server and players must agree on message layouts. Instead of writing parsers by hand, you describe messages once in a `.proto` file.

The flow in this project:

1. You write `proto/matchforge.proto` (the description).
2. The build runs `protoc` (the Protobuf compiler, which vcpkg installed) on it. This is the rule `add_custom_command` in `proto/CMakeLists.txt`.
3. `protoc` writes `matchforge.pb.h` and `matchforge.pb.cc` into `build/debug/proto/generated/`.
4. Those files are compiled into `matchforge_proto`.
5. Any target that links `matchforge_proto` can `#include "matchforge.pb.h"`.

Why the generated files live in the build folder: they are never edited by hand, so they should not be in the source tree or in git.

Why `add_custom_command` instead of a helper: newer CMake has `protobuf_generate(TARGET ...)`, but it needs CMake 3.23. This machine has 3.22. The custom command works on 3.21 and later.

### 2.5 Tests

- `TEST(VersionTest, ...)` defines one test. GoogleTest provides the macro.
- `gtest_discover_tests` runs after the build, asks the test program for its list of test names, and registers each one with CTest.
- `ctest --preset debug` runs every registered test and reports pass or fail.

### 2.6 Why the Dockerfile has two stages

A Docker image is a packaged filesystem that can run anywhere. Building the server needs compilers, CMake, Ninja, and vcpkg, which add hundreds of megabytes. The server itself needs only a few libraries.

A **multi-stage Dockerfile** has two `FROM` lines:

- **Stage 1 (`build`)** starts from `ubuntu:24.04`, installs all the build tools, and builds the server.
- **Stage 2 (`runtime`)** starts from `debian:bookworm-slim`, which is small. It copies only the finished binary from stage 1 with `COPY --from=build`.

The final image has no compiler, no source code, and no build cache. It is smaller and has less to attack.

Two more details:

- The vcpkg install runs in its own layer before the source is copied. Docker caches each layer. When only your code changes, Docker reuses the slow Boost build from cache.
- The server runs as a non-root user (`matchforge`), so a bug cannot change system files.

### 2.7 Docker Compose

`docker-compose.yml` describes two services:

- `redis` uses a fixed image version (`redis:7.4-alpine`) and reports healthy when `redis-cli ping` works.
- `server` waits for Redis to be healthy (`depends_on ... condition: service_healthy`) before starting.

`restart: "no"` means Docker does not restart the server when it exits. Milestone 0's server exits on purpose, so restarting would loop forever.

### 2.8 CI

`.github/workflows/ci.yml` runs on GitHub's Linux machines:

1. Installs build tools.
2. Clones vcpkg at the same pinned commit.
3. Restores cached library builds, keyed on `vcpkg.json`.
4. Runs configure, build, and test for `debug` and `release`, in parallel.

The workflow has `permissions: contents: read`, so a compromised step cannot write to the repo.

---

## 3. Design choices and alternatives

| Choice | Why | Alternatives considered |
|---|---|---|
| CMake + presets | Standard for C++. Presets remove repeated flags. | Make (harder to read); Bazel (heavier to learn). |
| vcpkg manifest mode with a pinned baseline | Same libraries everywhere, no manual installs. | Conan (also good; vcpkg matches the spec); system packages (differ by distro). |
| Protobuf | Compact binary messages, generated parsers, schema evolution. | JSON (readable but larger and slower); hand-written binary (error-prone). |
| `core` has no networking | Logic can be unit-tested without starting servers. | Mixing logic with networking (harder to test). |
| Strict warnings with `-Werror` | Catches bugs at compile time. | Warnings only (warnings get ignored). |
| ASan and UBSan in debug only | Find memory bugs early without slowing release builds. | Always on (slow); never on (bugs hide). |
| Multi-stage Docker build | Small, safer runtime image. | Single stage (large image with compilers). |
| Docker Compose | One command starts several containers. | Manual `docker run` (more steps to remember). |
| Non-root container user | Limits damage from a bug. | Running as root (simpler but unsafe). |

Things that were checked and not chosen:

- **`protobuf_generate` helper:** not available on CMake 3.22.
- **Debian bookworm vs Ubuntu runtime image:** both run the binary (tested). Debian is smaller.

---

## 4. Interview questions

**Q: What is the difference between a static library and an executable in CMake?**
A static library is a `.a` file of compiled code. It is not runnable on its own. An executable is a program you can run. In this project, `matchforge_core` and `matchforge_proto` are static libraries, and `matchforge_server` is an executable that links them.

**Q: Why does `builtin-baseline` matter?**
It pins the vcpkg registry to one commit, so every machine and every CI run uses the same library versions. Without it, a rebuild next month could pull in a new Boost and break the code. It also makes the binary cache work, because the cache key depends on the exact versions.

**Q: Why are the generated Protobuf files in the build folder?**
They are output of a build step, like object files. Generated files should be rebuilt from the `.proto` source, never edited, and never committed. Keeping them in the build folder means `git status` stays clean.

**Q: Why use two Docker stages?**
The build tools are large and only needed to compile. Copying only the finished binary into a slim image gives a smaller image with no compiler inside, which means less to attack and faster pulls.

**Q: What does `PRIVATE` vs `PUBLIC` mean in `target_link_libraries`?**
`PRIVATE` means the library is needed only to build this target. `PUBLIC` means anything that links this target also gets the library. `matchforge_core` links the warning flags as `PRIVATE` so the warnings do not leak into the server.

---

## 5. Things to try yourself

1. Change the version in `project(... VERSION 0.1.0 ...)` to `0.2.0`, rebuild, and run the server. The log should show the new version.
2. Add `int unusedValue = 1;` inside `main()` in `src/server/main.cpp`, then build the debug preset. `-Wall` reports an unused variable, and `-Werror` turns that warning into a build failure. Remove the line afterwards.
3. Run `cmake --preset debug` twice. The second run should be fast, because vcpkg reuses the installed libraries.
