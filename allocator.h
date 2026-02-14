#ifndef ALLOCATOR_H
#define ALLOCATOR_H
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "log.h"

/* ALLOCATOR INTERFACE */

#define DEFAULT_ALIGN (sizeof(void*))
#define KB(x) ((u64)x << 10ul)
#define MB(x) ((u64)x << 20ul)
#define GB(x) ((u64)x << 30ul)

typedef struct {
    void *(*alloc)(void *ctx, usize size);
    void *(*realloc)(void *ctx, void *ptr, usize new_size);
    void (*free)(void *ctx, void *ptr);
} Allocator;

static bool is_power_of_two(usize n) {
    return (n & (n - 1)) == 0;
}

static usize next_power_of_two(usize n) {
    if (is_power_of_two(n)) return n;
    return 1ul << (64 - __builtin_clzll(n));
}

static void *align_forward(usize ptr, int align) {
    assert(is_power_of_two(align));
    usize mod = ptr & (align - 1);

    if (mod != 0) {
        ptr += align - mod;
    }

    return (void*)ptr;
}

static usize page_size();

/* LIBC Allocator API */

static void *heap_allocate(void *ctx, usize size) {
    (void)ctx;
    return malloc(size);
}

static void *heap_realloc(void *ctx, void *ptr, usize size) {
    (void)ctx;
    return realloc(ptr, size);
}

static void heap_free(void *ctx, void *ptr) {
    (void)ctx;
    free(ptr);
}

typedef struct {
    Allocator allocator;
} LibCAllocator;

static LibCAllocator heap_allocator_init() {
    return (LibCAllocator){
        .allocator = {
            .alloc = heap_allocate,
            .realloc = heap_realloc,
            .free = heap_free,
        },
    };
}

/* ARENA API */

// The backing allocation from which the arena passes out new allocation references
typedef struct ArenaAllocation {
    struct ArenaAllocation *next;
    u8 *data;
    usize head;
    usize capacity;
} ArenaAllocation;

// Growable arena allocator which uses malloc as its backing allocator
typedef struct {
    Allocator allocator;
    ArenaAllocation *first;
    ArenaAllocation *last;
} Arena;

Arena arena_init(usize capacity);
void arena_deinit(Arena *a);
void arena_ensure_capacity(Arena *a, usize capacity);
usize arena_query_capacity(Arena *a);
void arena_reset(Arena *a);
void arena_set_head(Arena *a, usize head);
void *arena_alloc(void *arena, usize);
void *arena_realloc(void *arena, void *, usize);
void arena_free(void *arena, void *);

/*
    TEMP ARENA API
    Fixed-size Arena for small allocations, scratch space, per-frame allocations, etc.
    No procedures for freeing or reallocating, only resetting the allocation head.
 */

typedef struct {
    Allocator allocator;
    u8 *data;
    usize head;
    usize capacity;
} TempArena;

TempArena temp_arena_init(usize capacity);
void temp_arena_deinit(TempArena *ta);
void *temp_arena_alloc(void *arena, usize size);
void temp_arena_reset(TempArena *ta);

/* Generic Array API */

#define Array(T) struct {T *items; usize len; usize cap;}

#define array_init_capacity(allocator, array, capacity) do { \
    (array)->items = (typeof((array)->items))(allocator)->alloc((allocator), capacity * sizeof(*(array)->items)); \
    (array)->cap = capacity; \
    (array)->len = 0; \
} while(0)

#define array_reserve(alloc, array, capacity) do { \
    if ((capacity) > (array)->cap) { \
        (array)->items = (typeof((array)->items))(alloc)->realloc(alloc, (array)->items, (capacity) * sizeof(*(array)->items));\
        (array)->cap = (capacity); \
    }\
} while (0)

// May invalidate pointers if over capacity
#define array_append(alloc, array, item) do {\
    if ((array)->len + 1 > (array)->cap) { \
        array_reserve(alloc, array, (array)->cap * 2); \
    } \
    (array)->items[(array)->len++] = (item);\
} while (0)

// May invalidate pointers if over capacity
#define array_append_many(alloc, array, item_ptr, count) do { \
    if ((array)->len + count > (array)->cap) { \
        array_reserve(alloc, array, (array)->cap * 2); \
    } \
    memcpy((array)->items + (array)->len, item_ptr, count * sizeof(*item_ptr)); \
    (array)->len += count; \
} while(0)

// Will invalidate pointers
#define array_resize(alloc, array, size) do {\
    array_reserve(alloc, array, size);\
    (array)->len = size;\
} while (0)

#define array_last(array) ((array)->items[(array)->len - 1])
#define array_last_ptr(array) &((array)->items[(array)->len - 1])

#define array_pop(array) ((array)->len--, (array)->items[(array)->len])

#define each(T, i, array) (T *i = (array)->items; i < (array)->items + (array)->len; ++i)

#endif
