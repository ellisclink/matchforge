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
## D-008: Spec and plan approved (2026-10-12)
- Decision: SPEC.md (7 acceptance criteria) and PLAN.md (4 phases) approved as written. Phase 1 may begin.
- Reason: User review, including the plain-English summary.
