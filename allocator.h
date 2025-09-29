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

// The backing allocation from which the arena passes out new allocation references
typedef struct ArenaAllocation {
    struct ArenaAllocation *next;
    u8 *data;
    usize head;
    usize capacity;
} ArenaAllocation;

typedef struct {
    Allocator allocator;
    ArenaAllocation *first;
    ArenaAllocation *last;
} Arena;

static void *arena_alloc(void *, usize);
static void *arena_realloc(void*, void *, usize);
static void arena_free(void *);

static ArenaAllocation *_arena_new_allocation(Arena *a, usize capacity) {
    ArenaAllocation *new = (ArenaAllocation*)malloc(sizeof(ArenaAllocation));
    if (!a->first) {
        a->first = new;
    } else {
        a->last->next = new;
    }
    memset(new, 0, sizeof(*new));
    new->data = (u8*)malloc(capacity);
    if (!new->data) {
        err("Failed to allocate new data\n");
        return NULL;
    }
    new->capacity = capacity;
    memset(new->data, 0, capacity);
    a->last = new;

    return new;
}

static bool _arena_has_capacity_for_size(Arena *a, usize size) {
    ArenaAllocation *head_alloc = a->last;
    if (head_alloc->head + size <= head_alloc->capacity) {
        return true;
    }

    return false;
}

static Arena arena_init(usize capacity) {
    Arena a = {0};
    a.allocator = (Allocator){
        .alloc = arena_alloc,
        .realloc = arena_realloc,
        .free = arena_free,
    };
    a.first = _arena_new_allocation(&a, capacity);
    return a;
}

static void arena_ensure_capacity(Arena *a, usize capacity) {
    if (!_arena_has_capacity_for_size(a, capacity)) {
        _arena_new_allocation(a, capacity);
    }
}

static void *arena_alloc(void *ctx, usize size) {
    usize align = DEFAULT_ALIGN;
    Arena *a = (Arena*)ctx;
    ArenaAllocation *head_alloc;
    if (_arena_has_capacity_for_size(a, size)) {
        head_alloc = a->last;
    } else {
        head_alloc = _arena_new_allocation(a, next_power_of_two(size));
    }
    void *data = &head_alloc->data[head_alloc->head];
    void *aligned = align_forward(data, align);
    usize delta = ((usize)aligned - (usize)data);
    if (head_alloc->head + size + delta <= head_alloc->capacity) {
        head_alloc->head += delta + size;
        memset(aligned, 0, size);

        return aligned;
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
    for (ArenaAllocation *node = a->first; node != NULL; node = node->next) {
        node->head = 0;
    }
}

static void arena_set_head(Arena *a, usize head) {
    a->last->head = head;
}

static void arena_deinit(Arena *a) {
    if (!a) return;
    for (ArenaAllocation *node = a->first; node != NULL; node = node->next) {
        free(node->data);
    }
}

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

#define array_append(alloc, array, item) do {\
    if ((array)->len + 1 > (array)->cap) { \
        array_reserve(alloc, array, (array)->cap * 2); \
    } \
    (array)->items[(array)->len++] = (item);\
} while (0)

#define array_append_many(alloc, array, item_ptr, count) do { \
    if ((array)->len + count > (array)->cap) { \
        array_reserve(alloc, array, (array)->cap * 2); \
    } \
    memcpy((array)->items + (array)->len, item_ptr, count * sizeof(*item_ptr)); \
    (array)->len += count; \
} while(0)

#define array_resize(alloc, array, size) do {\
    array_reserve(alloc, array, size);\
    (array)->len = size;\
} while (0)

#define array_last(array) &((array)->items[(array)->len - 1])

#define each_item(T, i, array) (T *i = (array)->items; i < (array)->items + (array)->len; ++i)

#endif
