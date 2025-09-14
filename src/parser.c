#include <setjmp.h>
#include <limits.h>
#include <float.h>

#include "fast_double_parser.h"

#define MIN_STACK_SIZE 1024

static inline bool parser_in_object(Parser p)
{
        return stack_peek(&p->stack) == STACK_OBJECT;
}

static inline bool parser_in_array(Parser p)
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
        size_t at = p->mis->start ? mis_tell(p->mis) : 0;

        throw_parse_error_at(p, error_code, at);
}

static inline int parse_start_object(Parser p)
{
        ASSERT(mis_peek(p->mis) == '{');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_OBJECT))
                throw_parse_error(p, JSONPG_ERROR_STACK_OVERFLOW);
        return STACK_OBJECT;
}

static inline int parse_end_object(Parser p)
{
        ASSERT(mis_peek(p->mis) == '}');
        ASSERT(stack_peek(&p->stack) == STACK_OBJECT);

        mis_take(p->mis);
        int type = stack_pop(&p->stack);

        if(-1 == type)
                throw_parse_error(p, JSONPG_ERROR_STACK_UNDERFLOW);
        return type;
}

static inline int parse_start_array(Parser p)
{
        ASSERT(mis_peek(p->mis) == '[');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_ARRAY))
                throw_parse_error(p, JSONPG_ERROR_STACK_OVERFLOW);
        return STACK_ARRAY;
}

static inline int  parse_end_array(Parser p)
{
        ASSERT(mis_peek(p->mis) == ']');
        ASSERT(stack_peek(&p->stack) == STACK_ARRAY);

        mis_take(p->mis);
        int type = stack_pop(&p->stack);

        if(-1 == type)
                throw_parse_error(p, JSONPG_ERROR_STACK_UNDERFLOW);
        return type;
}

static inline void parse_true(Parser p)
{
        const MemoryInputStream mis = p->mis;

        ASSERT(mis_peek(mis) == 't');

        mis_take(mis);
        if(!mis_consume(mis, 'r')
                        || !mis_consume(mis, 'u')
                        || !mis_consume(mis, 'e'))
                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
}

static inline void parse_false(Parser p)
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

static inline void parse_null(Parser p)
{
        const MemoryInputStream mis = p->mis;

        ASSERT(mis_peek(mis) == 'n');

        mis_take(mis); // 'n'
        if(!mis_consume(mis, 'u')
                        || !mis_consume(mis, 'l')
                        || !mis_consume(mis, 'l'))
                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
}

static inline unsigned parse_hex4(Parser p, size_t esc_offset)
{
        const MemoryInputStream mis = p->mis;

        unsigned codepoint = 0;
        for(int i = 0 ; i < 4 ; i++) {
                Byte c = mis_peek(mis);
                codepoint <<= 4;
                if(c >= '0' && c <= '9')
                        codepoint += (unsigned)(c - '0');
                else if(c >= 'A' && c <= 'F')
                        codepoint += 10 + (unsigned)(c - 'A');
                else if(c >= 'a' && c <= 'f')
                        codepoint += 10 + (unsigned)(c - 'a');
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
        const Byte e = mis_peek(mis);
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

static inline Byte mis_consume_whitespace(MemoryInputStream mis)
{
        Byte c;

        while((c = mis_peek(mis)) == ' ' || c == '\n' || c == '\r' || c== '\t')
                 mis_take(mis);

        return c;

}

static inline Byte consume_whitespace(Parser p, bool allow_comments)
{
        const MemoryInputStream mis = p->mis;
        Byte c;

        if(!allow_comments) {
                return mis_consume_whitespace(mis);
        }

        while(true) {
                c = mis_consume_whitespace(mis);

                if(c != '/')
                        return c;

                mis_take(mis); // '/'
                c = mis_peek(mis);
                if(c == '*') {
                        mis_take(mis); // '*'
                        while(true) {
                                c = mis_find(mis, '*');
                                if(c == '*' && mis_consume(mis, '/'))
                                        break;
                                else if(mis_eof(mis))
                                        return '\0';
                        }
                } else if(c == '/') {
                        c = mis_find(mis, '\n');
                        if(mis_eof(mis))
                                return '\0';
                } else {        
                        throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                }
        }
}

static inline size_t parse_string_in_stream(Parser p, Byte terminator, Bytes *bytes)
{
        const MemoryInputStream mis = p->mis;

        mis_string_start(mis);

        while(true) {
                Byte c = mis_peek(mis);
                if(c == terminator) {
                        return mis_string_complete(mis, bytes);
                } else if(c == '\\') {
                        mis_string_update(mis);
                        unsigned codepoint = parse_escape(p);
                        utf8_encode(codepoint, mis_writer(mis));
                        mis_string_restart(mis);
                } else if(c >= 0x80) {
                        if(-1 == utf8_validate_sequence(mis))
                                throw_parse_error(p, JSONPG_ERROR_UTF8);
                } else if(c < 0x20) {
                        throw_parse_error(p, JSONPG_ERROR_INVALID);
                } else {
                        mis_take(mis);
                }
        }
}

static inline size_t parse_nqstring_in_stream(Parser p, Bytes *bytes)
{
        const MemoryInputStream mis = p->mis;

        mis_string_start(mis);

        while(true) {
                Byte c = mis_peek(mis);
                if(c == '\\') {
                        mis_string_update(mis);
                        mis_byte_copy(mis);
                        mis_string_restart(mis);
                } else if(c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                        return mis_string_complete(mis, bytes);
                } else if(c >= 0x80) {
                        if(-1 == utf8_validate_sequence(p->mis))
                                throw_parse_error(p, JSONPG_ERROR_UTF8);
                } else if(c < 0x20) {
                        throw_parse_error(p, JSONPG_ERROR_INVALID);
                } else {
                        mis_take(mis);
                }
        }
}

static inline size_t parse_string(Parser p, Bytes *bytes)
{
        ASSERT(mis_peek(p->mis) == '"');

        mis_take(p->mis); // "
        return parse_string_in_stream(p, '"', bytes);
}

static inline size_t parse_sqstring(Parser p, Bytes *bytes)
{
        ASSERT(mis_peek(p->mis) == '\'');

        mis_take(p->mis); // '
        return parse_string_in_stream(p, '\'', bytes);
}

static inline size_t parse_nqstring(Parser p, Bytes *bytes)
{
        return parse_nqstring_in_stream(p, bytes);
}

// Lots of sign changing in parse_number so turn off warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
static JsonType parse_number(Parser p, double *real_result, long *integer_result)
{
        // Max digits for long,
        // double is 15-17 but we will lose those when converting
        static const int max_sig_digits = 19;

        // By taking ascii '0' from unsigned char
        // We can test for digits with a single comparison (<10)
        // Rather than two ('0' <= x && x <= '9')

        // Non-digit chars of interest
        static const Byte minus = ((Byte)'-') - '0';
        static const Byte point = ((Byte)'.') - '0';
        static const Byte lower_e = ((Byte)'e') - '0';
        static const Byte upper_e = ((Byte)'E') - '0';
        static const Byte plus = ((Byte)'+') - '0';

        const MemoryInputStream mis = p->mis;

        // If fast parsing fails might need to call
        // strtod, which needs to start from the beginning
        size_t start_pos = mis_tell(mis);

        bool force_double = false;
        bool negative = false;
        uint64_t sum;
        int64_t exponent = 0;
        int sig_digits = 0;

        Byte c = mis_take(mis) - '0';

        if(c == minus) {
                negative = true;
                c = mis_take(mis) - '0';
        }
        if(c < 10) {
                sum = c;
                sig_digits += (sum != 0);
        } else {
                throw_parse_error(p, JSONPG_ERROR_NUMBER);
        }

        c = mis_peek(mis) - '0';
        if(sum) {
                while(c < 10) {
                        mis_take(mis);
                        if(sig_digits++ < max_sig_digits) {
                                sum = sum * 10 + c;
                        } else {
                                exponent++;
                        }
                        c = mis_peek(mis) - '0';
                }
        }
        if(c == point) {
                mis_take(mis);
                force_double = true;

                c = mis_peek(mis) - '0';
                if(c < 10) {
                        mis_take(mis);
                        if(sig_digits < max_sig_digits) {
                                sum = 10 * sum + c;
                                exponent--;
                                sig_digits += (sum != 0);
                        }
                        c = mis_peek(mis) - '0';
                } else {
                        throw_parse_error(p, JSONPG_ERROR_NUMBER);
                }

                while(c < 10) {
                        mis_take(mis);
                        if(sig_digits < max_sig_digits) {
                                sum = 10 * sum + c;
                                exponent--;
                                sig_digits += (sum != 0);
                        }
                        c = mis_peek(mis) - '0';
                }
        }
        if(c == lower_e || c == upper_e) {
                mis_take(mis);
                force_double = true;
                int exp_sign = 1;
                int exp = 0;

                c = mis_peek(mis) - '0';
                if(c == minus) {
                        mis_take(mis);
                        exp_sign = -1;
                        c = mis_peek(mis) - '0';
                } else if(c == plus) {
                        mis_take(mis);
                        c = mis_peek(mis) - '0';
                }
                if(c < 10) {
                        mis_take(mis);
                        exp = 10 * exp + c;
                        c = mis_peek(mis) - '0';
                        while(c < 10) {
                                mis_take(mis);
                                exp = 10 * exp + c;
                                if(exp > 1000)
                                        throw_parse_error(p, JSONPG_ERROR_NUMBER);
                                c = mis_peek(mis) - '0';
                        }
                } else {
                        throw_parse_error(p, JSONPG_ERROR_NUMBER);
                }
                exponent += exp_sign * exp;
        }
        
        // Force double if either too many significant digits 
        // or sum is too big for signed long
        force_double = force_double 
                || sig_digits > max_sig_digits
                || (negative ? sum > 1 + (uint64_t)LONG_MAX : sum > LONG_MAX);

        if(force_double) {
                bool success = false;
                if (exponent >= FASTFLOAT_SMALLEST_POWER ||
                                exponent <= FASTFLOAT_LARGEST_POWER) {
                        *real_result = compute_float_64(exponent, sum, negative, &success);
                }
                if(!success) {
                        const char *start = (const char *)mis_at(mis, start_pos);
                        char *end = parse_float_strtod(start, real_result);
                        if(!end)
                                throw_parse_error(p, JSONPG_ERROR_NUMBER);
                        mis_adjust(mis, (Bytes)end);
                }
                return JSONPG_REAL;
        } else {
                *integer_result = negative ? -sum : sum;
                return JSONPG_INTEGER;
        }
}
#pragma GCC diagnostic pop

static void parser_set_bytes(Parser p, Bytes bytes, size_t count)
{
        // Skip leading byte order mark
        unsigned skip = utf8_bom_bytes(bytes, count);
        bytes += skip;
        count -= skip;

        // The advantages of having a null terminated, writeable, byte array
        // outweighs the cost of copying
        Bytes b = allocator_alloc(p->allocator, count + 1);
        memcpy(b, bytes, count);
        b[count] = '\0';

        mis_set_bytes(p->mis, b, count);
}

static void parser_set_dom_info(Parser p, DomInfo di)
{
        p->dom_info = di;
}

static inline uint16_t get_stack_size(uint16_t stack_size)
{
        return stack_size > MIN_STACK_SIZE ? stack_size : MIN_STACK_SIZE;
}

static Parser parser_new(Allocator a, uint16_t stack_size, unsigned flags)
{
        // The bit stack (keeps track of object/array nesting)
        // Is allocated space directly after the parser struct
        size_t struct_bytes = sizeof(struct jsonpg_parser_s);
        Parser p = allocator_alloc(a, struct_bytes + (unsigned)((stack_size + 7) / 8));
        if(!p)
                return NULL;

        p->allocator = a;
        p->result = (ParseResult) {};

        p->mis = mis_new(a);
        if(!p->mis)
                return NULL;

        p->dom_info = (DomInfo){};

        p->stack = (struct stack_s) {
                .ptr = 0,
                .size = stack_size,
                .stack = (((Byte *)p) + struct_bytes)
        };
        p->state = STATE_START;
        p->flags = flags;

        return p;
}

void jsonpg_parser_free(Parser p)
{
        allocator_free(p->allocator);
}


Parser jsonpg_parser_new_opt(ParserOpts opts)
{
        uint16_t stack_size = get_stack_size(opts.max_nesting);
        unsigned flags = opts.flags;

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
