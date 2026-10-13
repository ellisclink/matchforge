# matchforge

A multiplayer lobby and Elo matchmaking server in C++20. Players register and log in, join a
queue for a game mode in a region, and are matched with players of similar skill. Game
servers report results, and everyone's Elo rating is updated.

Version 0.9.0: all nine roadmap milestones are built (see `.workflow/ROADMAP.md`).

## Features

- **Protocol:** raw TCP, each message a Protobuf `Envelope` behind a 4-byte big-endian length.
- **Accounts:** Argon2id password hashing (libsodium), 256-bit session tokens, Redis storage.
- **Matchmaking:** queues per mode and region; a rating window that widens the longer a
  player waits (100 to 600 points); snake-draft team balancing; 1v1, 2v2 and 4v4 modes.
- **Ratings:** Elo with team averages; results reported by game servers with a shared
  secret; atomic updates in Redis.
- **Concurrency:** Boost.Asio with a thread pool; every session and the lobby own a strand,
  so shared state needs no locks; slow work (hashing, Redis) runs on worker threads.
- **Protection:** frame size limit, per-connection rate limit, idle timeout, login lockout,
  strict input validation.
- **Configuration:** `config/server.json`, validated at startup.
- **Operations:** Docker Compose stack with Redis, Prometheus metrics and a Grafana dashboard;
  a bot load simulator.

## Quick start (Docker)

```bash
export MATCHFORGE_RESULT_SECRET=change-me
docker compose -f deploy/docker-compose.yml up --build -d
```

| Service | Address | What it is |
|---|---|---|
| server | `localhost:7777` | the matchmaking server (TCP) |
| Prometheus | http://localhost:9090 | metrics storage and queries |
| Grafana | http://localhost:3000 | dashboard "matchforge" (view without login) |

Run a load test against it:

```bash
docker compose -f deploy/docker-compose.yml --profile load run --rm bots --bots 100 --rounds 5
```

Stop everything:

```bash
docker compose -f deploy/docker-compose.yml down
```

## Build and test (WSL2 / Ubuntu)

You need cmake, ninja, g++, vcpkg (with `VCPKG_ROOT` set), and the `autoconf autoconf-archive
automake libtool` packages for libsodium.

```bash
export VCPKG_ROOT=$HOME/vcpkg
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

The Redis store tests are skipped unless a test Redis is available:

```bash
docker run -d --name mf-test-redis -p 6380:6379 redis:7.4-alpine
MATCHFORGE_TEST_REDIS_PORT=6380 ctest --preset debug --output-on-failure
```

Release build: `cmake --preset release && cmake --build --preset release`.

## Running the server directly

```bash
./build/release/src/server/matchforge_server --config config/server.json
```

| Option or variable | Meaning |
|---|---|
| `--config <file>` | settings file; without it, built-in defaults are used |
| `--port <n>` | overrides the port from the config |
| `MATCHFORGE_REDIS_HOST`, `MATCHFORGE_REDIS_PORT` | override the Redis address |
| `MATCHFORGE_RESULT_SECRET` | the secret game servers send with results; unset means results are rejected |

Settings in `config/server.json`: port, frame limit, metrics port, worker threads, Redis
address, matching interval, rating window, K-factor, regions, modes, and the abuse limits.
Unknown or out-of-range values stop the server with a message naming the setting.

## Protocol summary

| Client sends | Server answers |
|---|---|
| `Heartbeat` | `Heartbeat` (server clock) |
| `RegisterRequest`, `LoginRequest` | `AuthResponse` (token, rating) or `ErrorResponse` |
| `JoinQueueRequest` (after login) | `QueueStatus`, later `MatchFound` |
| `LeaveQueueRequest` | `QueueStatus{queued=false}` |
| `ReportMatchResult` (game servers, with secret) | `MatchResultAck` (new ratings) |

Error codes follow HTTP: 400 bad request, 401 not logged in or bad credentials, 403 not
allowed, 404 unknown match, 409 conflict, 429 too many requests, 503 storage unavailable.

## Load simulator

`matchforge_bots --bots 100 --rounds 5 --mode duel --region eu [--secret <s>]` prints matches
joined, errors, and time to match (average, p50, p95, max). Run `--help` for every option.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `VCPKG_ROOT` is empty | Run `export VCPKG_ROOT=$HOME/vcpkg`, or open a new interactive terminal. |
| libsodium fails, needs `autoconf-archive` | `sudo apt install -y autoconf autoconf-archive automake libtool` |
| CMake cannot find Ninja | `sudo apt install -y ninja-build` |
| `-Werror` stops the build | Fix the warning the first error line names; do not remove `-Werror`. |
| Server exits with "invalid config" | The message names the setting; fix it in the JSON file. |
| Results are rejected with 403 | Set `MATCHFORGE_RESULT_SECRET` for the server and the reporter. |

## Learn more

- [docs/architecture.md](docs/architecture.md): modules, threads and strands, message flow.
- [docs/milestone-0-explained.md](docs/milestone-0-explained.md): the build, vcpkg, Protobuf, Docker, CI.
- [docs/milestones-explained.md](docs/milestones-explained.md): milestones 1 to 9, with design
  choices and interview questions.
- `.workflow/`: the spec, plan, decisions and evidence for every milestone.

## Known limits

No TLS yet, one server process holds the queue (no horizontal scaling), and rate limits are
per connection and per username, not per IP address. See `docs/architecture.md`.
