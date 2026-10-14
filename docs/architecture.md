# matchforge architecture

This document explains how the parts of matchforge fit together: which module does what,
which threads run what, and what happens on the wire when players log in, queue, get
matched, and have a result reported.

---

## 1. The big picture

```
                 TCP :7777 (length-prefixed Protobuf)
  game clients  ───────────────────────────────────────►  matchforge_server
  bots          ───────────────────────────────────────►   ├─ net      (Server, Session, framing)
  game servers  ── ReportMatchResult (+ shared secret) ─►   ├─ auth     (Argon2id, tokens, throttle)
                                                            ├─ lobby    (queue service, results)
                                                            ├─ core     (Matchmaker, Elo, limits) ← pure
                                                            ├─ store    (Redis / in-memory)
                                                            ├─ config   (JSON settings)
                                                            └─ metrics  (Prometheus)
                                                                 │            │
                                                     Redis :6379 ┘            └ HTTP :9100 /metrics
                                                                                    │
                                                              Prometheus :9090 ─────┘
                                                                    │
                                                              Grafana :3000 (dashboard)
```

## 2. Modules and the rules between them

| Module | Library | Depends on | Purpose |
|---|---|---|---|
| `proto/` | `matchforge_proto` | Protobuf | Generated message classes |
| `src/core/` | `matchforge_core` | nothing | Elo, Matchmaker, RateLimiter, LoginThrottle, validation |
| `src/store/` | `matchforge_store` | Boost.Redis | `AccountStore` interface; Redis and in-memory versions |
| `src/auth/` | `matchforge_auth` | core, store, libsodium | Register, login, password hashing, session tokens |
| `src/lobby/` | `matchforge_lobby` | core, store, proto | Queue service on one strand; announces matches; applies results |
| `src/net/` | `matchforge_framing`, `matchforge_net` | auth, lobby, proto, Asio | Framing, one `Session` per connection, the `Server` acceptor |
| `src/config/` | `matchforge_config` | core, nlohmann-json | Loads and validates `config/server.json` |
| `src/metrics/` | `matchforge_metrics` | prometheus-cpp | Counters, gauges, histograms |
| `src/server/` | `matchforge_server` | everything | Wires it all together in `main()` |
| `src/bots/` | `matchforge_bots` | framing, proto | Load simulator |

Two rules keep the design testable:

1. **`core` has no dependencies.** It does not know about sockets, Redis, threads or clocks.
   The matchmaker, Elo and the limiters receive the current time as an argument, so tests can
   say "40 seconds later" without waiting 40 seconds.
2. **Dependencies point inward.** `net` knows about `lobby` and `auth`; they do not know about
   `net`. The lobby talks back to players through a tiny interface (`LobbyClient`) that
   `Session` implements. That is the *dependency inversion* idea: the inner module defines
   the interface, the outer module implements it.

## 3. Threads, strands and who owns what

The server runs three kinds of threads:

| Threads | How many | Runs | Must never |
|---|---|---|---|
| I/O threads | one per CPU core (at least 2) | every network callback, the lobby, timers | block (no sleeps, no Redis, no hashing) |
| Worker threads | `workerThreads` (default 4) | Argon2id hashing, every Redis call | touch session or lobby state directly |
| Redis I/O thread | 1 | the Boost.Redis connection | anything else |

A **strand** is a queue of handlers that Asio runs one at a time, even when many I/O threads
are available. matchforge uses strands instead of mutexes:

- **Each `Session` has its own strand.** Its reads, writes, timers and replies never run at the
  same time, so its write queue and state need no lock.
- **The `LobbyService` has one strand** that owns the whole queue (`Matchmaker`), the list of
  waiting connections and the list of reportable matches. Sessions do not lock anything:
  they *post* a request to the lobby strand and return. This is the CLAUDE.md rule
  "shared matchmaking state is owned by one strand; other code posts to it".

Crossing between them always uses `post`:

```
Session strand ──post──► worker pool (hash password, read Redis)
worker pool    ──post──► lobby strand  (put the player in the queue)
lobby strand   ──deliver()──► Session::deliver ──post──► Session strand (write MatchFound)
```

Because every hop is a post, no thread ever waits for another, and no I/O thread ever blocks.

## 4. A complete game, message by message

```
client A                server                                   Redis
   │ RegisterRequest ──►  Session ─post─► worker: validate, Argon2id hash
   │                                             SETNX account:a <hash> ───────────► ok
   │                                             SET session:<token> a EX 86400 ───► ok
   │ ◄── AuthResponse{token, rating 1500}  (back on the session strand)
   │ JoinQueueRequest{duel, eu} ──► lobby.join ─post─► worker: GET rating:a ───────► (none)
   │                                                ─post─► lobby strand: Matchmaker.join
   │ ◄── QueueStatus{queued}
   ...                 every 250 ms the lobby strand runs Matchmaker.findMatches(now)
   │ ◄── MatchFound{id, both rosters, your_team 0}         (B gets the same with your_team 1)

game server
   │ ReportMatchResult{id, winning_team 0, secret} ──► lobby strand: check secret (constant
   │                                                   time), check id, remove it (once only)
   │                                                ─post─► worker: SETNX rating:a 1500,
   │                                                         INCRBY rating:a +16 (and -16 for B)
   │ ◄── MatchResultAck{new ratings}
```

## 5. Failure handling at a glance

| Problem | What happens |
|---|---|
| Frame length above `maxFrameBytes` | Connection closed before the body is read |
| Bytes that are not a valid Envelope | `ErrorResponse 400`, then the connection closes |
| Valid but unexpected message | `ErrorResponse 400`, connection stays open |
| Queue request before login | `ErrorResponse 401` |
| Too many messages | `ErrorResponse 429`; after 50 refusals in a row, disconnect |
| Silent connection | Closed after `idleTimeoutSeconds` |
| 5 wrong passwords for one name | That name is locked for 5 minutes (`429`) |
| Redis down | Commands time out after 2 s; the client gets `ErrorResponse 503` |
| Player disconnects while queued | Removed before the next matching pass |
| Same match reported twice | Second report gets `404` |
| Wrong result secret | `403`; nothing changes |
| Bad config file | Server refuses to start, with the exact setting named |

## 6. What is not built (yet)

- **TLS.** Passwords and tokens cross the network in plain text. A real deployment needs TLS
  (Asio supports it; OpenSSL is already a dependency).
- **Horizontal scaling.** The queue lives in one process. Many servers would need the queue
  in a shared store, or one matchmaker per region.
- **Per-IP limits.** Rate limits are per connection and per username, not per address.
- **Reconnect into a found match.** A player who drops right after `MatchFound` is not
  told again.
