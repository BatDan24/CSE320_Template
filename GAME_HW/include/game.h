#ifndef GAME_H
#define GAME_H

#include <stddef.h>

#include "sync.h"

/*
 * The shared game world.
 *
 * A game_t is read and modified concurrently by many threads:
 *   - worker threads apply their client's commands (join, move, turn, fire,
 *     leave) and answer queries (scores, stats);
 *   - the tick thread advances bullets, applies damage, respawns tanks, and
 *     produces one frame per tick.
 *
 * All synchronization is internal: every public game_* function below is
 * thread-safe and may be called from any thread at any time. Functions that
 * only look at the world take the lock as a READER, functions that change it
 * take the lock as a WRITER (first readers-writers problem from lecture).
 * The only exceptions are game_init() and game_destroy(), which must be
 * called while no other thread can use the game.
 */

/* ---------------------------------------------------------------------- */
/* Rules                                                                   */
/* ---------------------------------------------------------------------- */

#define MAX_PLAYERS 16          /* Tanks in the arena at once */
#define MAX_NAME_LEN 15         /* Name: 1..15 chars of [A-Za-z0-9_-] */
#define MAX_HP 3                /* Hits needed to destroy a tank */
#define MAX_BULLETS_PER_TANK 3  /* Bullets one tank may have in flight */
#define MAX_BULLETS (MAX_PLAYERS * MAX_BULLETS_PER_TANK)
#define BULLET_SPEED 2          /* Cells a bullet travels per tick */
#define MOVE_COOLDOWN_TICKS 2   /* Ticks between two successful moves */
#define FIRE_COOLDOWN_TICKS 5   /* Ticks between two shots */
#define RESPAWN_TICKS 40        /* Ticks a destroyed tank stays dead */
#define MAX_EVENTS 128          /* Events buffered between two ticks */
#define EVENT_TEXT_LEN 64

#define MAP_MAX_W 64
#define MAP_MAX_H 48
#define MAP_WALL '#'
#define MAP_FLOOR '.'

/* Buffer size that is always large enough for one frame or score table. */
#define FRAME_BUF_SIZE 32768

typedef enum { DIR_N = 0, DIR_E = 1, DIR_S = 2, DIR_W = 3 } dir_t;

/* Result codes of the game_* functions. */
typedef enum {
    GAME_OK = 0,
    GAME_ERR_FULL,       /* Arena already holds MAX_PLAYERS tanks */
    GAME_ERR_NAME_TAKEN, /* Another tank already uses this name */
    GAME_ERR_BAD_NAME,   /* Name is empty, too long, or has bad characters */
    GAME_ERR_NO_TANK,    /* No tank with this id */
    GAME_ERR_DEAD,       /* Tank is destroyed and waiting to respawn */
    GAME_ERR_COOLDOWN,   /* Tank must wait before moving/firing again */
    GAME_ERR_BLOCKED,    /* Destination is a wall, outside, or occupied */
    GAME_ERR_NO_AMMO     /* Tank already has MAX_BULLETS_PER_TANK in flight */
} game_result_t;

typedef struct {
    int w, h;
    char cells[MAP_MAX_H][MAP_MAX_W + 1]; /* MAP_WALL or MAP_FLOOR */
} game_map_t;

typedef struct {
    int in_use;               /* Slot holds a tank */
    int id;                   /* Unique for the lifetime of the server */
    char name[MAX_NAME_LEN + 1];
    int x, y;                 /* Cell (last cell while dead) */
    dir_t dir;                /* Direction the barrel points */
    int hp;                   /* 0 while dead, else 1..MAX_HP */
    int alive;
    int kills, deaths;        /* Tanks destroyed / times destroyed */
    int shots, hits;          /* Shots fired / shots that damaged a tank */
    int bullets_active;       /* Own bullets currently in flight */
    long next_move_tick;      /* Earliest tick of the next move */
    long next_fire_tick;      /* Earliest tick of the next shot */
    long respawn_tick;        /* Tick at which a dead tank respawns */
} tank_t;

typedef struct {
    int in_use;
    int id;
    int owner_slot;           /* Index into game_t.tanks */
    int owner_id;
    int x, y;
    dir_t dir;
} bullet_t;

/* Counters over the lifetime of the game. */
typedef struct {
    long tick;
    int players;              /* Tanks currently in the arena */
    long joins, leaves;
    long shots, hits, kills;
} game_stats_t;

typedef struct {
    game_map_t map;
    tank_t tanks[MAX_PLAYERS];
    bullet_t bullets[MAX_BULLETS];
    /* occ[y][x] is (slot + 1) of the ALIVE tank on cell (x, y), or 0. */
    int occ[MAP_MAX_H][MAP_MAX_W];

    long tick;
    int next_tank_id;
    int next_bullet_id;
    unsigned int seed;        /* rand_r() state for spawn positions */
    game_stats_t stats;

    /* Events since the last tick, emitted with the next frame. */
    char events[MAX_EVENTS][EVENT_TEXT_LEN];
    int nevents;

    /* Readers-writers lock (first readers-writers problem). */
    int readcnt;              /* Readers currently inside; protected by rmutex */
    sem_t rmutex;             /* Protects readcnt */
    sem_t w;                  /* Held by the writer, or by the group of readers */
} game_t;

/* ---------------------------------------------------------------------- */
/* Setup                                                                   */
/* ---------------------------------------------------------------------- */

/*
 * Initialize g with the map in map_path (or the built-in arena if map_path
 * is NULL) and the given random seed. Returns 0 on success, -1 if the map
 * cannot be read or is invalid (see game_load_map).
 */
int game_init(game_t *g, const char *map_path, unsigned int seed);

/* Release all resources held by g. */
void game_destroy(game_t *g);

/*
 * Parse a map file into m. A map is 1..MAP_MAX_H lines of equal length
 * 1..MAP_MAX_W consisting only of MAP_WALL and MAP_FLOOR; a trailing
 * newline on the last line is optional. Returns 0 on success, -1 on error.
 */
int game_load_map(const char *path, game_map_t *m);

/* Copy the built-in default arena into m. */
void game_default_map(game_map_t *m);

/* ---------------------------------------------------------------------- */
/* Writers                                                                 */
/* ---------------------------------------------------------------------- */

/*
 * Add a tank named name at a random free cell, facing north, with full HP.
 * If no cell is free the tank starts dead and spawns as soon as one is.
 * On success stores the new tank's id in *id_out and returns GAME_OK.
 * Errors: GAME_ERR_BAD_NAME, GAME_ERR_NAME_TAKEN, GAME_ERR_FULL.
 */
game_result_t game_add_tank(game_t *g, const char *name, int *id_out);

/* Remove tank id and all of its bullets. Errors: GAME_ERR_NO_TANK. */
game_result_t game_remove_tank(game_t *g, int id);

/*
 * Turn tank id to face d, then try to advance one cell in that direction.
 * On success stores the new position in *x_out and *y_out (either may be NULL).
 * The turn happens even if the move is blocked. A blocked move does not
 * start the move cooldown.
 * Errors: GAME_ERR_NO_TANK, GAME_ERR_DEAD, GAME_ERR_COOLDOWN, GAME_ERR_BLOCKED.
 */
game_result_t game_move(game_t *g, int id, dir_t d, int *x_out, int *y_out);

/* Turn tank id to face d without moving. Errors: NO_TANK, DEAD. */
game_result_t game_turn(game_t *g, int id, dir_t d);

/*
 * Fire one bullet from tank id into the cell in front of it.
 *  - If that cell is a wall (or outside the map) the shot is wasted.
 *  - If that cell holds a tank, the tank is hit immediately.
 *  - Otherwise a bullet appears there and moves BULLET_SPEED cells per tick.
 * Errors: GAME_ERR_NO_TANK, GAME_ERR_DEAD, GAME_ERR_COOLDOWN, GAME_ERR_NO_AMMO.
 */
game_result_t game_fire(game_t *g, int id);

/*
 * Advance the world by one tick:
 *   1. increment the tick counter;
 *   2. move every bullet BULLET_SPEED cells, one cell at a time, applying
 *      hits and removing bullets that hit a wall or a tank;
 *   3. respawn dead tanks whose respawn tick has arrived.
 * If frame is not NULL, write the resulting frame (see game_snapshot),
 * including the events since the previous tick, into frame. The event list
 * is cleared. Returns the length of the frame text (0 if frame is NULL).
 */
size_t game_tick(game_t *g, char *frame, size_t cap);

/*
 * Debugging helper: move tank id to (x, y) facing d, alive with full HP.
 * Errors: GAME_ERR_NO_TANK, GAME_ERR_BLOCKED (wall/outside/occupied).
 */
game_result_t game_place_tank(game_t *g, int id, int x, int y, dir_t d);

/* ---------------------------------------------------------------------- */
/* Readers                                                                 */
/* ---------------------------------------------------------------------- */

/*
 * Write the current world as text into buf (at most cap bytes, always
 * NUL-terminated) and return its length. Format (see README):
 *   FRAME <tick> <ntanks> <nbullets> <nevents>
 *   T <id> <name> <x> <y> <dir> <hp> <alive> <kills> <deaths> <shots> <hits>
 *   B <id> <owner_id> <x> <y> <dir>
 *   E <event text>
 *   END
 * game_snapshot never includes events (nevents is 0).
 */
size_t game_snapshot(game_t *g, char *buf, size_t cap);

/*
 * Write the score table into buf and return its length:
 *   SCORES <n>
 *   S <id> <name> <kills> <deaths> <shots> <hits>     (n lines)
 *   END
 * Rows are sorted by kills (descending), then deaths (ascending), then id.
 */
size_t game_scores(game_t *g, char *buf, size_t cap);

/* Copy the lifetime counters into *out. */
void game_get_stats(game_t *g, game_stats_t *out);

/* Copy tank id into *out. Errors: GAME_ERR_NO_TANK. */
game_result_t game_get_tank(game_t *g, int id, tank_t *out);

/* Write the map into buf as "MAP <w> <h>\n", h rows, then "END\n". */
size_t game_map_text(game_t *g, char *buf, size_t cap);

/*
 * Check the internal consistency of the world. Returns 0 if every
 * invariant holds, otherwise -1 (and writes a description into why, if
 * why is not NULL). Invariants:
 *   - every alive tank is on a floor cell and occ[][] points at it;
 *   - occ[][] marks no other cells, so no two alive tanks share a cell;
 *   - 0 < hp <= MAX_HP for alive tanks and hp == 0 for dead tanks;
 *   - bullets_active matches the bullets actually in flight;
 *   - every bullet is on a floor cell and belongs to an existing tank;
 *   - lifetime counters satisfy kills <= hits <= shots.
 */
int game_check_invariants(game_t *g, char *why, size_t cap);

/* ---------------------------------------------------------------------- */
/* Helpers (pure functions, no locking needed)                             */
/* ---------------------------------------------------------------------- */

/* 1 if name is 1..MAX_NAME_LEN chars of [A-Za-z0-9_-], else 0. */
int game_valid_name(const char *name);
/* 'N', 'E', 'S', or 'W'. */
char game_dir_char(dir_t d);
/* Parse "N"/"E"/"S"/"W" (either case). Returns 0 on success, -1 otherwise. */
int game_parse_dir(const char *s, dir_t *out);
/* Protocol token for a result, e.g. "COOLDOWN" for GAME_ERR_COOLDOWN. */
const char *game_result_str(game_result_t r);

#endif /* GAME_H */
