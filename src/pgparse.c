
#define JSONPG_PARSE_ERROR(P, C)        parse_error((P), JSONPG_ERROR_##C, )
#define JSPONG_ERROR_TRAILING_COMMA 1

typedef unsigned char Byte;
typedef unsigned char *Bytes;

void parse_error(JsonpgParser p, unsigned error_code)
{
        p->error = (JsonpgError) {
                .code = error_code,
                .at = p->offset
        };
        longjmp(p->env);
}

bool parse_comma(p)
{
        bool seen_comma = false;
        if(input_peek(p) == ',') {
                input_take(p);
#ifndef JSON_FLAG_TRAILING_COMMAS
                return true;
#else
                seen_comma = true;
#endif
        }

#ifdef JSONPG_FLAG_OPTIONAL_COMMAS
        if(!input_eof(p) && !seen_comma)
                return true;
#endif

#ifndef JSONPG_FLAG_TRAILING_COMMAS
        if(seen_comma)
                JSONPG_PARSE_ERROR(p, TRAILING_COMMA);
#endif
        return false;
}


void parse_true(JpgParser p)
{
        JSONPG_ASSERT(input_peek(p) == 't');

        input_take(p);
        if(!input_consume(p, 'r')
                        || !input_consume(p, 'u')
                        || !input_comsume(p, 'e'))
                parse_error(p, UNEXPECTED);
}

void parse_false(JpgParser p)
{
        JSONPG_ASSERT(input_peek(p) == 'f');

        input_take(p);
        if(!input_consume(p, 'a')
                        || !input_consume(p, 'l')
                        || !input_consume(p, 's')
                        || !input_comsume(p, 'e'))
                parse_error(p, UNEXPECTED);
}

void parse_null(JpgParser p)
{
        JSONPG_ASSERT(input_peek(p) == 'n');

        input_take(p); // 'n'
        if(!input_consume(p, 'u')
                        || !input_consume(p, 'l')
                        || !input_comsume(p, 'l'))
                parse_error(p, UNEXPECTED);
}

unsigned parse_hex4(JpgParser p, size_t esc_offset)
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

unsigned parse_escape(JpgParser p)
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

MemoryInputStream mis_new(Allocator a, Bytes bytes, size_t count)
{
        MemoryInputStream is = memory_allocate(a,
                                        sizeof(struct memory_input_stream_s));
        if(!mis)
                return NULL;
        is->bytes = bytes;
        is->count = count;
        is->current = 0;
}

size_t mis_tell(MemoryInputStream mis)
{
        JSONPG_ASSERT(mis);

        return mis->current;
}

Bytes mis_at(MemoryInputStream mis, size_t pos)
{
        JSONPG_ASSERT(mis);

        if(pos >= mis->count)
                return NULL;

        return mis->bytes + pos;
}

size_t mis_length(MemoryInputStream mis)
{
        JSONPG_ASSERT(mis);

        return mis->count;
}

bool mis_eof(MemoryInputStream mis)
{
        JSONPG_ASSERT(mis);
        JSONPG_ASSERT(mis->current <= mis->count);

        return mis->current == mis->count;
}

Byte mis_peek(MemoryInputStream mis)
{
        JSONPG_ASSERT(mis);

        if(mis_eof(mis))
                return '\0';

        return mis->bytes[mis->current];
}

Byte mis_take(MemoryInputStream mis)
{
        JSONPG_ASSERT(mis);

        if(mes_eof(mis))
                return '\0';

        return mis->bytes[mis->current++];
}

bool mis_consume(MemoryInputStream mis, Byte b)
{
        JSONPG_ASSERT(mis);

        if(mis_peek(mis) != b)
                return false;

        mis_take(mis);
        return true;
}

void consume_whitespace(Parser p)
{
        JSONPG_ASSERT(p);

        MemoryInputStream is = p->is;

        Bytes c;

#ifdef JSONPG_FLAG_COMMENTS
        while(true) {
#endif

        while((c = mis_peek(is)) == ' ' || c == '\n' || c == '\r' || c== '\t')
                mis_take(is);

#ifdef JSONPG_FLAG_COMMENTS
        if(mis_consume(is, '/')) {
                if(mis_consume(is, '*')) {
                        while(true) {
                                if(mis_peek(is) == '\0') {
                                        return;
                                } else if(mis_consume(is, '*')) {
                                        if(mis_consume(is, '/'))
                                                break;
                                } else
                                        mis_take(is);
                        }
                } else if(mis_consume(is, '/')) {
                        while(mis_peek(is) != '\0' && mis_take(mis) != '\n')
                                ;
                } else {
                        return;
                }
                continue;
        } else {
                break;
        }
#endif
}

MemoryOutputStream mos_new(allocator a)
{
        JSONPG_ASSERT(a);

        MemoryOutputStream os = memory_allocate(a, 
                                        sizeof(struct memory_output_stream_s));
        if(!os)
                return NULL;
        os->allocator = a
        os->capacity = 0;
        os->count = 0;
        os->buffer = NULL;

        return os;
}

size_t mos_length(MemoryOutputStream os)
{
        JSONPG_ASSERT(os);

        return os->count;
}

Bytes mos_pop(MemoryOutputStream os)
{
        JSONPG_ASSERT(os);

        os->count = 0;
        return os->buffer;
}

void mos_advance(MemoryOutputStream os, size_t amount)
{
        JSONPG_ASSERT(os);

        os->count += amount;
}

Bytes mos_grow(MemoryOutputStream os, size_t incr)
{
        JSONPG_ASSERT(os);

        size_t size = os->capacity 
                ? os->capacity << 1
                : JSONPG_DEFAULT_BUFFER_SIZE;

        size_t required = os->capacity + incr;

        while(size < required)
                size << 1;

        Bytes new;
        if(os->buffer)
                new = memory_reallocate(os->allocator, buffer, size);
        else
                new = memory_allocate(os->allocator, size);

        if(new) {
                os->buffer = new;
                os->capacity = size;
        }

        return new;
}


Bytes mos_reserve(MemoryOutputStream os, size_t count)
{
        JSONPG_ASSERT(os);

        if(count > os->capacity - os->count)
                if(!mos_grow(os, count))
                        return NULL;
        return os->buffer + os->count;
}



size_t cow_length(CowStream cs)
{
        JSONPG_ASSERT(cs);

        return cs->copied
                ? mos_length(cs->os)
                : input_length(cs->is) - cs->ptr;
}

Bytes cow_pop(CowStream cs)
{
        JSONPG_ASSERT(cs);

        return cs->copied
                ? mos_pop(cs->os)
                : input_at(cs->is, cs->ptr);
}

void cow_advance(CowStream cs, size_t count)
{
        JSONPG_ASSERT(cs);

        mos_advance(cs->os, count);
        cs->is_ptr = input_tell(cs->is);
}

Bytes cow_reserve(CowStream cs, size_t count)
{
        JSONPG_ASSERT(cs);

        cs->copied = true;
        MemoryOutputStream os = cs->os;
        InputStream is = cs->is;
        size_t to_copy = input_tell(is) - cs->ptr;
        Bytes s = mos_reserve(os, to_copy + count);
        if(!s)
                return NULL;

        memcpy(s, input_at(cs->is, cs->is_ptr), to_copy);
        mos_advance(os, to_copy);
        return s;
}

bool cow_finalize(CowStream cs)
{
        JSONPG_ASSERT(cs);

        return cs->copied
                ? cow_reserve(cs, 0)
                : true;
}

void parse_string_to_cow(JpgParser p, CowStream cs)
{
        while(true) {
                //copy_safe_chars(p->is, os);
                
                unsigned char c = input_peek(p);
                if(c == '\\') {
                        unsigned codepoint = parse_escape(p);
                        Bytes s = cow_reserve(cs, 4);
                        if(!s)
                                parse_error(p, MEMORY);
                        int count = utf8_encode(codepoint, s);
                        if(count == -1)
                                parse_error(p, UTF8);
                        cow_advance(cs, count);
                } else if(c == '"') {
                        if(!cow_finalize(cs))
                                parse_error(p, MEMORY);
                        input_take(p);
                        return;
                } else if(c < 0x20) {
                        parse_error(p, INVALID);
                } else if(c >= 0x80) {
                        if(-1 == utf8_sequence_length(p))
                                parse_error(p, UTF8);
                } else {
                        input_take(p);
                }
        }
}

size_t parse_string(JpgParser p, Bytes *bytes)
{
        JSONPG_ASSERT(p);
        JSONPG_ASSERT(input_peek(p) == '"');

        input_take(p); // "

        CowStream cs = cow_new(p);
        if(!cs)
                return 0;

        parse_string_to_cow(p, cs);
        size_t length = cow_length(cs);
        Bytes str = cow_pop(cs);
        *bytes = str;
        return length;
}

double pow_10(unsigned n) {
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

double fast_path(double d, int e)
{
        if(e < -308)
                return 0.0;
        else if(e >= 0)
                return d * pow_10(e);
        else
                return d / pow_10(-e);
}


double strtod_normal(double d, int e) 
{
        if(e < -308) {
                d = fast_path(d, -308);
                d = fast_path(d, e + 308);
        } else {
                d = fast_path(d, e);
        }
        return d;
}

bool parse_number(JsgParser p, *double real, *long integer)
{
        bool force_double = false;
        bool overflow = false;
        sign = 1;
        int64_t sum, new_sum;
        int exponent = 0;
        unsigned char c = input_take(p);

        if(c == '-') {
                sign = -1;
                c = input_take(p);
        }
        if(c >= '0' && c <= '9')
                sum = c - '0';
        else
                parse_error(p, NUMBER);

        c = input_take(p);
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
                        c = input_take(p);
                }
        }
        if(c == '.') {
                force_double = true;

                c = input_take(p);
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
                        c = input_take(p);
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
                        c = input_take(p);
                }
        }
        if(c == 'e' || c == 'E') {
                force_double = true;
                int exp_sign = 1;
                int exp = 0;

                c = input_take(p);
                if(c == '-') {
                        exp_sign = -1;
                        c = input_take(p);
                } else if(c == '+') {
                        c = input_take(p);
                }
                if(c >= '0' && c <='9') {
                        exp = 10 * exp + c - '0';
                        c = input_take(p);
                        while(c >= '0' && c <= '9') {
                                exp = 10 * exp + c - '0';
                                if(exp < 0)
                                        parse_error(p, NUMBER);
                                c = input_take(p);
                        }
                } else {
                        parse_error(p, NUMBER);
                }
                exponent += exp_sign * exp;
        }

        if(force_double || overflow) {
                *real = sign * strtod((double)sum, exponent);
                return true;
        } else {
                *integer = sign * sum;
                return false;
        }
}
        

void parse_value(JpgParser p, Nesting nesting)
{
        Bytes bytes;
        size_t count;
        int initial_level = nesting->level;
        bool is_key = nesting->type == JSONPG_OBJECT;

        do {
                Byte b = input_peek(p);

                if(is_key) {
                        if(b == '"')
                                count = parse_string(p, &bytes);
#ifdef JSONPG_FLAG_SINGLE_QUOTE
                        else if(b == '\'') {
                                count = parse_squote_string(p, &bytes);
#endif
#ifdef JSONPG_FLAG_KEY_NO_QUOTE
                        else
                                count = parse_nquote_string(p, &bytes);
#else
                        else
                                parse_error(p, KEY);
#endif
                        output_key_bytes(p, bytes, count);
                        consume_whitespace(p);
                        if(input_consume(p, ':'))
                                parse_error(p, COLON);
                        consume_whitespace(p);

                        is_key = false;
                        b = input_peek(p);
                }

                switch(b) {
                case '"':
                        count = parse_string(p, &bytes);
                        output_string_bytes(p, bytes, count);
                        break;
                case '{': 
                        input_take(p);
                        consume_whitespace(p);
                        nesting = nesting_push(p, JSONPG_OBJECT);
                        output_start_object(p);
                        if(!input_consume(p, '}')) {
                                consume_whitespace(p);
                                is_key = true;
                                continue;
                        }
                        output_end_object(p);
                        nesting = nesting_pop(p);
                        break;
                case '[':
                        input_take(p);
                        consume_whitespace(p);
                        nesting = nesting_push(p, JSONPG_ARRAY);
                        output_start_array(p);
                        if(!input_consume(p,']')) {
                                consume_whitespace(p);
                                continue;
                        }
                        output_end_array(p);
                        nesting = nesting_pop(p);
                        break;
                case 't':
                        parse_true(p);
                        output_boolean(p, true);
                        break;
                case 'f':
                        parse_false(p);
                        output_boolean(p, false)
                        break;
                case 'n':
                        parse_null(p);
                        output_null(p);
                        break;

#ifdef JSONPG_FLAG_SINGLE_QUOTE
                case '\'':
                        count = parse_squote_string(p, &bytes);
                        output_string_bytes(p, bytes, count);
                        break;
#endif
#ifdef JSONPG_FLAG_TRAILING_COMMA
                case '}':
                        input_take(p);
                        output_end_object(p);
                        nesting = nesting_pop(p);
                        break;
                case ']':
                        input_take(p);
                        output_end_array(p);
                        nesting = nesting_pop(p);
                        break;
#endif
                default:
                        if(b == '-' || ('0' <= b && b <= '9')) {
                                double d;
                                long l;
                                if(parse_number(p, &d, &l))
                                        output_real(p, d);
                                else
                                        output_integer(p, l)
                                break;
                        }

#ifdef JSONPG_STRING_NO_QUOTE
                        count = parse_nquote_string(p, &bytes);
                        output_string_bytes(p, bytes, count);
                        break;
#else
                        parse_error(p, UNEXPECTED);
#endif
                }

                consume_whitespace(p);

                if(nesting->type == JSONPG_OBJECT) {
                        if(input_consume(p, '}')) {
                                consume_whitespace(p);
                                output_end_object(p);
                                nesting = nesting_pop(p);
                        }
                } else if(nesting->type == JSONPG_ARRAY) {
                        if(input_consume(p, ']')) {
                                consume_whitespace(p);
                                output_end_array(p);
                                nesting = nesting_pop(p);
                        }
                }
                if(nesting->level > initial->level) {
                        if(input_consume(p, ',')) {
                                consume_whitespace(p);
                                is_key = nesting->type == JSONPG_OBJECT;
                                continue;
                        }
#ifdef JSONPG_FLAG_OPTIONAL_COMMA
                        is_key = nesting->type == JSONPG_OBJECT;
                        continue;
#endif
                        parse_error(p, UNEXPECTED);
                } 

                return;

        } while(!input_eof(p));
}




void parse_json(JsonpgParser p)
{
        Nesting nesting = nesting_new(p);

#if defined(JSONPG_FLAG_IS_OBJECT)
        nesting_push(p, JSONPG_OBJECT)
        begin_object(p);
        do {
                parse_value(p, nesting);
        } while(parse_comma(p));
        end_object(p, element_count);
        nesting_pop(p);

#elif defined(JSONPG_FLAG_IS_ARRAY)
        nesting_push(p, JSONPG_ARRAY)
        begin_array(p);
        do {
                parse_value(p, nesting);
        } while(parse_comma(p));
        end_array(p, element_count);
        nesting_pop(p);
        
#else
        parse_value(p, nesting);

#endif

#ifndef JSONPG_FLAG_IGNORE_TRAILING_CONTENT
        if(!parse_eof(p))
                parse_error(p, UNEXPECTED);
#endif

}
        
Parser parser_new(Bytes bytes, size_t count, Generator g)
{
        Allocator a = allocator_new();
        Parser p = allocator_alloc(sizeof(parser_s));
        if(!p) {
                allocator_free(a);
                return NULL;
        }
        p->allocator = a;
        p->is = mis_new(a, bytes, count);
        p->g = g;

        return p;
}

ParseResult parse(Bytes bytes, size_t count, Generator g)
{
        Parser p = parser_new(bytes, count, g);
        if(!p)
                return NULL;

        if(0 == setjmp(p->env))
                parse_json(p);

        ParseResult result = p->result;
        parser_free(p);

        return result;
}





