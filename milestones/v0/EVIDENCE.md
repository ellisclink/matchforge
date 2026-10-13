# Evidence
## Task 1.1 - 2026-10-12
- Command: `cmake --version; ninja --version; $VCPKG_ROOT/vcpkg version` (in the NVIDIA-Workbench distro)
- Result: pass (cmake 3.22.1, ninja 1.10.1, vcpkg 2026-09-26)
- Output summary: cmake 3.22.1; ninja 1.10.1; vcpkg package management program version 2026-09-26-51bf87ca6e9bf3e622d84ff323bd202ab1ca0c0b
## Task 1.2 - 2026-10-12
- Command: `clang-format --dump-config` and `find . -type d` (in ~/src/matchforge)
- Result: pass
- Output summary: clang-format 14.0.0 parses .clang-format (IndentWidth 4, ColumnLimit 100, AccessModifierOffset -4, IncludeBlocks Regroup). All layout folders from CLAUDE.md exist; each empty one holds a .gitkeep.
- Note: `clang-format --dry-run --Werror` has no source files to check yet; it becomes meaningful in Phase 2.
## Task 2.1 - 2026-10-12
- Command: `python3 -m json.tool vcpkg.json` and `vcpkg x-update-baseline --dry-run` (in ~/src/matchforge)
- Result: pass
- Output summary: JSON valid, 8 dependencies, builtin-baseline e456309491fd875487bf02b8a0dca76c1b1b00cc (the vcpkg copy at ~/vcpkg). All 8 port folders exist in that baseline (boost-asio 1.92.0, boost-redis 1.92.0, protobuf 6.33.4, gtest 1.18.0, spdlog 1.17.0, nlohmann-json 3.12.0, libsodium 1.0.22, prometheus-cpp 1.3.0). vcpkg reported "There were no changes in the ports."
- Note: vcpkg downloaded its own CMake 4.4.3 into its folder for its internal tools. Our build still uses the system CMake 3.22.1.
## Task 2.2 to 2.5 and 3.1 - 2026-10-12
- Command: `cmake --preset debug && cmake --build --preset debug` (VCPKG_ROOT=/home/workbench/vcpkg)
- Result: pass (first attempt failed: libsodium needed autoconf-archive; installed, then passed)
- Output summary: 11 build steps, including "Generating C++ from matchforge.proto". Protobuf 33.4.0 found from vcpkg.
## Task 3.1 (AC-1) - 2026-10-12
- Command: `ctest --preset debug --output-on-failure`
- Result: pass. 100% tests passed, 0 failed out of 1 (VersionTest.ReportsTheVersionCMakeConfigured).
## AC-3 - 2026-10-12
- Command: `./build/debug/src/server/matchforge_server; echo $?` and the same for matchforge_bots
- Result: pass. Server printed "[info] matchforge server 0.1.0", exit 0. Bots printed "[info] matchforge bots 0.1.0", exit 0.
## AC-2 - 2026-10-12
- Command: `cmake --preset release && cmake --build --preset release && ctest --preset release`
- Result: pass. Release build compiled. 1 of 1 test passed. Server ran with exit 0.
- Sanitizer check: debug compile_commands.json contains -fsanitize=address,undefined; release contains none.
## AC-6 - 2026-10-12
- Command: `docker run --rm -v <repo>:/repo rhysd/actionlint:1.7.7 .github/workflows/ci.yml`
- Result: pass (exit 0) after fixing runner-context error in job env (fixed by using $HOME and $GITHUB_ENV).
## AC-7 - 2026-10-12 (partial)
- Command: `clang-format -i` then `clang-format --dry-run --Werror` on all .cpp and .h files
- Result: pass for all C++ sources.
## Dockerfile lint - 2026-10-12
- Command: `docker run --rm -i hadolint/hadolint:v2.12.0 < deploy/Dockerfile`
- Result: pass (exit 0). DL3008 (unpinned apt versions) suppressed with a reason in the file.
## AC-5 - 2026-10-12 (under interpretation D-011)
- Command: `docker compose -f deploy/docker-compose.yml up --build -d`, then `docker ps -a` and `docker inspect deploy-server-1`
- Result: pass. Image built (multi-stage). deploy-redis-1 Up and healthy. deploy-server-1 logged "[info] matchforge server 0.1.0" and exited with code 0.
- Runtime check: `ldd` on the server binary shows only system libraries (libstdc++, libgcc_s, libc, libm). Nothing from vcpkg is needed at runtime.
## D-010 test - 2026-10-13
- Command: copied the Docker-built server out of deploy-server and ran it in debian:bookworm-slim: `docker run --rm -v <dir>:/app debian:bookworm-slim /app/matchforge_server`
- Result: pass. Printed "[info] matchforge server 0.1.0", exit 0.
- After switching the runtime stage to debian:bookworm-slim: `docker compose up --build -d` works; server exit 0; process runs as uid 999 (matchforge).
## Sanitizer check - 2026-10-13
- Command: g++ -std=c++20 -fsanitize=address,undefined on a deliberate out-of-bounds write
- Result: pass. AddressSanitizer reported heap-buffer-overflow with the file and line.
## AC-4 (second test, D-014) - 2026-10-13
- Command: `cmake --build --preset debug && ctest --preset debug`, then the same with `release`
- Result: pass on both presets. "100% tests passed, 0 tests failed out of 2" (VersionTest.ReportsTheVersionCMakeConfigured and EnvelopeTest.HeartbeatSurvivesSerializeAndParse).
## AC-7 (after adding the test) - 2026-10-13
- Command: `clang-format --dry-run --Werror` on every .cpp and .h file, using clang-format 14.0.6 in a python:3.12-slim container
- Result: pass (FORMAT_EXIT=0)
## AC-5 (accepted under D-011) - 2026-10-13
- Command: `docker compose up --build` and `docker compose up -d --force-recreate` after a full `--no-cache` build
- Result: pass. Build exit 0. Redis healthy. Server logs "[info] matchforge server 0.1.0" and exits 0.
## CI - 2026-10-13
- Run 38331642058 on commit 4e5b671: success on both presets, all steps, including "Check formatting".
## Compose config - 2026-10-12
- Command: `docker compose -f deploy/docker-compose.yml config -q`
- Result: pass (exit 0).
