# matchforge

A multiplayer lobby and Elo matchmaking server in C++. Players log in, queue for a match mode in a region, and are grouped with players of similar skill.

Status: Milestone 0 (project setup). The programs only print their version. Matchmaking and networking come in later milestones.

## Build and test (WSL2 / Ubuntu)

You need cmake, ninja, g++ and vcpkg installed, with `VCPKG_ROOT` pointing at your vcpkg folder.

```bash
export VCPKG_ROOT=$HOME/vcpkg
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Release build:

```bash
cmake --preset release
cmake --build --preset release
```

## What the presets do

`CMakePresets.json` is plain JSON, so it cannot hold comments. This table explains each setting.

| Preset | Build type | Sanitizers | Used for |
|---|---|---|---|
| `base` (hidden) | Set by the presets below | Off by default | Shared settings: Ninja generator, vcpkg toolchain, and where files go |
| `debug` | Debug | On (ASan and UBSan) | Day-to-day work and tests. Catches memory bugs early |
| `release` | Release | Off | The fast build that goes into the Docker image |

Shared settings, in the `base` preset:

- `generator: Ninja`: the build tool that runs the compiler, faster than make.
- `CMAKE_TOOLCHAIN_FILE`: points CMake at vcpkg, so it can find the libraries in `vcpkg.json`.
- `VCPKG_INSTALLED_DIR`: installs the libraries into `vcpkg_installed/` in the project folder. Docker uses the same location, so the libraries are cached in one layer.
- `binaryDir`: each preset builds into its own folder (`build/debug`, `build/release`), so the two never overwrite each other.

The `buildPresets` and `testPresets` sections let `cmake --build --preset debug` and `ctest --preset debug` pick the matching configuration without repeating the settings.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `VCPKG_ROOT` is empty, or `cmake --preset` says it cannot find the toolchain file | Your shell did not load `~/.bashrc`. Run `export VCPKG_ROOT=$HOME/vcpkg` in the same terminal, or open a new interactive terminal. |
| libsodium fails with "requires the following programs from the system package manager: autoconf-archive" | Run `sudo apt install -y autoconf autoconf-archive automake libtool`, then build again. |
| CMake says it cannot find Ninja | Install it with `sudo apt install -y ninja-build`. |
| A build fails with `-Werror` on your own code | That is the strict warning rule working. Read the first line of the error. It names the warning, for example `unused-variable`. Fix the code instead of removing `-Werror`. |

## Learn more

- [docs/milestone-0-explained.md](docs/milestone-0-explained.md): how the build, vcpkg, Protobuf, Docker, and CI fit together, with interview questions.

## Run with Docker

```bash
docker compose -f deploy/docker-compose.yml up --build
```

The server container prints its version and exits. Redis keeps running.
