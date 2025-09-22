#ifndef ALLOCATOR_H
#define ALLOCATOR_H
/* ALLOCATOR INTERFACE */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "log.h"

#define DEFAULT_ALIGN (sizeof(void*))

typedef struct {
    void *(*alloc)(void *ctx, usize size);
    void *(*realloc)(void *ctx, void *ptr, usize new_size);
    void (*free)(void *ctx);
} Allocator;

static bool is_power_of_two(usize n) {
    return (n & (n - 1)) == 0;
}

static usize next_power_of_two(usize n) {
    if (is_power_of_two(n)) return n;
    return 1ul << (64 - __builtin_clzll(n));
}

static void *align_forward(void *ptr, int align) {
    usize p = (usize)ptr;
    assert(is_power_of_two(align));
    usize mod = p & (align - 1);

    if (mod != 0) {
        p += align - mod;
    }

    return (void*)p;
}

static void *heap_allocate(void *ctx, usize size) {
    (void)ctx;
    return malloc(size);
}

static void *heap_realloc(void *ctx, void *ptr, usize size) {
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
    u8 *data;
    usize head;
    usize capacity;
} Arena;

static void *arena_alloc(void *, usize);
static void *arena_realloc(void*, void *, usize);
static void arena_free(void *);

static Arena arena_init(usize capacity) {
    Arena a = {0};
    a.allocator = (Allocator){
        .alloc = arena_alloc,
        .free = arena_free,
        .realloc = arena_realloc,
    };
    a.capacity = capacity;
    a.data = (u8*)malloc(capacity);
    return a;
}

static bool arena_resize(Arena *a, usize new_capacity) {
    a->data = (u8*)realloc(a->data, new_capacity);
    if (!a->data) {
        return false;
    }
    a->capacity = new_capacity;
    return true;
}

static void *arena_alloc(void *ctx, usize size) {
    usize align = DEFAULT_ALIGN;
    Arena *a = (Arena*)ctx;
    void *data = &a->data[a->head];
    void *aligned = align_forward(data, align);
    usize delta = ((usize)aligned - (usize)data);
    if (a->head + size + delta < a->capacity) {
        a->head += delta + size;
        memset(aligned, 0, size);

        return aligned;
    }

    if (arena_resize(a, a->capacity * 2)) {
        // is this fucked...?
        return arena_alloc(ctx, size);
    }

    return NULL;
}

// For now, simply allocates new memory without checking if old memory can be
// reused. Don't reuse a pointer passed into this function, always use the
// returned pointer.
// TODO check if this is the previous allocation and can therefore be reused
static void *arena_realloc(void *ctx, void *ptr, usize size)
{
    return arena_alloc(ctx, size);
}

static void arena_free(void *ctx) {}

// Resets the head to zero, allowing for re-use of arena without reallocating
static void arena_reset(Arena *a) {
    a->head = 0;
}

static void arena_set_head(Arena *a, usize head) {
    a->head = head;
}

static void arena_deinit(Arena *a) {
    if (!a) return;
    free(a->data);
}

#endif
