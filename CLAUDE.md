# matchforge

A multiplayer lobby and matchmaking server in C++. Players log in, queue for a match mode in a region, and are matched by Elo rating. Built as a portfolio project; the author must be able to explain every part of it in a software engineering interview.

## Tech stack

- C++20, compiled with GCC or Clang on Linux (developed in WSL2)
- Networking: Boost.Asio, raw TCP
- Wire format: Protobuf, each message framed with a 4-byte big-endian length prefix
- State: in-memory queues; Redis for accounts, sessions and ratings (Boost.Redis client)
- Password hashing: Argon2id via libsodium
- Config: JSON (nlohmann-json)
- Logging: spdlog
- Metrics: prometheus-cpp, scraped by Prometheus, shown in Grafana
- Build: CMake + vcpkg (manifest mode), CMakePresets.json
- Tests: GoogleTest, run with ctest
- Deployment: Docker Compose (server, Redis, Prometheus, Grafana)
- CI: GitHub Actions

## Layout

```
proto/            .proto definitions
src/core/         matchmaking logic, no networking (pure, unit-testable)
src/net/          TCP server, framing, sessions
src/store/        Redis access (accounts, sessions, ratings)
src/server/       server executable entry point and wiring
src/bots/         bot load simulator executable
tests/            GoogleTest suites, mirroring src/
config/           match modes and server config (JSON)
deploy/           Dockerfile, docker-compose.yml, Prometheus and Grafana config
docs/             architecture notes and diagrams
```

## Commands

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
docker compose -f deploy/docker-compose.yml up --build
```

## Coding rules

- Modern C++: RAII, no raw `new`/`delete`, prefer `std::unique_ptr`; `std::shared_ptr` only where shared ownership is real (e.g. Asio session lifetimes).
- Naming: `PascalCase` types, `camelCase` functions and variables, `m_` prefix for members, `kPascalCase` constants.
- Format with the repo's `.clang-format` before finishing a task.
- Keep `src/core` free of networking and Redis so it can be tested with plain unit tests.
- Comments explain *why*, not *what*.

## Concurrency rules

- One `io_context` run by a thread pool; per-session work goes through a strand.
- Never block an I/O thread: no sleeps, no synchronous Redis or disk calls.
- Shared matchmaking state is owned by one strand; other code posts to it rather than locking.

## Security rules

- Hash passwords with Argon2id (libsodium `crypto_pwhash_str`); never store or log plaintext passwords.
- Never log passwords or session tokens.
- Reject frames larger than the configured maximum message size.
- Validate every incoming message before acting on it.

## How to work

1. Read this file and the current milestone prompt.
2. Propose a short plan (files to create or change, key design choices) and **wait for approval** before writing code.
3. Work in small steps. Only build what the current milestone asks for.
4. Every new module gets GoogleTest tests. Build and run all tests before saying a task is done.
5. Commit after each logical step using Conventional Commits (`feat:`, `fix:`, `test:`, `build:`, `docs:`).

## Explaining the work (required)

The author is a student who must defend this code in interviews. After each step, write an **Explanation** section covering:

- **What was built:** each file and its role, in plain language.
- **How it works:** walk through the important code paths, explaining any C++, Asio, Protobuf or Redis concept a student may not know yet.
- **Design choices:** why this approach, what alternatives were considered, and the tradeoffs.
- **Interview questions:** 3 to 5 questions an interviewer might ask about this step, with short model answers.

Keep explanations concrete and tied to the actual code. If the author asks about something, explain it before moving on.
