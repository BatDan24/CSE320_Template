#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>

#include "game.h"
#include "msgq.h"
#include "sbuf.h"
#include "sync.h"

#define DEFAULT_TICK_RATE 20
#define DEFAULT_WORKERS 32
#define MAX_TICK_RATE 100
#define MAX_WORKERS 64
#define SBUF_SIZE 16         /* Accepted connections waiting for a worker */
#define OUTBOX_SIZE 64       /* Messages queued for one client */
#define SEND_TIMEOUT_SEC 2   /* A client that stops reading is dropped */

typedef struct {
    const char *port;
    const char *map_path;    /* NULL: built-in arena */
    int tick_rate;           /* Ticks per second */
    int workers;             /* Size of the worker thread pool */
    int duration;            /* Seconds until GAMEOVER; 0 = never */
    unsigned int seed;
    int verbose;
} server_config_t;

typedef enum {
    SESSION_LOBBY = 0,       /* Connected, has not sent JOIN or WATCH */
    SESSION_PLAYER,          /* Controls a tank; receives frames */
    SESSION_SPECTATOR        /* Receives frames only */
} session_mode_t;

/*
 * One connected client. Created and destroyed by the worker thread serving
 * the connection. While it is registered, other threads reach it through
 * server_t.sessions:
 *   - `mode` may only be written while holding reg_mutex;
 *   - `outbox` has its own synchronization and may be used by any thread;
 *   - `connfd` is only read by other threads (for shutdown()).
 * All other fields are private to the worker (and `sender` to the worker
 * and the sender thread).
 */
typedef struct session {
    int connfd;
    int slot;                /* Index in server_t.sessions */
    session_mode_t mode;
    int tank_id;             /* Valid when mode == SESSION_PLAYER */
    char name[MAX_NAME_LEN + 1];
    char addr[64];           /* "host:port" of the client, for logging */
    msgq_t outbox;           /* Messages waiting for the sender thread */
    pthread_t sender;        /* Thread that drains outbox to connfd */
} session_t;

typedef struct {
    server_config_t cfg;
    game_t game;
    int listenfd;

    /* Connected descriptors from the main thread to the workers. */
    sbuf_t conns;
    pthread_t *workers;
    pthread_t ticker;

    /* Session registry. reg_mutex protects sessions[], nsessions, stopping,
     * and every session's mode. */
    sem_t reg_mutex;
    session_t **sessions;    /* cfg.workers entries, NULL when free */
    int nsessions;
    int stopping;            /* Set once; the server is shutting down */

    /* Server-wide counters, protected by stats_mutex. */
    sem_t stats_mutex;
    long commands;           /* Non-empty command lines received */
    long frames_sent;        /* Frames queued to clients */
    long frames_dropped;     /* Frames skipped because an outbox was full */
} server_t;

/*
 * Lock ordering. To rule out deadlock, a thread that needs more than one of
 * these locks at a time must acquire them in this order:
 *     game lock  ->  reg_mutex  ->  outbox mutex  ->  stats_mutex
 * In practice the server never holds the game lock while taking another
 * lock: game_* functions acquire and release it internally.
 */

/* server.c ------------------------------------------------------------- */

/* Initialize s: load the map, open the listening socket. 0 or -1. */
int server_init(server_t *s, const server_config_t *cfg);
/*
 * Start the tick thread and the worker pool, then accept connections on the
 * calling (main) thread until the server is asked to stop (SIGINT/SIGTERM
 * or the end of the match). Shuts down every thread before returning.
 */
void server_run(server_t *s);
/* Release everything owned by s. Call after server_run() returns. */
void server_destroy(server_t *s);

/*
 * Ask the server to stop. Safe to call from any thread, any number of
 * times. If wake_acceptor is non-zero, the main thread is woken up from
 * accept() by a dummy connection.
 */
void server_request_stop(server_t *s, int wake_acceptor);
/* Non-zero once server_request_stop() has been called. */
int server_stopping(server_t *s);

/*
 * Add session to the registry. Returns 0 on success, or -1 if the server
 * is stopping (the session must then be discarded). The check and the
 * insertion happen atomically under reg_mutex.
 */
int server_register(server_t *s, session_t *session);
/* Remove session from the registry. */
void server_unregister(server_t *s, session_t *session);
/* Change a registered session's mode (under reg_mutex). */
void server_set_mode(server_t *s, session_t *session, session_mode_t mode);
/*
 * Queue a copy of msg in the outbox of every session that is a player or a
 * spectator. Never blocks: a full outbox skips the message. Returns the
 * number of sessions that were skipped; if delivered is not NULL, stores
 * the number of sessions that received the message.
 */
int server_broadcast(server_t *s, const char *msg, int *delivered);
/* Number of registered sessions. */
int server_session_count(server_t *s);

/* Thread-safe counter updates. */
void server_count_command(server_t *s);
void server_count_frames(server_t *s, int sent, int dropped);
long server_command_count(server_t *s);

/* session.c ------------------------------------------------------------ */

/* Worker thread routine: serve connections from s->conns until given -1. */
void *worker_thread(void *vargp);
/* Serve one client on connfd until it disconnects. Closes connfd. */
void serve_client(server_t *s, int connfd);

#endif /* SERVER_H */
