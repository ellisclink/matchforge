# File-by-file guide to matchforge

This guide describes every file in the project: what it is for, how it works, and why it was
written that way. It is meant to be read alongside the code. If you only read one other
document first, read [architecture.md](architecture.md) (the big picture); this guide fills in
the detail.

**How to use it.** Part 1 gives a reading order. Parts 2 to 14 go through the folders. Part 15
follows a single login through the code, naming the file and function at each step. Part 16
lists the C++ ideas used and where to see each one.

Numbers in brackets, like `[112]`, are test counts. Line counts are approximate.

---

## Part 1: where to start reading

If you want to understand the project in the least time, read the files in this order:

1. `proto/matchforge.proto`: the vocabulary. Every message the server understands.
2. `src/core/Matchmaker.h`: the heart of the product, with no networking in it.
3. `src/net/Framing.h` and `src/net/Session.h`: how bytes become messages.
4. `src/auth/AuthService.cpp`: a complete feature (register, login) across threads.
5. `src/lobby/LobbyService.h`: how the queue is protected without locks.
6. `src/server/main.cpp`: how everything is wired together at startup.
7. `tests/net/TestSupport.h`: how the tests start a real server and talk to it.

Then read the rest by folder.

---

## Part 2: files in the repository root

### `CLAUDE.md`
Your project rules: tech stack, folder layout, coding style (naming, RAII, no raw `new`),
concurrency rules (one strand owns shared state, never block I/O threads), security rules
(Argon2id, never log secrets, reject oversized frames), and the working method (plan first,
tests for every module, Conventional Commits). It was copied in unchanged. Every design
decision in the code traces back to a rule here.

### `README.md`
The front page. Features, quick start with Docker, ports, how to build and test, the
protocol summary, config options, troubleshooting, and known limits.

### `CMakeLists.txt` (top level, ~80 lines)
The build recipe CMake reads first. In order:
1. `cmake_minimum_required(3.21)` and `project(matchforge VERSION 0.9.0 ...)`. The version is
   used later: `src/core` turns it into the `MATCHFORGE_VERSION` string.
2. C++20 for everything, with compiler extensions off, so the code is standard C++.
3. `option(MATCHFORGE_SANITIZERS ...)`: a switch, off by default, turned on by the debug preset.
4. `matchforge_warnings`, an **INTERFACE library**: it has no code, only compiler flags
   (`-Wall -Wextra -Wpedantic -Werror`, plus AddressSanitizer and UBSan when the option is on).
   Any target that links it inherits those flags, so the rules live in one place.
5. `find_package(...)` for each dependency installed by vcpkg: spdlog, GTest, Boost.Asio,
   Boost.Redis, nlohmann-json, prometheus-cpp, libsodium, OpenSSL, Threads.
6. `enable_testing()`, then one `add_subdirectory(...)` per folder.

*Why it works this way:* each folder has its own `CMakeLists.txt` that defines its library, so
the dependency rules ("net may use lobby, lobby may not use net") are written down where the
library is defined, and the compiler/linker enforce them.

### `CMakePresets.json`
Named build settings so you type `cmake --preset debug` instead of a long command.
- A hidden `base` preset: Ninja as the build tool, the vcpkg toolchain file taken from the
  `VCPKG_ROOT` environment variable, libraries installed into `vcpkg_installed/`, and each
  preset building into its own folder (`build/debug`, `build/release`).
- `debug`: Debug build with sanitizers on (catches memory bugs).
- `release`: optimised build, what goes into Docker.
- Matching build and test presets, so `ctest --preset debug` knows which build to test and
  prints output only on failure.

*JSON cannot hold comments, so the README explains these settings in a table.*

### `vcpkg.json`
The shopping list of third-party libraries ("manifest mode"): boost-asio, boost-redis, openssl,
protobuf, gtest, spdlog, nlohmann-json, libsodium, prometheus-cpp. The `builtin-baseline`
pins one commit of the vcpkg catalogue so every machine builds the same versions.
`openssl` is listed explicitly because Boost.Redis includes OpenSSL headers but its vcpkg
package does not install them (decision D-020).

### `.clang-format`
Formatting rules: Google style with 4-space indent and 100-column lines. `clang-format -i`
rewrites a file to match; CI fails if any file is not formatted.

### `.editorconfig`
Tells editors the same basics (UTF-8, LF line endings, trailing whitespace trimmed, indent
size) so files look the same in any editor.

### `.gitignore`
Keeps generated or private files out of git: `build/`, `vcpkg_installed/`, editor folders,
logs, and `.env` files (where secrets might live).

### `.dockerignore`
Keeps the same kind of files out of the Docker build context, so builds are fast and local
build output or secrets never reach an image.

### `config/server.json`
The default server settings (port, frame limit, metrics port, worker threads, Redis address,
matching rules, regions, modes, abuse limits). Documented in Part 9. It is copied into the
Docker image at `/etc/matchforge/server.json`.

---

## Part 3: `proto/`: the message vocabulary

### `proto/matchforge.proto` (~110 lines)
Defines every message in Protocol Buffers. Protobuf is a way to describe data once and
generate code for reading and writing it in any language.

- **`Envelope`** wraps every message on the wire. Its `oneof payload` means exactly one of
  eleven payload types is set, so the receiver reads the envelope and checks which arrived:
  `Heartbeat`, `ErrorResponse`, `RegisterRequest`, `LoginRequest`, `AuthResponse`,
  `JoinQueueRequest`, `LeaveQueueRequest`, `QueueStatus`, `MatchFound`, `ReportMatchResult`,
  `MatchResultAck`.
- Each field has a number (`heartbeat = 1`, ...). Numbers are what travel on the wire, so they
  must never be reused or renumbered; new messages get new numbers at the end.
- Passwords appear only in `RegisterRequest` and `LoginRequest`; comments mark them
  "never log".
- `MatchFound` carries both team rosters plus `your_team`, so a client can show the lobby.
- `ReportMatchResult` carries a `result_secret`: it is how a game server proves it may change
  ratings.

### `proto/CMakeLists.txt`
Turns the `.proto` file into C++ during the build:
1. `find_package(Protobuf)` gives us `protoc` (the compiler) and the runtime library.
2. `add_custom_command` is a build rule: "to produce `matchforge.pb.cc` and `.pb.h`, run
   `protoc` on the `.proto` file". CMake re-runs it only when the `.proto` changes. (The newer
   `protobuf_generate` helper needs CMake 3.23; this machine has 3.22, hence the manual rule,
   decision D-009.)
3. Generated files go into the build folder, never the source tree.
4. They are compiled into the library `matchforge_proto`. Generated code is not compiled with
   `-Werror`, because we do not control its style.

---

## Part 4: `src/core/`: pure logic with no dependencies

Rule from CLAUDE.md: **core has no networking, no Redis, no threads, and no reading of the
clock.** Anything time-dependent receives the current time as an argument. That is why these
files can be tested exhaustively in milliseconds. Library: `matchforge_core`.

### `Version.h` / `Version.cpp` [1 test]
One function, `version()`, returning "0.9.0". CMake defines the macro `MATCHFORGE_VERSION`
from the project version; `Version.cpp` returns it. The test checks the function and the macro
agree. It is the smallest possible example of a library, a header and a test.

### `Validation.h` / `Validation.cpp` [5 tests]
`isValidUsername` (3 to 20 characters: letters, digits, underscore) and `isValidPassword`
(8 to 128 bytes). Usernames are restricted so they cannot spoof other names or smuggle control
characters into logs. The password upper bound stops anyone sending megabytes of text that
the slow hashing step would then spend time on.

### `Elo.h` / `Elo.cpp` [7 tests]
The rating maths.
- `expectedScore(a, b) = 1 / (1 + 10^((b - a) / 400))`: the probability that a player rated
  `a` beats one rated `b`. Equal ratings give 0.5; a 400-point lead gives about 0.91.
- `ratingDelta(rating, opponent, score, K)`: `K * (actual - expected)`, rounded. With K = 32,
  beating an equal player is +16; an upset over someone 200 points stronger is about +24.

### `Matchmaker.h` / `Matchmaker.cpp` [14 tests]
The queue and the matching algorithm. Types: `MatchMode` (name, team size), `MatchSettings`
(window base 100, growth 10 per second, max 600), `QueuedPlayer`, `Match` (two teams),
`JoinResult`.

How it works:
- `join` checks the mode and region exist, then `try_emplace`s the player into a
  `std::map` keyed by username. Joining twice does nothing, so a player keeps their original
  join time (they do not lose their place).
- `findMatches(now)` does the work:
  1. Split the queue into **buckets** by (mode, region), because players only meet others in
     the same bucket.
  2. Sort each bucket by (rating, join time, name). The extra keys make the order fully
     defined, so the same queue always gives the same matches.
  3. Slide along the list, looking at runs of `2 x teamSize` neighbours. For a run, the
     **spread** is highest minus lowest rating, and the **allowed gap** is the largest
     `ratingWindow` among its players. A player's window is `base + growth x secondsWaited`,
     capped at the maximum.
  4. If spread <= allowed gap, the run becomes a match: its players leave the queue and teams
     are chosen by a **snake draft** (strongest to A, next two to B, next two to A, ...), which
     keeps team totals close. Otherwise move one place along (dropping the lowest-rated player
     from consideration) and try again.
- Using the **widest** window in the run (not the narrowest) guarantees a long-waiting player
  is eventually matched.

*Alternatives not chosen:* a global optimum assignment (more code, harder to explain; listed
in the backlog), and matching immediately on every join (less choice, more CPU).

### `RateLimiter.h` / `RateLimiter.cpp` [4 tests]
A **token bucket**. The bucket holds up to `burst` tokens and refills at `ratePerSecond`. Each
message needs one token. `allow(now)` first adds the tokens earned since the last call
(lazy refill, so no timer), then spends one if available. Normal traffic never runs out;
a short burst is fine because the bucket starts full; a flood empties it.

### `LoginThrottle.h` / `LoginThrottle.cpp` [5 tests]
Slows password guessing. `recordFailure(name, now)` counts wrong passwords in a window; at the
limit (5) the name is locked until `now + lockout`. `isLocked` is checked before any password
work. `recordSuccess` clears the count. Entries are pruned once the map reaches 10,000 names,
so someone trying random usernames cannot grow memory without limit.

*Trade-off, stated in the header:* someone who knows your username can lock you out for five
minutes by failing on purpose. Per-IP limits would reduce that and are in the backlog.

### `src/core/CMakeLists.txt`
Builds `matchforge_core` from the files above and defines the `MATCHFORGE_VERSION` macro. Links
only `matchforge_warnings`: core depends on nothing.

---

## Part 5: `src/store/`: where accounts, sessions and ratings live

Library: `matchforge_store`. The rest of the server talks to an **interface**, not to Redis.

### `AccountStore.h`
An abstract class with seven methods: `createAccount`, `findPasswordHash`, `saveSession`,
`findSessionUser`, `findRating`, `saveRating`, `adjustRating`. Every method may throw
`std::runtime_error` if the backend fails. Having an interface means tests use an in-memory
store, and a different database could be swapped in later.

`adjustRating(name, delta, startingRating)` is the important one: it adds a change to a rating
**atomically** and returns the new value, creating the rating from `startingRating` if the
player has none. Atomic means two results for the same player arriving at the same moment are
both applied.

### `InMemoryAccountStore.h` / `.cpp` [6 tests]
Keeps everything in `unordered_map`s guarded by one `std::mutex` (worker threads call it in
parallel). Sessions store an expiry time on `steady_clock`; `findSessionUser` forgets expired
ones. `adjustRating` uses `try_emplace` to insert the starting value only when missing, then
adds the delta under the lock.

### `RedisAccountStore.h` / `.cpp` [4 tests, need a Redis]
The real storage. Keys: `account:<name>` (the password hash), `session:<token>` (the
username, with an expiry), `rating:<name>` (the Elo number).

How the threading works. Boost.Redis is asynchronous and not thread-safe, so the class owns
its **own I/O thread** (`m_io`, kept alive by a work guard) and the connection. Callers are the
worker threads. Each call goes through the private template `run()`:
1. Copy the request into a heap-allocated `Pending` object held by a `shared_ptr` (request,
   response, and a `std::promise`).
2. `post` the command onto the Redis I/O thread; its callback sets the promise.
3. The worker waits on the matching `std::future` for at most **2 seconds**.

Two details matter. First, the shared state means that if a command times out and the worker
gives up, the I/O thread can still finish it later without touching freed memory. Second, the
timeout exists because Boost.Redis keeps retrying a dead connection forever, so without it a
Redis outage would freeze every worker.

Commands: `SETNX` to create an account (set only if absent, so "check the name is free" and
"claim it" are one atomic step), `GET`, `SET ... EX` to store a session that expires
by itself, and for ratings `SETNX` then `INCRBY` sent together as one request.

### `BoostRedisSource.cpp`
Six lines: `#include <boost/redis/src.hpp>`. Boost.Redis is separately compiled; this one
translation unit builds its implementation. Without it the linker reports undefined
references to `boost::redis::connection`.

### `src/store/CMakeLists.txt`
Builds the library and links Boost.Redis, Boost.Asio, OpenSSL (headers), and threads.

---

## Part 6: `src/auth/`: register, login, passwords, tokens

Library: `matchforge_auth`.

### `PasswordHasher.h` / `.cpp` [4 tests]
Three functions on top of libsodium:
- `hashPassword`: `crypto_pwhash_str` with the "interactive" limits (about 64 MB and 50 to
  100 ms). The result is one string containing the algorithm (Argon2id), settings, a fresh
  random salt, and the hash.
- `verifyPassword`: `crypto_pwhash_str_verify` against that string.
- `newSessionToken`: 32 random bytes from libsodium's random generator, encoded as URL-safe
  base64 without padding (43 characters).

Two passwords hashed twice give different strings (random salt), the hash never contains the
password, and the 50 tokens generated in a test are all different.

### `AuthService.h` / `AuthService.cpp` [10 tests]
`registerAccount` and `login` each `post` a task onto the **worker pool** and return at once;
the answer arrives by callback. They never run on a network thread because hashing is slow.

`doRegister`: validate the username and password (cheap, before hashing), lowercase the name,
hash, `createAccount`, create and save a session token, return it with the starting rating.

`doLogin`: validate, load the stored hash, and verify the password. Details that matter:
- Unknown user and wrong password produce the **same** error message.
- When the account does not exist it still verifies against a precomputed **dummy hash**, so
  the response time does not reveal which usernames exist.
- Success loads the saved rating (default 1500) and creates a session.

`AuthService::login` wraps `doLogin` with the **throttle**: it asks `LoginThrottle` first
(a mutex guards it because workers run in parallel) and refuses locked names with 429 without
touching the password. Only wrong credentials count as failures, not storage outages.

Any storage exception becomes a generic 503 to the client; the real cause is logged without
the password.

### `src/auth/CMakeLists.txt`
Links store, core and Boost.Asio publicly (headers expose them), libsodium, spdlog and
metrics privately.

---

## Part 7: `src/lobby/`: the queue as a service

Library: `matchforge_lobby`. This is where the concurrency rule is applied.

### `LobbyClient.h`
A tiny interface: `deliver(std::string serializedEnvelope)`. It is how the lobby sends a
message to a player without knowing about sockets. `Session` implements it. This is
**dependency inversion**: the lower layer defines the interface, the upper layer implements
it, so the lobby never depends on the network code.

### `LobbyService.h` / `LobbyService.cpp` [10 tests over TCP]
**Ownership.** All lobby state is touched only by handlers running on one **strand**: the
`Matchmaker`, `m_waiting` (username to `weak_ptr<LobbyClient>`), `m_matches` (matches whose
result is still unreported) and the counters. Public methods do no work themselves; they
`post` a task to the strand. No mutex exists in this class.

Flows:
- **`join`**: runs on a worker first (reads the rating from Redis, which can be slow), then
  posts to the strand where `doJoin` calls `Matchmaker::join`. Replies with `QueueStatus`, or
  an error: 400 unknown mode or region, 409 already queued, 503 storage down.
- **`leave`** and **`disconnect`**: remove the player. They compare connections with
  `owner_before`, so a stale connection cannot cancel the queue entry of a newer one.
- **`tick`** (a timer on the strand, every 250 ms): (1) remove players whose `weak_ptr` has
  expired, meaning they disconnected; (2) forget matches older than two hours; (3) run
  `findMatches(now)`; (4) for each match create a random 16-character id, `announce` it, and
  record metrics.
- **`announce`**: builds one `MatchFound` with both rosters and sends each player a copy with
  their own `your_team`.
- **`reportResult`**: (1) check the secret in **constant time** (a normal `==` stops at the
  first wrong character, leaking how much was right); (2) check `winning_team` is -1, 0 or 1;
  (3) find the match and **erase it immediately** on the strand, so a second report fails with
  404 even while the first is still being saved; (4) compute each team's Elo change from the
  team average ratings; (5) a worker applies it with `adjustRating` and the reporter receives
  `MatchResultAck` with everyone's new ratings.

Why `weak_ptr` for waiting players: the lobby must not keep a closed connection alive. An
expired pointer simply means "this player left".

### `src/lobby/CMakeLists.txt`
Links core, store and Asio publicly; proto, auth (for `newSessionToken`, reused for match
ids), spdlog and metrics privately.

---

## Part 8: `src/net/`: bytes, connections, the listener

Libraries: `matchforge_framing` (just `Framing.cpp`) and `matchforge_net` (sessions and the
server). Framing is separate so the bot client can use it without linking the server.

### `Framing.h` / `Framing.cpp` [7 tests]
TCP is a stream of bytes with no message boundaries. A **frame** puts a 4-byte big-endian
length in front of each message.
- `encodeFrame(payload)` writes the length one byte at a time, most significant first, so it
  does not depend on the machine's byte order.
- `FrameDecoder` collects bytes with `append` and returns complete payloads from `nextFrame`.
  It returns nothing until the whole frame has arrived (even if bytes trickle in one at a
  time), splits several frames from one read, and **fails as soon as a length header exceeds
  the limit**, before reading the body. A failed decoder stays failed.

### `Session.h` / `Session.cpp` [8 + 4 tests over TCP]
One object per connected client; the most important file for understanding the server's
behaviour. It inherits two things:
- `enable_shared_from_this`: every pending operation holds a `shared_ptr` to the session, so it
  lives exactly until the last operation finishes (one of the few places shared ownership is
  genuinely needed).
- `LobbyClient`: so the lobby can push messages to it via `deliver`.

Each session has its **own strand** (`m_strand`), so its reads, writes and timers never run at
the same time, and its state needs no lock. `SessionLimits` (frame limit, idle timeout, message
rate and burst) arrive from the config.

The read loop (`doRead`): `async_read_some` into a 4 KB buffer; on data, append to the decoder
and re-arm the idle timer; then for each complete frame run `admitMessage()` (token bucket,
metrics) and `handleFrame()`; if the decoder failed, close; otherwise read again.

`handleFrame` parses the `Envelope` and switches on the payload:
- Bytes that do not parse: `ErrorResponse 400`, then `closeAfterFlush` (the stream is no longer
  trustworthy, but the error must reach the client first).
- Heartbeat: reply with the server clock.
- Register / Login: hand to `AuthService`; its callback runs on a worker, so it `post`s back to
  this strand, where `handleAuthResult` records the username and sends the reply.
- JoinQueue / LeaveQueue: require a logged-in session (else 401), then call the lobby.
- ReportResult: passed to the lobby, which checks the secret.
- Server-only message types sent by a client: 400 but the connection stays open.

Writing: replies go into `m_writeQueue`; only one `async_write` may run on a socket at a time,
so `enqueue` starts writing only if the queue was empty and each completed write starts the next.

Protection: `armIdleTimer` is pushed back on every received message (a heartbeat keeps a quiet
client alive); `admitMessage` answers 429 when the bucket is empty and, after 50 refusals in a
row, flushes the replies and disconnects. `close` cancels the timer, tells the lobby the player
left, and shuts the socket.

### `Server.h` / `Server.cpp`
The listening socket. The constructor binds immediately (a port already in use throws here, at
startup, not later). Port 0 asks the OS for any free port, and `localPort()` reports it (tests
use this). `doAccept` waits for a client, creates a `Session` with `make_shared`, starts it,
and waits again. `stop()` closes the acceptor.

### `src/net/CMakeLists.txt`
Defines both libraries and what each links.

---

## Part 9: `src/config/`: the settings file

Library: `matchforge_config`.

### `ServerConfig.h` / `ServerConfig.cpp` [9 tests]
`ServerConfig` is a plain struct of settings with sensible defaults. `parseConfig(json)` starts
from the defaults and overlays the file:
- Helper functions `readInt`, `readDouble`, `readString` check **type and range** for each
  field and throw `ConfigError` naming the setting (for example `'port' must be between 1 and
  65535`).
- `requireOnlyKeys` rejects **unknown keys**, so a typo like `kfactor` fails loudly instead of
  silently keeping the default.
- `validate` checks settings against each other: base window not above the maximum, burst not
  below the rate, metrics port different from the game port, at least one region and mode,
  names 1 to 20 characters of letters, digits, `_`, `-`, and no duplicates.

Secrets are not read from the file; the result secret comes only from the environment.
One quirk to know: a single `loginLockoutSeconds` value is used for both the failure-counting
window and the lockout length.

### `src/config/CMakeLists.txt`
Links nlohmann-json (the parser) privately and core publicly.

---

## Part 10: `src/metrics/`: numbers for Prometheus

Library: `matchforge_metrics`.

### `Metrics.h` / `Metrics.cpp` [3 tests]
Creates every metric once in a Prometheus `Registry` and offers small methods to record into
them. Three kinds: a **counter** only goes up, a **gauge** goes up and down, a **histogram**
counts values into buckets.

| Metric | Kind | Meaning |
|---|---|---|
| `matchforge_connections_total` | counter | connections accepted |
| `matchforge_connections_open` | gauge | connections open now |
| `matchforge_messages_received_total` | counter | messages from clients |
| `matchforge_messages_rate_limited_total` | counter | messages refused by the rate limit |
| `matchforge_logins_total{result}` | counter | success, failure, locked |
| `matchforge_registrations_total{result}` | counter | success, failure |
| `matchforge_players_queued` | gauge | players waiting now |
| `matchforge_matches_created_total{mode}` | counter | matches formed |
| `matchforge_queue_wait_seconds` | histogram | join to match, buckets 0.1 to 120 s |
| `matchforge_results_applied_total` | counter | results applied to ratings |

prometheus-cpp uses atomic operations, so any thread can record without locks. Components take
a `Metrics*` that may be null: tests that do not care pass nothing and pay nothing. The server
hosts the HTTP endpoint (`/metrics`) using `prometheus::Exposer` in `main.cpp`.

---

## Part 11: `src/server/`: the program

### `main.cpp` (~190 lines)
The entry point. It builds objects in dependency order so destruction (reverse order) is safe:
1. Parse the command line (`--config <file>`, `--port <n>`); invalid input exits with code 2.
2. Load the config file (or defaults), then apply overrides: `--port`, then the environment
   (`MATCHFORGE_REDIS_HOST`, `MATCHFORGE_REDIS_PORT`, and `MATCHFORGE_RESULT_SECRET`, which is
   only ever read from the environment). A bad config stops the server with the setting named.
3. Create `Metrics` first (everything records into it) and start the `/metrics` endpoint.
4. Create `RedisAccountStore`, the worker `thread_pool`, and `AuthService`.
5. Create the `io_context`, the `signal_set` that turns Ctrl+C or `docker stop` (SIGINT,
   SIGTERM) into a clean stop, the `LobbyService`, and the `Server`.
6. Run the `io_context` on as many threads as there are CPU cores (at least 2) and wait.
7. On shutdown, join the workers so unfinished hashing finishes before the store is destroyed.

Exit codes: 0 clean stop, 1 runtime failure (such as the port in use), 2 bad arguments or config.

### `src/server/CMakeLists.txt`
Links every library the program wires together.

---

## Part 12: `src/bots/`: the load simulator

### `BotOptions.h` / `.cpp` [5 tests]
Parses `--host --port --bots --rounds --mode --region --secret --timeout`. Every number is
range-checked, unknown options and missing values give a clear message, and `--help` returns
the usage text.

### `BotClient.h` / `.cpp` [2 tests]
A small **blocking** client (fine here, because each bot has its own thread and the "never
block" rule applies to the server's I/O threads). It resolves the host, connects, sets an
OS-level receive timeout, and offers `send`, `receive`, and `waitFor(type)`, which skips
unrelated messages (like `QueueStatus`) until the wanted one arrives or an error appears.

### `BotReport.h` / `.cpp` [3 tests]
`summarize()` computes count, average, p50, p95 and max using the nearest-rank percentile
method: the p95 of 100 sorted values is the 95th.

### `main.cpp`
Starts one thread per bot. Each bot registers a unique account, then plays `rounds` matches:
join, wait for `MatchFound`, record how long it waited. With `--secret`, the first player on
team 0 reports a random winner, so ratings move over a long run. Results are collected under a
mutex. It prints matches, errors, unmatched and wait percentiles, and exits non-zero if
anything failed, so scripts can use it as a check.

### `src/bots/CMakeLists.txt`
A library (`matchforge_botlib`) holds the testable parts; the executable is just `main.cpp`.

---

## Part 13: `tests/`: 112 tests

Run them with `ctest --preset debug`. The Redis tests skip themselves unless
`MATCHFORGE_TEST_REDIS_PORT` is set.

| File | Tests | What it checks |
|---|---|---|
| `core/VersionTest.cpp` | 1 | version string matches the CMake macro |
| `core/ValidationTest.cpp` | 5 | username and password boundaries, UTF-8 rejected |
| `core/EloTest.cpp` | 7 | expected scores, zero-sum 1v1, upsets, K-factor, hand-checked numbers |
| `core/MatchmakerTest.cpp` | 14 | pairing, regions and modes kept apart, window growth to the second, max window, widest-window rule, snake-draft totals, leftovers, double join, leave, bad config |
| `core/RateLimiterTest.cpp` | 4 | burst then refuse, refill, cap, steady rate |
| `core/LoginThrottleTest.cpp` | 5 | lock after 5, lock expiry, window reset, success clears, names independent |
| `proto/EnvelopeTest.cpp` | 1 | a Heartbeat survives serialize and parse |
| `auth/PasswordHasherTest.cpp` | 4 | verify right and wrong, no plaintext in hash, salts differ, tokens unique |
| `auth/AuthServiceTest.cpp` | 10 | register, validation, duplicates, case-insensitive names, login, wrong password, unknown user looks the same, errors never echo the password, lockout |
| `store/InMemoryAccountStoreTest.cpp` | 6 | accounts, sessions, expiry, ratings, `adjustRating` |
| `store/RedisAccountStoreTest.cpp` | 4 | the same against real Redis; atomic rating; Redis down throws |
| `net/FramingTest.cpp` | 7 | big-endian header, partial frames byte by byte, several frames per read, empty payload, oversize rejected and stays failed |
| `net/ServerTest.cpp` | 8 | heartbeat, malformed closes, oversize closes, server-only messages rejected, register, duplicate, login, wrong password |
| `net/SessionLimitsTest.cpp` | 4 | idle timeout, heartbeats keep alive, 429 over burst, flood disconnect |
| `lobby/LobbyTest.cpp` | 10 | login required, two players matched, bad mode and region, duplicate join, leave, disconnected not matched, result updates ratings (+16/-16), wrong secret 403, draw, **16 concurrent clients each matched exactly once** |
| `metrics/MetricsTest.cpp` | 3 | values recorded, text format, real server records them |
| `bots/BotOptionsTest.cpp` | 5 | defaults, every option, bad numbers, unknown options, help |
| `bots/BotReportTest.cpp` | 3 | empty input, percentiles 1 to 100, single sample |
| `bots/BotClientTest.cpp` | 2 | bot protocol against a real server, `waitFor` stops at an error |
| `config/ServerConfigTest.cpp` | 9 | defaults, **the shipped file is valid**, nested values, typos, wrong types, ranges, contradictions, bad names, invalid JSON |

### `tests/net/TestSupport.h`: the shared test machinery
- `IoThread`: runs an `io_context` on a background thread. It holds a **work guard**; without
  one, `run()` returns immediately when nothing is queued yet, and later work never runs (an
  early bug that made server tests hang).
- `TestServer`: a complete server (in-memory accounts, lobby, listener on a free port, and a
  `Metrics`) with chosen limits. Members are declared in dependency order so destruction is
  safe.
- `ServerFixture`: a GoogleTest fixture that starts a fresh `TestServer` per test.
- `TestClient`: a blocking client with a **10-second receive timeout**, so a missing reply fails
  the test instead of hanging it, plus helpers like `registerAs`, `joinQueue`, `expect(type)`.

Testing techniques you can reuse: pass time in as an argument (no sleeping), keep logic pure so
it needs no servers, use interfaces for fake storage, and give every network wait a timeout.

### `tests/CMakeLists.txt`
Builds one test program from all the files, links every library, defines
`MATCHFORGE_SOURCE_DIR` (so a test can open `config/server.json`), and uses
`gtest_discover_tests` so each `TEST` is registered with `ctest` individually.

---

## Part 14: `deploy/`, `.github/`, `docs/`, `.workflow/`

### `deploy/Dockerfile`
**Two stages.** The *build* stage (Ubuntu 24.04) installs compilers and tools, clones vcpkg at
the baseline from `vcpkg.json`, installs the libraries as their own layer (so code-only changes
reuse the slow part), then builds the release preset. The *runtime* stage starts from a fresh
Ubuntu 24.04 and copies only the server binary, the bots binary and the config, then runs as a
non-root user. The final image has no compiler and no source code. Both stages use the same
Ubuntu release because the binary needs the same C library it was built against (Debian
bookworm crashed it; decision D-019).

### `deploy/docker-compose.yml`
Five services:
- `redis`: pinned image with a health check.
- `server`: waits for Redis to be healthy, gets the Redis address and result secret from
  environment variables, publishes port 7777, restarts unless stopped.
- `prometheus`: scrapes the server every 5 seconds; UI bound to localhost.
- `grafana`: dashboards from files; UI bound to localhost; anonymous viewing, admin password
  from the environment.
- `bots`: the load simulator, under the `load` profile so it runs only on request.

### `deploy/prometheus/prometheus.yml`
One scrape job pointing at `server:9100`.

### `deploy/grafana/provisioning/...` and `deploy/grafana/dashboards/matchforge.json`
Grafana reads these at startup: a data source named `Prometheus`, a rule to load dashboards
from a folder, and the dashboard itself. Eight panels: connections open, players queued,
matches created, results applied, matches per second by mode, p50 and p95 queue wait
(`histogram_quantile` over the histogram buckets), logins per second by result, and messages
per second including rate-limited ones.

### `.github/workflows/ci.yml`
On every push and pull request: checks formatting with a pinned clang-format 14.0.6, installs
build tools, clones vcpkg at the pinned baseline, restores a cache of built libraries, then
configures, builds and tests **both** presets in parallel. The workflow token is read-only and
newer pushes cancel older runs. Known gap: CI does not start a Redis, so the Redis store tests
are skipped there.

### `.github/dependabot.yml`
Weekly pull requests for newer GitHub Actions and Docker base images.

### `docs/`
- `architecture.md`: modules, threads and strands, a full game message by message, failure table.
- `milestone-0-explained.md` and `milestones-explained.md`: what each milestone built, how,
  alternatives, and interview questions.
- `file-guide.md`: this document.

### `.workflow/`
The project's memory, written as the work happened: `ROADMAP.md` (the nine milestones and their
status), `SPEC.md` and `PLAN.md` (the milestone 0 spec and plan), `DECISIONS.md` (every decision
with its reason and the alternatives rejected), `EVIDENCE.md` (the exact commands and results
that prove each milestone), `BACKLOG.md` (ideas deliberately left out), `STATE.md`,
`RESEARCH.md`, `MILESTONES.md`, and `SESSION-REPORT-...md`. The `milestones/v0/` folder holds
the archived milestone 0 record.

---

## Part 15: one login, traced through the code

A client sends a `LoginRequest`. Here is every file and function it touches.

1. **`Server::doAccept`** (`src/net/Server.cpp`) accepted the connection earlier and created a
   `Session`; **`Session::start`** posted the first read and started the idle timer.
2. Bytes arrive. **`Session::doRead`** appends them to the **`FrameDecoder`**
   (`Framing.cpp`). `nextFrame()` returns the payload once the whole frame is there.
3. **`Session::admitMessage`** spends a token from the **`RateLimiter`**
   (`core/RateLimiter.cpp`) and counts the message in **`Metrics`**.
4. **`Session::handleFrame`** parses the `Envelope` (code generated from `matchforge.proto`) and
   sees `kLogin`. It calls **`AuthService::login`** with a callback, and returns at once.
5. **`AuthService::login`** (`auth/AuthService.cpp`) posts a task to the **worker pool** and
   returns. On a worker: **`LoginThrottle::isLocked`** is checked (refuse with 429 if locked).
6. **`doLogin`** runs: **`Validation`** checks the formats; **`AccountStore::findPasswordHash`**
   reads `account:<name>` from Redis (`RedisAccountStore::run` posts to the Redis thread and
   waits at most 2 seconds); **`verifyPassword`** (`PasswordHasher.cpp`) runs Argon2id (against
   a dummy hash if the account is missing).
7. On success: **`newSessionToken`** makes a token, **`saveSession`** stores it with a 24-hour
   expiry, **`findRating`** loads the Elo rating. On failure: the throttle records the failure.
   **`Metrics::loginAttempt`** counts the result.
8. The callback (still on the worker) `post`s `handleAuthResult` onto the **session strand**.
9. **`Session::handleAuthResult`** stores the username on the session (so later queue requests
   are allowed) and writes an `AuthResponse` with the token and rating, or an `ErrorResponse`.
10. **`Session::enqueue` / `doWrite`** frame the reply with **`encodeFrame`** and write it.

Four thread hops (I/O thread, worker, Redis thread, back to the session strand) and no thread
ever waits on another while holding a lock. That is the whole concurrency design in one trace.

To see it in a test, read `tests/net/ServerTest.cpp`, `LoginAfterRegisterReturnsASessionAnd
KeepsTheConnection`, and `tests/auth/AuthServiceTest.cpp`.

---

## Part 16: C++ and library ideas, and where to see them

| Idea | What it is | Where |
|---|---|---|
| Interface (abstract class) | A contract many classes can fulfil | `AccountStore`, `LobbyClient` |
| RAII | Resources released by destructors | `IoThread`, `RedisAccountStore` (joins its thread), every `lock_guard` |
| `shared_ptr` / `enable_shared_from_this` | Shared lifetime for async handlers | `Session` |
| `weak_ptr` | A reference that does not keep the object alive | `LobbyService::m_waiting` |
| Move semantics (`std::move`) | Hand a value over instead of copying | every `post` lambda that captures a password or callback |
| Lambdas with captures | Small anonymous functions holding state | handlers in `Session::doRead`, `LobbyService::scheduleTick` |
| `std::optional` | "A value or nothing" without sentinel values | `AccountStore::findRating`, config command line |
| `std::chrono` | Typed time durations | rate limiter, throttle, matchmaker windows |
| Templates | Code generated per type | `RedisAccountStore::run`, `readInt` in the config loader |
| `std::promise` / `std::future` | Wait for a result from another thread | `RedisAccountStore::run` |
| `std::mutex` / `lock_guard` | Mutual exclusion | `InMemoryAccountStore`, the login throttle, bot results |
| Strand | Handlers run one at a time with no mutex | `Session::m_strand`, `LobbyService::m_strand` |
| Async handlers | Start an operation, get called back later | `async_read_some`, `async_write`, `async_accept`, timers |
| Work guard | Keeps an `io_context` running with no pending work | `RedisAccountStore`, `IoThread` |
| `constexpr` | Values fixed at compile time | limits and error codes in several files |
| Dependency injection | Pass dependencies in, do not create them inside | the clock in core; stores, services and `Metrics*` everywhere |
| Constant-time comparison | Comparison whose time does not depend on the data | `secretsMatch` in `LobbyService.cpp` |
| Token bucket | A rate limiter that allows bursts | `RateLimiter` |
| Atomic read-modify-write | One indivisible update | Redis `SETNX`, `INCRBY` in `adjustRating` |

If a term here is new, `docs/milestones-explained.md` explains it where it is first used, and
`.workflow/PLAN.md` has a plain-language glossary of the build and deployment tools.
