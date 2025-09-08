

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

static jsonpg_value accept_boolean(Parser p, bool is_true);
{
        p->state = state_change_value(p->state);
        p->result.type = is_true ? JSONPG_TRUE : JSONPG_FALSE;
        return p->result;
}

static jsonpg_value accept_null(Parser p)
{
        p->state = state_change_value(p->state);
        p->result.type = JSONPG_NULL;
        return p->result;
}

static jsonpg_value accept_integer(Parser p, long integer)
{
        p->state = state_change_value(p->state);
        p->result.type = JSONPG_INTEGER;
        p->result.number.integer = integer;
        return p->result;
}

static jsonpg_value accept_real(Parser p, double real)
{
        p->state = state_change_value(p->state);
        p->result.type = JSONPG_REAL;
        p->result.number.real = real;
        return p->result;
}

static jsonpg_value accept_string(Parser p, Bytes bytes, size_t count)
{
        p->state = state_change_value(p->state);
        p->result.type = JSONPG_STRING;
        p->result.string.bytes = bytes;
        p->result.string.count = count;
        return p->result;
}

static jsonpg_value accept_key(Parser p, Bytes bytes, size_t count)
{
        p->state = STATE_KEY;
        p->result.type = JSONPG_KEY;
        p->result.string.bytes = bytes;
        p->result.string.count = count;
        return p->result;
}

static jsonpg_value accept_start_object(Parser p)
{
        p->state = STATE_OBJECT;
        p->result.type = JSONPG_START_OBJECT;
        return p->result;
}

static jsonpg_value accept_end_object(Parser p)
{
        p->state = state_change_end(p);
        p->result.type = JSONPG_END_OBJECT;
        return p->result;
}

static jsonpg_value accept_start_array(Parser p)
{
        p->state = STATE_ARRAY;
        p->result.type = JSONPG_START_ARRAY;
        return p->result;
}

static jsonpg_value accept_end_array(Parser p)
{
        p->state = state_change_end(p);
        p->result.type = JSONPG_END_ARRAY;
        return p->result;
}

static jsonpg_value parse_next(Parser p)
{
        const MemoryInputStream mis = p->mis;
        const bool opt_comment = p->flags & JSONPG_FLAG_COMMENT;
        ParserState state = p->state;
        unsigned char *bytes;
        size_t count;
        
        parser_consume_whitespace(p, opt_comment);
        Byte b = mis_peek(mis);

        if(state=STATE_EOF || b == '\0') {
                parse_error(p, JSONPG_ERROR_EOF);

        // States that are not just expecting values
        switch(state) {

        case STATE_KEY_VALUE:
                if(b == '}') {
                        parse_end_object(p);
                        return accept_end_object(p);
                } else if(b == ',') {
                        mis_take(mis);
                        parser_consume_whitespace(p, opt_comment);
                        b = mis_peek(mis);
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
                        return accept_end_object(p);
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
                if(mis_consume(mis, ':'))
                        parse_error(p, COLON);
                
                return accept_key(p, bytes, count); 

        case STATE_ARRAY_VALUE:
                if(b == ']') {
                        parse_end_array(p);
                        return accept_end_object(p);
                } else if(b == ",") {
                        mis_take(mis);
                        parser_consume_whitespace(p, opt_comment);
                        b = mis_peek(mis);
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
                        return accept_end_object(p);
                }
                break;

        case STATE_DONE:
                if(!(p->flags & JSONPG_FLAG_IGNORE_TRAILING)) {
                        parser_consume_whitespace(p, opt_comment);
                        if(!mis_eof(mis))
                                parse_error(p, JSONPG_ERROR_UNEXPECTED);
                }
                p->state = STATE_EOF;
                return accept_eof(p);

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
                return accept_boolean(false);

        case 'n':
                parse_null(p);
                return accept_null(p);

        case '\'':
                if(!p->flags & JSONPG_FLAG_SINGLE_QUOTE)
                        parse_error(p, UNEXPECTED);

                count = parse_sqstring(p, &bytes);
                return accept_string(p, bytes, count);

        case '}':
                if(!(state == STATE_OBJECT_COMMA
                                        && (p->flags & JSONPG_FLAG_TRAILING_COMMA)))
                        parse_error(p, UNEXPECTED);

                parse_end_object(p);
                return accept_end_object(p);

        case ']':
                if(!(state == STATE_ARRAY_COMMA
                                        && (p->flags & JSONPG_FLAG_TRAILING_COMMA)))
                        parse_error(p, UNEXPECTED);

                parse_end_array(p);
                return accept_end_array(p);

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

                if(!(p->flags & JSONPG_FLAG_STRING_NO_QUOTE))
                        parse_error(p, UNEXPECTED);

                count = parse_nqstring(p, &bytes);
                return accept_string(p, bytes, count);
        }

}

jsonpg_value parser_parse_next(Parser p)
{
        if(0 == setjmp(p->env))
                return parse_next(p);

        return p->result;
}

jsonpg_type jsonpg_parse_next(Parser p)
{
        if(p->mis->count)
                return parser_parse_next(p);
        else
                return dom_parse_next(p);
}

