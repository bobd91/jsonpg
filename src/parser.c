/*
 * jsonpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * parser.c
 *   provides parsing functionality shared by the sax/event style parser
 *   and the pull parser
 */

#include <setjmp.h>
#include <limits.h>
#include <float.h>
#include <string.h>
#include <stdarg.h>

#define MIN_STACK_SIZE 1024

static inline bool parser_in_object(parser *p)
{
        return stack_peek(&p->stack) == STACK_OBJECT;
}

static inline bool parser_in_array(parser *p)
{
        return stack_peek(&p->stack) == STACK_ARRAY;
}

static size_t parse_position(parser *p)
{
        return p->mis->start ? mis_tell(p->mis) : 0;
}

static parse_result make_parse_result(parser *p, json_type type, ...)
{
        va_list ap;
        parse_result result;
        va_start(ap, type);
        
        result.type = type;
        result.position = parse_position(p);

        switch(type) {
        case JSONPG_STRING:
        case JSONPG_KEY:
                result.string.bytes = va_arg(ap, byte *);
                result.string.count = va_arg(ap, size_t);
                break;
        case JSONPG_REAL:
                result.number.real = va_arg(ap, double);
                break;
        case JSONPG_INTEGER:
                result.number.integer = va_arg(ap, long);
                break;
        case JSONPG_ERROR:
                result.error.code = va_arg(ap, error_code);
                result.error.text = error_text(result.error.code);
                break;
        default:
                // nothing to do
        }
        va_end(ap);

        return result;
}

[[noreturn]]
static void throw_parse_error_at(parser *p, error_code error_code, size_t at)
{
        p->result = make_error_return(error_code, at);
        longjmp(p->env, 1);
}

[[noreturn]]
static void throw_parse_error(parser *p, error_code error_code)
{
        throw_parse_error_at(p, error_code, parse_position(p));
}

static inline int parse_start_object(parser *p)
{
        ASSERT(mis_peek(p->mis) == '{');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_OBJECT))
                throw_parse_error(p, JSONPG_ERROR_STACK_OVERFLOW);
        return STACK_OBJECT;
}

static inline int parse_end_object(parser *p)
{
        ASSERT(mis_peek(p->mis) == '}');
        ASSERT(stack_peek(&p->stack) == STACK_OBJECT);

        mis_take(p->mis);
        int type = stack_pop(&p->stack);

        if(-1 == type)
                throw_parse_error(p, JSONPG_ERROR_STACK_UNDERFLOW);
        return type;
}

static inline int parse_start_array(parser *p)
{
        ASSERT(mis_peek(p->mis) == '[');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_ARRAY))
                throw_parse_error(p, JSONPG_ERROR_STACK_OVERFLOW);
        return STACK_ARRAY;
}

static inline int  parse_end_array(parser *p)
{
        ASSERT(mis_peek(p->mis) == ']');
        ASSERT(stack_peek(&p->stack) == STACK_ARRAY);

        mis_take(p->mis);
        int type = stack_pop(&p->stack);

        if(-1 == type)
                throw_parse_error(p, JSONPG_ERROR_STACK_UNDERFLOW);
        return type;
}

static inline void parse_true(parser *p)
{
        memory_input_stream *const mis = p->mis;

        ASSERT(mis_peek(mis) == 't');

        mis_take(mis);
        if(!mis_consume(mis, 'r')
                        || !mis_consume(mis, 'u')
                        || !mis_consume(mis, 'e'))
                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
}

static inline void parse_false(parser *p)
{
        memory_input_stream *const mis = p->mis;

        ASSERT(mis_peek(mis) == 'f');

        mis_take(mis);
        if(!mis_consume(mis, 'a')
                        || !mis_consume(mis, 'l')
                        || !mis_consume(mis, 's')
                        || !mis_consume(mis, 'e'))
                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
}

static inline void parse_null(parser *p)
{
        memory_input_stream *const mis = p->mis;

        ASSERT(mis_peek(mis) == 'n');

        mis_take(mis); // 'n'
        if(!mis_consume(mis, 'u')
                        || !mis_consume(mis, 'l')
                        || !mis_consume(mis, 'l'))
                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
}

static unsigned parse_hex4(parser *p)
{
        memory_input_stream *const mis = p->mis;

        unsigned codepoint = 0;
        for(int i = 0 ; i < 4 ; i++) {
                byte c = mis_peek(mis);
                codepoint <<= 4;
                if(c >= '0' && c <= '9')
                        codepoint += (unsigned)(c - '0');
                else if(c >= 'A' && c <= 'F')
                        codepoint += 10 + (unsigned)(c - 'A');
                else if(c >= 'a' && c <= 'f')
                        codepoint += 10 + (unsigned)(c - 'a');
                else 
                        throw_parse_error(p, JSONPG_ERROR_ESCAPE);

                mis_take(mis);
        }
        return codepoint;
}

static unsigned parse_escape(parser *p)
{
        static const unsigned char escape[256] = {
                ['"'] = '"',  ['/'] = '/',  ['\\'] = '\\', ['b'] = '\b', 
                ['f'] = '\f', ['n'] = '\n', ['r'] = '\r',  ['t'] = '\t'
        };

        memory_input_stream *const mis = p->mis;

        mis_take(mis); // '\\'

        const byte e = mis_peek(mis);

        if(escape[e]) {
                mis_take(mis);
                return (unsigned)escape[e];
        }

        if(e == 'u') {
                mis_take(mis);
                unsigned codepoint = parse_hex4(p);
                if(codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                        // Got surrogate but high (first one) must be 0xD800-0xDBFF
                        if(codepoint <= 0xDBFF) {
                                // high surrogate must be followed by low
                                if(!(mis_consume(mis, '\\')
                                                && mis_consume(mis, 'u')))
                                        throw_parse_error(p, JSONPG_ERROR_SURROGATE);

                                const unsigned codepoint2 = parse_hex4(p);

                                if(codepoint2 < 0xDC00 || codepoint2 > 0xDFFF)
                                        throw_parse_error(p, JSONPG_ERROR_SURROGATE);

                                codepoint = (((codepoint - 0xD800) << 10)
                                                | (codepoint2 - 0xDC00)) + 0x10000;
                        } else {
                                throw_parse_error(p, JSONPG_ERROR_SURROGATE);
                        }
                }
                return codepoint;
        } else {
                throw_parse_error(p, JSONPG_ERROR_ESCAPE);
        }
}

static inline byte mis_consume_whitespace(memory_input_stream *mis)
{
        byte c;

        while((c = mis_peek(mis)) == ' ' || c == '\n' || c == '\r' || c== '\t')
                 mis_take(mis);

        return c;

}

static byte consume_whitespace(parser *p, bool allow_comments)
{
        memory_input_stream *const mis = p->mis;
        byte c;

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
                                if(c == '*') {
                                        mis_take(mis); // '*'
                                        if(mis_consume(mis, '/'))
                                                break;
                                }
                                if(mis_eof(mis))
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

static size_t parse_string_in_stream(parser *p, byte **bytes, const bool validate_utf8)
{
        memory_input_stream *const mis = p->mis;

        mis_string_start(mis);

        while(true) {
                byte c = mis_peek(mis);
                if(c == '"') {
                        return mis_string_complete(mis, bytes);
                } else if(c == '\\') {
                        mis_string_update(mis);
                        unsigned codepoint = parse_escape(p);
                        utf8_encode(codepoint, mis_writer(mis));
                        mis_string_restart(mis);
                } else if(validate_utf8 && c >= 0x80) {
                        if(!mis_validate_utf8(mis))
                                throw_parse_error(p, JSONPG_ERROR_UTF8);
                } else if(c < 0x20) {
                        throw_parse_error(p, JSONPG_ERROR_INVALID);
                } else {
                        mis_take(mis);
                }
        }
}

static inline size_t parse_string(parser *p, byte **bytes, const bool validate_utf8)
{
        ASSERT(mis_peek(p->mis) == '"');

        mis_take(p->mis); // "
        return parse_string_in_stream(p, bytes, validate_utf8);
}

// This function, along with formatting numbers, takes much more cpu
// than any other parse function so we try and save as many tests/branches as we can
//
// If the number is supplied without decimal point and exponent, and it falls 
// in the range of a signed long, we treat it as a signed long.
// Otherwise it is stored in a double.
// We try to parse doubles with our C conversion of 
// https://github.com/lemire/fast_double_parser which can be significantly
// faster than the standard library strtod but does not work for all
// valid double precision numbers.  We fallback to strtod where necessary.
//
// Lots of sign changing in parse_number so turn off warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

static json_type parse_number(parser *p, double *real_result, long *integer_result)
{
        // We only take the most significant digits
        // Max digits for long is 19
        // double is 15-17 so we may lose some digits when converting
        static const int max_sig_digits = 19;

        // By taking ascii '0' from unsigned char
        // We convert '0' => 0 etc, which we will need to do anyway
        // plus we can test for digits with a single comparison (<10)
        // Rather than two ('0' <= x && x <= '9')
        // It does make comparing with '.', 'e', 'E' more complex but
        // the -'0' for these can be done at compile time
        static const byte point = ((byte)'.') - '0';
        static const byte lower_e = ((byte)'e') - '0';
        static const byte upper_e = ((byte)'E') - '0';

        memory_input_stream *const mis = p->mis;

        // If fast parsing fails might need to call
        // strtod, which needs to start from the beginning
        size_t start_pos = mis_tell(mis);

        bool force_double = false;
        bool negative = false;
        uint64_t sum;
        int64_t exponent = 0;
        int sig_digits = 0;

        byte c = mis_take(mis);

        if(c == '-') {
                negative = true;
                c = mis_take(mis);
        }
        c -= '0';
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
                if(c >= 10)
                        throw_parse_error(p, JSONPG_ERROR_NUMBER);

                do {
                        mis_take(mis);
                        if(sig_digits < max_sig_digits) {
                                sum = 10 * sum + c;
                                exponent--;
                                sig_digits += (sum != 0);
                        }
                        c = mis_peek(mis) - '0';
                } while(c < 10);

        }
        if(c == lower_e || c == upper_e) {
                mis_take(mis);
                force_double = true;
                int exp_sign = 1;
                int exp = 0;

                c = mis_peek(mis);
                if(c == '-') {
                        mis_take(mis);
                        exp_sign = -1;
                        c = mis_peek(mis);
                } else if(c == '+') {
                        mis_take(mis);
                        c = mis_peek(mis);
                }
                c -= '0';
                if(c >= 10)
                        throw_parse_error(p, JSONPG_ERROR_NUMBER);

               do {
                        mis_take(mis);
                        exp = 10 * exp + c;
                        if(exp > 1000)
                                throw_parse_error(p, JSONPG_ERROR_NUMBER);
                        c = mis_peek(mis) - '0';
                } while (c < 10);

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
                        mis_adjust(mis, (byte *)end);
                }
                return JSONPG_REAL;
        } else {
                *integer_result = negative ? -sum : sum;
                return JSONPG_INTEGER;
        }
}
#pragma GCC diagnostic pop

static void parser_set_bytes(parser *p, byte *bytes, size_t count)
{
        // Skip leading byte order mark
        unsigned skip = utf8_bom_bytes(bytes, count);
        bytes += skip;
        count -= skip;

        // The advantages of having a null terminated, writeable, byte array
        // outweighs the cost of copying
        byte *b = allocator_alloc(p->allocator, count + 1);
        memcpy(b, bytes, count);
        b[count] = '\0';

        mis_set_bytes(p->mis, b, count);
}

static void parser_set_dom_info(parser *p, dom_info di)
{
        p->dom_info = di;
}

static inline unsigned get_stack_size(uint16_t stack_size)
{
        return stack_size > MIN_STACK_SIZE ? stack_size : MIN_STACK_SIZE;
}

static parser *parser_new(allocator *a, uint16_t stack_size, unsigned flags)
{
        // The bit stack (keeps track of object/array nesting)
        // Is allocated space directly after the parser struct
        size_t struct_bytes = sizeof(parser);
        parser *p = allocator_alloc(a, struct_bytes + (unsigned)((stack_size + 7) / 8));
        if(!p)
                return NULL;

        p->allocator = a;
        p->result = (parse_result) {};

        p->mis = mis_new(a);
        if(!p->mis)
                return NULL;

        p->dom_info = (dom_info){};

        p->stack = (stack) {
                .ptr = 0,
                .size = stack_size,
                .stack = (((byte *)p) + struct_bytes)
        };
        p->state = STATE_START;
        p->flags = flags;

        return p;
}

void jsonpg_parser_free(parser *p)
{
        allocator_free(p->allocator);
}


parser *jsonpg_parser_new_opt(parser_opts opts)
{
        unsigned stack_size = get_stack_size(opts.max_nesting);
        unsigned flags = opts.allow;

        allocator *a = allocator_new();
        if(!a)
                return NULL;

        parser *p = parser_new(a, stack_size, flags);

        if(!p) {
                allocator_free(a);
                return NULL;
        }

        if(1 != (opts.bytes != NULL) + (opts.string != NULL) + (opts.dom != NULL)) {
                p->result = make_error_return(JSONPG_ERROR_OPT, 0);
                return p;
        }

        if(opts.bytes) {
                parser_set_bytes(p, opts.bytes, opts.count);
        } else if(opts.string) {
                parser_set_bytes(p, (byte *)opts.string, strlen(opts.string));
        } else if(opts.dom) {
                parser_set_dom_info(p, dom_parser_info(opts.dom));
        }

        return p;
}

parse_result jsonpg_parse_result(parser *p)
{
        return p->result;
}
