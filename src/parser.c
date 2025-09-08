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

static double pow_10(unsigned n) {
    static const double e[] = { // 1e-0...1e308: 309 * 8 bytes = 2472 bytes
        1e+0,  
        1e+1,  1e+2,  1e+3,  1e+4,  1e+5,  1e+6,  1e+7,  1e+8,  1e+9,  1e+10, 1e+11, 1e+12, 1e+13, 1e+14, 1e+15, 1e+16, 1e+17, 1e+18, 1e+19, 1e+20, 
        1e+21, 1e+22, 1e+23, 1e+24, 1e+25, 1e+26, 1e+27, 1e+28, 1e+29, 1e+30, 1e+31, 1e+32, 1e+33, 1e+34, 1e+35, 1e+36, 1e+37, 1e+38, 1e+39, 1e+40,
        1e+41, 1e+42, 1e+43, 1e+44, 1e+45, 1e+46, 1e+47, 1e+48, 1e+49, 1e+50, 1e+51, 1e+52, 1e+53, 1e+54, 1e+55, 1e+56, 1e+57, 1e+58, 1e+59, 1e+60,
        1e+61, 1e+62, 1e+63, 1e+64, 1e+65, 1e+66, 1e+67, 1e+68, 1e+69, 1e+70, 1e+71, 1e+72, 1e+73, 1e+74, 1e+75, 1e+76, 1e+77, 1e+78, 1e+79, 1e+80,
        1e+81, 1e+82, 1e+83, 1e+84, 1e+85, 1e+86, 1e+87, 1e+88, 1e+89, 1e+90, 1e+91, 1e+92, 1e+93, 1e+94, 1e+95, 1e+96, 1e+97, 1e+98, 1e+99, 1e+100,
        1e+101,1e+102,1e+103,1e+104,1e+105,1e+106,1e+107,1e+108,1e+109,1e+110,1e+111,1e+112,1e+113,1e+114,1e+115,1e+116,1e+117,1e+118,1e+119,1e+120,
        1e+121,1e+122,1e+123,1e+124,1e+125,1e+126,1e+127,1e+128,1e+129,1e+130,1e+131,1e+132,1e+133,1e+134,1e+135,1e+136,1e+137,1e+138,1e+139,1e+140,
        1e+141,1e+142,1e+143,1e+144,1e+145,1e+146,1e+147,1e+148,1e+149,1e+150,1e+151,1e+152,1e+153,1e+154,1e+155,1e+156,1e+157,1e+158,1e+159,1e+160,
        1e+161,1e+162,1e+163,1e+164,1e+165,1e+166,1e+167,1e+168,1e+169,1e+170,1e+171,1e+172,1e+173,1e+174,1e+175,1e+176,1e+177,1e+178,1e+179,1e+180,
        1e+181,1e+182,1e+183,1e+184,1e+185,1e+186,1e+187,1e+188,1e+189,1e+190,1e+191,1e+192,1e+193,1e+194,1e+195,1e+196,1e+197,1e+198,1e+199,1e+200,
        1e+201,1e+202,1e+203,1e+204,1e+205,1e+206,1e+207,1e+208,1e+209,1e+210,1e+211,1e+212,1e+213,1e+214,1e+215,1e+216,1e+217,1e+218,1e+219,1e+220,
        1e+221,1e+222,1e+223,1e+224,1e+225,1e+226,1e+227,1e+228,1e+229,1e+230,1e+231,1e+232,1e+233,1e+234,1e+235,1e+236,1e+237,1e+238,1e+239,1e+240,
        1e+241,1e+242,1e+243,1e+244,1e+245,1e+246,1e+247,1e+248,1e+249,1e+250,1e+251,1e+252,1e+253,1e+254,1e+255,1e+256,1e+257,1e+258,1e+259,1e+260,
        1e+261,1e+262,1e+263,1e+264,1e+265,1e+266,1e+267,1e+268,1e+269,1e+270,1e+271,1e+272,1e+273,1e+274,1e+275,1e+276,1e+277,1e+278,1e+279,1e+280,
        1e+281,1e+282,1e+283,1e+284,1e+285,1e+286,1e+287,1e+288,1e+289,1e+290,1e+291,1e+292,1e+293,1e+294,1e+295,1e+296,1e+297,1e+298,1e+299,1e+300,
        1e+301,1e+302,1e+303,1e+304,1e+305,1e+306,1e+307,1e+308
    };
    JSONPG_ASSERT(n <= 308);
    return e[n];
}

static double fast_path(double d, int e)
{
        if(e < -308)
                return 0.0;
        else if(e >= 0)
                return d * pow_10(e);
        else
                return d / pow_10(-e);
}


static double strtod_normal(double d, int e) 
{
        if(e < -308) {
                d = fast_path(d, -308);
                d = fast_path(d, e + 308);
        } else {
                d = fast_path(d, e);
        }
        return d;
}

static static jsonpg_type parse_number(Parser p, *double real, *long integer)
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
        bytes += utf8_bom_bytes(bytes, count);

        mis_set_bytes(p->mis, bytes, count);
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
