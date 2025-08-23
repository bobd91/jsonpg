#include <stdlib.h>

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
