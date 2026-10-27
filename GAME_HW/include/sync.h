#ifndef SYNC_H
#define SYNC_H

/*
 * Semaphore and thread helpers used throughout the server.
 *
 * P() and V() are the semaphore operations from lecture:
 *   P(s): wait until s > 0, then decrement s     (sem_wait)
 *   V(s): increment s, waking one waiting thread (sem_post)
 *
 * The capitalized wrappers (Sem_init, Pthread_create, Malloc, ...) follow the
 * textbook convention: they call the real function and terminate the program
 * with a message if it fails, so callers do not need to check return values.
 */

#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>

/* Print "msg: strerror(errno)" to stderr and exit(EXIT_FAILURE). */
void unix_error(const char *msg);
/* Print "msg: strerror(code)" to stderr and exit(EXIT_FAILURE). */
void posix_error(int code, const char *msg);

/* Semaphore wrappers */
void Sem_init(sem_t *s, int pshared, unsigned int value);
void Sem_destroy(sem_t *s);
void P(sem_t *s);
void V(sem_t *s);
/* Non-blocking P: returns 0 if the semaphore was decremented, -1 if it was 0. */
int P_try(sem_t *s);

/* Pthreads wrappers */
void Pthread_create(pthread_t *tid, const pthread_attr_t *attr,
                    void *(*routine)(void *), void *argp);
void Pthread_join(pthread_t tid, void **thread_return);
void Pthread_detach(pthread_t tid);

/* Memory wrappers */
void *Malloc(size_t size);
void *Calloc(size_t nmemb, size_t size);
char *Strdup(const char *s);

#endif /* SYNC_H */
