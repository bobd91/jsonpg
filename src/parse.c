#include <math.h>
#include <errno.h>

#define MIN_STACK_SIZE 1024

static int set_string_value(jsonpg_parser p, token t)
{
        if(p->write_buf->count) {
                if(write_b(t->pos, p->current - t->pos))
                        return -1;
                p->result.string.length = get_content(&p->result.string.bytes);
                p->write_buf->count = 0;
        } else {
                p->result.string.bytes = t->pos;
                p->result.string.length = p->current - t->pos;
        }
        return 0;
}

void jsonpg_parser_free(jsonpg_parser p) 
{
        if(!p)
                return;

        arena_free(p->arena);   
}


jsonpg_type jsonpg_parse_next(jsonpg_parser p)
{
        if(p->input)
                return parse_next(p);
        else
                return dom_parse_next(p);
}

static jsonpg_value parse(jsonpg_parser p, jsonpg_generator g)
{
        jsonpg_type type;
        int abort = 0;
        while(!abort && JSONPG_EOF != (type = jsonpg_parse_next(p)))
                abort = generate(g, type, &p->result);

        jsonpg_value val;
        if(abort) {
                val.type = JSONPG_ERROR;
                val.error = g->error.code
                        ? g->error
                        : make_error(JSONPG_ERROR_ABORT, 0);
        } else {
                val = (jsonpg_value) { .type = JSONPG_EOF };
        }

        return val;
}

static void parser_set_bytes(
                jsonpg_parser p, 
                uint8_t *bytes, 
                size_t count)
{
        p->input = p->current = bytes;
        p->input_size = count;
        p->last = bytes + count;
        p->stack.ptr = p->stack.ptr_min;
        p->token_ptr = 0;
        p->state = STATE_INITIAL;

        // Skip leading byte order mark
        p->current += utf8_bom_bytes(p->input, p->input_size);
}

void parser_set_dom_info(jsonpg_parser p, dom_info di)
{
        p->dom_info = di;
}

jsonpg_parser parser_reset(jsonpg_parser p)
{
        str_buf_reset(p->write_buf);
        
        p->input = NULL;

        p->dom_info = (dom_info){};

        return p;
}

jsonpg_value jsonpg_parse_opt(jsonpg_parse_opts opts)
{
        Generator g;
        Parser p;
        
        p = jsonpg_parser_new(
                        .max_nesting = opts.max_nesting,
                        .flags = opts.flags);
        if(!p)
                return make_error_return(JSONPG_ERROR_ALLOC, 0);

        if(1 != (opts.bytes != NULL) + (opts.string != NULL) + (opts.dom != NULL)) {
                opt_error(p);
                return p->result;
        }

        if(opts.bytes) {
                 parser_set_bytes(p, opts.bytes, opts.count);
        } else if(opts.string) {
                 parser_set_bytes(p, (uint8_t *)opts.string, strlen(opts.string));
        } else if(opts.dom) {
                parser_set_dom_info(p, dom_parser_info(opts.dom));
        }

        if(1 != (opts.callbacks != NULL) + (opts.generator != NULL)) {
                opt_error(p);
                return p->result;
        }

        if(opts.callbacks) {
                g = generator_new(0);
                if(!g) {
                        alloc_error(p);
                        return p->result;
                }
                generator_set_callbacks(g, opts.callbacks, opts.ctx);
        } else {
                g = generator_reset(opts.generator);
        }
        
        jsonpg_value result = parse_generate(p, g);

        jsonpg_parser_free(p);
        if(opts.callbacks)
                jsonpg_generator_free(g);

        return result;
}

static uint16_t get_stack_size(uint16_t stack_size)
{
        return stack_size > MIN_STACK_SIZE ? stack_size : MIN_STACK_SIZE;
}

Parser jsonpg_parser_new_opt(jsonpg_parser_opts opts)
{
        uint16_t stack_size = get_stack_size(opts.max_nesting);
        uint16_t flags = opts.flags;

        size_t struct_bytes = sizeof(struct jsonpg_parser_s);
        arena a = arena_new();
        if(!a)
                return NULL;
        Parser p = arena_alloc(a, struct_bytes + ((stack_size + 7) / 8));
        if(p) {
                p->arena = a;
                p->write_buf = str_buf_new(a, 0);
                if(!p->write_buf) {
                        jsonpg_parser_free(p);
                        return NULL;
                }

                p->input = NULL;

                p->stack.size = stack_size;
                p->stack.stack = (uint8_t *)(((void *)p) + struct_bytes);
                p->flags = flags;
        }
        return p;
}

jsonpg_value jsonpg_parse_result(jsonpg_parser p)
{
        return p->result;
}

jsonpg_error_value jsonpg_parse_error(jsonpg_parser p)
{
        return p->result.error;
}
