#include "allocator.h"

static void *_mem_reserve(usize size) {
    void *ptr = mmap(NULL, size, 0, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr == MAP_FAILED) {
        err("Memory reserve failed: %s\n", strerror(errno));
    }

    return ptr;
}

static void _mem_commit(void *ptr, usize size) {
    if (mprotect(ptr, size, PROT_READ | PROT_WRITE) < 0) {
        err("Memory commit failed: %s\n", strerror(errno));
    }
}

static void _mem_release(void *ptr, usize size) {
    munmap(ptr, size);
}

Arena *arena_init_opt(ArenaOptions opt) {
    usize reserve = opt.reserve_size ? next_power_of_two(opt.reserve_size) : ARENA_DEFAULT_RESERVE;
    usize commit = opt.commit_size ? next_power_of_two(opt.commit_size) : ARENA_DEFAULT_COMMIT;
    if (commit > reserve) {
        reserve <<= 10;
    }
    Arena *a;
    if (opt.backing_buffer) {
        a = (Arena*)opt.backing_buffer;
    } else {
        a = _mem_reserve(reserve);
        _mem_commit(a, commit);
    }

    a->current = a;
    a->reserve = reserve;
    a->commit = commit;
    a->head = ARENA_HEADER_SIZE;

    a->allocator = (Allocator){
        .alloc = arena_alloc,
        .realloc = arena_realloc,
        .free = arena_free,
    };
    return a;
}

Arena *arena_init() {
    return arena_init_opt((ArenaOptions){.reserve_size=ARENA_DEFAULT_RESERVE, .commit_size=ARENA_DEFAULT_COMMIT});
}

// Checks if arena can hold size, otherwise adds a new arena to the list
void arena_ensure_reserve_size(Arena *a, usize size) {
    Arena *cur = a->current;
    usize size_actual = cur->head + size;
    if (size_actual > cur->reserve) {
        // make new arena
        Arena *new = arena_init_opt((ArenaOptions){.reserve_size = next_power_of_two(size_actual)});
        new->prev = cur;
        for (Arena *n = cur; n != NULL; n = n->prev) {
            n->current = new;
        }
    }
}

usize arena_query_size(Arena *a) {
    usize sum = 0;
    for (Arena *node = a->current; node != NULL; node = node->prev) {
        sum += node->head;
    }

    return sum;
}

void *arena_alloc(void *ctx, usize size) {
    usize align = DEFAULT_ALIGN;
    Arena *a = (Arena*)ctx;
    arena_ensure_reserve_size(a, size);
    Arena *cur = a->current;

    usize head = (usize)cur + cur->head;
    void *aligned = align_forward(head, align);
    usize delta = (usize)aligned - head;
    usize req_capacity = cur->head + delta + size;
    if (req_capacity > cur->commit) {
        usize commit_size = next_power_of_two(size);
        _mem_commit((u8*)cur + cur->commit, commit_size);
        cur->commit += commit_size;
    }

    memset(aligned, 0, size);

    cur->head += size + delta;

    return aligned;
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
    for (Arena *cur = a->current; cur != NULL; cur = cur->prev) {
        arena_set_head(cur, ARENA_HEADER_SIZE);
    }
}

usize arena_get_head(Arena *a) {
    return a->current->head;
}

void arena_set_head(Arena *a, usize head) {
    a->head = head >= ARENA_HEADER_SIZE ? head : ARENA_HEADER_SIZE;
}

void arena_deinit(Arena *a) {
    if (!a) return;
    for (Arena *n = a->current, *prev = NULL; n != NULL; n = prev) {
        prev = n->prev;
        _mem_release(n, n->reserve);
    }
}

// TEMP ALLOC

TempAlloc temp_alloc_begin(Arena *arena) {
    return (TempAlloc){
        .arena = arena,
        .start = arena->current->head,
    };
}

void temp_alloc_end(TempAlloc *ta) {
    ta->arena->current->head = ta->start;
}

// STACK ALLOCATOR

void *_stack_allocator_realloc(void *ctx, void *ptr, usize size) {
    return stack_allocator_alloc(ctx, size);
}

StackAllocator stack_allocator_init(u8 *data, usize size) {
    return (StackAllocator){
        .allocator = (Allocator){
            .alloc = stack_allocator_alloc,
            .realloc = _stack_allocator_realloc,
        },
        .data = data,
        .capacity = size,
    };
}

void _stack_allocator_reset(void *ctx) {
    StackAllocator *sa = (StackAllocator*)ctx;
    memset(sa->data, 0, sa->head);
    sa->head = 0;
}

void *stack_allocator_alloc(void *ctx, usize size) {
    StackAllocator *sa = (StackAllocator*)ctx;
    if (sa->head + size > sa->capacity) {
        err("Stack allocator full\n");
        return NULL;
    }
    void *res = sa->data + sa->head;
    sa->head += size;
    memset(res, 0, size);
    return res;
}
