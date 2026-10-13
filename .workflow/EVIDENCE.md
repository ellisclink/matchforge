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
## Compose config - 2026-10-12
- Command: `docker compose -f deploy/docker-compose.yml config -q`
- Result: pass (exit 0).
