# Roadmap (developed by the assistant from CLAUDE.md, at the user's request)

No milestone prompts exist for v1 to v9. The user asked for the remaining features to be built from the overall goal in CLAUDE.md. This order is the one proposed earlier, and the user approved it by asking for it to be built.

| Milestone | Goal | Done when |
|---|---|---|
| v1 | TCP framing, sessions, Heartbeat/ErrorResponse over the wire | Client gets a heartbeat reply; malformed and oversize input is rejected |
| v2 | Accounts and login: Argon2id passwords (libsodium), session tokens, stored in Redis | Register and log in over the network; plaintext passwords are never stored or logged |
| v3 | Matchmaking core in `src/core`: Elo ratings and queues by mode and region | Unit tests show equal-skill players are paired and ratings move correctly |
| v4 | Lobby over the network: join queue, get matched, leave queue | A matched pair of clients receives a match message |
| v5 | Strands and concurrency for shared matchmaking state | Matchmaking state is changed only on its strand; a stress test passes |
| v6 | Ratings persisted in Redis | Ratings survive a server restart |
| v7 | Bot simulator: many fake players that load-test the server | Bots connect, queue, and report throughput |
| v8 | Config and validation: JSON match modes and server settings; every input validated | Bad config is rejected at startup; bad messages are rejected |
| v9 | Prometheus metrics, Grafana dashboard, monitoring in Compose | Prometheus scrapes the server; Grafana shows players online and matches made |

Rules that apply to every milestone (from CLAUDE.md): `src/core` has no networking or Redis; never block an I/O thread; never log passwords or tokens; validate every message; GoogleTest for every new module; one Conventional Commit per task.

## Status (2026-10-13)

| Milestone | Branch | Status |
|---|---|---|
| v0 setup | milestone-0-setup | done (PR #1) |
| v1 framing and sessions | milestone-1-networking | done |
| v2 accounts and login | milestone-2-accounts | done |
| v3 matchmaking core | milestone-3-matchmaking | done |
| v4-v6 lobby, concurrency, ratings | milestone-4-lobby | done |
| v7 bots | milestone-7-bots | done |
| v8 config and limits | milestone-8-config | done |
| v9 metrics and dashboards | milestone-9-monitoring | done |

Each branch is built on the one before it, so milestone-9-monitoring contains everything.
