# Milestones 1 to 9 explained

Milestone 0 (project setup) is explained in [milestone-0-explained.md](milestone-0-explained.md).
This document covers everything after it. Each milestone has four parts, as CLAUDE.md asks:
what was built, how it works, the design choices, and interview questions with model answers.

For how the pieces fit together, read [architecture.md](architecture.md) first.

---

## Milestone 1: TCP framing and sessions

### What was built
- `src/net/Framing.*`: `encodeFrame` puts a 4-byte big-endian length in front of a message;
  `FrameDecoder` collects incoming bytes and hands back whole messages.
- `src/net/Session.*`: one object per connected client. It reads, decodes, answers, writes.
- `src/net/Server.*`: the listening socket. It accepts clients and starts a Session for each.
- The server program now runs until Ctrl+C or `docker stop`.

### How it works
TCP is a byte stream with no message boundaries: one read can return half a message or
three messages at once. The 4-byte length prefix tells the receiver where each message
ends. `FrameDecoder::nextFrame` returns nothing until a full frame has arrived, and it
refuses a length above the limit *before* reading the body, so a client cannot make the
server buffer gigabytes.

Each Session reads with `async_read_some`. "Async" means the call returns at once and Asio
calls a handler later, when data arrives; while waiting, the thread serves other clients.
Writes go through a queue, because only one `async_write` may run on a socket at a time.

### Design choices
- **Length prefix instead of a delimiter** (like a newline): Protobuf bytes can contain any
  value, so no delimiter is safe.
- **`shared_ptr` + `enable_shared_from_this` for sessions:** a session must live until its
  last pending operation finishes. Each handler holds a `shared_ptr`, so the session is
  destroyed exactly when nothing refers to it anymore. This is one of the few places where
  shared ownership is real (CLAUDE.md allows it for "Asio session lifetimes").
- **Malformed input closes the connection; unsupported input does not:** after a bad
  frame the stream can no longer be trusted to line up on frame boundaries.

### Interview questions
**Q: Why can't you just read 100 bytes and treat that as one message?**
A: TCP does not keep message boundaries. A read returns whatever bytes have arrived, which
may be part of a message or several messages. Framing adds the boundaries back.

**Q: What happens if a client sends a length header of 2 GB?**
A: The decoder compares the length with the limit as soon as the 4 header bytes arrive and
fails immediately, and the session closes the connection without allocating anything.

**Q: Why does the session use `shared_from_this()` in its handlers?**
A: The handler may run after the code that created the session has moved on. Capturing a
`shared_ptr` keeps the session alive until the handler has run, avoiding a use-after-free.

---

## Milestone 2: accounts and login

### What was built
- `RegisterRequest`, `LoginRequest`, `AuthResponse` messages.
- `src/core/Validation.*`: username (3 to 20 letters, digits, underscore) and password (8 to
  128 bytes) rules.
- `src/auth/PasswordHasher.*`: Argon2id hashing and 256-bit session tokens via libsodium.
- `src/auth/AuthService.*`: register and login, run on worker threads.
- `src/store/`: the `AccountStore` interface, an in-memory version and a Redis version.

### How it works
A password is never stored. `crypto_pwhash_str` turns it into a string holding the
algorithm settings, a random salt and the hash; at login, `crypto_pwhash_str_verify` checks a
password against that string. Argon2id is deliberately slow (about 64 MB of memory and
50 to 100 ms per hash), so an attacker who steals the database can try only a few guesses per
second per core.

Because hashing is slow, it runs on a worker pool, never on a network thread. The Redis
store is used the same way: Boost.Redis runs on its own thread, and each command waits for
its reply on the worker thread that called it, with a 2-second timeout.

### Design choices
- **Argon2id over bcrypt:** memory-hard, so GPU cracking is expensive; CLAUDE.md requires it.
- **Same error for "no such user" and "wrong password", plus a dummy hash check** for unknown
  users: neither the message nor the response time reveals which usernames exist.
- **`SETNX` to create accounts:** "check the name is free" and "claim it" are one atomic
  Redis command, so two simultaneous registrations cannot both succeed.
- **Interface + in-memory implementation:** tests run without Redis; the Redis version has
  its own tests that run when a test Redis is available.

### Interview questions
**Q: Why hash passwords instead of encrypting them?**
A: Encryption can be reversed by anyone with the key. A hash cannot be reversed; the server
only needs to check a password, never to read it back.

**Q: What is a salt and why does each password get a new one?**
A: Random bytes mixed into the hash. With a unique salt, two users with the same password get
different hashes, and precomputed tables of common passwords become useless.

**Q: Why not call Redis directly from the network thread?**
A: A Redis call waits for the network. A network thread that waits cannot serve anyone else,
so one slow Redis reply would stall every connected client on that thread.

---

## Milestone 3: the matchmaking core

### What was built
- `src/core/Elo.*`: expected score and rating change.
- `src/core/Matchmaker.*`: queues per mode and region; `findMatches(now)` forms matches.

### How it works
**Elo.** The chance that A beats B is `1 / (1 + 10^((B - A) / 400))`. After the game, A's
rating changes by `K * (actual - expected)`. With K = 32, beating an equal player gives +16;
an upset over a player 200 points stronger gives +24.

**Matching.** Players in the same mode and region are sorted by rating. The matchmaker
slides along the sorted list looking at runs of `2 * teamSize` neighbours. A run becomes a
match when its rating spread (highest minus lowest) fits the **rating window** of the
longest-waiting player in it. The window starts at 100, grows by 10 per second of waiting,
and stops at 600. Teams are split with a **snake draft** (A, B, B, A, A, B...) so their
total ratings are close.

### Design choices
- **Pure logic, time passed in:** fully deterministic tests ("after 20 seconds the window is
  300") with no sleeping and no threads.
- **Widest window in the group:** guarantees a long-waiting player is eventually matched;
  the cost is that a newcomer may get a slightly wider gap.
- **Hard maximum window:** some matches are worse than no match; 600 points is roughly a
  97% expected win for the stronger side.
- **Greedy over sorted runs, not a global optimum:** O(n log n), easy to reason about, and
  good in practice. A global assignment (for example, minimising total spread) is a
  possible later improvement.

### Interview questions
**Q: Why does the rating window grow with time?**
A: It trades match quality for waiting time. At first only close matches are accepted; a
player who has waited long accepts a wider gap, so nobody waits forever.

**Q: Why are only consecutive players in the sorted list considered as a group?**
A: For any group of n players, the consecutive run in sorted order has the smallest
possible spread among groups starting at that player, so skipping players can never give a
tighter group.

**Q: How do you test time-dependent logic without waiting?**
A: Pass the time in as a parameter (dependency injection of the clock). The test chooses
`start + 20s` directly.

---

## Milestones 4, 5 and 6: lobby, concurrency and rated results

These three were built together because they share one design.

### What was built
- Messages: `JoinQueueRequest`, `LeaveQueueRequest`, `QueueStatus`, `MatchFound` (both
  rosters and `your_team`), `ReportMatchResult`, `MatchResultAck`.
- `src/lobby/LobbyService.*`: the queue as a service; `LobbyClient` interface for replies.
- Ratings stored in Redis (`rating:<name>`), read on join, updated on results.
- Tests over real TCP, including 16 clients on separate threads queueing at once.

### How it works
All lobby state lives on **one strand**. `join` first reads the player's rating on a worker
(Redis), then posts the player onto the lobby strand. A timer on the same strand runs a
matching pass every 250 ms: it drops players whose connection closed, forms matches, and
sends each player `MatchFound` through `LobbyClient::deliver`, which hops onto that
session's strand before writing.

A finished match is reported by a **game server** with a shared secret
(`MATCHFORGE_RESULT_SECRET`). The lobby checks the secret in constant time, removes the match
from its table (so it can only be reported once), computes the Elo change from the team
averages, and a worker applies it with `SETNX` + `INCRBY`.

### Design choices
- **A strand instead of a mutex:** no locks, no deadlocks, and the matchmaker stays the
  simple single-threaded class from Milestone 3.
- **Tick-based matching (every 250 ms) instead of matching on every join:** batching gives
  the algorithm more players to choose from and bounds CPU use under load.
- **Results from game servers, not from players:** players could lie about who won. A shared
  secret is the simplest trust model; per-server keys or signed tokens would be the next step.
- **Constant-time secret comparison:** a normal comparison stops at the first wrong character,
  which leaks through timing how much of a guess was right.
- **`INCRBY` (relative change) instead of `SET` (absolute value):** a player can finish two
  matches whose reports arrive at the same time; adding each change atomically means neither
  update is lost.
- **`weak_ptr` to waiting connections:** the lobby must not keep a closed connection alive;
  an expired pointer simply means "this player left".

### Interview questions
**Q: How does the lobby avoid race conditions without a mutex?**
A: Only handlers on the lobby strand touch lobby state, and a strand runs one handler at a
time. Other threads never call into the state directly; they post work to the strand.

**Q: Why is the rating read on a worker thread before joining the queue?**
A: Reading Redis waits on the network. Doing it on the lobby strand would freeze matching
for everyone during that wait.

**Q: What stops a player from reporting a fake win?**
A: Reports require the result secret, which only game servers have. Players never see it.

**Q: Two results for the same player arrive at the same time. What prevents a lost update?**
A: Each result is applied as `INCRBY delta`, which Redis executes atomically, instead of
reading the rating, adding, and writing it back.

---

## Milestone 7: bot load simulator

### What was built
- `matchforge_bots`: N bot threads, each registering, queueing and getting matched for R
  rounds; with `--secret`, one bot per match reports a random result.
- A report: matches joined, errors, unmatched, and time-to-match average, p50, p95, max.
- A compose service under the `load` profile.

### How it works
Each bot is a blocking client on its own thread (blocking is fine in a test client). It
measures the time from sending `JoinQueueRequest` to receiving `MatchFound`. Percentiles use
the nearest-rank method: p95 is the value at position `ceil(0.95 * n)` in sorted order.

### Design choices
- **Percentiles, not just the average:** an average hides a slow tail; p95 shows what
  the unluckiest players experience.
- **Exit code reflects failures:** a script or CI job can treat a bad run as a failure.
- **Reusable library (`matchforge_botlib`) with tests:** the option parser and the math are
  checked without running a load test.

### Interview questions
**Q: Why report p95 and not only the mean?**
A: Latency is skewed. A mean of 250 ms can hide 5% of players waiting 2 seconds; p95 makes
that visible.

**Q: How did the bots reveal behaviour, not just bugs?**
A: After a few rounds with random results, ratings spread apart, and p95 rose because
far-apart players waited for their windows to widen, as designed.

---

## Milestone 8: configuration and abuse limits

### What was built
- `config/server.json` and `src/config/ServerConfig.*` (nlohmann-json).
- `core::RateLimiter` (token bucket) and per-connection idle timeout.
- `core::LoginThrottle`: 5 wrong passwords lock a username for 5 minutes.

### How it works
**Config.** Every field is optional; missing fields keep defaults. Types and ranges are
checked, unknown keys are errors, and related fields are checked together. A bad file stops
the server at startup with the exact setting named. Secrets never come from the file.

**Token bucket.** Each connection has a bucket of `messageBurst` tokens refilled at
`messagesPerSecond`. Each message spends one; with none left the server answers `429`. After
50 refusals in a row it sends the queued replies and hangs up.

**Idle timeout.** A per-session timer is pushed back on every received message; if it
fires, the connection is closed. Heartbeats keep idle-but-connected clients alive.

### Design choices
- **Unknown keys are errors:** a typo like `kfactor` would otherwise silently keep the default.
- **Token bucket:** allows natural bursts (a client sending a few messages at once) while
  capping the long-run rate, with O(1) memory and lazy refill (no timer needed).
- **Per-username lockout:** stops password guessing that reconnects to dodge per-connection
  limits. The trade-off (someone can lock you out for 5 minutes) is accepted and documented.

### Interview questions
**Q: Why a token bucket instead of "at most N messages per second" windows?**
A: Fixed windows let a client send 2N messages around a window boundary. A token bucket
smooths the rate and still allows short bursts.

**Q: Why is the result secret not in the config file?**
A: Config files get committed, copied and shared. Secrets come from the environment (or a
secret manager), so they are never in the repository.

---

## Milestone 9: metrics and dashboards

### What was built
- `src/metrics/Metrics.*`: Prometheus counters, gauges and a histogram.
- `/metrics` HTTP endpoint on port 9100 (configurable; 0 turns it off).
- Compose services: Prometheus (scrapes every 5 s) and Grafana with a provisioned dashboard.

### How it works
Components hold an optional `Metrics*`. Sessions count connections and messages, auth counts
logins by result, and the lobby records matches by mode, players queued and queue wait.
prometheus-cpp uses atomics, so any thread can record without locks. Prometheus pulls the
values; Grafana queries Prometheus, for example
`histogram_quantile(0.95, sum by (le) (rate(matchforge_queue_wait_seconds_bucket[5m])))`
for the p95 queue wait.

### Design choices
- **Pull (Prometheus scrapes) over push:** the server does not need to know where monitoring
  lives, and a dead server shows up as a failed scrape.
- **Histogram for queue wait:** percentiles can be computed across time and across servers;
  a pre-computed percentile cannot be combined.
- **Nullable pointer injection:** tests and tools pay nothing; no global state.
- **Provisioned Grafana:** the dashboard and data source come from files in the repo, so
  every environment gets the same dashboard.
- **Monitoring UIs bound to localhost:** Prometheus has no login; it must not be public.

### Interview questions
**Q: What is the difference between a counter and a gauge?**
A: A counter only goes up (total logins); graphs show its rate. A gauge goes up and down
(players queued right now).

**Q: Why use a histogram instead of recording the average wait?**
A: An average hides the tail and cannot be split into percentiles later. Histogram buckets
can be summed across time ranges and servers, then turned into any percentile.
