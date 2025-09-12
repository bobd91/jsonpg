#include <setjmp.h>
#include <limits.h>
#include <float.h>

#include "fast_double_parser.h"

#define MIN_STACK_SIZE 1024

static bool parser_in_object(Parser p)
{
        return stack_peek(&p->stack) == STACK_OBJECT;
}

static bool parser_in_array(Parser p)
{
        return stack_peek(&p->stack) == STACK_ARRAY;
}
//
// static bool parser_in_any(Parser p)
// {
//         return p->stack.ptr > 0;
// }
//
[[noreturn]]
static void throw_parse_error_at(Parser p, ErrorCode error_code, size_t at)
{
        p->result = make_error_return(error_code, at);
        longjmp(p->env, 1);
}

[[noreturn]]
static void throw_parse_error(Parser p, ErrorCode error_code)
{
        size_t at = p->mis->bytes ? p->mis->current : 0;

        throw_parse_error_at(p, error_code, at);
}

static int parse_start_object(Parser p)
{
        ASSERT(mis_peek(p->mis) == '{');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_OBJECT))
                throw_parse_error(p, JSONPG_ERROR_STACK_OVERFLOW);
        return STACK_OBJECT;
}

static int parse_end_object(Parser p)
{
        ASSERT(mis_peek(p->mis) == '}');
        ASSERT(stack_peek(&p->stack) == STACK_OBJECT);

        mis_take(p->mis);
        int type = stack_pop(&p->stack);

        if(-1 == type)
                throw_parse_error(p, JSONPG_ERROR_STACK_UNDERFLOW);
        return type;
}

static int parse_start_array(Parser p)
{
        ASSERT(mis_peek(p->mis) == '[');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_ARRAY))
                throw_parse_error(p, JSONPG_ERROR_STACK_OVERFLOW);
        return STACK_ARRAY;
}

static int  parse_end_array(Parser p)
{
        ASSERT(mis_peek(p->mis) == ']');
        ASSERT(stack_peek(&p->stack) == STACK_ARRAY);

        mis_take(p->mis);
        int type = stack_pop(&p->stack);

        if(-1 == type)
                throw_parse_error(p, JSONPG_ERROR_STACK_UNDERFLOW);
        return type;
}

static void parse_true(Parser p)
{
        const MemoryInputStream mis = p->mis;

        ASSERT(mis_peek(mis) == 't');

        mis_take(mis);
        if(!mis_consume(mis, 'r')
                        || !mis_consume(mis, 'u')
                        || !mis_consume(mis, 'e'))
                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
}

static void parse_false(Parser p)
{
        const MemoryInputStream mis = p->mis;

        ASSERT(mis_peek(mis) == 'f');

        mis_take(mis);
        if(!mis_consume(mis, 'a')
                        || !mis_consume(mis, 'l')
                        || !mis_consume(mis, 's')
                        || !mis_consume(mis, 'e'))
                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
}

static void parse_null(Parser p)
{
        const MemoryInputStream mis = p->mis;

        ASSERT(mis_peek(mis) == 'n');

        mis_take(mis); // 'n'
        if(!mis_consume(mis, 'u')
                        || !mis_consume(mis, 'l')
                        || !mis_consume(mis, 'l'))
                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
}

static unsigned parse_hex4(Parser p, size_t esc_offset)
{
        const MemoryInputStream mis = p->mis;

        unsigned codepoint = 0;
        for(int i = 0 ; i < 4 ; i++) {
                char c = mis_peek(mis);
                codepoint <<= 4;
                if(c >= '0' && c <= '9')
                        codepoint += c - '0';
                else if(c >= 'A' && c <= 'F')
                        codepoint += 10 + c -'A';
                else if(c >= 'a' && c <= 'f')
                        codepoint += 10 + c - 'a';
                else 
                        throw_parse_error_at(p, JSONPG_ERROR_ESCAPE, esc_offset);

                mis_take(mis);
        }
        return codepoint;
}

static unsigned parse_escape(Parser p)
{
        static const unsigned char escape[256] = {
                ['"'] = '"',  ['/'] = '/',  ['\\'] = '\\', ['b'] = '\b', 
                ['f'] = '\f', ['n'] = '\n', ['r'] = '\r',  ['t'] = '\t'
        };

        const MemoryInputStream mis = p->mis;
        const size_t esc_offset = mis_tell(mis);
        mis_take(mis);
        const unsigned char e = mis_peek(mis);
        if(escape[e]) {
                mis_take(mis);
                return (unsigned)escape[e];
        }
        if(e == 'u') {
                mis_take(mis);
                unsigned codepoint = parse_hex4(p, esc_offset);
                if(codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                        // Got surrogate but high (first one) must be 0xD800-0xDBFF
                        if(codepoint <= 0xDBFF) {
                                // high surrogate must be followed by low
                                if(!mis_consume(mis, '\\')
                                                || !mis_consume(mis, 'u'))
                                        throw_parse_error_at(p, JSONPG_ERROR_SURROGATE, esc_offset);

                                const unsigned codepoint2 = parse_hex4(p, esc_offset + 6);

                                if(codepoint2 < 0xDC00 || codepoint2 > 0xDFFF)
                                        throw_parse_error_at(p, JSONPG_ERROR_SURROGATE, esc_offset + 6);

                                codepoint = (((codepoint - 0xD800) << 10)
                                                | (codepoint2 - 0xDC00)) + 0x10000;
                        } else {
                                throw_parse_error_at(p, JSONPG_ERROR_SURROGATE, esc_offset);
                        }
                }
                return codepoint;
        } else {
                throw_parse_error_at(p, JSONPG_ERROR_ESCAPE, esc_offset);
        }
}

static void mis_consume_whitespace(MemoryInputStream mis)
{
        Byte c;
        Bytes bytes = mis->bytes + mis->current;
        Bytes ptr = bytes;
        while(((c = *bytes++) == ' ' 
                                || c == '\n' 
                                || c == '\r' 
                                || c == '\t')
                      )
                ;
        mis_adjust(mis, bytes - 1 - ptr);

        // while((c = mis_peek(mis)) == ' ' || c == '\n' || c == '\r' || c== '\t')
        //         mis_take(mis);
}

void consume_whitespace(Parser p, bool allow_comments)
{
        const MemoryInputStream mis = p->mis;
        Byte c;

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
                                        } else if(mis_eof(mis)) {
                                                return;
                                        } else {
                                                mis_take(mis);
                                        }
                                }
                        } else if(c == '/') {
                                while(mis_take(mis) != '\n')
                                        if(mis_eof(mis))
                                                return;
                                        ;
                        } else {        
                                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                        }
                        continue;
                } else {
                        break;
                }
        }
}

static void parse_string_to_cow(Parser p, CowStream cow, Byte terminator)
{
        const MemoryInputStream mis = p->mis;

        while(true) {
                //copy_safe_chars(p->is, os);
                
                unsigned char c = mis_peek(mis);
                if(c == '\\') {
                        Bytes s = cow_reserve(cow, 4);
                        unsigned codepoint = parse_escape(p);
                        if(!s)
                                throw_parse_error(p, JSONPG_ERROR_ALLOC);
                        int count = utf8_encode(codepoint, s);
                        if(count == -1)
                                throw_parse_error(p, JSONPG_ERROR_UTF8);
                        cow_adjust(cow, count - 4);
                } else if(c == terminator) {
                        if(!cow_finalize(cow))
                                throw_parse_error(p, JSONPG_ERROR_ALLOC);
                        mis_take(mis);
                        return;
                } else if(c < 0x20) {
                        throw_parse_error(p, JSONPG_ERROR_INVALID);
                } else if(c >= 0x80) {
                        if(-1 == utf8_validate_sequence(mis))
                                throw_parse_error(p, JSONPG_ERROR_UTF8);
                } else {
                        mis_take(mis);
                }
        }
}

static void parse_nqstring_to_cow(Parser p, CowStream cow)
{
        const MemoryInputStream mis = p->mis;

        while(true) {
                //copy_safe_chars(p->is, os);
                
                unsigned char c = mis_peek(mis);
                if(c == '\\') {
                        mis_take(mis);
                        // no quote escapes the next char so just skip backslash
                        // But that puts us out of sync with cow so
                        // reserve 0 to trigger cow write
                        cow_reserve(cow, 0);
                } else if(c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                        if(!cow_finalize(cow))
                                throw_parse_error(p, JSONPG_ERROR_ALLOC);
                        mis_take(mis);
                        return;
                } else if(c < 0x20) {
                        throw_parse_error(p, JSONPG_ERROR_INVALID);
                } else if(c >= 0x80) {
                        if(-1 == utf8_validate_sequence(p->mis))
                                throw_parse_error(p, JSONPG_ERROR_UTF8);
                } else {
                        mis_take(mis);
                }
        }
}

static size_t parse_string_terminator(Parser p, Bytes *bytes, Byte terminator)
{
        const CowStream cow = p->cow;
        cow_start(cow);

        parse_string_to_cow(p, cow, terminator);
        size_t length = cow_length(cow);
        Bytes str = cow_pop(cow);
        *bytes = str;
        return length;
}

static size_t parse_string(Parser p, Bytes *bytes)
{
        ASSERT(mis_peek(p->mis) == '"');
        mis_take(p->mis); // "
        return parse_string_terminator(p, bytes, '"');
}

static size_t parse_sqstring(Parser p, Bytes *bytes)
{
        ASSERT(mis_peek(p->mis) == '\'');
        mis_take(p->mis); // '
        return parse_string_terminator(p, bytes, '\'');
}

static size_t parse_nqstring(Parser p, Bytes *bytes)
{
        const CowStream cow = p->cow;
        cow_start(cow);

        parse_nqstring_to_cow(p, cow);
        size_t length = cow_length(cow);
        Bytes str = cow_pop(cow);
        *bytes = str;
        return length;
}

static JsonType parse_number(Parser p, double *real_result, long *integer_result)
{
        Byte internal_bytes[64];

        // Max digits for long,
        // double is 15-17 but we will lose those when converting
        static int max_sig_digits = 19;

        const MemoryInputStream mis = p->mis;

        Bytes bytes = mis->bytes + mis->current;
        int input_size = mis->count - mis->current;
        if(input_size < 64) {
                memcpy(internal_bytes, bytes, input_size);
                bytes = internal_bytes;
                bytes[input_size] = '\0';
        }
        Bytes ptr = bytes;



        // If fast parsing fails might need to call
        // strtod, which needs to start from the beginning
        //size_t start_pos = mis_tell(mis);

        bool force_double = false;
        bool negative = false;
        uint64_t sum;
        int64_t exponent = 0;
        int sig_digits = 0;

        Byte c = *bytes++; //mis_take(mis);

        if(c == '-') {
                negative = true;
                c = *bytes++; //mis_take(mis);
        }
        if(c >= '0' && c <= '9') {
                sum = c - '0';
                if(sum)
                        sig_digits++;
        } else {
                throw_parse_error(p, JSONPG_ERROR_NUMBER);
        }

        c = *bytes; //mis_peek(mis);
        if(sum) {
                while(c >= '0' && c <= '9') {
                        bytes++; //mis_take(mis);
                        if(sig_digits++ < max_sig_digits) {
                                sum = sum * 10 + c - '0';
                        } else {
                                exponent++;
                        }
                        c = *bytes; //mis_peek(mis);
                }
        }
        if(c == '.') {
                bytes++; //mis_take(mis);
                force_double = true;

                c = *bytes; //mis_peek(mis);
                if(c >= '0' && c <= '9') {
                        bytes++; //mis_take(mis);
                        if(sig_digits < max_sig_digits) {
                                sum = 10 * sum + c - '0';
                                exponent--;
                                if(sum)
                                        sig_digits++;
                        }
                        c = *bytes; //mis_peek(mis);
                } else {
                        throw_parse_error(p, JSONPG_ERROR_NUMBER);
                }

                while(c >= '0' && c <= '9') {
                        bytes++; //mis_take(mis);
                        if(sig_digits < max_sig_digits) {
                                sum = 10 * sum + c - '0';
                                exponent--;
                                if(sum)
                                        sig_digits++;
                        }
                        c = *bytes; //mis_peek(mis);
                }
        }
        if(c == 'e' || c == 'E') {
                bytes++; //mis_take(mis);
                force_double = true;
                int exp_sign = 1;
                int exp = 0;

                c = *bytes; //mis_peek(mis);
                if(c == '-') {
                        bytes++; //mis_take(mis);
                        exp_sign = -1;
                        c = *bytes; //mis_peek(mis);
                } else if(c == '+') {
                        bytes++; //mis_take(mis);
                        c = *bytes; //mis_peek(mis);
                }
                if(c >= '0' && c <='9') {
                        bytes++; //mis_take(mis);
                        exp = 10 * exp + c - '0';
                        c = *bytes; //mis_peek(mis);
                        while(c >= '0' && c <= '9') {
                                bytes++; //mis_take(mis);
                                exp = 10 * exp + c - '0';
                                if(exp > 1000)
                                        throw_parse_error(p, JSONPG_ERROR_NUMBER);
                                c = *bytes; //mis_peek(mis);
                        }
                } else {
                        throw_parse_error(p, JSONPG_ERROR_NUMBER);
                }
                exponent += exp_sign * exp;
        }
        
        // Force double if either too many significant digits 
        // or sum is too big for signed long
        force_double = force_double || sig_digits > max_sig_digits;
        force_double = force_double || (negative 
                                        ? sum > 1 + (uint64_t)LONG_MAX
                                        : sum > LONG_MAX);

        if(force_double) {
                bool success = false;
                if (exponent >= FASTFLOAT_SMALLEST_POWER ||
                                exponent <= FASTFLOAT_LARGEST_POWER) {
                        *real_result = compute_float_64(exponent, sum, negative, &success);
                }
                if(!success) {
                        const char *start = (char *)ptr; //(char *)mis_at(mis, start_pos);
                        const char *end = parse_float_strtod(start, real_result);
                        if(!end)
                                throw_parse_error(p, JSONPG_ERROR_NUMBER);
                        mis_adjust(mis, end - start); // - mis_tell(mis));
                } else {
                        mis_adjust(mis, bytes - ptr);
                }
                return JSONPG_REAL;
        } else {
                mis_adjust(mis, bytes - ptr);
                *integer_result = negative ? -sum : sum;
                return JSONPG_INTEGER;
        }
}

static void parser_set_bytes(Parser p, Bytes bytes, size_t count)
{
        // Skip leading byte order mark
        unsigned skip = utf8_bom_bytes(bytes, count);
        bytes += skip;
        count -= skip;

        Bytes b = allocator_alloc(p->allocator, count + 1);
        memcpy(b, bytes, count);
        b[count] = '\0';

        mis_set_bytes(p->mis, b/*ytes + skip*/, count/* - skip*/);
        cow_start(p->cow);
}

static void parser_set_dom_info(Parser p, DomInfo di)
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

        p->stack = (struct stack_s){
                .ptr = 0,
                .size = stack_size,
                .stack = (((void *)p) + struct_bytes)
        };
        p->flags = flags;

        return p;
}

Parser jsonpg_parser_new_opt(ParserOpts opts)
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

ErrorInfo jsonpg_parse_error(Parser p)
{
        return p->result.error;
}
