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
## D-009: Call protoc directly instead of protobuf_generate (2026-10-12)
- Decision: proto/CMakeLists.txt uses add_custom_command to run protoc. It does not use the protobuf_generate helper.
- Reason: The helper's TARGET form needs CMake 3.23. This machine has CMake 3.22.1.
- Rejected alternatives: Upgrading CMake (extra install, and the user did not ask for it).
## D-020 (2026-10-13): OpenSSL added as an explicit dependency
- Decision: vcpkg.json lists openssl; CMake finds OpenSSL and links it into matchforge_store.
- Reason: Boost.Redis includes Asio's SSL headers even without TLS. The boost-redis port does not install OpenSSL, so the build failed with "openssl/conf.h: No such file".
- Not used: TLS. Connections to Redis and clients are plain for now (see BACKLOG.md).
## D-021 (2026-10-13): Password and session design (Milestone 2)
- Hashing: Argon2id with libsodium's interactive limits (about 64 MB and 50 to 100 ms per hash). Hashing runs on 4 worker threads, never on network threads.
- Login timing: a dummy hash is checked when the account does not exist, so response time does not reveal which usernames exist.
- Errors: wrong password and unknown user give the same 401 message.
- Usernames: stored lowercase, so "Alice" and "alice" are one account.
- Sessions: 256-bit random token (URL-safe base64, 43 characters), kept in Redis for 24 hours.
- Rejected alternatives: bcrypt (less memory-hard), scrypt (libsodium Argon2id is the choice in CLAUDE.md), plain sessions in memory (lost on restart).
## D-022 (2026-10-13): Redis calls are blocking calls made from worker threads
- Decision: RedisAccountStore runs Boost.Redis on its own I/O thread. Callers (auth workers) wait for each command to finish.
- Reason: Boost.Redis objects are not thread-safe, and CLAUDE.md forbids blocking network threads. Blocking a worker thread is acceptable.
- Known limit: one connection serves all workers. Under load, use a connection pool (backlog).
## D-023 (2026-10-13): Project version 0.2.0
- Decision: project() VERSION and vcpkg version-string are 0.2.0.
- Reason: Milestone 2 adds accounts.
## D-019 (2026-10-13): Runtime image is ubuntu:24.04 again; supersedes D-010
- Decision: the runtime stage uses ubuntu:24.04, the same release as the build stage.
- Reason: from v1 the server is built with Ubuntu 24.04's C library and C++ runtime (GLIBC_2.38, GLIBCXX_3.4.32). debian:bookworm-slim does not have them. With bookworm, the container crashed on start: "version GLIBCXX_3.4.32 not found". The earlier bookworm test passed only because the Milestone 0 binary used fewer symbols. The risk noted in D-010 happened.
- Rejected alternatives: building the binary on bookworm (would need a Debian build stage); static linking (more work for little gain now).
- Lesson: test the runtime image with the binary you ship, every time the code changes.
## D-015 (2026-10-13): Remaining milestones built from the overall goal
- Decision: v1 to v9 follow ROADMAP.md. The user asked for the rest of the features to be built from CLAUDE.md's overall goal, with no milestone prompts.
- Reason: the user delegated this. The order is the one proposed earlier.
- Rejected alternatives: waiting for milestone prompts (the user does not have them).
## D-016 (2026-10-13): The server now runs until stopped; supersedes D-011's exit-after-log behavior
- Decision: matchforge_server listens on a port (default 7777) and runs until SIGINT or SIGTERM. Compose uses restart: unless-stopped and publishes port 7777.
- Reason: a server that exits cannot accept clients. D-011 was written for Milestone 0 only.
- Rejected alternatives: keeping the exit-after-log behavior behind a flag (adds a mode with no use).
## D-017 (2026-10-13): Frame size limit is 64 KiB
- Decision: any frame whose length header is above 65536 bytes closes the connection without a reply.
- Reason: a limit stops a client from making the server buffer unlimited memory. 64 KiB is far above any planned message.
- Rejected alternatives: no limit (unsafe); a configurable limit now (config arrives in v8).
## D-018 (2026-10-13): Error policy
- Decision: a message that does not parse gets ErrorResponse code 400 and the connection then closes. A valid envelope with no payload or with an ErrorResponse gets ErrorResponse 400 and the connection stays open.
- Reason: after a bad frame the byte stream is not trustworthy, so the connection closes. A well-formed but unsupported message is only a bad request, so the connection stays open.
## D-011 (ACCEPTED 2026-10-13): AC-5 means both containers start
- Decision: the server container starts, logs its version, and exits 0. Redis stays running and healthy. This is how AC-5 is read.
- Reason: AC-3 requires the server to exit after logging. A long-running server belongs to a later milestone.
- User decision: accepted by the user on 2026-10-13.
## D-014 (ACCEPTED 2026-10-13): Second test for Protobuf round-trip
- Decision: tests/proto/EnvelopeTest.cpp adds a test that sets a Heartbeat in an Envelope, serializes it, parses it back, and checks the payload and timestamp. AC-4 now means two tests.
- Reason: Compiling the generated code does not prove it works at runtime. This test does.
- User decision: accepted by the user on 2026-10-13.
- Rejected alternatives: keeping one test (does not prove the Protobuf code runs).
## D-010 (REVISED 2026-10-13): Runtime image is debian:bookworm-slim, as in D-007
- Decision: The build stage uses ubuntu:24.04. The runtime stage uses debian:bookworm-slim.
- Reason: First I believed bookworm's older libstdc++ might not run the GCC 13 binary. I tested it: the binary built in the image runs on debian:bookworm-slim and exits 0. The server also runs as the non-root user matchforge there. Bookworm is smaller and matches the option accepted in D-007, so no deviation is needed.
- Risk to watch: the binary is built with GCC 13 and runs on bookworm's GCC 12 libstdc++. This works today. If a later milestone breaks it, switch the runtime stage to a newer Debian or Ubuntu image.
- Rejected alternatives: ubuntu:24.04 for runtime (works, but larger than needed).
## D-011 (INTERPRETATION, needs your OK): AC-5 means both containers start
- Decision: compose sets restart: "no" on the server. The server prints its version and exits, so its container ends with exit 0. Redis stays running and healthy.
- Reason: AC-3 requires the server to exit after logging. "Starts both containers" can only mean the server starts and exits cleanly. A long-running server arrives in a later milestone.
## D-012: Strict warnings are an INTERFACE target (2026-10-12)
- Decision: matchforge_warnings carries -Wall -Wextra -Wpedantic -Werror. Our targets link to it. Generated protobuf code is excluded.
- Reason: One place to change the rules; generated code is outside our control.
## D-013: Fix for libsodium build (2026-10-12)
- Decision: autoconf-archive is added to the apt installs in CI and the Dockerfile. It was missing and libsodium's build fails without it.
## D-008: Spec and plan approved (2026-10-12)
- Decision: SPEC.md (7 acceptance criteria) and PLAN.md (4 phases) approved as written. Phase 1 may begin.
- Reason: User review, including the plain-English summary.

## D-024 (2026-10-13): Matching algorithm
- Decision: sort each (mode, region) bucket by rating; accept a run of 2*teamSize consecutive players when its spread fits the widest waiting window in the run (base 100, +10 per second, max 600); split teams with a snake draft.
- Reason: simple, O(n log n), deterministic, and a long-waiting player is always eventually matched.
- Rejected alternatives: global assignment minimising total spread (more complex, harder to explain); narrowest window in the group (newcomers would block long waiters).
## D-025 (2026-10-13): Lobby on one strand with a 250 ms matching tick
- Decision: LobbyService owns the Matchmaker on a single strand; sessions post requests; a timer runs findMatches every tick (configurable).
- Reason: CLAUDE.md concurrency rule; no locks; batching gives the matcher more choice and bounds CPU.
- Rejected alternatives: mutex around the matchmaker; matching on every join.
## D-026 (2026-10-13): Results come from game servers with a shared secret
- Decision: ReportMatchResult must carry MATCHFORGE_RESULT_SECRET (env only), compared in constant time; each match can be reported once, within 2 hours.
- Reason: players cannot be trusted to report their own wins.
- Rejected alternatives: player votes (collusion), per-match signed tokens (more moving parts; possible later).
## D-027 (2026-10-13): Rating updates
- Decision: Elo with team-average ratings, K = 32; change computed from match-time ratings and applied with SETNX + INCRBY.
- Reason: atomic relative updates never lose a result when two arrive for one player at once.
- Rejected alternatives: read-modify-write SET (lost updates).
## D-028 (2026-10-13): Bot simulator design
- Decision: one blocking client thread per bot; report p50/p95; compose "load" profile.
- Reason: simple and realistic enough for hundreds of bots; percentiles expose tail latency.
## D-029 (2026-10-13): Configuration file
- Decision: config/server.json via nlohmann-json; optional fields with defaults; strict types, ranges and unknown-key errors; secrets only from the environment.
## D-030 (2026-10-13): Per-connection limits
- Decision: token bucket 20 msg/s with burst 40 (429 when empty, disconnect after 50 refusals in a row, after flushing replies); idle timeout 60 s.
## D-031 (2026-10-13): Login lockout per username
- Decision: 5 wrong passwords in 5 minutes lock the name for 5 minutes; locked attempts get 429 without hashing.
- Trade-off accepted: someone can lock a known username for 5 minutes. Per-IP limits are backlog.
## D-032 (2026-10-13): Metrics and dashboards
- Decision: prometheus-cpp pull endpoint on :9100; components take an optional Metrics*; Prometheus scrapes every 5 s; Grafana provisioned from repo files; both UIs bound to 127.0.0.1.
- Rejected alternatives: push gateway (extra service), global metrics singleton (hidden state in tests).
## D-033 (2026-10-13): Framing as its own library
- Decision: matchforge_framing holds only Framing.cpp so clients (bots) do not link the server.
## D-034 (2026-10-13): Version 0.9.0
- Decision: project and vcpkg version 0.9.0 after milestone 9. 1.0.0 is reserved for after TLS and a review.
