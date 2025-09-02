
#define JSONPG_PARSE_ERROR(P, C)        parse_error((P), JSONPG_ERROR_##C)
#define JSPONG_ERROR_TRAILING_COMMA 1

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



void parse_element(JspParser p)
{
        consume_whitespace(p);
        parse_value(p);
        consume_whitespace(p);
}

void parse_member(JspParser p)
{
        consume_whitespace(p);
        parse_string(p, true);
        consume_whitespace(p);
        if(!input_peek(p) == ':')
                parser_error(p, COLON);
        input.take(p);
        parse_element(p);
}

void parse_object(JpgParser p)
{
        input_take(p); // '{'

        unsigned element_count = 0;

        output_begin_object(p->g);
        consume_whitespace(p);
        while(input_peek(p) != '}') {
                parse_member(p);
                element_count++;
                if(input_peek(p) == ',') {
                        input_take(p);
                        consume_whitespace(p);
                        continue;
                }
#ifdef JSONPG_FLAG_OPTIONAL_COMMA
                if(!input_eof(p))
                        continue;
#endif
                break;
        }
#ifndef
        output_end_object(p->g, element_count);
}

void parse_array(JpgParser p)
{
        input_take(p); // '['

        unsigned element_count = 0;

        output_begin_array(p->g);
        consume_whitespace(p);
        while(input_peek(p) != ']') {
                parse_element(p);
                element_count++;
                if(input_peek(p) == ',') {
                        input_take(p);
                        consume_whitespace(p);
                        continue;
                }
#ifdef JSONPG_FLAG_OPTIONAL_COMMA
                if(!input_eof(p))
                        continue;
#endif
                break;
        }
#ifndef
        output_end_array(p->g, element_count);
}

void parse_true(JpgParser p)
{
        input_take(p); // 't'
        if(input_consume(p, 'r')
                        && input_consume(p, 'u')
                        && input_comsume(p, 'e'))
                output_bool(p->g, true);
        else
                parse_error(p, UNEXPECTED);
}

void parse_false(JpgParser p)
{
        input_take(p); // 'f'
        if(input_consume(p, 'a')
                        && input_consume(p, 'l')
                        && input_consume(p, 's')
                        && input_comsume(p, 'e'))
                output_bool(p->g, true);
        else
                parse_error(p, UNEXPECTED);
}

void parse_null(JpgParser p)
{
        input_take(p); // 'n'
        if(input_consume(p, 'u')
                        && input_consume(p, 'l')
                        && input_comsume(p, 'l'))
                output_bool(p->g, true);
        else
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


void copy_safe_chars(InputStream is, StackStream os)
{
#if defined(JSONPG_SSE2) || defined(JSONPG_SSE42)
        // rapidjson/include/rapidjson/reader.h#ScanCopyUnescapedString
        // but not set up for testing SSE ATM
#elif defined(JSONPG_NEON)
        // rapidjson/include/rapidjson/reader.h#ScanCopyUnescapedString
        // but not set up for testing NEON ATM
#endif
        // does nothing if no SIMD available
}

void parse_string_to_stack(JpgParser p, StackStream os)
{
        while(true) {

                copy_safe_chars(p->is, os);
                
                unsigned char c = input_peek(p);
                if(c == '\\') {
                        unsigned codepoint = parse_escape(p);
                        stack_stream_codepoint(os, p, codepoint);
                } else if(c == '"') {
                        input_take(p);
                        break;
                } else if(c < 0x20) {
                        parse_error(p, INVALID);
                } else {
                        int bytes validate_utf8_sequence(p);
                        stack_stream_put_n(os, p);
                }
        }
        stack_stream_emit(p);
}

void parse_string(JpgParser p, bool is_key)
{
        input_take(p); // "

        StackStream os = stack_stream_new(p);
        parse_string_to_stack(p, os);
        size_t length = stack_stream_length(os);
        ByteString str = stack_stream_pop(os);
        if(is_key)
                output_key_bytes(p->g, str, length);
        else
                output_string_bytes(p->g, str, length);
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

void parse_number(JsgParser p, bool is_positive)
{
        size_t number_offset = input_tell(p) - is_positive ? 0 : 1;
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
                parse_error(p, NUMBER, number_offset);

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
                        parse_error(p, NUMBER, number_offset);

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
                                        parse_error(p, NUMBER, number_offset);
                                c = input_take(p);
                        }
                } else {
                        parse_error(p, NUMBER, number_offset);
                }
                exponent += exp_sign * exp;
        }

        if(!force_double && exponent >= 0) {
                output_integer(p->g, sign * sum);
        } else {
                output_real(p->g, sign * strtod_normal((double)sum, exponent));
        }
}
        

void parse_value(JpgParser p)
{
        switch(input_peek(p)) {
        case '"': 
                parse_string(p, false); 
                return;
        case '{': 
                parse_object(p);
                return;
        case '[':
                parse_array(p);
                return;
        case 't':
                parse_true(p);
                return;
        case 'f':
                parse_false(p);
                return;
        case 'n':
                parse_null(p);
                return;
        case '-':
                parse_negative_number(p);
                return;
        case '0':
                parse_zero_number(p, true);
                return;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
                parse_number(p, true);
                return;
#ifdef JSONPG_FLAG_SINGLE_QUOTE_STRING
        case '\'':
                parse_squote_string(p, false);
                return;
#endif
        default:
#ifdef JSONPG_NO_QUOTE_STRING)
                parse_nquote_string(p, false);
                return;
#else
        }
        parse_error(p, UNEXPECTED);
}

void parse_json(JsonpgParser p)
{

#if defined(JSONPG_FLAG_IS_OBJECT)
        unsigned element_count = 0;

        begin_object(g);
        do {
                parse_member(p);
                element_count++;
        } while(parse_comma(p));
        end_object(g, element_count);

#elif defined(JSONPG_FLAG_IS_ARRAY)
        unsigned element_count = 0;

        begin_array(g);
        do {
                parse_element(p);
                element_count++;
        } while(parse_comma(p));
        end_array(g, element_count);
        
#else
        parse_element(p);

#endif

#ifndef JSONPG_FLAG_TRAILING_CONTENT
        if(!parse_eof(p))
                parse_error(p, UNIDENTIFIED);
#endif

}
        
JpgParseResult parse(JpgByteStream is, JpgGenerator g)
{
        Parser p = parser_new(is, g);
        if(!p)
                return NULL;

        if(0 == setjmp(p->env))
                parse_json(p);

        ParseResult result = p->result;
        parser_free(p);

        return result;
}





