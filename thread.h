#ifndef THREAD_H
#define THREAD_H

#ifdef _WIN32
#else
#include <pthread.h>
typedef pthread_mutex_t Mutex;
typedef pthread_t ThreadID;
#define mutex_lock(m) pthread_mutex_lock(&(m))
#define mutex_try_lock(m) pthread_mutex_trylock(&(m))
#define mutex_release(m) pthread_mutex_unlock(&(m))
#define mutex_init(m) pthread_mutex_init(&(m), NULL)
#define mutex_deinit(m) pthread_mutex_destroy(&(m))
#endif

typedef struct Thread {
	ThreadID id;
} Thread;

static Thread thread_create(void *(*thread_fn)(void *), void *args);
static bool32 thread_join(Thread);
static bool32 thread_detach(Thread);

#endif
