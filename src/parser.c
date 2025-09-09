#include "parser.h"
static bool parser_in_object(p)
{
        return stack_peek(p) == STACK_OBJECT;
}

static bool parser_in_array(p)
{
        return stack_peek(p) == STACK_ARRAY;
}

static bool parser_in_any(p)
{
        return stack_depth(p) > 0;
}

static bool parser_eof(Parser p)
{
        return mis_eof(p->mis);
}

static void parse_error(Parser p, unsigned error_code)
{
        p->error = (JsonpgError) {
                .code = error_code,
                .at = p->offset
        };
        longjmp(p->env);
}

static void parse_start_object(p)
{
        JSONPG_ASSERT(mis_peek(p->mis) == '{');

        mis_take(p->mis);
        stack_push(p, JSONPG_OBJECT);
}

static void parse_end_object(p)
{
        JSONPG_ASSERT(mis_peek(p->mis) == '}');
        JSONPG_ASSERT(stack_peek(p) == STACK_OBJECT);

        mis_take(p->mis);
        stack_pop(p);
}

static void parse_start_array(p)
{
        JSONPG_ASSERT(mis_peek(p->mis) == '[');

        mis_take(p->mis);
        stack_push(p, JSONPG_ARRAY);
}

static void parse_end_array(p)
{
        JSONPG_ASSERT(mis_peek(p->mis) == ']');
        JSONPG_ASSERT(stack_peek(p) == STACK_ARRAY);

        mis_take(p->mis);
        stack_pop(p);
}

static void parse_true(Parser p)
{
        JSONPG_ASSERT(mis_peek(p->mis) == 't');

        mis_take(p->mis);
        if(!input_consume(p, 'r')
                        || !input_consume(p, 'u')
                        || !input_comsume(p, 'e'))
                parse_error(p, UNEXPECTED);
}

static void parse_false(Parser p)
{
        JSONPG_ASSERT(mis_peek(p->mis) == 'f');

        mis_take(p->mis);
        if(!input_consume(p, 'a')
                        || !input_consume(p, 'l')
                        || !input_consume(p, 's')
                        || !input_comsume(p, 'e'))
                parse_error(p, UNEXPECTED);
}

static void parse_null(Parser p)
{
        JSONPG_ASSERT(mis_peek(p->mis) == 'n');

        mis_take(p->mis); // 'n'
        if(!input_consume(p, 'u')
                        || !input_consume(p, 'l')
                        || !input_comsume(p, 'l'))
                parse_error(p, UNEXPECTED);
}

static unsigned parse_hex4(Parser p, size_t esc_offset)
{
        unsigned codepoint = 0;
        for(int i = 0 ; i < 4 ; i++) {
                char c = mis_peak(p->mis);
                codepoint <<= 4;
                if(c >= '0' && c <= '9')
                        codepoint += c - '0';
                else if(c >= 'A' && c <= 'F')
                        codepoint += 10 + c -'A';
                else if(c >= 'a' && c <= 'f')
                        codepoint += 10 + c - 'a';
                else 
                        parse_error(p, ESCAPE, esc_offset);

                mis_take(p->mis);
        }
        return codepoint;
}

static unsigned parse_escape(Parser p)
{
        static const unsigned char escape[256] = {
                ['"'] = '"',  ['/'] = '/',  ['\\'] = '\\', ['b'] = '\b', 
                ['f'] = '\f', ['n'] = '\n', ['r'] = '\r',  ['t'] = '\t'
        };

        const size_t esc_offset = mis_tell(p->mis);
        mis_take(p->mis);
        const unsigned char e = mis_peek(p->mis);
        if(escape[e]) {
                mis_take(p->mis);
                return (unsigned)escape[e];
        }
        if(e == 'u') {
                mis_take(p->mis);
                unsigned codepoint = parse_hex4(p, esc_offset);
                if(codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                        // Got surrogate but high (first one) must be 0xD800-0xDBFF
                        if(codepoint <= 0xDBFF) {
                                // high surrogate must be followed by low
                                if(!input_consume(p, '\\')
                                                || !input_consume(p, 'u')))
                                        parse_error(p, SURROGATE, esc_offset);

                                const unsigned codepoint2 = parse_hex4(p, esc_offset + 6);

                                if(codepoint2 < 0xDC00 || codepoint2 > 0xDFFF)
                                        parse_error(p, SURROGATE, esc_offset + 6);

                                codepoint = (((codepoint - 0xD800) << 10)
                                                | (codepoint2 - 0xDC00)) + 0x10000;
                        } else {
                                parse_error(p, SURROGATE, esc_offset);
                        }
                }
                return codepoint;
        } else {
                parse_error(p, ESCAPE, esc_offset);
        }
}

static void mis_consume_whitespace(MemoryInputStream mis)
{
        Bytes c;

        while((c = mis_peek(is)) == ' ' || c == '\n' || c == '\r' || c== '\t')
                mis_take(is);
}

void consume_whitespace(Parser p, bool allow_comments)
{
        MemoryInputStream mis = p->mis;
        Bytes c;

        if(!allow_comments) {
                mis_consume_whitespace(mis);
                return;
        }

        while(true) {
                mis_consume_whitespace(mis);

                if(mis_consume(mis, '/')) {
                        c = mis_peek(mis);
                        if(c == '*') {
                                mis_take(mis);
                                while(true) {
                                        if(mis_consume(mis, '*')) {
                                                if(mis_consume(mis, '/'))
                                                        break;
                                        } else if(mis_peek(mis) == '\0') {
                                                return;
                                        } else {
                                                mis_take(mis);
                                        }
                                }
                        } else if(c == '/') {
                                while(mis_consume(mis) != '\n')
                                        if(mis_peek(mis) == '\0')
                                                return;
                                        ;
                        } else {        
                                parse_error(p, UNEXPECTED);
                        }
                        continue;
                } else {
                        break;
                }
        }
}

static static void parse_string_to_cow(Parser p, CowStream cow, Byte terminator)
{
        MemoryInputStream mis = p->mis;

        while(true) {
                //copy_safe_chars(p->is, os);
                
                unsigned char c = mis_peek(mis);
                if(c == '\\') {
                        unsigned codepoint = parse_escape(p);
                        Bytes s = cow_reserve(cow, 4);
                        if(!s)
                                parse_error(p, MEMORY);
                        int count = utf8_encode(codepoint, s);
                        if(count == -1)
                                parse_error(p, UTF8);
                        cow_adjust(cow, 4 - count);
                } else if(c == terminator) {
                        if(!cow_finalize(cow))
                                parse_error(p, MEMORY);
                        mis_take(mis);
                        return;
                } else if(c < 0x20) {
                        parse_error(p, INVALID);
                } else if(c >= 0x80) {
                        if(-1 == utf8_sequence_length(p))
                                parse_error(p, UTF8);
                } else {
                        mis_take(mis);
                }
        }
}

static static void parse_nqstring_to_cow(Parser p, CowStream cow)
{
        MemoryInputStream mis = p->mis;

        while(true) {
                //copy_safe_chars(p->is, os);
                
                unsigned char c = mis_peek(mis);
                if(c == '\\') {
                        mis_take(mis);
                        // no quote escapes the next char so just skip \
                        // But that puts us out of sync with cow so
                        // reserve 0 to trigger cow write
                        cow_reserve(cow, 0);
                } else if(c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                        if(!cow_finalize(cow))
                                parse_error(p, MEMORY);
                        mis_take(mis);
                        return;
                } else if(c < 0x20) {
                        parse_error(p, INVALID);
                } else if(c >= 0x80) {
                        if(-1 == utf8_sequence_length(p))
                                parse_error(p, UTF8);
                } else {
                        mis_take(mis);
                }
        }
}

static size_t parse_string(Parser p, Bytes *bytes, byte terminator)
{
        CowStream cow = p->cow;
        cow_start(cow);

        parse_string_to_cow(p, cow, terminator);
        size_t length = cow_length(cow);
        Bytes str = cow_pop(cow);
        *bytes = str;
        return length;
}

static size_t parse_string(Parser p, Bytes *bytes)
{
        JSONPG_ASSERT(mis_peek(p->mis) == '"');
        mis_take(p->mis); // "
        parse_string(p, bytes, '"');
}

static size_t parse_sqstring(Parser p, Bytes *bytes)
{
        JSONPG_ASSERT(mis_peek(p->mis) == '\'');
        mis_take(p->mis); // '
        parse_string(p, bytes, '\'');
}

static size_t parse_nqstring(Parser p, Bytes *bytes)
{
        CowStream cow = p->cow;
        cow_start(cow);

        parse_nqstring_to_cow(p, cs);
        size_t length = cow_length(cs);
        Bytes str = cow_pop(cs);
        *bytes = str;
        return length;
}

static static JsonType parse_number(Parser p, *double real, *long integer)
{
        MemoryInputStream mis = p->mis;

        bool force_double = false;
        bool overflow = false;
        sign = 1;
        int64_t sum, new_sum;
        int exponent = 0;
        Byte c = mis_take(is);

        if(c == '-') {
                sign = -1;
                c = mis_take(mis);
        }
        if(c >= '0' && c <= '9')
                sum = c - '0';
        else
                parse_error(p, NUMBER);

        c = mis_take(mis);
        if(sum) {
                while(c >= '0' && c <= '9')
                        if(overflow || c == '0') {
                                exponent++;
                        } else {
                                new_sum = sum * pow_10(exponent) + c - '0';
                                if(new_sum < sum)
                                        overflow = true;
                                else
                                        sum = new_sum;
                        }
                        c = mis_take(mis);
                }
        }
        if(c == '.') {
                force_double = true;

                c = mis_take(mis);
                if(!overflow && exponent) {
                        new_sum = sum * pow_10(exponent);
                        if(new_sum < sum) 
                                overflow = true;
                        else {
                                sum = new_sum;
                                exponent = 0;
                        }
                }

                if(c >= '0' && c <= '9')
                        if(!overflow) {
                                new_sum = 10 * sum + c - '0';
                                if(new_sum < sum) {
                                        overflow = true;
                                } else {
                                        sum = new_sum;
                                        exponent--;
                                }
                        }
                        c = mis_take(mis);
                else
                        parse_error(p, NUMBER);

                while(c >= '0' && c <= '9')
                        if(!overflow) {
                                new_sum = 10 * sum + c - '0';
                                if(new_sum < sum) {
                                        overflow = true;
                                } else {
                                        sum = new_sum;
                                        exponent--;
                                }
                        }
                        c = mis_take(mis);
                }
        }
        if(c == 'e' || c == 'E') {
                force_double = true;
                int exp_sign = 1;
                int exp = 0;

                c = mis_take(mis);
                if(c == '-') {
                        exp_sign = -1;
                        c = mis_take(mis);
                } else if(c == '+') {
                        c = mis_take(mis);
                }
                if(c >= '0' && c <='9') {
                        exp = 10 * exp + c - '0';
                        c = mis_take(mis);
                        while(c >= '0' && c <= '9') {
                                exp = 10 * exp + c - '0';
                                if(exp < 0)
                                        parse_error(p, NUMBER);
                                c = mis_take(mis);
                        }
                } else {
                        parse_error(p, NUMBER);
                }
                exponent += exp_sign * exp;
        }

        if(force_double || overflow) {
                *real = sign * strtod_normal((double)sum, exponent);
                return JSONPG_REAL;
        } else {
                *integer = sign * sum;
                return JSONPG_INTEGER;
        }
}

static void parser_set_bytes(Parser p, Bytes bytes, size_t count)
{
        // Skip leading byte order mark
        unsigned skip = utf8_bom_bytes(bytes, count);

        JSONPG_ASSERT(count >= skip);

        mis_set_bytes(p->mis, bytes + skip, count - skip);
        cow_start(p->cow);
}

static void parser_set_dom_info(Parser p, dom_info di)
{
        p->dom_info = di;
}

static uint16_t get_stack_size(uint16_t stack_size)
{
        return stack_size > MIN_STACK_SIZE ? stack_size : MIN_STACK_SIZE;
}

void jsonpg_parser_free(Parser p)
{
        allocator_free(p->allocator);
}

Parser parser_new(Allocator a, uint16_t stack_size, uint16_t flags)
{
        size_t struct_bytes = sizeof(struct jsonpg_parser_s);
        Parser p = allocator_alloc(a, struct_bytes + ((stack_size + 7) / 8));
        if(!p)
                return NULL;

        p->allocator = a;
        p->result = (ParseResult) {};

        p->mis = mis_new(a);
        if(!p->mis)
                return NULL;

        p->cow = cow_new(a, p->mis);
        if(!p->cow)
                return NULL;

        p->dom_info = (DomInfo){};

        p->stack = {
                .ptr = 0,
                .size = stack_size,
                .stack = (Bytes *)(((void *)p) + struct_bytes)
        };
        p->flags = flags;

        return p;
}

Parser jsonpg_parser_new_opt(jsonpg_parser_opts opts)
{
        uint16_t stack_size = get_stack_size(opts.max_nesting);
        uint16_t flags = opts.flags;

        Allocator a = allocator_new();
        if(!a)
                return NULL;

        Parser p = parser_new(a, stack_size, flags);

        if(!p) {
                allocator_free(a);
                return NULL;
        }

        if(1 != (opts.bytes != NULL) + (opts.string != NULL) + (opts.dom != NULL)) {
                opt_error(p);
                return p;
        }

        if(opts.bytes) {
                parser_set_bytes(p, opts.bytes, opts.count);
        } else if(opts.string) {
                parser_set_bytes(p, (Bytes)opts.string, strlen(opts.string));
        } else if(opts.dom) {
                parser_set_dom_info(p, dom_parser_info(opts.dom));
        }

        return p;
}

ParseResult jsonpg_parse_result(Parser p)
{
        return p->result;
}

jsonpg_error_value jsonpg_parse_error(Parser p)
{
        return p->result.error;
}
