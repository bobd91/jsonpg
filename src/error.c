#include <stdio.h>

static const char * error_msgs[] = {
        [JSONPG_ERROR_OPT]              = "Invalid option",
        [JSONPG_ERROR_ALLOC]            = "Out of memory",
        [JSONPG_ERROR_NUMBER]           = "Invalid number",
        [JSONPG_ERROR_UTF8]	        = "Invalid UTF-8",
        [JSONPG_ERROR_SURROGATE]	= "Invalid surrogate",
        [JSONPG_ERROR_STACK_OVERFLOW]	= "Stack overflow",
        [JSONPG_ERROR_STACK_UNDERFLOW]	= "Stack underflow",
        [JSONPG_ERROR_EXPECTED_VALUE]	= "Value expected",
        [JSONPG_ERROR_EXPECTED_KEY]	= "Key expected",
        [JSONPG_ERROR_NO_OBJECT]	= "Not in object",
        [JSONPG_ERROR_NO_ARRAY]	        = "Not in array",
        [JSONPG_ERROR_ESCAPE]	        = "Invalid escape",
        [JSONPG_ERROR_UNEXPECTED]	= "Unexpected input",
        [JSONPG_ERROR_INVALID]	        = "Invalid input",
        [JSONPG_ERROR_TERMINATED]	= "Generator terminated",
        [JSONPG_ERROR_EOF]	        = "Unexpected end of input"
};
        

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

static const char *error_text(ErrorCode code)
{
        static const int msg_count = sizeof(error_msgs) / sizeof(error_msgs[0]);

        if(code >= 0 && code < msg_count)
                return error_msgs[code];
        else
                return "Unknown error";
}

static ErrorInfo make_error(ErrorCode code)
{
        return (ErrorInfo){ 
                .code = code, 
                .text = error_text(code)
        };
}

static ParseResult make_error_return(ErrorCode code, size_t at)
{
        return (ParseResult) {
                        .type = JSONPG_ERROR,
                        .position = at,
                        .error = make_error(code)
        };
}

static ParseResult make_pg_error_return(Parser p, Generator g)
{
        ParseResult r = p->result;
        if(r.type == JSONPG_ERROR) {
                // Terminations come from generator
                // Which MAY have set error info
                if(r.error.code == JSONPG_ERROR_TERMINATED
                                && g->error.code) {
                        r.error = g->error;
                }
        } else {
                r = make_error_return(JSONPG_ERROR_UNEXPECTED, 0);
        }
        return r;
}


