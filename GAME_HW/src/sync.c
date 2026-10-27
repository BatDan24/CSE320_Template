#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sync.h"

/* TODO bodies are placeholders. Implement the contracts in sync.h. */

void unix_error(const char *msg)
{
    // TODO: Print "msg: strerror(errno)" to stderr and exit(EXIT_FAILURE).
    (void)msg;
}

void posix_error(int code, const char *msg)
{
    // TODO: Print "msg: strerror(code)" to stderr and exit(EXIT_FAILURE).
    (void)code;
    (void)msg;
}

/*
 * Semaphore wrappers
 */

void Sem_init(sem_t *s, int pshared, unsigned int value)
{
    // TODO: sem_init(), exiting with an error message on failure.
    (void)s;
    (void)pshared;
    (void)value;
}

void Sem_destroy(sem_t *s)
{
    // TODO: sem_destroy(), exiting with an error message on failure.
    (void)s;
}

void P(sem_t *s)
{
    // TODO: sem_wait(), retrying when interrupted by a signal (EINTR).
    (void)s;
}

void V(sem_t *s)
{
    // TODO: sem_post(), exiting with an error message on failure.
    (void)s;
}

int P_try(sem_t *s)
{
    // TODO: sem_trywait(). Return 0 if decremented, -1 if the value was 0
    // (EAGAIN). Retry on EINTR; exit on any other error.
    (void)s;
    return -1;
}

/*
 * Pthreads wrappers
 */

void Pthread_create(pthread_t *tid, const pthread_attr_t *attr,
                    void *(*routine)(void *), void *argp)
{
    // TODO: pthread_create(), exiting with an error message on failure.
    (void)tid;
    (void)attr;
    (void)routine;
    (void)argp;
}

void Pthread_join(pthread_t tid, void **thread_return)
{
    // TODO: pthread_join(), exiting with an error message on failure.
    (void)tid;
    (void)thread_return;
}

void Pthread_detach(pthread_t tid)
{
    // TODO: pthread_detach(), exiting with an error message on failure.
    (void)tid;
}

/*
 * Memory wrappers
 */

void *Malloc(size_t size)
{
    // TODO: malloc(), exiting with an error message on failure.
    (void)size;
    return NULL;
}

void *Calloc(size_t nmemb, size_t size)
{
    // TODO: calloc(), exiting with an error message on failure.
    (void)nmemb;
    (void)size;
    return NULL;
}

char *Strdup(const char *s)
{
    // TODO: strdup(), exiting with an error message on failure.
    (void)s;
    return NULL;
}
