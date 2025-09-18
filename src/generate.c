/*
 * jsonpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * generate.c
 *   provides a dual role
 *   1 - delivers parse events from parser to user provided callback
 *   2 - delivers user generated events to produce JSON output
 *
 *   during development/testing it will assert() that the events make
 *   a valid JSON document (for example,  [2} will be rejected)
 */
#include <assert.h>

#ifndef NDEBUG
static bool can_value(generator *g)
{
        if(g->stack.size && stack_peek(&g->stack) == STACK_OBJECT) {
                if(g->key_next) {
                        g->error = make_error(JSONPG_ERROR_EXPECTED_KEY);
                        return false;
                } else {
                        g->key_next = true;
                }
        }
        return true;

}

static bool can_key(generator *g)
{
        if(g->stack.size && !g->key_next) {
                g->error = make_error(JSONPG_ERROR_EXPECTED_VALUE);
                return false;
        }
        g->key_next = false;
        return true;
}

static bool can_push(generator *g, int type)
{
        if(!can_value(g))
                return false;
        if(g->stack.size) {
                if(-1 == stack_push(&g->stack, type)) {
                        g->error = make_error(JSONPG_ERROR_STACK_OVERFLOW);
                        return false;
                }
                g->key_next = type == STACK_OBJECT;
        }
        return true;
        
}

static bool can_pop(generator *g, int type)
{
        int cur_type;
        if(g->stack.size) {
                cur_type = stack_peek(&g->stack);
                if(cur_type == -1) {
                        g->error = make_error(JSONPG_ERROR_STACK_UNDERFLOW);
                        return false;
                } else if(type != cur_type) {
                        g->error = make_error((type == STACK_OBJECT)
                                ? JSONPG_ERROR_NO_OBJECT
                                : JSONPG_ERROR_NO_ARRAY);
                        return false;
                } else if(type == STACK_OBJECT && !g->key_next) {
                        g->error = make_error(JSONPG_ERROR_EXPECTED_VALUE);
                        return false;
                }
                stack_pop(&g->stack);
                g->key_next = STACK_OBJECT == stack_peek(&g->stack);
        }
        return true;
}
#endif  // ifndef NDEBUG

bool jsonpg_null(generator *g)
{
        ASSERT(can_value(g));

        return (!g->callbacks->null) || g->callbacks->null(g->ctx);
}

bool jsonpg_boolean(generator *g, bool is_true)
{
        ASSERT(can_value(g));

        return  (!g->callbacks->boolean) || g->callbacks->boolean(g->ctx, is_true);
}

bool jsonpg_integer(generator *g, long integer)
{
        ASSERT(can_value(g));

        return  (!g->callbacks->integer) || g->callbacks->integer(g->ctx, integer);
}

bool jsonpg_real(generator *g, double real)
{
        ASSERT(can_value(g));

        return  (!g->callbacks->real) || g->callbacks->real(g->ctx, real);
}

bool jsonpg_string(generator *g, const byte *bytes, size_t count)
{
        ASSERT(can_value(g));

        return (!g->callbacks->string) || g->callbacks->string(g->ctx, bytes, count);
}

bool jsonpg_key(generator *g, const byte *bytes, size_t count)
{
        ASSERT(can_key(g));

        return (!g->callbacks->key) || g->callbacks->key(g->ctx, bytes, count);
}

bool jsonpg_start_array(generator *g)
{
        ASSERT(can_push(g, STACK_ARRAY));

        return  (!g->callbacks->start_array) ||g->callbacks->start_array(g->ctx);
}

bool jsonpg_end_array(generator *g)
{
        ASSERT(can_pop(g, STACK_ARRAY));

        return  (!g->callbacks->end_array) ||g->callbacks->end_array(g->ctx);
}

bool jsonpg_start_object(generator *g)
{
        ASSERT(can_push(g, STACK_OBJECT));

        return (!g->callbacks->start_object) ||g->callbacks->start_object(g->ctx);
}

bool jsonpg_end_object(generator *g)
{
        ASSERT(can_pop(g, STACK_OBJECT));

        return  (!g->callbacks->end_object) ||g->callbacks->end_object(g->ctx);
}

static generator *generator_reset(generator *g, unsigned flags)
{
        g->count = 0;
        g->validate_utf8 = !(flags & JSONPG_ALLOW_INVALID_UTF8_OUT);
        g->error = (error_info) {};
        return g;
}

static generator *generator_new(unsigned stack_size, unsigned flags)
{
        allocator *a = allocator_new();
        if(!a)
                return NULL;
        generator *g = allocator_alloc(a, sizeof(generator)
                        + (stack_size >> 3));
        if(!g)
                return NULL;

        g->allocator = a;
        g->key_next = false;
        g->validate_utf8 = !(flags & JSONPG_ALLOW_INVALID_UTF8_OUT);

        g->stack = (stack) {
                .ptr = 0,
                .size = stack_size,
                .stack = ((byte *)g) + sizeof(generator)
        };

        return g;
}

static generator *generator_set_callbacks(
                generator *g,
                callbacks *callbacks, 
                void *ctx)
{
        g->callbacks = callbacks;
        g->ctx = ctx;
        return g;
}

void jsonpg_generator_free(generator *g)
{
        if(!g)
                return;

        allocator_free(g->allocator);
}

generator *jsonpg_generator_new_opt(generator_opts opts)
{
        unsigned flags = opts.allow;

        if(1 < (opts.dom == true) + (opts.callbacks != NULL))
                return NULL;
        
        unsigned indent = opts.indent <= 8 ? opts.indent : 8;

        generator *g = generator_new(opts.max_nesting, flags);
        if(!g)
                return NULL;

        else if(opts.dom)
                return dom_generator(g);
        else if(opts.callbacks)
                return generator_set_callbacks(g, opts.callbacks, opts.ctx);
        else
                return json_generator(g, indent);
}

error_info jsonpg_result_error(generator *g)
{
        return g->error;
}
