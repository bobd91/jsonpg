/*
 * These functions provide the pull parse functionality.
 *
 * As a pull parser returns one JSON item at a time it needs to keep track of 
 * where it is up to so that it can resume when next called.
 *
 * The stack mechanism used in the normal parse is used to keep track of the 
 * nesting of arrays/objects but we need additional information to be able to resume. 
 * This is stored in the parser .state member and can have the following values:
 *
 * STATE_START          - at the start of the parse
 * STATE_OBJECT         - in object, expecting key
 * STATE_KEY            - in object after key:
 * STATE_KEY_VALUE      - in object after key:value (but before ,)
 * STATE_ARRAY          - in array, expecting value
 * STATE_ARRAY          - in array after value (but before ,)
 * STATE_DONE           - parse complete
 * STATE_EOF            - parse at end of input
 *
 * There are two more states which are only used during a single
 * parse step and so never get stored in parser->state
 *
 * STATE_OBJECT_COMMA   - in object after ,
 * STATE_ARRAY_COMMA    - in array after , 
 */ 

static inline ParseState state_change_value(ParseState state)
{
        if(state == STATE_START) return STATE_DONE;
        if(state == STATE_KEY) return STATE_KEY_VALUE;
        if(state == STATE_ARRAY || state == STATE_ARRAY_VALUE) return STATE_ARRAY_VALUE; 
        assert(false && "Invalid state");
}

static inline ParseState state_change_end(Parser p)
{
        if(parser_in_object(p)) return STATE_KEY_VALUE; 
        if(parser_in_array(p)) return STATE_ARRAY_VALUE; 
        return STATE_DONE;
}

static inline JsonType accept_boolean(Parser p, bool is_true)
{
        p->state = state_change_value(p->state);
        p->result = parse_result(p,
                         is_true ? JSONPG_TRUE : JSONPG_FALSE);
        return p->result.type;
}

static inline JsonType accept_null(Parser p)
{
        p->state = state_change_value(p->state);
        p->result = parse_result(p, JSONPG_NULL);
        return p->result.type;
}

static inline JsonType accept_integer(Parser p, long integer)
{
        p->state = state_change_value(p->state);
        p->result = parse_result(p, JSONPG_INTEGER, integer);
        return p->result.type;
}

static inline JsonType accept_real(Parser p, double real)
{
        p->state = state_change_value(p->state);
        p->result = parse_result(p, JSONPG_REAL, real);
        return p->result.type;
}

static inline JsonType accept_string(Parser p, Bytes bytes, size_t count)
{
        p->state = state_change_value(p->state);
        p->result = parse_result(p, JSONPG_STRING, bytes, count);
        return p->result.type;
}

static inline JsonType accept_key(Parser p, Bytes bytes, size_t count)
{
        p->state = STATE_KEY;
        p->result = parse_result(p, JSONPG_KEY, bytes, count);
        return p->result.type;
}

static inline JsonType accept_start_object(Parser p)
{
        p->state = STATE_OBJECT;
        p->result = parse_result(p, JSONPG_START_OBJECT);
        return p->result.type;
}

static inline JsonType accept_end_object(Parser p)
{
        p->state = state_change_end(p);
        p->result = parse_result(p, JSONPG_END_OBJECT);
        return p->result.type;
}

static inline JsonType accept_start_array(Parser p)
{
        p->state = STATE_ARRAY;
        p->result = parse_result(p, JSONPG_START_ARRAY);
        return p->result.type;
}

static inline JsonType accept_end_array(Parser p)
{
        p->state = state_change_end(p);
        p->result = parse_result(p, JSONPG_END_ARRAY);
        return p->result.type;
}

static inline JsonType accept_eof(Parser p)
{
        p->result = parse_result(p, JSONPG_EOF);
        return p->result.type;
}

static JsonType parse_next(Parser p)
{
        const MemoryInputStream mis = p->mis;
        const bool opt_comments = p->flags & JSONPG_FLAG_COMMENTS;
        ParseState state = p->state;
        unsigned char *bytes;
        size_t count;
        
        Byte b = consume_whitespace(p, opt_comments);

        if(state == STATE_EOF)
                throw_parse_error(p, JSONPG_ERROR_EOF);

        while(true) {
                // States that are not just expecting values
                switch(state) {

                case STATE_KEY_VALUE:
                        if(b == '}') {
                                parse_end_object(p);
                                return accept_end_object(p);
                        } else if(b == ',') {
                                mis_take(mis);
                                b = consume_whitespace(p, opt_comments);
                        } else if(!(p->flags & JSONPG_FLAG_OPTIONAL_COMMAS)) {
                                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                        }

                        if(!(p->flags & JSONPG_FLAG_TRAILING_COMMAS)) {
                                state = STATE_OBJECT_COMMA;
                                continue;
                        }

                        state = STATE_OBJECT;
                        // fallthrough 

                case STATE_OBJECT:
                        if(b == '}') {
                                parse_end_object(p);
                                return accept_end_object(p);
                        }
                        
                        state = STATE_OBJECT_COMMA;
                        // fallthrough

                case STATE_OBJECT_COMMA:
                        if(b == '"') {
                                count = parse_string(p, &bytes);
                        } else if((p->flags & JSONPG_FLAG_SINGLE_QUOTES) && b == '\'') {
                                count = parse_sqstring(p, &bytes);
                        } else if(p->flags & JSONPG_FLAG_UNQUOTED_KEYS) {
                                count = parse_nqstring(p, &bytes);
                        } else {
                                throw_parse_error(p, JSONPG_ERROR_EXPECTED_KEY);
                        }

                        b = consume_whitespace(p, opt_comments);
                        if(b != ':')
                                throw_parse_error(p, JSONPG_ERROR_EXPECTED_KEY);
                        
                        mis_take(mis); // ':'
                        return accept_key(p, bytes, count); 

                case STATE_ARRAY_VALUE:
                        if(b == ']') {
                                parse_end_array(p);
                                return accept_end_array(p);
                        } else if(b == ',') {
                                mis_take(mis);
                                b = consume_whitespace(p, opt_comments);
                        } else if(!(p->flags & JSONPG_FLAG_OPTIONAL_COMMAS)) {
                                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                        }
                        
                        if(!(p->flags & JSONPG_FLAG_TRAILING_COMMAS)) {
                                state = STATE_ARRAY_COMMA;
                                break;
                        }

                        state = STATE_ARRAY;

                        // fallthrough

                case STATE_ARRAY:
                        if(b == ']') {
                                parse_end_array(p);
                                return accept_end_array(p);
                        }
                        break;

                case STATE_DONE:
                        consume_whitespace(p, opt_comments);
                        if(!mis_eof(mis)) {
                                if(p->flags & JSONPG_FLAG_MULTIPLE_VALUES) {
                                        state = STATE_START;
                                        break;
                                }

                                if(!(p->flags & JSONPG_FLAG_IGNORE_TRAILING_CHARS))
                                        throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                        }
                        p->state = STATE_EOF;
                        return accept_eof(p);

                default:
                        // Handle other states below
                }

                // state in START, KEY, ARRAY, ARRAY_COMMA
                // just expecting a value, { or [

                assert(state == STATE_START 
                                || state == STATE_KEY 
                                || state == STATE_ARRAY 
                                || state == STATE_ARRAY_COMMA);

                switch(b) {
                case '"':
                        count = parse_string(p, &bytes);
                        return accept_string(p, bytes, count); 

                case '{': 
                        parse_start_object(p);
                        return accept_start_object(p);

                case '[':
                        parse_start_array(p);
                        return accept_start_array(p);

                case 't':
                        parse_true(p);
                        return accept_boolean(p, true);

                case 'f':
                        parse_false(p);
                        return accept_boolean(p, false);

                case 'n':
                        parse_null(p);
                        return accept_null(p);

                case '\'':
                        if(!(p->flags & JSONPG_FLAG_SINGLE_QUOTES))
                                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);

                        count = parse_sqstring(p, &bytes);
                        return accept_string(p, bytes, count);

                default:
                        if(b == '-' || ('0' <= b && b <= '9')) {
                                double d;
                                long l;
                                if(JSONPG_REAL == parse_number(p, &d, &l)) {
                                        return accept_real(p, d);
                                } else {
                                        return accept_integer(p, l);
                                }
                        }

                        if(!(p->flags & JSONPG_FLAG_UNQUOTED_STRINGS))
                                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);

                        count = parse_nqstring(p, &bytes);
                        return accept_string(p, bytes, count);
                }
        }

}

static inline JsonType parser_parse_next(Parser p)
{
        if(0 == setjmp(p->env))
                return parse_next(p);

        return p->result.type;
}

JsonType jsonpg_parse_next(Parser p)
{
        if(p->mis->start)
                return parser_parse_next(p);
        else
                return dom_parse_next(p);
}

