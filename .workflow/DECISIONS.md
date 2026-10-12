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
## D-004 (PROPOSED, awaiting approval): 4-space indent
- Reason: Readability in a multi-module project. Google default is 2.
## D-005 (PROPOSED, awaiting approval): Pin vcpkg builtin-baseline
- Reason: Reproducible builds and effective binary caching in CI.
## D-006 (PROPOSED, awaiting approval): -Wall -Wextra -Werror; ASan/UBSan in the debug preset
- Reason: Catch bugs early; standard C++ practice.
## D-007 (PROPOSED, awaiting approval): .gitkeep in empty layout folders; pin Docker base image versions
- Reason: Git does not track empty folders; "latest" tags make builds non-reproducible.
