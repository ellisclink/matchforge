# Spec: matchforge v1 - TCP framing and sessions

## Goal
Clients can connect to the server over TCP, send framed Protobuf messages, and get replies. This is the networking base for every later milestone.

## Users and main flows
- A client connects, sends a Heartbeat, and receives a Heartbeat back with the server's clock.
- A client sends a malformed message and receives an ErrorResponse, then the connection closes.
- A client sends a frame larger than the limit, and the connection closes without a reply.

## In scope
- Length-prefixed framing (4-byte big-endian length), with a size limit.
- A Session per connection, serialized by a strand.
- A Server that accepts connections, on a port from the command line (default 7777).
- The server program runs until Ctrl+C or SIGTERM, then shuts down cleanly.
- Unit tests for framing; integration tests for the server.

## Non-goals
- Login, accounts, lobby, or matchmaking (v2 and later).
- Redis (not used yet).
- Configuration files (v8). The port and frame limit are fixed values for now.

## Acceptance criteria
- [ ] AC-1: `ctest --preset debug` passes, including 12 tests (framing, server, and earlier tests)
- [ ] AC-2: `ctest --preset release` passes
- [ ] AC-3: the running server answers a Heartbeat frame with a Heartbeat frame that carries a non-zero clock value
- [ ] AC-4: a malformed frame gets an ErrorResponse, then the connection closes
- [ ] AC-5: a frame above 64 KiB closes the connection without a reply
- [ ] AC-6: Ctrl+C stops the server cleanly (log line "server stopped", exit 0)
- [ ] AC-7: `docker compose up` starts the server and it stays running; the published port 7777 answers a heartbeat
- [ ] AC-8: `clang-format --dry-run --Werror` passes on all sources
- [ ] AC-9: CI passes on both presets

## Assumptions (see DECISIONS.md)
- D-015 (roadmap), D-016 (server now runs until stopped; supersedes D-011's exit-after-log), D-017 (64 KiB limit), D-018 (error policy)
