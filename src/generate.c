#include <assert.h>

#ifndef NDEBUG
static bool can_value(Generator g)
{
        if(g->stack.size && stack_peek(&g->stack) == STACK_OBJECT) {
                if(g->key_next) {
                        g->error = make_error(JSONPG_ERROR_EXPECTED_KEY, g->count);
                        return false;
                } else {
                        g->key_next = true;
                }
        }
        return true;

}

static bool can_key(Generator g)
{
        if(g->stack.size && !g->key_next) {
                g->error = make_error(JSONPG_ERROR_EXPECTED_VALUE, g->count);
                return false;
        }
        g->key_next = false;
        return true;
}

static bool can_push(Generator g, int type)
{
        if(!can_value(g))
                return false;
        if(g->stack.size) {
                if(-1 == stack_push(&g->stack, type)) {
                        g->error = make_error(JSONPG_ERROR_STACK_OVERFLOW,
                                        g->count);
                        return false;
                }
                g->key_next = type == STACK_OBJECT;
        }
        return true;
        
}

static bool can_pop(Generator g, int type)
{
        int cur_type;
        if(g->stack.size) {
                cur_type = stack_peek(&g->stack);
                if(cur_type == -1) {
                        g->error = make_error(JSONPG_ERROR_STACK_UNDERFLOW,
                                        g->count);
                        return false;
                } else if(type != cur_type) {
                        g->error = make_error((type == STACK_OBJECT)
                                ? JSONPG_ERROR_NO_OBJECT
                                : JSONPG_ERROR_NO_ARRAY,
                                        g->count);
                        return false;
                } else if(type == STACK_OBJECT && !g->key_next) {
                        g->error = make_error(JSONPG_ERROR_EXPECTED_VALUE,
                                        g->count);
                        return false;
                }
                stack_pop(&g->stack);
                g->key_next = STACK_OBJECT == stack_peek(&g->stack);
        }
        return true;
}
#endif  // ifndef NDEBUG

bool jsonpg_null(Generator g)
{
        ASSERT(can_value(g));

        return (!g->callbacks->null) || g->callbacks->null(g->ctx);
}

bool jsonpg_boolean(Generator g, bool is_true)
{
        ASSERT(can_value(g));

        return  (!g->callbacks->boolean) || g->callbacks->boolean(g->ctx, is_true);
}

bool jsonpg_integer(Generator g, long integer)
{
        ASSERT(can_value(g));

        return  (!g->callbacks->integer) || g->callbacks->integer(g->ctx, integer);
}

bool jsonpg_real(Generator g, double real)
{
        ASSERT(can_value(g));

        return  (!g->callbacks->real) || g->callbacks->real(g->ctx, real);
}

bool jsonpg_string(Generator g, uint8_t *bytes, size_t count)
{
        ASSERT(can_value(g));

        return (!g->callbacks->string) || g->callbacks->string(g->ctx, bytes, count);
}

bool jsonpg_key(Generator g, uint8_t *bytes, size_t count)
{
        ASSERT(can_key(g));

        return (!g->callbacks->key) || g->callbacks->key(g->ctx, bytes, count);
}

bool jsonpg_start_array(Generator g)
{
        ASSERT(can_push(g, STACK_ARRAY));

        return  (!g->callbacks->start_array) ||g->callbacks->start_array(g->ctx);
}

bool jsonpg_end_array(Generator g)
{
        ASSERT(can_pop(g, STACK_ARRAY));

        return  (!g->callbacks->end_array) ||g->callbacks->end_array(g->ctx);
}

bool jsonpg_start_object(Generator g)
{
        ASSERT(can_push(g, STACK_OBJECT));

        return (!g->callbacks->start_object) ||g->callbacks->start_object(g->ctx);
}

bool jsonpg_end_object(Generator g)
{
        ASSERT(can_pop(g, STACK_OBJECT));

        return  (!g->callbacks->end_object) ||g->callbacks->end_object(g->ctx);
}

static Generator generator_reset(Generator g)
{
        g->count = 0;
        g->error = (ErrorInfo) {};
        return g;
}

static Generator generator_new(uint16_t stack_size)
{
        Allocator a = allocator_new();
        if(!a)
                return NULL;
        Generator g = allocator_alloc(a, sizeof(generator_s)
                        + (stack_size >> 3));
        if(!g)
                return NULL;

        g->allocator = a;
        g->key_next = false;

        g->stack = (struct stack_s) {
                .ptr = 0,
                .size = stack_size,
                .stack = ((void *)g) + sizeof(generator_s)
        };

        return g;
}

static Generator generator_set_callbacks(
                Generator g,
                Callbacks *callbacks, 
                void *ctx)
{
        g->callbacks = callbacks;
        g->ctx = ctx;
        return g;
}

void jsonpg_generator_free(Generator g)
{
        if(!g)
                return;

        allocator_free(g->allocator);
}

Generator jsonpg_generator_new_opt(GeneratorOpts opts)
{
        if(1 < (opts.dom == true) + (opts.callbacks != NULL))
                return NULL;
        
        unsigned indent = opts.indent;
        if(indent < 0)
                indent = 0;
        else if (indent > 8)
                indent = 8;

        Generator g = generator_new(opts.max_nesting);
        if(!g)
                return NULL;

        else if(opts.dom)
                return dom_generator(g);
        else if(opts.callbacks)
                return generator_set_callbacks(g, opts.callbacks, opts.ctx);
        else
                return json_generator(g, indent);
}
