#ifndef ALLOCATOR_H
#define ALLOCATOR_H
/* ALLOCATOR INTERFACE */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define DEFAULT_ALIGN (sizeof(void*))

typedef struct {
    void *(*alloc)(void *ctx, size_t size);
    void *(*realloc)(void *ctx, void *ptr, size_t new_size);
    void (*free)(void *ctx);
} Allocator;

static bool is_power_of_two(size_t n) {
    return (n & (n - 1)) == 0;
}

static size_t next_power_of_two(size_t n) {
    if (is_power_of_two(n)) return n;
    return 1ul << (64 - __builtin_clzll(n));
}

static void *align_forward(void *ptr, int align) {
    size_t p = (size_t)ptr;
    assert(is_power_of_two(align));
    size_t mod = p & (align - 1);

    if (mod != 0) {
        p += align - mod;
    }

    return (void*)p;
}

static void *heap_allocate(void *ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}

static void *heap_realloc(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    return realloc(ptr, size);
}

typedef struct {
    Allocator allocator;
} HeapAllocator;

static HeapAllocator heap_allocator_init() {
    return (HeapAllocator){
        .allocator = {
            .alloc = heap_allocate,
            .realloc = heap_realloc,
            .free = NULL,
        },
    };
}

/* ARENA API */
// TODO support growing arena

typedef struct {
    Allocator allocator;
    uint8_t *data;
    size_t head;
    size_t capacity;
} Arena;

static void *arena_alloc(void *, size_t);
static void *arena_realloc(void*, void *, size_t);
static void arena_free(void *);

static Arena arena_init(size_t capacity) {
    Arena a = {0};
    a.allocator = (Allocator){
        .alloc = arena_alloc,
        .free = arena_free,
        .realloc = arena_realloc,
    };
    a.capacity = capacity;
    a.data = (uint8_t*)malloc(capacity);
    return a;
}

static void *arena_alloc(void *ctx, size_t size) {
    size_t align = DEFAULT_ALIGN;
    Arena *a = (Arena*)ctx;
    void *data = &a->data[a->head];
    void *aligned = align_forward(data, align);
    size_t delta = ((size_t)aligned - (size_t)data);
    if (a->head + size + delta < a->capacity) {
        a->head += delta + size;
        memset(aligned, 0, size);

        return aligned;
    }

    return NULL;
}

// For now, simply returns a pointer to the arena's head. Don't reuse a pointer
// passed into this function, always use the returned pointer.
static void *arena_realloc(void *ctx, void *ptr, size_t size) {
    return arena_alloc(ctx, size);
}

static void arena_free(void *ctx) {}

// Resets the head to zero, allowing for re-use of arena without reallocating
static void arena_reset(Arena *a) {
    a->head = 0;
}

static void arena_deinit(Arena *a) {
    free(a->data);
}

#endif
