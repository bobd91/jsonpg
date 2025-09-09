#include <stdlib.h>
#include <string.h>

#define ALLOCATOR_DEFAULT_ALLOCS 1024

// These are the low level allocators 
// Default to malloc, realloc and free but can be replaced
static void *(*pg_alloc)(size_t) = malloc;
static void *(*pg_realloc)(void *, size_t) = realloc;
static void (*pg_dealloc)(void *) = free;

void jsonpg_set_allocators(
                void *(*malloc)(size_t),
                void *(*realloc)(void *, size_t),
                void (*free)(void *))
{
        pg_alloc = malloc;
        pg_realloc = realloc;
        pg_dealloc = free;
}


// This is not an arena allocator, but it looks like one from the outside.
// Internally it just keeps track of all allocations so that it can free
// them when the allocator is freed.

typedef struct allocator_s *Allocator;

struct allocator_s {
        size_t used;
        size_t capacity;
        void ** allocs;
};

static Allocator allocator_new()
{
        Allocator a = pg_alloc(sizeof(struct allocator_s));
        if(!a)
                return NULL;

        void **allocs = pg_alloc(ALLOCATOR_DEFAULT_ALLOCS * sizeof(void *));
        if(!allocs) {
                pg_dealloc(a);
                return NULL;
        }

        JSONPG_LOG("Arena %p created with allocation buffer %p\n", 
                        a, ALLOCATOR_DEFAULT_ALLOCS, allocs);

        a->used = 0;
        a->capacity = ALLOCATOR_DEFAULT_ALLOCS;
        a->allocs = allocs;

        return a;
}

static void allocator_free(Allocator a)
{
        if(!a)
                return;
        
        for(int i = 0 ; i < a->used ; i++) {
                JSONPG_LOG("Allocation[%ld] %p freed\n", i, a->allocs[i]);
                pg_dealloc(a->allocs[i]);
        }

        JSONPG_LOG("Allocation buffer %p freed\n", a->allocs);
        pg_dealloc(a->allocs);

        JSONPG_LOG("Arena %p freed\n", a);
        pg_dealloc(a);
}

static void *allocator_alloc(Allocator a, size_t size)
{
        if(a->used == a->capacity) {
                void **new_a = pg_realloc(a->allocs, a->capacity << 1);
                if(!new_a)
                        return NULL;
                a->capacity <<= 1;
                a->allocs = new_a;
        }

        void *p = pg_alloc(size);
        if(!p)
                return NULL;

        JSONPG_LOG("Arena %p allocated %ld bytes to %p\n", a, size, p);

        a->allocs[a->used++] = p;
        return p;
}

static void *allocator_realloc(Allocator a, void *p, size_t new_size)
{
        for(int i = 0 ; i < a->used ; i++) {
                if(p == a->allocs[i]) {
                        void *np = pg_realloc(p, new_size);
                        if(!np)
                                return NULL;

                        JSONPG_LOG("Arena %p reallocated %ld bytes from %p to %p\n",
                                        a, new_size, p, np);

                        a->allocs[i] = np;
                        return np;
                }
        }
        return NULL;
}
