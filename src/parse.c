#include <math.h>
#include <errno.h>

#define MIN_STACK_SIZE 1024

#define TOKEN_INFO_DEFAULT      0x00
#define TOKEN_INFO_IS_STRING    0x01
#define TOKEN_INFO_HAS_QUOTE    0x03 // implies _IS_STRING
#define TOKEN_INFO_IS_ESCAPE    0x04
#define TOKEN_INFO_IS_SURROGATE 0x08
#define TOKEN_INFO_COPY_FORWARD 0x10

#define write_b(X, Y)   (str_buf_append(p->write_buf, (X), (Y)))
#define write_c(X)      (str_buf_append_c(p->write_buf, (X)))
#define get_content(X)  (str_buf_content(p->write_buf, (X)))


typedef struct reader_s *reader;

struct reader_s {
        ssize_t (*read)(reader, void *, size_t);
        void *ctx;
};

// must be same size and order as token_type
static int token_type_info[] = {
        TOKEN_INFO_DEFAULT,
        TOKEN_INFO_DEFAULT,
        TOKEN_INFO_DEFAULT,
        TOKEN_INFO_HAS_QUOTE,
        TOKEN_INFO_HAS_QUOTE,
        TOKEN_INFO_HAS_QUOTE,
        TOKEN_INFO_HAS_QUOTE,
        TOKEN_INFO_IS_STRING,
        TOKEN_INFO_IS_STRING,
        TOKEN_INFO_COPY_FORWARD,
        TOKEN_INFO_COPY_FORWARD,
        TOKEN_INFO_IS_ESCAPE,
        TOKEN_INFO_IS_ESCAPE,
        TOKEN_INFO_IS_ESCAPE | TOKEN_INFO_COPY_FORWARD,
        TOKEN_INFO_IS_SURROGATE | TOKEN_INFO_COPY_FORWARD
};

static int push_token(jsonpg_parser p, token_type type)
{
        assert(p->token_ptr < TOKEN_MAX && "Token stack overflow");

        token t = &p->tokens[p->token_ptr++];
        t->type = type;
        t->pos = p->current;

        if(token_type_info[type] & TOKEN_INFO_HAS_QUOTE ) {
                // Quoted string/key starts after quote
                t->pos++;
        } else if(token_type_info[type] & TOKEN_INFO_IS_ESCAPE) {
                // Copy previous bytes from enclosing string
                assert(p->token_ptr > 0 && "Push escape token with no enclosing string");

                uint8_t *start = p->tokens[p->token_ptr - 1].pos;
                if(write_b(start, p->current - start))
                        return -1;

                // Escape starts after "\"
                t->pos++;
        }
        return 0;
}

static token pop_token(jsonpg_parser p)
{
        assert(p->token_ptr > 0 && "Token stack underflow");
        
        return &p->tokens[--p->token_ptr];
}
static jsonpg_type begin_object(jsonpg_parser p)
{
        return(0 == push_stack(&p->stack, STACK_OBJECT))
                ? JSONPG_BEGIN_OBJECT
                : set_result_error(p, JSONPG_ERROR_STACKOVERFLOW);
}

static jsonpg_type end_object(jsonpg_parser p)
{
        return (0 == pop_stack(&p->stack))
                ? JSONPG_END_OBJECT
                : set_result_error(p, JSONPG_ERROR_STACKUNDERFLOW);
}

static jsonpg_type begin_array(jsonpg_parser p)
{
        return(0 == push_stack(&p->stack, STACK_ARRAY))
                ? JSONPG_BEGIN_ARRAY
                : set_result_error(p, JSONPG_ERROR_STACKOVERFLOW);
}

static jsonpg_type end_array(jsonpg_parser p)
{
        return (0 == pop_stack(&p->stack))
                ? JSONPG_END_ARRAY
                : set_result_error(p, JSONPG_ERROR_STACKUNDERFLOW);
}

static jsonpg_type accept_integer(jsonpg_parser p, token t)
{
        errno = 0;
        long integer = strtol((char *)t->pos, NULL, 10);
        if(errno) 
                return number_error(p);
        p->result.number.integer = integer;
        return JSONPG_INTEGER;
}

static jsonpg_type accept_real(jsonpg_parser p, token t)
{
        errno = 0;
        double real = strtod((char *)t->pos, NULL);
        if(errno) 
                return number_error(p);

        if(!(real == 0 || isnormal(real))) 
                return number_error(p);

        p->result.number.real = real;
        return JSONPG_REAL;
}

static int set_string_value(jsonpg_parser p, token t)
{
        if(p->write_buf->count) {
                if(write_b(t->pos, p->current - t->pos))
                        return -1;
                p->result.string.length = get_content(&p->result.string.bytes);
        } else {
                p->result.string.bytes = t->pos;
                p->result.string.length = p->current - t->pos;
        }
        return 0;
}

static jsonpg_type accept_string(jsonpg_parser p, token t)
{
        if(set_string_value(p, t))
                return alloc_error(p);
        return JSONPG_STRING;
}

static jsonpg_type accept_key(jsonpg_parser p, token t)
{
        if(set_string_value(p, t))
                return alloc_error(p);
        return JSONPG_KEY;
}

static void reset_string_after_escape(jsonpg_parser p)
{
        // After escape set enclosing string token.pos
        // to point to after the escape sequence
        assert(p->token_ptr > 0 && "Pop escape token with no enclosing string");
        
        p->tokens[p->token_ptr - 1].pos = p->current + 1;
}

static int process_escape(jsonpg_parser p, token t)
{
        // Only JSON specified escape chars get here
        static char *escapes = "bfnrt\"\\/";
        char *c = strchr(escapes, *t->pos);

        assert(c && "Invalid escape character");

        if(write_c("\b\f\n\r\t\"\\/"[c - escapes]))
                return -1;

        reset_string_after_escape(p);
        return 0;
}

static int process_escape_chars(jsonpg_parser p, token t)
{
        if(write_c(*t->pos))
                return -1;

        reset_string_after_escape(p);
        return 0;
}

static int parse_4hex(uint8_t *hex_ptr) {
        int value = 0;
        for(int i = 0 ; i < 4 ; i++) {
                int v = *hex_ptr++;
                if(v >= '0' && v <= '9') {
                        v -= '0';
                } else if(v >= 'A' && v <= 'F') {
                        v -= 'A' - 10;
                } else if(v >= 'a' && v <= 'f') {
                        v -= 'a' - 10;
                } else {
                        assert(0 && "Invalid hex character");
                }
                value = (value << 4) | v;
        }
        return value;
}

static int process_escape_u(jsonpg_parser p, token t)
{
        // \uXXXX or \uXXXX\uXXXX
        // token points to first "u"
        int cp = parse_4hex(1 + t->pos);
        if(p->current - t->pos > 9)
                cp = surrogate_pair_to_codepoint(cp, parse_4hex(7 + t->pos));
        if(write_utf8_codepoint(cp, p->write_buf))
                return -1;

        reset_string_after_escape(p);
        return 0;
}

static int input_read(jsonpg_parser p, uint8_t *start)
{
       uint8_t *pos = start;
       int max = p->input_size - (start - p->input);
       while(max) {
               int l = p->reader->read(p->reader, pos, max);
               if(l < 0)
                       return -1;
               if(l == 0)
                       break;
               pos += l;
               max -= l;
       }
       p->last = pos;
       p->current = start;
       
       return max == 0;
}

static int parser_read_next(jsonpg_parser p)
{
        assert(p->input_is_ours && "Cannot write to user supplied buffer");

        uint8_t *start = p->input;
        if(p->token_ptr > 0) {
                // We have a token on the stack
                // Perform any special processing required
                // to survive the crossing of input buffer boundaries
                token t = &p->tokens[p->token_ptr - 1];
                int tinfo = token_type_info[t->type];
                if(tinfo & TOKEN_INFO_COPY_FORWARD) {
                        // copy bytes from current input needed
                        // for parsing next input
                        uint8_t *tpos;
                        if(tinfo & TOKEN_INFO_IS_SURROGATE) {
                                // sub-token of escape_u
                                // and we need to copy entire escape sequence
                                assert(p->token_ptr > 1 
                                                && "Surrogate token without parent");
                                tpos = p->tokens[p->token_ptr - 2].pos;
                        } else {
                                tpos = t->pos;
                        }
                        while(tpos < p->last)
                                *start++ = *tpos++;
                        
                } else if(tinfo & TOKEN_INFO_IS_STRING) {
                        // write string bytes as we are still in a string
                        if(write_b(t->pos, p->last - t->pos))
                                return -1;
                        // And adjust token to start of string continuation
                        t->pos = start;

                }
        }
        int l = input_read(p, start);
        if(l >= 0)
                p->seen_eof = (l == 0);

        return l;
}

static ssize_t read_fd(reader r, void *buf, size_t count)
{
        return read(CTX_TO_INT(r->ctx), buf, count);
}
void jsonpg_parser_free(void *ptr) 
{
        if(ptr) {
                jsonpg_parser p = ptr;
                if(p->input_is_ours)
                        pg_dealloc(p->input);
                str_buf_free(p->write_buf);
                pg_dealloc(p);
        }
}

static jsonpg_type parse(
                jsonpg_parser p,
                jsonpg_generator g)
{
        jsonpg_type type;
        int abort = 0;
        while(!abort && JSONPG_EOF != (type = jsonpg_parse_next(p))) {
                abort = generate(g, type, &p->result);
        }

        return type;
}

static jsonpg_type parse_bytes(
                jsonpg_parser p, 
                uint8_t *json, 
                size_t count,
                jsonpg_generator g)
{
        if(p->reader) {
                pg_dealloc(p->reader);
                p->reader = NULL;
        }
        p->write_buf = str_buf_reset(p->write_buf);
        if(p->input_is_ours)
                pg_dealloc(p->input);

        p->input = p->current = json;
        p->input_size = count;
        p->input_is_ours = 0;
        p->last = json + count;
        p->seen_eof = 1;
        p->stack.ptr = p->stack.ptr_min;
        p->token_ptr = 0;
        p->state = STATE_INITIAL;

        // Skip leading byte order mark
        p->current += bom_bytes(p->input, p->input_size);

        return g
                ? parse(p, g)
                : JSONPG_ROOT;
}

jsonpg_type parse_reader(
                jsonpg_parser p,
                reader r,
                jsonpg_generator g)
{
        if(!(p->input_is_ours && p->input)) {
                p->input = pg_alloc(BUF_SIZE);
                if(!p->input)
                        return alloc_error(p);
                p->input_size = BUF_SIZE;
                p->input_is_ours = 1;
        }
        p->write_buf = str_buf_reset(p->write_buf);
        p->reader = r;
        int l = input_read(p, p->input);
        if(l < 0)
                return file_read_error(p);
        p->seen_eof = (0 == l);
        p->stack.ptr = p->stack.ptr_min;
        p->token_ptr = 0;
        p->state = STATE_INITIAL;

        // Skip leading byte order mark
        p->current += bom_bytes(p->input, p->input_size);

        return g
                ? parse(p, g)
                : JSONPG_ROOT;
}

static reader file_reader(int fd)
{
        reader r = pg_alloc(sizeof(struct reader_s));
        if(!r)
                return NULL;
        r->read = read_fd;
        r->ctx = INT_TO_CTX(fd);

        return r;
}
//
// static void reader_free(void *p)
// {
//         pg_dealloc(p);
// }
//
static jsonpg_type parse_fd(
                jsonpg_parser p, 
                int fd, 
                jsonpg_generator g)
{
        reader r = file_reader(fd);
        if(!r)
                return alloc_error(p);
        return parse_reader(p, r, g);
}

jsonpg_type jsonpg_parse_opt(jsonpg_parser p, jsonpg_parse_opts opts)
{
        jsonpg_generator g;

        if((1 != (opts.fd >= 0)
                        + (opts.stream != NULL)
                        + (opts.bytes != NULL)
                        + (opts.string != NULL))
        || (1 < (opts.dom != NULL)
                        + (opts.callbacks != NULL)
                        + (opts.generator != NULL))) {
                p->result.error.code = JSONPG_ERROR_OPTS;
                p->result.error.at = 0;
                return JSONPG_ERROR;
        }

        if(opts.dom) {
                g = jsonpg_dom_generator(opts.dom);
        } else if(opts.callbacks) {
                g = jsonpg_callback(opts.callbacks, opts.context);
        } else {
                g = opts.generator;
        }

        if(opts.fd >= 0)
                return parse_fd(p, opts.fd, g);
        else if(opts.stream)
                return parse_fd(p, fileno(opts.stream), g);
        else if(opts.bytes)
                return parse_bytes(p, opts.bytes, opts.count, g);
        else
                return parse_bytes(p, (uint8_t *)opts.string, strlen(opts.string), g);
}

static uint16_t flags_from_opts(jsonpg_parser_opts opts)
{
        return (opts.comments ? FLAG_COMMENTS : 0)
                | (opts.trailing_commas ? FLAG_TRAILING_COMMAS : 0)
                | (opts.optional_commas ? FLAG_OPTIONAL_COMMAS : 0)
                | (opts.single_quotes ? FLAG_SINGLE_QUOTES : 0)
                | (opts.unquoted_keys ? FLAG_UNQUOTED_KEYS : 0)
                | (opts.unquoted_strings ? FLAG_UNQUOTED_STRINGS : 0)
                | (opts.escape_characters ? FLAG_ESCAPE_CHARACTERS : 0)
                | (opts.is_object && !opts.is_array ? FLAG_IS_OBJECT : 0)
                | (opts.is_array && !opts.is_object ? FLAG_IS_ARRAY : 0);
}

static uint16_t get_stack_size(uint16_t stack_size)
{
        return stack_size > MIN_STACK_SIZE ? stack_size : MIN_STACK_SIZE;
}

jsonpg_parser jsonpg_parser_new_opt(jsonpg_parser_opts opts)
{
        uint16_t stack_size = get_stack_size(opts.max_nesting);
        uint16_t flags = flags_from_opts(opts);

        size_t struct_bytes = sizeof(struct jsonpg_parser_s);
        // 1-8 => 1, 9-16 => 2, etc
        size_t stack_bytes = (stack_size + 7) / 8;
        jsonpg_parser p = pg_alloc(struct_bytes + stack_bytes);
        if(p) {
                p->write_buf = str_buf_empty();
                if(!p->write_buf) {
                        pg_dealloc(p);
                        return NULL;
                }
                p->reader = NULL;
                p->input = NULL;
                p->input_is_ours = 0;
                p->stack.size = stack_size;
                p->stack.stack = (uint8_t *)(((void *)p) + struct_bytes);
                p->flags = flags;

                if(flags & FLAG_IS_OBJECT) {
                        p->stack.ptr = 0;
                        push_stack(&p->stack, STACK_OBJECT);
                        p->stack.ptr_min = 1;
                } else if(flags & FLAG_IS_ARRAY) {
                        p->stack.ptr = 0;
                        push_stack(&p->stack, STACK_ARRAY);
                        p->stack.ptr_min = 1;
                } else {
                        p->stack.ptr_min = 0;
                }
        }
        return p;
}

jsonpg_error_info jsonpg_parse_error(jsonpg_parser p)
{
        return p->result.error;
}
