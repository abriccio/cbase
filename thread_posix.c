#include "thread.h"

static Thread thread_create(void *(*thread_fn)(void *), void *args) {
	pthread_t id;
	Thread t;
	int res = pthread_create(&id, NULL, thread_fn, args);
	if (res != 0) {
		err("pthread_create failed: %s\n", strerror(errno));
		return (Thread){0};
	}

	t.id = id;
	return t;
}

static bool32 thread_join(Thread thread) {
	// TODO Maybe handle return values?
	if (pthread_join(thread.id, NULL) != 0) {
		err("pthread_join failed: %s\n", strerror(errno));
		return FALSE;
	}
	return TRUE;
}

static bool32 thread_detach(Thread thread) {
	if (pthread_detach(thread.id) != 0) {
		err("pthread_detach failed: %s\n", strerror(errno));
		return FALSE;
	}
	return TRUE;
}

