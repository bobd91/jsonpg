#include <stdio.h>

#ifdef JSONPG_DEBUG
static void dump_p(Parser p)
{
        fprintf(stderr, "Parser Error:\n");
        fprintf(stderr, "Error: %d\n", p->result.error.code);
        fprintf(stderr, "At Position: %ld\n", p->result.error.at);
        if(p->mis->count) {
                fprintf(stderr, "Input Length: %ld\n", p->mis->count);
                fprintf(stderr, "Input Processed: %ld\n", p->mis->count - p->mis->ptr);
        } else {
                fprintf(stderr, "Parsing DOM\n");
        }
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

static void dump_g(Generator g)
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

static ErrorInfo make_error(ErrorCode code, size_t at)
{
        return (ErrorInfo){ .code = code, .at = at };
}

static ParseResult make_error_return(ErrorCode code, size_t at)
{
        return (ParseResult) {
                        .type = JSONPG_ERROR,
                        .error = make_error(code, at)
        };
}

static ParseResult make_pg_error_return(Parser p, Generator g)
{
        ParseResult r = p->result;
        if(r.type == JSONPG_ERROR) {
                // Terminations come from generator
                // Which MAY have set error info
                if(r.error.code == JSONPG_TERMINATED
                                && g->error.code) {
                        r.error = g->error;
                }
        } else {
                r = make_error_return(JSONPG_ERROR_UNEXPECTED, 0);
        }
        return r;
}

static void set_generator_error(Generator g, ErrorCode code)
{
        g->error = make_error(code, g->count);

#ifdef JSONPG_DEBUG
        dump_g(g);
#endif
}

static JsonType set_result_error(Parser p, ErrorCode code) 
{
        p->result.type = JSONPG_ERROR;
        p->result.error.code = code;
        p->result.error.at = (p->input && p->current)
                ? p->current - p->input
                : 0;

#ifdef JSONPG_DEBUG
        dump_p(p);
#endif

        return JSONPG_ERROR;
}

static JsonType parse_error(Parser p)
{
        return set_result_error(p, JSONPG_ERROR_PARSE);
}

static JsonType number_error(Parser p)
{
        return set_result_error(p, JSONPG_ERROR_NUMBER);
}

static JsonType alloc_error(Parser p)
{
        return set_result_error(p, JSONPG_ERROR_ALLOC);
}

static JsonType file_read_error(Parser p)
{
        return set_result_error(p, JSONPG_ERROR_FILE_READ);
}

static JsonType opt_error(Parser p)
{
        return set_result_error(p, JSONPG_ERROR_OPT);
}


