#include <stdio.h>

#ifdef JSONPG_DEBUG
static void dump_p(jsonpg_parser p)
{
        fprintf(stderr, "Parser Error:\n");
        fprintf(stderr, "Error: %d\n", p->result.error.code);
        fprintf(stderr, "At Position: %ld\n", p->result.error.at);
        fprintf(stderr, "Input Length: %ld\n", p->last - p->input);
        fprintf(stderr, "Input Processed: %ld\n", p->current - p->input);
        fprintf(stderr, "Stack Size: %d\n", p->stack.size);
        fprintf(stderr, "Stack Pointer: %d\n", p->stack.ptr);
        fprintf(stderr, "Stack: ");
        for(int i = 0 ; i < p->stack.ptr ; i++) {
                int offset = i >> 3;
                int mask = 1 << (i & 0x07);
                fprintf(stderr, "%c", 
                                (mask & p->stack.stack[offset]) ? '[' : '{');
        }
        fprintf(stderr, "\n");

}

static void dump_g(jsonpg_generator g)
{
        fprintf(stderr, "Generator Error:\n");
        fprintf(stderr, "Error: %d\n", g->error.code);
        fprintf(stderr, "At Token: %ld\n", g->error.at);
        fprintf(stderr, "Stack Size: %d\n", g->stack.size);
        fprintf(stderr, "Stack Pointer: %d\n", g->stack.ptr);
        fprintf(stderr, "Stack: ");
        for(int i = 0 ; i < g->stack.ptr ; i++) {
                int offset = i >> 3;
                int mask = 1 << (i & 0x07);
                fprintf(stderr, "%c", 
                                (mask & g->stack.stack[offset]) ? '[' : '{');
        }
        fprintf(stderr, "\n");
}
#endif

static jsonpg_error_value make_error(jsonpg_error_code code, size_t at)
{
        return (jsonpg_error_value){ .code = code, .at = at };
}

static jsonpg_value make_error_return(jsonpg_error_code code, size_t at)
{
        return (jsonpg_value) {
                        .type = JSONPG_ERROR,
                        .error = make_error(code, at)
        };
}

static void set_generator_error(jsonpg_generator g, jsonpg_error_code code)
{
        g->error = make_error(code, g->count);

#ifdef JSONPG_DEBUG
        dump_g(g);
#endif
}

static jsonpg_type set_result_error(jsonpg_parser p, jsonpg_error_code code) 
{
        p->result.type = JSONPG_ERROR;
        p->result.error.code = code;
        p->result.error.at = (p->input && p->current)
                ? p->processed + (p->current - p->input)
                : 0;

#ifdef JSONPG_DEBUG
        dump_p(p);
#endif

        return JSONPG_ERROR;
}

static jsonpg_type parse_error(jsonpg_parser p)
{
        return set_result_error(p, JSONPG_ERROR_PARSE);
}

static jsonpg_type number_error(jsonpg_parser p)
{
        return set_result_error(p, JSONPG_ERROR_NUMBER);
}

static jsonpg_type alloc_error(jsonpg_parser p)
{
        return set_result_error(p, JSONPG_ERROR_ALLOC);
}

static jsonpg_type file_read_error(jsonpg_parser p)
{
        return set_result_error(p, JSONPG_ERROR_FILE_READ);
}

static jsonpg_type opt_error(jsonpg_parser p)
{
        return set_result_error(p, JSONPG_ERROR_OPT);
}


