#ifndef ALLOCATOR_H
#define ALLOCATOR_H
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include "types.h"
#include "log.h"
#include "util.h"

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

static bool32 is_power_of_two(usize n) {
    return (n & (n - 1)) == 0;
}

static usize next_power_of_two(usize n) {
    if (is_power_of_two(n)) return n;
    return 1ul << (64 - clz64(n));
}

static void *align_forward(usize ptr, int align) {
    assert(is_power_of_two(align));
    usize mod = ptr & (align - 1);

    if (mod != 0) {
        ptr += align - mod;
    }

    return (void*)ptr;
}

static usize page_size() {
    return (usize)getpagesize();
}

/* ARENA API */

#define ARENA_DEFAULT_RESERVE MB(64)
#define ARENA_DEFAULT_COMMIT KB(64)

typedef struct {
    // Specify the total amount of virtual memory to reserve
    usize reserve_size;
    // Specify the initial amount of memory to commit (i.e. make accessible)
    usize commit_size;
    // Optionally provide the Arena with a backing buffer which it will perform all allocations against
    u8 *backing_buffer;
} ArenaOptions;

typedef struct Arena Arena;
struct Arena {
    Allocator allocator;
    Arena *prev;
    Arena *current;
    usize head;
    usize reserve;
    usize commit;
};
#define ARENA_HEADER_SIZE sizeof(Arena)

Arena *arena_init_opt(ArenaOptions opt);
// Create an Arena with default options
Arena *arena_init();
void arena_deinit(Arena *a);
void arena_ensure_reserve_size(Arena *a, usize capacity);
usize arena_query_size(Arena *a);
void arena_reset(Arena *a);
void arena_set_head(Arena *a, usize head);
usize arena_get_head(Arena *a);
void *arena_alloc(void *arena, usize);
void *arena_realloc(void *arena, void *, usize);
void arena_free(void *arena, void *);

/*
    TEMP ALLOC API
    Not an Allocator but an API on top of existing Arena for small allocations,
    scratch space, per-frame allocations, etc. These functions create a
    "bookmark" for the backing arena, which will be rewound to for memory re-use
    after the temp region is ended. You will use regular Arena procedures in
    between calls to temp_alloc_begin and temp_alloc_end.

    If your desired lifetime is entirely local, consider using StackAllocator.
 */

typedef struct {
    Arena *arena;
    usize start;
} TempAlloc;

TempAlloc temp_alloc_begin(Arena *);
void temp_alloc_end(TempAlloc *ta);

/*
 * STACK ALLOCATOR API
 * Stack-based allocator with no need for freeing. Realloc operation is the same as allocating.
 * Can reset the allocator head with STACK_ALLOC_RESET
 */

 // TODO Make this just an API on top of regular Arena, we just give the Arena a stack-alloc'd
 // backing buffer

typedef struct {
    Allocator allocator;
    u8 *data;
    usize head;
    usize capacity;
} StackAllocator;

StackAllocator stack_allocator_init(u8 *data, usize size);
static void _stack_allocator_reset(void *ctx);
#define STACK_ALLOC_BEGIN(sz) u8 _sa_data[sz] = {0}; StackAllocator _sa = stack_allocator_init(_sa_data, (sz))
// Reset the allocator head to 0
#define STACK_ALLOC_RESET _stack_allocator_reset(&_sa)
#define STACK_ALLOC (&_sa.allocator)

void *stack_allocator_alloc(void *ctx, usize size);

/* Generic Array API */

#define Array(T) struct {T *items; usize len; usize cap;}

// Compute length of compile-time array
#define array_len(a) (sizeof(a) / sizeof(a[0]))

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

#define array_reset(array) ((array)->len = 0)

#define array_first(array) (*(array)->items)
#define array_first_ptr(array) ((array)->items)
#define array_last(array) ((array)->items[(array)->len - 1])
#define array_last_ptr(array) &((array)->items[(array)->len - 1])

#define array_pop(array) ((array)->len--, (array)->items[(array)->len])

#define each(T, i, array) (T *i = (array)->items; i < (array)->items + (array)->len; ++i)

#endif
