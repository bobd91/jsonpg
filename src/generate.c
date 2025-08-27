#include <assert.h>


static int cannot_value(jsonpg_generator g)
{
        if(g->stack.size
                        && peek_stack(&g->stack) == STACK_OBJECT
                        && g->key_next) {
                g->error = make_error(JSONPG_ERROR_EXPECTED_KEY, g->count);
                return 1;
        }
        return 0;

}

static int cannot_key(jsonpg_generator g)
{
        if(g->stack.size && !g->key_next) {
                g->error = make_error(JSONPG_ERROR_EXPECTED_VALUE, g->count);
                return 1;
        }
        g->key_next = 0;
        return 0;
}

static int cannot_push(jsonpg_generator g, int type)
{
        if(cannot_value(g))
                return 1;
        if(g->stack.size) {
                if(-1 == push_stack(&g->stack, type)) {
                        g->error = make_error(JSONPG_ERROR_STACK_OVERFLOW,
                                        g->count);
                        return 1;
                }
                g->key_next = type == STACK_OBJECT;
        }
        return 0;
        
}

static int cannot_pop(jsonpg_generator g, int type)
{
        int cur_type;
        if(g->stack.size) {
                cur_type = peek_stack(&g->stack);
                if(cur_type == -1) {
                        g->error = make_error(JSONPG_ERROR_STACK_UNDERFLOW,
                                        g->count);
                        return 1;
                } else if(type != cur_type) {
                        g->error = make_error((type == STACK_OBJECT)
                                ? JSONPG_ERROR_NO_OBJECT
                                : JSONPG_ERROR_NO_ARRAY,
                                        g->count);
                        return 1;
                } else if(type == STACK_OBJECT && !g->key_next) {
                        g->error = make_error(JSONPG_ERROR_EXPECTED_VALUE,
                                        g->count);
                        return 1;
                }
                pop_stack(&g->stack);
                g->key_next = STACK_OBJECT == peek_stack(&g->stack);
        }
        return 0;
}

int jsonpg_null(jsonpg_generator g)
{
        return  cannot_value(g)
                || (g->callbacks->null 
                        && g->callbacks->null(g->ctx));
}

int jsonpg_boolean(jsonpg_generator g, bool is_true)
{
        return  cannot_value(g)
                || (g->callbacks->boolean 
                        && g->callbacks->boolean(g->ctx, is_true));
}

int jsonpg_integer(jsonpg_generator g, long integer)
{
        return  cannot_value(g)
                || (g->callbacks->integer
                        && g->callbacks->integer(g->ctx, integer));
}

int jsonpg_real(jsonpg_generator g, double real)
{
        return  cannot_value(g)
                || (g->callbacks->real 
                        && g->callbacks->real(g->ctx, real));
}

int jsonpg_string(jsonpg_generator g, uint8_t *bytes, size_t count)
{
        return cannot_value(g)
                || (g->callbacks->string
                        && g->callbacks->string(g->ctx, bytes, count));
}

int jsonpg_key(jsonpg_generator g, uint8_t *bytes, size_t count)
{
        return cannot_key(g)
                || (g->callbacks->key
                        && g->callbacks->key(g->ctx, bytes, count));
}

int jsonpg_begin_array(jsonpg_generator g)
{
        return  cannot_push(g, STACK_ARRAY)
                || (g->callbacks->begin_array 
                        && g->callbacks->begin_array(g->ctx));
}

int jsonpg_end_array(jsonpg_generator g)
{
        return  cannot_pop(g, STACK_ARRAY)
                || (g->callbacks->end_array 
                        && g->callbacks->end_array(g->ctx));
}

int jsonpg_begin_object(jsonpg_generator g)
{
        return cannot_push(g, STACK_OBJECT)
                || (g->callbacks->begin_object 
                        && g->callbacks->begin_object(g->ctx));
}

int jsonpg_end_object(jsonpg_generator g)
{
        return  cannot_pop(g, STACK_OBJECT)
                || (g->callbacks->end_object 
                        && g->callbacks->end_object(g->ctx));
}

static int gen_error(jsonpg_generator g, int code, int at)
{
        g->error = make_error(code, at);
        (void)(g->callbacks->error 
                && g->callbacks->error(g->ctx, code, at));
        return 1; // always abort after error
}

static jsonpg_generator generator_reset(jsonpg_generator g)
{
        g->count = 0;
        return g;
}

static void set_generator_error(jsonpg_generator g, jsonpg_error_code code)
{
        g->error = make_error(code, g->count);
}
        
static int generate(jsonpg_generator g, jsonpg_type type, jsonpg_value *value)
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
jsonpg_generator generator_new(
                jsonpg_callbacks *callbacks, 
                void *ctx,
                void (*free_ctx)(void *),
                uint16_t stack_size)
{
        jsonpg_generator g = pg_alloc(sizeof(struct jsonpg_generator_s)
                        + (stack_size >> 3));
        if(!g)
                return NULL;
        g->callbacks = callbacks;
        g->ctx = ctx;
        g->free_ctx = free_ctx;

        g->key_next = 0;

        g->stack = (struct stack_s) {
                .ptr = 0,
                .ptr_min = 0,
                .size = stack_size,
                .stack = ((void *)g) + sizeof(struct jsonpg_generator_s)
        };

        return g;
}

void jsonpg_generator_free(jsonpg_generator g)
{
        if(!g)
                return;

        if(g->free_ctx)
                (*g->free_ctx)(g->ctx);

        pg_dealloc(g);
}

jsonpg_generator callback_generator(jsonpg_callbacks *callbacks, void *ctx)
{
        return generator_new(callbacks, ctx, NULL, 0);
}

jsonpg_generator jsonpg_generator_new_opt(jsonpg_generator_opts opts)
{
        if(1 != (opts.fd > 0) 
                        + (opts.buffer == true)
                        + (opts.dom == true)
                        + (opts.writer != NULL)
                        + (opts.callbacks != NULL))
                return NULL;

        if(opts.fd >= 0)
                return file_printer(opts.fd, opts.indent, opts.max_nesting);
        else if(opts.buffer)
                return buffer_printer(opts.indent, opts.max_nesting);
        else if(opts.writer)
                return write_printer(opts.writer, opts.indent, opts.max_nesting);
        else if(opts.dom)
                return dom_generator();
        else
                return callback_generator(opts.callbacks, opts.ctx);

}
