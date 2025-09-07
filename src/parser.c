#include "parser.h"

static void parse_start_object(p)
{
        JSONPG_ASSERT(input_peek(p) == '{');

        input_take(p);
        stack_push(p, JSONPG_OBJECT);
}

static void parse_end_object(p)
{
        JSONPG_ASSERT(input_peek(p) == '}');
        JSONPG_ASSERT(stack_peek(p) == STACK_OBJECT);

        input_take(p);
        input_pop(p);
}

static void parse_start_array(p)
{
        JSONPG_ASSERT(input_peek(p) == '[');

        input_take(p);
        stack_push(p, JSONPG_ARRAY);
}

static void parse_end_array(p)
{
        JSONPG_ASSERT(input_peek(p) == ']');
        JSONPG_ASSERT(stack_peek(p) == STACK_ARRAY);

        input_take(p);
        input_pop(p);
}

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

static Byte parser_peek(Parser p)
{
        return input_peek(p);
}

static bool parser_consume(Parser p, Byte c)
{
        return input_consume(p, c);
}

static bool parser_eof(Parser p)
{
        return input_eof(p);
}

static void parse_error(JsonpgParser p, unsigned error_code)
{
        p->error = (JsonpgError) {
                .code = error_code,
                .at = p->offset
        };
        longjmp(p->env);
}

static void parse_true(Parser p)
{
        JSONPG_ASSERT(input_peek(p) == 't');

        input_take(p);
        if(!input_consume(p, 'r')
                        || !input_consume(p, 'u')
                        || !input_comsume(p, 'e'))
                parse_error(p, UNEXPECTED);
}

static void parse_false(Parser p)
{
        JSONPG_ASSERT(input_peek(p) == 'f');

        input_take(p);
        if(!input_consume(p, 'a')
                        || !input_consume(p, 'l')
                        || !input_consume(p, 's')
                        || !input_comsume(p, 'e'))
                parse_error(p, UNEXPECTED);
}

static void parse_null(Parser p)
{
        JSONPG_ASSERT(input_peek(p) == 'n');

        input_take(p); // 'n'
        if(!input_consume(p, 'u')
                        || !input_consume(p, 'l')
                        || !input_comsume(p, 'l'))
                parse_error(p, UNEXPECTED);
}

static unsigned parse_hex4(Parser p, size_t esc_offset)
{
        unsigned codepoint = 0;
        for(int i = 0 ; i < 4 ; i++) {
                char c = input_peak(p);
                codepoint <<= 4;
                if(c >= '0' && c <= '9')
                        codepoint += c - '0';
                else if(c >= 'A' && c <= 'F')
                        codepoint += 10 + c -'A';
                else if(c >= 'a' && c <= 'f')
                        codepoint += 10 + c - 'a';
                else 
                        parse_error(p, ESCAPE, esc_offset);

                input_take(p);
        }
        return codepoint;
}

static unsigned parse_escape(Parser p)
{
        static const unsigned char escape[256] = {
                ['"'] = '"',  ['/'] = '/',  ['\\'] = '\\', ['b'] = '\b', 
                ['f'] = '\f', ['n'] = '\n', ['r'] = '\r',  ['t'] = '\t'
        };

        const size_t esc_offset = input_tell(p);
        input_take(p);
        const unsigned char e = input_peek(p);
        if(escape[e]) {
                input_take(p);
                return (unsigned)escape[e];
        }
        if(e == 'u') {
                input_take(p);
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

static void consume_whitespace(MemoryInputStream mis)
{
        Bytes c;

        while((c = mis_peek(is)) == ' ' || c == '\n' || c == '\r' || c== '\t')
                mis_take(is);
}


void jsonpg_consume_whitespace_only(Parser p)
{
        consume_whitespace(p->is);
}

void jsonpg_consume_whitespace_comments(Parser p)
{
        MemoryInputStream is = p->is;
        Bytes c;

        while(true) {
                consume_whitespace(is);

                if(mis_consume(is, '/')) {
                        c = mis_peek(is);
                        if(c == '*') {
                                mis_take(is);
                                while(true) {
                                        if(mis_consume(is, '*')) {
                                                if(mis_consume(is, '/'))
                                                        break;
                                        } else if(mis_peek(is) == '\0') {
                                                return;
                                        } else {
                                                mis_take(is);
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

static static void parse_string_to_cow(Parser p, CowStream cs, Byte terminator)
{
        MemoryInputStream mis = p->is;

        while(true) {
                //copy_safe_chars(p->is, os);
                
                unsigned char c = mis_peek(mis);
                if(c == '\\') {
                        unsigned codepoint = parse_escape(p);
                        Bytes s = cow_reserve(cs, 4);
                        if(!s)
                                parse_error(p, MEMORY);
                        int count = utf8_encode(codepoint, s);
                        if(count == -1)
                                parse_error(p, UTF8);
                        cow_adjust(cs, 4 - count);
                } else if(c == terminator) {
                        if(!cow_finalize(cs))
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

static static void parse_nqstring_to_cow(Parser p, CowStream cs)
{
        MemoryInputStream mis = p->is;

        while(true) {
                //copy_safe_chars(p->is, os);
                
                unsigned char c = mis_peek(mis);
                if(c == '\\') {
                        input_take(p);
                        // Skip \ puts us out of sync with cow
                        // reserve 0 will trigger cow write
                        cow_reserve(cs, 0);
                } else if(c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                        if(!cow_finalize(cs))
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

static static size_t parse_string(Parser p, Bytes *bytes, byte terminator)
{
        CowStream cs = cow_new(p); // TODO: 1 cow stream per parser
        if(!cs)
                return 0;

        parse_string_to_cow(p, cs, terminator);
        size_t length = cow_length(cs);
        Bytes str = cow_pop(cs);
        *bytes = str;
        return length;
}

static size_t parse_string(Parser p, Bytes *bytes)
{
        JSONPG_ASSERT(input_peek(p) == '"');
        input_take(p); // "
        parse_string(p, bytes, '"');
}

static size_t parse_sqstring(Parser p, Bytes *bytes)
{
        JSONPG_ASSERT(input_peek(p) == '\'');
        input_take(p); // '
        parse_string(p, bytes, '\'');
}

static size_t parse_nqstring(Parser p, Bytes *bytes)
{
        CowStream cs = cow_new(p);
        if(!cs)
                return 0;

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

static ParseState state_change_value(ParseState state)
{
        if(state == STATE_START) return STATE_DONE;
        if(state == STATE_KEY) return STATE_KEY_VALUE;
        if(state == STATE_ARRAY) return STATE_ARRAY_VALUE;
        assert(false && "Invalid state");
}

static ParseState state_change_end(Parser p)
{
        if(parser_in_object(p)) return STATE_KEY_VALUE;
        if(parser_in_array(p)) return STATE_ARRAY_VALUE;
        return STATE_DONE;
}

static jsonpg_value make_boolean(Parser p, bool is_true);
{
        p->result.type = is_true ? JSONPG_TRUE : JSONPG_FALSE;
        return p->result;
}

static jsonpg_value make_null(Parser p)
{
        p->result.type = JSONPG_NULL;
        return p->result;
}

static jsonpg_value make_integer(Parser p, long integer)
{
        p->result.type = JSONPG_INTEGER;
        p->result.number.integer = integer;
        return p->result;
}

static jsonpg_value make_real(Parser p, double real)
{
        p->result.type = JSONPG_REAL;
        p->result.number.real = real;
        return p->result;
}

static jsonpg_value make_string(Parser p, Bytes bytes, size_t count)
{
        p->result.type = JSONPG_STRING;
        p->result.string.bytes = bytes;
        p->result.string.count = count;
        return p->result;
}

static jsonpg_value make_key(Parser p, Bytes bytes, size_t count)
{
        p->result.type = JSONPG_KEY;
        p->result.string.bytes = bytes;
        p->result.string.count = count;
        return p->result;
}

static jsonpg_value make_start_object(Parser p)
{
        p->result.type = JSONPG_START_OBJECT;
        return p->result;
}

static jsonpg_value make_end_object(Parser p)
{
        p->result.type = JSONPG_END_OBJECT;
        return p->result;
}

static jsonpg_value make_start_array(Parser p)
{
        p->result.type = JSONPG_START_ARRAY;
        return p->result;
}

static jsonpg_value make_end_array(Parser p)
{
        p->result.type = JSONPG_END_ARRAY;
        return p->result;
}

static jsonpg_value parse_next(Parser p)
{
        const bool opt_comment = p->flags & JSONPG_FLAG_COMMENT;
        ParserState state = p->state;
        unsigned char *bytes;
        size_t count;
        
        parser_consume_whitespace(p, opt_comment);
        Byte b = parser_peek(p);

        if(state=STATE_EOF || b == '\0') {
                parse_error(p, JSONPG_ERROR_EOF);

        // States that are not just expecting values
        switch(state) {

        case STATE_KEY_VALUE:
                if(b == '}') {
                        parse_end_object(p);
                        p->state = state_change_end(p);
                        return make_end_object(p);
                } else if(b == ',') {
                        parser_take(p);
                        parser_consume_whitespace(p, opt_comment);
                        b = parser_peek(p);
                        state = STATE_OBJECT_COMMA;
                        break;
                } else if(!(p->flags & JSONPG_FLAG_OPTIONAL_COMMA)) {
                        parse_error(p, JSONPG_ERROR_UNEXPECTED);
                }

                state = STATE_OBJECT;

                // fall through as we are now in STATE_OBJECT

        case STATE_OBJECT:
                if(b == '}') {
                        parse_end_object(p);
                        p->state = state_change_end(p);
                        return make_end_object(p);
                } else if(b == '"') {
                        count = parse_string(p, &bytes);
                } else if((p->flags & JSONPG_FLAG_SINGLE_QUOTE) && b == '\'') {
                        count = parse_sqstring(p, &bytes);
                } else if(p->flags & JSONPG_FLAG_KEY_NO_QUOTE) {
                        count = parse_nqstring(p, &bytes);
                } else {
                        parse_error(p, KEY);
                }

                parser_consume_whitespace(p, opt_comment);
                if(parser_consume(p, ':'))
                        parse_error(p, COLON);
                
                p->state = STATE_KEY;
                return make_key(p, bytes, count); 

        case STATE_ARRAY_VALUE:
                if(b == ']') {
                        parse_end_array(p);
                        p->state = state_change_end(p);
                        return make_end_object(p);
                } else if(b == ",") {
                        parser_take(p);
                        parser_consume_whitespace(p, opt_comment);
                        b = parser_peek(p);
                        state = STATE_ARRAY_COMMA;
                        break;
                } else if(!(p->flags & JSONPG_FLAG_OPTIONAL_COMMA)) {
                        parse_error(p, JSONPG_ERROR_UNEXPECTED);
                }

                state = STATE_ARRAY;

                // We could fall through as we are now in STATE_ARRAY
                // but there is no point as that only handles the ']'
                // case and we already know b != ']'
                break;

        case STATE_ARRAY:
                if(b == ']') {
                        parse_end_array(p);
                        p->state = state_change_end(p);
                        return make_end_object(p);
                }
                break;

        case STATE_DONE:
                if(!(p->flags & JSONPG_FLAG_IGNORE_TRAILING)) {
                        parser_consume_whitespace(p, opt_comment);
                        if(!parser_eof(p))
                                parse_error(p, JSONPG_ERROR_UNEXPECTED);
                }
                p->state = STATE_EOF;
                return make_eof(p);

        default:
                // Handle other states below
        }

        // state in 
        //      START - any value { or [
        //      KEY - any value { or [
        //      OBJECT_COMMA - any value { or [ (or } if trailing commas)
        //      ARRAY - any value { or [ (not ] as we checked above)
        //      ARRAY_COMMA - any value { or [ (or ] if trailing commas)
        //
        // states STATE_OBJECT_COMMA and STATE_ARRAY_COMMA
        // are transient states that never get set in the parser
        // but are needed here to handle trailing comma option

        assert(state == START || state == KEY || state == OBJECT_COMMA
                        || state == ARRAY || state == ARRAY_COMMA);

        switch(b) {
        case '"':
                count = parse_string(p, &bytes);
                p->state = state_change_value(p->state);
                return make_string(p, bytes, count); 

        case '{': 
                parse_start_object(p);
                p->state = STATE_OBJECT;
                return make_start_object(p);

        case '[':
                parse_start_array(p);
                p->state = STATE_ARRAY;
                return make_start_array(p);

        case 't':
                parse_true(p);
                p->state = state_change_value(p->state);
                return make_boolean(p, true);

        case 'f':
                parse_false(p);
                p->state = state_change_value(p->state);
                return make_boolean(false);

        case 'n':
                parse_null(p);
                p->state = state_change_value(p->state);
                return make_null(p);

        case '\'':
                if(!p->flags & JSONPG_FLAG_SINGLE_QUOTE)
                        parse_error(p, UNEXPECTED);

                count = parse_sqstring(p, &bytes);
                p->state = state_change_value(p->state);
                return make_string(p, bytes, count);

        case '}':
                if(!(state == STATE_OBJECT_COMMA
                                        && (p->flags & JSONPG_FLAG_TRAILING_COMMA)))
                        parse_error(p, UNEXPECTED);

                parse_end_object(p);
                p->state = state_change_end(p);
                return make_end_object(p);

        case ']':
                if(!(state == STATE_ARRAY_COMMA
                                        && (p->flags & JSONPG_FLAG_TRAILING_COMMA)))
                        parse_error(p, UNEXPECTED);

                parse_end_array(p);
                p->state = state_change_end(p);
                return make_end_array(p);

        default:
                if(b == '-' || ('0' <= b && b <= '9')) {
                        double d;
                        long l;
                        if(JSONPG_REAL == parse_number(p, &d, &l)) {
                                p->state = state_chaneg_value(p->state);
                                return make_real(p, d);
                        } else {
                                p->state = state_chaneg_value(p->state);
                                return make_integer(p, l);
                        }
                }

                if(!(p->flags & JSONPG_FLAG_STRING_NO_QUOTE))
                        parse_error(p, UNEXPECTED);

                count = parse_nqstring(p, &bytes);
                p->state = state_change_value(p->state);
                return make_string(p, bytes, count);
        }

}

jsonpg_value jsonpg_parse_next(Parser p)
{
        if(0 == setjmp(p->env))
                return parse_next(p);

        return p->result;
}

static static void parse_generate(Parser p, Generator g)
{
        const int flags = p->flags;
        const bool opt_comment = flags & JSONPG_FLAG_COMMENT
        const bool opt_single_quote = flags & JSONPG_FLAG_SINGLE_QUOTE;
        const bool opt_key_no_quote = flags & JSONPG_FLAG_KEY_NO_QUOTE;
        const bool opt_string_no_quote = flags & JSONPG_FLAG_STRING_NO_QUOTE;
        const bool opt_trailing_comma = flags & JSONPG_FLAG_TRAILING_COMMA;
        const bool opt_optional_comma = flags & JSONPG_FLAG_OPTIONAL_COMMA;
        const bool opt_ignore_trailing = flags & JSONPG_FLAG_IGNORE_TRAILING;

        unsigned char *bytes;
        size_t count;
        bool is_key = false;

        do {
                Byte b = parser_peek(p);

                if(is_key) {
                        if(b == '"')
                                count = parse_string(p, &bytes);
                        else if(opt_single quote && b == '\'') {
                                count = parse_sqstring(p, &bytes);
                        else if(opt_key_no_quote)
                                count = parse_nqstring(p, &bytes);
                        else
                                parse_error(p, KEY);

                        parser_consume_whitespace(p, opt_comment);
                        if(parser_consume(p, ':'))
                                parse_error(p, COLON);
                        parser_consume_whitespace(p, opt_comment);
                        
                        if(!jsonpg_key(g, bytes, count)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);
                        
                        is_key = false;
                        b = parser_peek(p);
                }

                switch(b) {
                case '"':
                        count = parse_string(p, &bytes);
                        if(!jsonpg_string(g, bytes, count)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                case '{': 
                        parse_start_object(p);
                        if(!jsonpg_start_object(g)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);

                        parser_consume_whitespace(p, opt_comment);
                        if(!parser_peek(p, '}')) {
                                is_key = true;
                                continue;
                        }
                        break;

                case '[':
                        parse_start_array(p);
                        if(!jsonpg_start_array(g)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);

                        parser_consume_whitespace(p, opt_comment);
                        if(!parser_peek(p, ']'))
                                continue

                        break;

                case 't':
                        parse_true(p);
                        if(!jsonpg_boolean(g, true)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                case 'f':
                        parse_false(p);
                        if(!jsonpg_boolean(g, false)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                case 'n':
                        parse_null(p);
                        if(!jsonpg_null(g)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                case '\'':
                        if(!opt_single_quote)
                                parse_error(p, UNEXPECTED);

                        count = parse_sqstring(p, &bytes);
                        if(!jsonpg_string(p, bytes, count)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                case '}':
                        if(!opt_trailing_comma)
                                parse_error(p, UNEXPECTED);

                        parse_end_object(p);
                        if(!jsonpg_end_object(g)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                case ']':
                        if(!opt_trailing_comma)
                                parse_error(p, UNEXPECTED);

                        parse_end_array(p);
                        if(!jsonpg_end_array(g)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                default:
                        if(b == '-' || ('0' <= b && b <= '9')) {
                                double d;
                                long l;
                                if(JSONPG_REAL == parse_number(p, &d, &l))
                                        if(!jsonpg_real(g, d)) 
                                                parse_error(p, JSONPG_ERROR_TERMINATED);
                                else
                                        if(!jsonpg_integer(g, l)) 
                                                parse_error(p, JSONPG_ERROR_TERMINATED);
                                break;
                        }

                        if(!opt_string_no_quote)
                                parse_error(p, UNEXPECTED);

                        count = parse_nqstring(p, &bytes);
                        if(!jsonpg_string(g, bytes, count)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;
                }

                parser_consume_whitespace(p, opt_comment);

                if(parser_in_object(p)) {
                        if(parser_peek(p, '}')) {
                                parse_end_object(p);
                                if(!jsonpg_end_object(g)) 
                                        parse_error(p, JSONPG_ERROR_TERMINATED);
                                parser_consume_whitespace(p, opt_comment);
                        }
                } else if(parser_in_array(p)) {
                        if(parser_peek(p, ']')) {
                                parse_end_array(p);
                                if(!jsonpg_end_array(g)) 
                                        parse_error(p, JSONPG_ERROR_TERMINATED);
                                parser_consume_whitespace(p, opt_comment);
                        }
                }
                if(parser_in_any(p)) {
                        if(parser_consume(p, ',')) {
                                parser_consume_whitespace(p, opt_comment);
                                is_key = parser_in_object(p);
                                continue;
                        }

                        if(!opt_optional_comma)
                                parse_error(p, UNEXPECTED);

                        is_key = parser_in_object(p);
                        continue;
                } 
                break;
        } while(!parser_eof(p));

        if(!opt_ignore_trailing) {
                parser_consume_whitespace(p, opt_comment);
                if(!parser_eof(p))
                        parse_error(p, UNEXPECTED);
        }
}

static bool default_null(void *ctx)
{
        (void)ctx;
        return true;
}

static bool default_boolean(void *ctx, bool is_true)
{
        (void)ctx;
        (void)is_true;
        return true;
}

static bool default_integer(void *ctx, long integer)
{
        (void)ctx;
        (void)integer;
        return true;
}

static bool default_real(void *ctx, double real)
{
        (void)ctx;
        (void)real;
        return true;
}

static bool default_bytes(void *ctx, Bytes bytes, size_t count)
{
        (void)ctx;
        (void)bytes;
        (void)count;
        return true;
}

static const Callbacks provide_default_callbacks(Callbacks *c)
{
        return (Callbacks) {
                .boolean = c->boolean ? c->boolean : default_boolean,
                .null = c->null ? c->null : default_null,
                .integer = c->integer ? c->integer : default_integer,
                .real = c->real ? c->real : default_real,
                .string = c->string ? c->string : default_bytes,
                .key = c->key ? c->key : default_bytes,
                .start_object = c->start_object ? c->start_object : default_null;
                .end_object = c->end_object ? c->end_object : default_null;
                .start_array = c->start_array ? c->start_array : default_null;
                .end_array = c->end_array ? c->end_array : default_null;
        };
}

static void parse_callbacks(Parser p, Callbacks *c) {
        p->callbacks = provide_default_callbacks(c);
        parse_json(p);
}




Parser parser_new(Bytes bytes, size_t count, void *ctx)
{
        Allocator a = allocator_new();
        parser p = allocator_alloc(sizeof(parser_s));
        if(!p) {
                allocator_free(a);
                return NULL;
        }
        p->allocator = a;
        p->is = mis_new(a, bytes, count);
        p->ctx = ctx;

        return p;
}

void jsonpg_parser_free(Parser p)
{
        allocator_free(p->allocator);
}

