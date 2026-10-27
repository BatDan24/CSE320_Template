#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "debug.h"
#include "global.h"
#include "log.h"
#include "net.h"
#include "protocol.h"
#include "server.h"

/* TODO bodies are placeholders. Implement the contracts in server.h and the
 * Server Architecture and Shutdown sections of the README.
 */

int server_init(server_t *s, const server_config_t *cfg)
{
    // TODO: Copy cfg, initialize the game, the sbuf, the registry, and the
    // counters, and open the listening socket.
    (void)s;
    (void)cfg;
    return -1;
}

void server_run(server_t *s)
{
    // TODO: Block SIGINT/SIGTERM, start the tick thread and the workers,
    // accept connections until stopped, then shut every thread down.
    (void)s;
}

void server_destroy(server_t *s)
{
    // TODO: Release everything server_init allocated.
    (void)s;
}

void server_request_stop(server_t *s, int wake_acceptor)
{
    // TODO: Set stopping under reg_mutex; optionally wake accept().
    (void)s;
    (void)wake_acceptor;
}

int server_stopping(server_t *s)
{
    // TODO: Read stopping under reg_mutex.
    (void)s;
    return 1;
}

int server_register(server_t *s, session_t *session)
{
    // TODO: Check stopping and insert in one critical section.
    (void)s;
    (void)session;
    return -1;
}

void server_unregister(server_t *s, session_t *session)
{
    // TODO: Remove the session from the registry.
    (void)s;
    (void)session;
}

void server_set_mode(server_t *s, session_t *session, session_mode_t mode)
{
    // TODO: Change the session's mode under reg_mutex.
    (void)s;
    (void)session;
    (void)mode;
}

int server_broadcast(server_t *s, const char *msg, int *delivered)
{
    // TODO: Queue a copy of msg for every player and spectator without
    // blocking (msgq_try_insert).
    (void)s;
    (void)msg;
    if (delivered != NULL)
        *delivered = 0;
    return 0;
}

int server_session_count(server_t *s)
{
    // TODO: Read nsessions under reg_mutex.
    (void)s;
    return 0;
}

void server_count_command(server_t *s)
{
    // TODO: Increment commands under stats_mutex.
    (void)s;
}

void server_count_frames(server_t *s, int sent, int dropped)
{
    // TODO: Add to frames_sent and frames_dropped under stats_mutex.
    (void)s;
    (void)sent;
    (void)dropped;
}

long server_command_count(server_t *s)
{
    // TODO: Read commands under stats_mutex.
    (void)s;
    return 0;
}
