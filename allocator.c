#include "allocator.h"
#include "unistd.h"

static usize page_size() {
    return (usize)getpagesize();
}

static ArenaAllocation *_arena_new_allocation(Arena *a, usize capacity) {
    ArenaAllocation *new = (ArenaAllocation*)malloc(sizeof(ArenaAllocation));
    if (!a->first) {
        a->first = new;
    } else {
        a->last->next = new;
    }
    memset(new, 0, sizeof(*new));
    if (capacity == 0)
        return new;
    
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

static bool _arena_allocation_has_capacity_for_size(ArenaAllocation *a, usize size, usize align) {
    void *aligned = align_forward((usize)a->data + a->head, align);
    usize delta = (usize)aligned - (usize)a->data;
    if (size + delta <= a->capacity) {
        return true;
    }

    return false;
}

static ArenaAllocation *_arena_allocation_for_size(Arena *a, usize size) {
    for (ArenaAllocation *node = a->first; node != NULL; node = node->next) {
        if (_arena_allocation_has_capacity_for_size(node, size, DEFAULT_ALIGN))
            return node;
    }

    return _arena_new_allocation(a, size);
}

Arena arena_init(usize capacity) {
    Arena a = {0};
    a.allocator = (Allocator){
        .alloc = arena_alloc,
        .realloc = arena_realloc,
        .free = arena_free,
    };
    _arena_new_allocation(&a, capacity);
    return a;
}

void arena_ensure_capacity(Arena *a, usize capacity) {
    for (ArenaAllocation *node = a->first; node != NULL; node = node->next) {
        if (_arena_allocation_has_capacity_for_size(node, capacity, DEFAULT_ALIGN)) {
            return;
        }
    }

    _arena_new_allocation(a, capacity);
}

usize arena_query_capacity(Arena *a) {
    usize sum = 0;
    for (ArenaAllocation *node = a->first; node != NULL; node = node->next) {
        sum += node->head;
    }

    return sum;
}

void *arena_alloc(void *ctx, usize size) {
    usize align = DEFAULT_ALIGN;
    Arena *a = (Arena*)ctx;
    ArenaAllocation *head_alloc = _arena_allocation_for_size(a, size);
    void *data = &head_alloc->data[head_alloc->head];
    void *aligned = align_forward((usize)data, align);
    usize delta = ((usize)aligned - (usize)data);
    if (head_alloc->head + size + delta <= head_alloc->capacity) {
        head_alloc->head += delta + size;
        memset(aligned, 0, size);

        return aligned;
    }

    err("Arena out of memory\n");
    return NULL;
}

// For now, simply allocates new memory without checking if old memory can be
// reused. Don't reuse a pointer passed into this function, always use the
// returned pointer.
// TODO check if this is the previous allocation and can therefore be reused
void *arena_realloc(void *ctx, void *ptr, usize size)
{
    return arena_alloc(ctx, size);
}

void arena_free(void *ctx, void *ptr) {}

// Resets the head to zero, allowing for re-use of arena without reallocating
void arena_reset(Arena *a) {
    for (ArenaAllocation *node = a->first; node != NULL; node = node->next) {
        node->head = 0;
    }
}

void arena_set_head(Arena *a, usize head) {
    a->last->head = head;
}

void arena_deinit(Arena *a) {
    if (!a) return;
    for (ArenaAllocation *node = a->first; node != NULL; node = node->next) {
        free(node->data);
    }
}


// TEMP ARENA

TempArena temp_arena_init(usize capacity) {
    return (TempArena){
        .allocator = {
            .alloc = temp_arena_alloc,
            .realloc = NULL,
            .free = NULL,
        },
        .data = malloc(capacity),
        .capacity = capacity,
        .head = 0,
    };
}

void temp_arena_deinit(TempArena *ta) {
    free(ta->data);
}

void *temp_arena_alloc(void *ta, usize size) {
    TempArena *arena = (TempArena*)ta;
    assert(arena->head + size <= arena->capacity);
    void *result = arena->data + arena->head;
    arena->head += size;
    return result;
}

void temp_arena_reset(TempArena *ta) {
    ta->head = 0;
}

