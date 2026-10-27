# Homework - CSE 320 - Multithreaded Tank Server
#### Professor Daniel Benz

## Introduction

In this assignment you will write the server for a real-time, multiplayer
tank-shooting game. Up to 16 players connect over TCP, drive tanks around an
arena, and shoot at each other. The server owns the game world, applies each
command as it arrives, advances bullets on a fixed clock, and streams the
world to every client many times per second.

Many clients and a game clock all read and write the same world at the same
time, so this homework practices:

- creating and joining POSIX threads;
- finding every shared variable and protecting it with semaphores (`P`/`V`);
- the lecture patterns: the **prethreaded server with `sbuf`**, the
  **producer-consumer problem**, and the **first readers-writers problem**;
- thread-safe code, and avoiding races and deadlocks;
- finding concurrency bugs with `helgrind`, `drd`, and ThreadSanitizer.

**YOU MUST WORK ON THIS ASSIGNMENT ON YOUR OWN. IF YOU SUBMIT CODE FROM ANOTHER STUDENT YOU WILL BE BROUGHT UP ON ACADEMIC DISHONESTY CHARGES!**

**CONCURRENCY BUGS ARE HARD TO FIND. START EARLY.**

A concurrent program that worked once is not necessarily correct. Your server
must be free of data races, not just appear to work: reason about every
shared variable.

## What you change

The starter compiles, but every function in `src/` is unfinished:
value-returning functions return placeholders, and void functions do nothing.
Replace those bodies, and add any `static` helper functions you need. Do not
change a function's signature.

| You edit | What goes there |
| --- | --- |
| `src/sync.c` | Semaphore, pthread, and memory wrappers |
| `src/sbuf.c` | The lecture's `sbuf` |
| `src/msgq.c` | Per-client outboxes |
| `src/net.c` | Sockets and robust I/O |
| `src/protocol.c` | Command parsing |
| `src/game.c` | Game rules and the readers-writers lock |
| `src/session.c` | Worker threads, sessions, sender threads, command handlers |
| `src/server.c` | Registry, broadcast, counters, tick thread, accept loop, shutdown |
| `src/log.c` | Verbose-mode logging |
| `src/main.c` | Argument parsing |

**Supplied, and not yours to edit:** every file in `include/` (the comments
there are part of this specification), `maps/`, and `clients/`.

## The Game

The arena is a grid of wall (`#`) and floor (`.`) cells; (0, 0) is the
top-left corner. Each tank sits on one floor cell and faces `N` (up), `E`,
`S`, or `W`. The constants live in `game.h`.

| Rule | Value |
| ---- | ----- |
| Players | at most 16; names are 1-15 chars of `[A-Za-z0-9_-]`, unique |
| Hit points | 3; the third hit destroys a tank |
| `MOVE d` | turn to `d`, then advance one cell if it is floor and has no live tank |
| Cooldowns | 2 ticks between moves, 5 ticks between shots |
| Bullets | at most 3 in flight per tank; each moves 2 cells per tick |
| Respawn | 40 ticks after being destroyed, on a random free cell |
| Clock | 20 ticks per second by default (`-t`) |

Details:

- A blocked move (wall, edge, or live tank) still turns the tank but does not
  start the cooldown.
- `FIRE` with a tank directly in front hits it immediately; into a wall, the
  shot is wasted (but still counted); otherwise a bullet appears in front.
- Each tick, bullets move one cell at a time. A bullet entering a wall
  disappears; a bullet entering (or driven into by) another live tank hits it
  and disappears.
- When a tank is destroyed, the shooter's `kills` and the victim's `deaths`
  go up. A destroyed tank has `hp` 0, takes no cell, and gets `ERR DEAD` for
  actions. A tank that joins a full map starts destroyed and spawns when a
  cell frees up.
- A player who leaves takes its tank and bullets along. Tank ids start at 1
  and are never reused.

A correct server always keeps these **invariants**, in every frame: no two
live tanks share a cell; nothing sits on a wall; live tanks have 1-3 hp and
destroyed tanks 0; at most 3 bullets per tank; total kills equal total
deaths; total hits equal total damage taken; and `STATS` equals the sum of
the per-tank counters. Missing locks break these under load.

## Building and Running

On Ubuntu, install the tools with:
`sudo apt install build-essential python3 python3-tk valgrind netcat-openbsd`.

| Command | What it does |
| ------- | ------------ |
| `make` | build `bin/tank_server` |
| `make debug` | build with `-g` and the `debug.h` macros enabled |
| `make tsan` | build `bin/tank_server_tsan` with ThreadSanitizer |
| `make clean` | remove `bin/` and `build/` |

```
Usage: bin/tank_server -p port [options]
  -p port      TCP port to listen on (required)
  -h           Print this help message
  -m map_file  Arena map file (default: built-in arena)
  -t rate      Game ticks per second, 1-100 (default: 20)
  -w workers   Worker threads, 1-64 (default: 32)
  -d seconds   End the match after this many seconds (default: 0 = never)
  -s seed      Random seed for spawn positions (default: current time)
  -v           Verbose: log connections, joins, and leaves to stdout
```

Bad arguments print the matching `PRINT_ERROR_*` from `global.h` to stderr and
exit with `EXIT_FAILURE`. The server prints `Tank server listening on port
9000 (20 ticks/s, 32 workers)` at start, one log line per event with `-v`,
and `Server shutting down`, `Final scores:`, and a score table when it stops
(Ctrl-C, `SIGTERM`, or the end of a `-d` match). Nothing else goes to stdout.

To play, start the server and then any mix of clients:

```bash
bin/tank_server -p 9000 -v
python3 clients/gui_client.py -p 9000 --name alice   # arrows/WASD, Shift+arrow turns, Space fires, Enter chats
python3 clients/bot_client.py -p 9000 -n 5           # CPU players (-s hunter|wander|sniper|spammer|mix)
python3 clients/gui_client.py -p 9000 --watch        # spectator
nc localhost 9000                                    # type commands by hand
```

The maps in `maps/` can be loaded with `-m`, for example
`bin/tank_server -p 9000 -m maps/maze.txt`. `maps/arena.txt` is the built-in
arena.

## Server Architecture

```
 clients --> main thread (accept) --sbuf_insert--> [ sbuf of connfds ]
                                                        | sbuf_remove
                        +-------------------------------+---------------+
                        v                               v               v
                   worker 0 (client A)            worker 1 (client B)  ...    tick thread
                        |  game_move/fire/...           |                         | game_tick
                        +--------> game_t (readers-writers lock) <----------------+
                        | replies                       | replies                 | frames, chat
                        v                               v                         v
                   outbox A (msgq)  <--------------  outbox B (msgq)  <------ server_broadcast
                        v                               v
                   sender A --> client A           sender B --> client B
```

| Thread | Count | Job |
| ------ | ----- | --- |
| main | 1 | accept connections into the `sbuf`; handle `SIGINT`/`SIGTERM`; shut down |
| worker | `-w` | take a connection from the `sbuf`, serve it until it disconnects, repeat |
| sender | 1 per client | the **only** writer of its client's socket: drains the client's outbox |
| tick | 1 | every tick: `game_tick`, broadcast the frame, end the match after `-d` seconds |

This is the lecture's **prethreaded server**: if all workers are busy, new
connections wait in the `sbuf`. Replies, frames, and chat for a client all go
into that client's **outbox**, a producer-consumer queue with one consumer.
This keeps messages from interleaving on the wire, and the tick thread uses
the non-blocking `msgq_try_insert` so a slow client only misses frames
instead of freezing the game. All threads are joinable and are joined at
shutdown, so `memcheck` can verify that nothing leaks.

**A session** (`session.c`): the worker allocates a `session_t`, sets
`SO_SNDTIMEO`, creates the outbox, registers the session (or closes the
connection if the server is stopping), starts the sender, then reads, parses,
counts, and executes commands. On EOF, error, or `QUIT` it unregisters the
session, removes the tank, queues the `NULL` poison pill, joins the sender
(which flushes the queue first), and frees everything. If a write fails or
times out, the sender calls `shutdown(connfd, SHUT_RDWR)` to wake the worker
and keeps draining until the poison pill.

**Shutdown** (`server.c`): `server_request_stop` sets `stopping` under
`reg_mutex` (from the tick thread it also connects to the server's own port to
wake `accept`). The main thread prints the final scores, calls
`shutdown(connfd, SHUT_RD)` on every registered session, joins the tick
thread, inserts one `-1` per worker into the `sbuf`, and joins the workers.
`server_run` blocks `SIGINT`/`SIGTERM` before creating threads and unblocks
them only in the main thread, so the handler always runs there. `SIGPIPE` is
ignored.

## Shared State and Synchronization

| Shared data | Protection |
| ----------- | ---------- |
| `game_t` (tanks, bullets, occupancy grid, counters, events, RNG seed) | readers-writers lock: `w`, `rmutex`, `readcnt` |
| `game_t.map` | none: written only before threads start |
| `sbuf_t conns`, every outbox (`msgq_t`) | `mutex`, `slots`, `items` semaphores |
| `sessions[]`, `nsessions`, `stopping`, each `session->mode` | `reg_mutex` |
| `commands`, `frames_sent`, `frames_dropped` | `stats_mutex` |
| stdout in verbose mode | `log_mutex` |

**The game lock** is the first readers-writers solution from lecture. Every
public `game_*` function locks internally: `game_snapshot`, `game_scores`,
`game_get_stats`, `game_get_tank`, and `game_check_invariants` are readers;
all others are writers. The lock must cover the whole check-then-act
operation:

- `game_move`: two tanks moving into the same free cell must not both succeed;
- `game_add_tank`: twelve clients joining as "dup" at once yield exactly one tank;
- `game_fire` (worker) and `game_tick` (tick thread) both do `victim->hp--`,
  which is exactly `cnt++` from `badcnt.c`.

**Other rules:**

- `server_register` must check `stopping` and insert in one critical section,
  or a late session can miss the shutdown and hang the server.
- Never block while holding a lock others need: `server_broadcast` holds
  `reg_mutex`, so it uses `msgq_try_insert`. No socket I/O under the game lock
  or `reg_mutex`.
- **Lock order** (deadlock avoidance): game lock → `reg_mutex` → outbox mutex
  → `stats_mutex`. In practice, never hold the game lock while calling into
  the registry.

## The Protocol

Messages are ASCII lines ending in `\n` (`\r\n` is accepted). Keywords and
directions are case-insensitive. Blank lines are ignored; a line over 254
characters gets `ERR LINE_TOO_LONG`. **Every non-empty line gets exactly one
reply, in order.** Frames, chat, and `GAMEOVER` may arrive between replies.

| Command | Allowed when | Reply |
| ------- | ------------ | ----- |
| `JOIN <name>` | not joined/watching | `WELCOME <id> <tick_rate>` + map block |
| `WATCH` | not joined/watching | `WATCHING <tick_rate>` + map block |
| `MOVE <N\|E\|S\|W>` | joined | `OK MOVE <x> <y> <dir>` |
| `TURN <N\|E\|S\|W>` | joined | `OK TURN <dir>` |
| `FIRE` | joined | `OK FIRE` |
| `SAY <text>` | joined | `CHAT <name> <text>` to all players and spectators, then `OK SAY` |
| `SCORES` | always | score block |
| `STATS` | always | `STATS tick=<t> players=<n> joins=<n> leaves=<n> shots=<n> hits=<n> kills=<n> commands=<n> sessions=<n>` |
| `PING` | always | `PONG` |
| `QUIT` | always | `BYE`, then the connection closes |

- **Map block:** `MAP <w> <h>`, `h` rows of `#`/`.`, `END`.
- **Score block:** `SCORES <n>`, `n` lines `S <id> <name> <kills> <deaths> <shots> <hits>` (by kills desc, deaths asc, id), `END`.
- `SAY` text is the rest of the line, 1-200 chars.
- `commands` counts every non-empty line received, including this `STATS`.

**Errors** are `ERR <CODE>`. The parser runs first, so `MOVE X` is
`BAD_ARGS` even before joining.

| Code | Meaning |
| ---- | ------- |
| `UNKNOWN_COMMAND` / `BAD_ARGS` / `LINE_TOO_LONG` | unknown keyword / wrong arguments / line too long |
| `NOT_JOINED` / `ALREADY_JOINED` | action without a tank / second `JOIN` or `WATCH` |
| `BAD_NAME` / `NAME_TAKEN` / `FULL` | invalid name / name in use / 16 players already |
| `DEAD` / `COOLDOWN` / `BLOCKED` / `NO_AMMO` | destroyed / too soon / move not possible / 3 bullets in flight |

**Frames** are sent to every player and spectator after each tick:

```
FRAME <tick> <ntanks> <nbullets> <nevents>
T <id> <name> <x> <y> <dir> <hp> <alive> <kills> <deaths> <shots> <hits>
B <id> <owner_id> <x> <y> <dir>
E <event>
END
```

Events since the last frame are `JOIN <id> <name>`, `LEAVE <id>`,
`SPAWN <id> <x> <y>`, and `KILL <killer> <victim>`. A frame is taken under the
game lock, so it is always consistent. At the end of a `-d` match, clients
receive `GAMEOVER` followed by a score block.

## Your Tasks

Implement the server above, in this order. Do not change the function
signatures in the headers, and keep `main.c` to argument parsing.

1. **`sync.c`, `sbuf.c`:** the `Sem_init`/`P`/`V`/`P_try`/`Pthread_*`/`Malloc`
   wrappers (exit on error; `P` retries on `EINTR`; `P_try` returns -1 on
   `EAGAIN`) and the lecture's `sbuf`.
2. **`msgq.c`:** `sbuf` for `char *` messages, plus a non-blocking
   `msgq_try_insert` and a `msgq_deinit` that frees leftovers.
3. **`net.c`:** `open_listenfd`, `open_clientfd`, and rio
   (`rio_readlineb`, `rio_writen`), retrying on `EINTR` and short writes.
4. **`protocol.c`:** `protocol_parse` with `strtok_r`.
5. **`game.c`:** the readers-writers lock, every `game_*` function, map
   loading, spawning with `rand_r(&g->seed)`, events, the text formats, and
   `game_check_invariants`. The built-in arena's rows are already in
   `game.c`.
6. **`session.c`:** worker loop, `serve_client`, the sender thread, and the
   command handlers. Queue `WELCOME` (or `WATCHING`) and the map as one
   message **before** switching the session's mode, so no frame arrives first.
7. **`server.c`, `log.c`, `main.c`:** registry, broadcast, counters, stop
   requests; a tick thread on `CLOCK_MONOTONIC` absolute deadlines; signal
   masks, the accept loop, and shutdown; a one-line-per-call `log_msg` using
   `localtime_r`; argument parsing.

## Thread Safety Rules

- Protect every shared variable. "Just an int" or `volatile` is not enough.
- **Banned:** `rand`, `srand`, `strtok`, `localtime`, `gmtime`, `ctime`,
  `asctime`, `inet_ntoa`, `gethostbyname`, `gethostbyaddr`. Use `rand_r`,
  `strtok_r`, `localtime_r`, and `getnameinfo`.
- Never pass a thread a pointer to a stack variable you will change; use heap
  objects.
- Use only semaphores for synchronization: no `pthread_mutex_*`,
  `pthread_cond_*`, `pthread_rwlock_*`, or atomics. No `select`, `poll`,
  `epoll`, `fork`, or non-blocking sockets.
- Do not use `sleep` to "fix" a race. Debug output goes to stderr via `debug.h`.

## Debugging

- Talk to the server by hand with `nc localhost 9000`, then watch the result
  in the GUI as a spectator.
- Bots make good load: `python3 clients/bot_client.py -p 9000 -n 8 -s spammer -r 0`
  pipelines commands as fast as the server accepts them. Watch for tanks
  sharing a cell, kills that do not match deaths, and a server that hangs on
  Ctrl-C.
- `make debug` enables the `debug`, `info`, `warn`, `error`, and `success`
  macros from `debug.h` (all print to stderr).
- **Race detectors** report unsynchronized accesses even when no harm was
  visible in that run. Run the server under one, play a short match with bots,
  and read the report:

  ```bash
  valgrind --tool=helgrind bin/tank_server -p 9000 -d 20 -w 8   # or --tool=drd
  valgrind --leak-check=full bin/tank_server -p 9000 -d 20 -w 8 # leaks
  make tsan && setarch -R bin/tank_server_tsan -p 9000 -d 20
  ```

  Valgrind runs the server many times slower, so give it fewer bots. The
  `setarch -R` avoids a `FATAL: ThreadSanitizer: unexpected memory mapping`
  crash on recent kernels. Your server should produce **no** helgrind or drd
  reports and no memcheck errors or definite leaks.

## References

- Lecture slides, Chapter 12: *Threads*, *Synchronization*, *Thread-Level Parallelism*
- Bryant and O'Hallaron, *CS:APP*, Chapters 11-12
- `man 7 pthreads`, `man 3 sem_init`, `man 2 shutdown`; [Helgrind](https://valgrind.org/docs/manual/hg-manual.html), [DRD](https://valgrind.org/docs/manual/drd-manual.html), [ThreadSanitizer](https://clang.llvm.org/docs/ThreadSanitizer.html)
