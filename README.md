# Texas Hold 'em

A concurrent Texas Hold 'em server in C that serves up to six clients over TCP sockets. The server follows a custom binary packet protocol (`JOIN`, `READY`, `INFO`, `ACK`/`NACK`, `END`) and keeps one synchronized game state across all connections. Game flow runs through dealing, flop, turn, river, and showdown, with hand evaluation from seven cards down to the best five.

The codebase separates networking from poker logic: a state machine drives rounds and betting, while a dedicated module ranks hands from high card through straight flush with full tie-breaking. Clients include an ncurses TUI and scripted/automated drivers for testing.

## Features

- **Protocol** — Structured packet exchange for join, readiness, game info, acknowledgements, and session end.
- **Server** — Non-blocking read discipline so the host can service multiple player ports without blocking the whole loop.
- **Game state** — Tracks stacks, bets, and player status (active, folded, left); advances dealer and turn order when someone leaves.
- **Hand evaluation** — Builds the best five-card hand from hole and board cards and compares hands across standard rankings.
- **Clients** — Interactive TUI (`ncurses`) and automated clients for scripted scenarios.
- **Tooling** — GNU Make build, optional Google Test harness for file-comparison tests, scripted log checks under `scripts/`.

## Tech stack

| Area | Technologies |
|------|----------------|
| Core | C, GCC, POSIX sockets, GNU Make |
| Server | Multi-client TCP, centralized game state, phased rounds |
| Client | Socket clients, ncurses TUI |
| Testing | Google Test (`file_comparison_test`), scripted inputs vs expected logs |
| Dev | Dev Container (Debian, GDB, `libncurses-dev`, Googletest in Dockerfile) |

## Project structure

```
include/          Public headers (protocol, game logic, utilities)
src/server/       Poker server, game logic, client action handling
src/client/       Socket clients, TUI, automation
src/shared/       Shared helpers (e.g. logging, utilities)
scripts/          Test inputs and expected player logs
build/            Makefile output (generated; not committed)
```

Build targets are defined in the `makefile` (e.g. `server.*`, `client.*`, `tui.*`). Use a POSIX environment or the `.devcontainer` so GCC, `make`, and `libncurses-dev` match the project’s expectations.

## Note

This project is for learning and portfolio use. It was generated from [BatDan24/cse220_socket_template](https://github.com/BatDan24/cse220_socket_template). It is not affiliated with any commercial poker or gaming brand.
