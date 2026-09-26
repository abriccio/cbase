#include "cbase.h"
#include "cbase.c"

static bool32 thread_test_done = FALSE;
static Mutex lock;

typedef struct {
	void (*progress_cb)(size_t bytes_read, size_t total_size);
	void (*finish_cb)(size_t bytes_read);
} BackgroundThreadArgs;

void *background_thread(void *args) {
	BackgroundThreadArgs *bta = (BackgroundThreadArgs*)args;
	size_t n;
	size_t total = 32 << 20;
	for (n = 0; n < total; n += (16 << 10)) {
		bta->progress_cb(n, total);
	}
	bta->finish_cb(n);
	return NULL;
}

void prog(size_t bytes_read, size_t total) {
	print("Read %zu KB / %zu KB\n", bytes_read / 1024, total / 1024);
}

void fin(size_t total) {
	print("Done reading %zu KB\n", total / 1024);
	mutex_lock(lock);
	thread_test_done = TRUE;
	mutex_release(lock);
}

void *message_thread(void *args) {
	(void)args;
	static BackgroundThreadArgs thread_args;
	thread_args.progress_cb = prog;
	thread_args.finish_cb = fin;
	Thread thread = thread_create(background_thread, &thread_args);
	thread_detach(thread);
	return NULL;
}

static bool32 thread_test() {
	mutex_init(lock);
	Thread thread = thread_create(message_thread, NULL);
	bool32 done = thread_test_done;
	while(!done) {
		mutex_lock(lock);
		done = thread_test_done;
		mutex_release(lock);
	}
	thread_join(thread);
	mutex_deinit(lock);

	return TRUE;
}

int main() {
	int result = 0;
	if (!thread_test()) {
		result = 1;
	}

	return result;
}
