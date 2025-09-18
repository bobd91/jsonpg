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

static inline parse_state state_change_value(parse_state state)
{
        if(state == STATE_START) return STATE_DONE;
        if(state == STATE_KEY) return STATE_KEY_VALUE;
        if(state == STATE_ARRAY || state == STATE_ARRAY_VALUE) return STATE_ARRAY_VALUE; 
        assert(false && "Invalid state");
}

static inline parse_state state_change_end(parser *p)
{
        if(parser_in_object(p)) return STATE_KEY_VALUE; 
        if(parser_in_array(p)) return STATE_ARRAY_VALUE; 
        return STATE_DONE;
}

static inline json_type accept_boolean(parser *p, bool is_true)
{
        p->state = state_change_value(p->state);
        p->result = make_parse_result(p,
                         is_true ? JSONPG_TRUE : JSONPG_FALSE);
        return p->result.type;
}

static inline json_type accept_null(parser *p)
{
        p->state = state_change_value(p->state);
        p->result = make_parse_result(p, JSONPG_NULL);
        return p->result.type;
}

static inline json_type accept_integer(parser *p, long integer)
{
        p->state = state_change_value(p->state);
        p->result = make_parse_result(p, JSONPG_INTEGER, integer);
        return p->result.type;
}

static inline json_type accept_real(parser *p, double real)
{
        p->state = state_change_value(p->state);
        p->result = make_parse_result(p, JSONPG_REAL, real);
        return p->result.type;
}

static inline json_type accept_string(parser *p, byte *bytes, size_t count)
{
        p->state = state_change_value(p->state);
        p->result = make_parse_result(p, JSONPG_STRING, bytes, count);
        return p->result.type;
}

static inline json_type accept_key(parser *p, byte *bytes, size_t count)
{
        p->state = STATE_KEY;
        p->result = make_parse_result(p, JSONPG_KEY, bytes, count);
        return p->result.type;
}

static inline json_type accept_start_object(parser *p)
{
        p->state = STATE_OBJECT;
        p->result = make_parse_result(p, JSONPG_START_OBJECT);
        return p->result.type;
}

static inline json_type accept_end_object(parser *p)
{
        p->state = state_change_end(p);
        p->result = make_parse_result(p, JSONPG_END_OBJECT);
        return p->result.type;
}

static inline json_type accept_start_array(parser *p)
{
        p->state = STATE_ARRAY;
        p->result = make_parse_result(p, JSONPG_START_ARRAY);
        return p->result.type;
}

static inline json_type accept_end_array(parser *p)
{
        p->state = state_change_end(p);
        p->result = make_parse_result(p, JSONPG_END_ARRAY);
        return p->result.type;
}

static inline json_type accept_eof(parser *p)
{
        p->result = make_parse_result(p, JSONPG_EOF);
        return p->result.type;
}

static json_type parse_next(parser *p)
{
        memory_input_stream *const mis = p->mis;
        const bool opt_comments = p->flags & JSONPG_ALLOW_COMMENTS;
        const bool validate_utf8 = !(p->flags & JSONPG_ALLOW_INVALID_UTF8_IN);
        
        parse_state state = p->state;
        byte *bytes;
        size_t count;
        
        byte b = consume_whitespace(p, opt_comments);

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
                        }

                        if(!(p->flags & JSONPG_ALLOW_TRAILING_COMMAS)) {
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
                        if(b != '"')
                                throw_parse_error(p, JSONPG_ERROR_EXPECTED_KEY);

                        count = parse_string(p, &bytes, validate_utf8);

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
                        } else {
                                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                        }
                        
                        if(!(p->flags & JSONPG_ALLOW_TRAILING_COMMAS)) {
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
                                if(p->flags & JSONPG_ALLOW_MULTIPLE_VALUES) {
                                        state = STATE_START;
                                        break;
                                }

                                if(!(p->flags & JSONPG_ALLOW_TRAILING_CHARS))
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
                        count = parse_string(p, &bytes, validate_utf8);
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
                        throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                }
        }

}

static json_type parser_parse_next(parser *p)
{
        if(0 == setjmp(p->env))
                return parse_next(p);

        return p->result.type;
}

json_type jsonpg_parse_next(parser *p)
{
        if(p->mis->start)
                return parser_parse_next(p);
        else
                return dom_parse_next(p);
}

