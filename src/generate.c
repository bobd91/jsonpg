#include <assert.h>

#ifndef NDEBUG
static bool can_value(Generator g)
{
        if(g->stack.size && peek_stack(&g->stack) == STACK_OBJECT) {
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
                if(-1 == push_stack(&g->stack, type)) {
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
                cur_type = peek_stack(&g->stack);
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
                pop_stack(&g->stack);
                g->key_next = STACK_OBJECT == peek_stack(&g->stack);
        }
        return true;
}
#endif  // ifndef NDEBUG

int jsonpg_null(Generator g)
{
        JSONPG_ASSERT(can_value(g));

        return (!g->callbacks->null) || g->callbacks->null(g->ctx);
}

int jsonpg_boolean(Generator g, bool is_true)
{
        JSONPG_ASSERT(can_value(g));

        return  (!g->callbacks->boolean) || g->callbacks->boolean(g->ctx, is_true);
}

int jsonpg_integer(Generator g, long integer)
{
        JSONPG_ASSERT(can_value(g));

        return  (!g->callbacks->integer) || g->callbacks->integer(g->ctx, integer);
}

int jsonpg_real(Generator g, double real)
{
        JSONPG_ASSERT(can_value(g));

        return  (!g->callbacks->real) || g->callbacks->real(g->ctx, real);
}

int jsonpg_string(Generator g, uint8_t *bytes, size_t count)
{
        JSONPG_ASSERT(can_value(g));

        return (!g->callbacks->string) || g->callbacks->string(g->ctx, bytes, count);
}

int jsonpg_key(Generator g, uint8_t *bytes, size_t count)
{
        JSONPG_ASSERT(can_key(g));

        return (!g->callbacks->key) || g->callbacks->key(g->ctx, bytes, count);
}

int jsonpg_begin_array(Generator g)
{
        JSONPG_ASSERT(can_push(g, STACK_ARRAY));

        return  (!g->callbacks->begin_array) ||g->callbacks->begin_array(g->ctx);
}

int jsonpg_end_array(Generator g)
{
        JSONPG_ASSERT(can_pop(g, STACK_ARRAY));

        return  (!g->callbacks->end_array) ||g->callbacks->end_array(g->ctx);
}

int jsonpg_begin_object(Generator g)
{
        JSONPG_ASSERT(can_push(g, STACK_OBJECT));

        return (!g->callbacks->begin_object) ||g->callbacks->begin_object(g->ctx);
}

int jsonpg_end_object(Generator g)
{
        JSONPG_ASSERT(can_pop(g, STACK_OBJECT));

        return  (!g->callbacks->end_object) ||g->callbacks->end_object(g->ctx);
}

static int gen_error(Generator g, int code, int at)
{
        g->error = make_error(code, at);
        (void)(g->callbacks->error 
                && g->callbacks->error(g->ctx, code, at));
        return 1; // always abort after error
}

static Generator generator_reset(Generator g)
{
        g->count = 0;
        g->error = (ErrorInfo) {};
        return g;
}

static int generate(Generator g, JsonType type, ParseResult *value)
{
        g->count++;
        switch(type) {
        case JSONPG_NULL:
                return jsonpg_null(g);     
        case JSONPG_FALSE:
        case JSONPG_TRUE:
                return jsonpg_boolean(g, JSONPG_TRUE == type);
        case JSONPG_INTEGER:
                return jsonpg_integer(g, value->number.integer);
        case JSONPG_REAL:
                return jsonpg_real(g, value->number.real);
        case JSONPG_STRING:
                return jsonpg_string(g, value->string.bytes, value->string.length);
        case JSONPG_KEY:
                return jsonpg_key(g, value->string.bytes, value->string.length);
        case JSONPG_BEGIN_ARRAY:
                return jsonpg_begin_array(g);
        case JSONPG_END_ARRAY:
                return jsonpg_end_array(g);
        case JSONPG_BEGIN_OBJECT:
                return jsonpg_begin_object(g);
        case JSONPG_END_OBJECT:
                return jsonpg_end_object(g);
        case JSONPG_ERROR:
                return gen_error(g, value->error.code, value->error.at);
        default:
                assert(!type && 0);
                return 1;
        }
}

static Generator generator_new(uint16_t stack_size)
{
        Allocator a = allocator_new();
        if(!a)
                return NULL;
        Generator g = allocator_alloc(a, sizeof(struct jsonpg_generator_s)
                        + (stack_size >> 3));
        if(!g)
                return NULL;

        g->allocator = a;
        g->key_next = false;

        g->stack = (struct stack_s) {
                .ptr = 0,
                .ptr_min = 0,
                .size = stack_size,
                .stack = ((void *)g) + sizeof(struct jsonpg_generator_s)
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
        
        int indent = opts.indent;
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
                return string_printer(g, indent);
}
