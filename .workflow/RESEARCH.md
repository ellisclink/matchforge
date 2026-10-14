# Research (2026-10-12)
- Windows: no cmake, ninja, g++, or clang. Checked with `command -v`.
- WSL distros: "Ubuntu" (24.04, g++ 13.3, no cmake, ninja, or vcpkg); "NVIDIA-Workbench" (22.04.4, g++ 11.4.0, cmake 3.22.1, ninja 1.10.1, clang-format 14, gh 2.4, vcpkg at ~/vcpkg, gh logged in as ellisclink).
- VCPKG_ROOT is exported in ~/.bashrc of NVIDIA-Workbench. It is empty in non-interactive shells, so presets need an interactive shell or an explicit export.
- Docker Desktop was not running at last check. It needs WSL integration on.
- apt candidates in Ubuntu 24.04: cmake 3.28.3, ninja 1.11.1.
- Repo ellisclink/matchforge: one commit (Initial commit), description present, no PRs or issues. Has a README.md.
