#ifndef X360_SHIM_PTHREAD_H
#define X360_SHIM_PTHREAD_H
/* o pouco de pthread que o port usa, sobre as threads e seções críticas do 360 (port/x360/src/posix360.c) */
#include <stddef.h>
typedef void *pthread_t;
typedef struct { size_t stack; } pthread_attr_t;
typedef struct { void *cs; } pthread_mutex_t;
typedef struct { void *ev; } pthread_cond_t;
#define PTHREAD_MUTEX_INITIALIZER { 0 }
#define PTHREAD_COND_INITIALIZER { 0 }
typedef struct { volatile long feito; } pthread_once_t;
#define PTHREAD_ONCE_INIT { 0 }
int pthread_once(pthread_once_t *, void (*)(void));
int pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int pthread_join(pthread_t, void **);
int pthread_detach(pthread_t);
int pthread_attr_init(pthread_attr_t *);
int pthread_attr_setstacksize(pthread_attr_t *, size_t);
int pthread_mutex_init(pthread_mutex_t *, const void *);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);
int pthread_cond_init(pthread_cond_t *, const void *);
int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int pthread_cond_signal(pthread_cond_t *);
int pthread_cond_broadcast(pthread_cond_t *);
#endif
