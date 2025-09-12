

static void parse_generate(Parser p, Generator g)
{
        const MemoryInputStream mis = p->mis;
        const int flags = p->flags;
        const bool opt_comments = flags & JSONPG_FLAG_COMMENTS;
        const bool opt_single_quotes = flags & JSONPG_FLAG_SINGLE_QUOTES;
        const bool opt_unquoted_keys = flags & JSONPG_FLAG_UNQUOTED_KEYS;
        const bool opt_unquoted_strings = flags & JSONPG_FLAG_UNQUOTED_STRINGS;
        const bool opt_trailing_commas = flags & JSONPG_FLAG_TRAILING_COMMAS;
        const bool opt_optional_commas = flags & JSONPG_FLAG_OPTIONAL_COMMAS;
        const bool opt_ignore_trailing_chars = flags & JSONPG_FLAG_IGNORE_TRAILING_CHARS;

        Bytes bytes;
        size_t count;

        bool more_todo = true;

        // STACK_NONE   - at the base level, not in object or array
        // STACK_OBJECT - in an object
        // STACK_ARRY   - in an array
        int stack_type = STACK_NONE;

        Byte b = consume_whitespace(p, opt_comments);

        do {

                if(stack_type == STACK_OBJECT) {
                         if(b == '"')
                                count = parse_string(p, &bytes);
                        else if(opt_single_quotes && b == '\'')
                                count = parse_sqstring(p, &bytes);
                        else if(opt_unquoted_keys)
                                count = parse_nqstring(p, &bytes);
                        else
                                throw_parse_error(p, JSONPG_ERROR_EXPECTED_KEY);

                        b = consume_whitespace(p, opt_comments);
                        if(!mis_consume(mis, ':'))
                                throw_parse_error(p, JSONPG_ERROR_EXPECTED_KEY);


                        if(!jsonpg_key(g, bytes, count)) 
                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        
                        b = consume_whitespace(p, opt_comments);
                }

                switch(b) {
                case '[':
                        stack_type = parse_start_array(p);
                        if(!jsonpg_start_array(g)) 
                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        b = consume_whitespace(p, opt_comments);
                        if(b ==  ']') {
                                stack_type = parse_end_array(p);
                                if(!jsonpg_end_array(g))
                                        throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                                break;
                        }
                        b = consume_whitespace(p, opt_comments);
                        continue;

                case '{':
                        stack_type = parse_start_object(p);
                        if(!jsonpg_start_object(g)) 
                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        b = consume_whitespace(p, opt_comments);
                        if(b ==  '}') {
                                stack_type = parse_end_object(p);
                                if(!jsonpg_end_object(g))
                                        throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                                break;
                        }
                        b = consume_whitespace(p, opt_comments);
                        continue;

                case '"':
                        count = parse_string(p, &bytes);
                        if(!jsonpg_string(g, bytes, count)) 
                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                 case 't':
                        parse_true(p);
                        if(!jsonpg_boolean(g, true)) 
                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                case 'f':
                        parse_false(p);
                        if(!jsonpg_boolean(g, false)) 
                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                case 'n':
                        parse_null(p);
                        if(!jsonpg_null(g)) 
                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;

                default:
                        if(b == '-' || ('0' <= b && b <= '9')) {
                                double d;
                                long l;
                                if(JSONPG_REAL == parse_number(p, &d, &l)) {
                                        if(!jsonpg_real(g, d)) 
                                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                                } else {
                                        if(!jsonpg_integer(g, l)) 
                                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                                } 
                                break;
                        }

                        if(!opt_unquoted_strings)
                                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);

                        count = parse_nqstring(p, &bytes);
                        if(!jsonpg_string(g, bytes, count)) 
                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        break;
                }

                while(true) {
                        b = consume_whitespace(p, opt_comments);
                        if(b == ',') {
                                mis_take(mis);
                                b = consume_whitespace(p, opt_comments);
                                if(!opt_trailing_commas)
                                        break;
                        }
                        if(b == '}'&& stack_type == STACK_OBJECT) {
                                stack_type = parse_end_object(p);
                                if(!jsonpg_end_object(g))
                                        throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        } else if(b == ']' && stack_type == STACK_ARRAY) {
                                stack_type = parse_end_array(p);
                                if(!jsonpg_end_array(g))
                                        throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        } else if(opt_optional_commas) {
                                break;
                        } else if(stack_type == STACK_NONE) {
                                more_todo = false;
                                break;
                        } else {
                                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                        }

                }

        } while(more_todo);

        if(!opt_ignore_trailing_chars) {
                consume_whitespace(p, opt_comments);
                if(!mis_eof(mis))
                        throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
        }
}

ParseResult parse(Parser p, Generator g)
{
        ParseResult val;

        if(0 == setjmp(p->env)) {
                parse_generate(p, g);
                val.type = JSONPG_EOF;
        } else {
                val = make_pg_error_return(p, g);
        }

        return val;
}

ParseResult jsonpg_parse_opt(ParseOpts opts)
{
        Generator g;
        Parser p;
        
        p = jsonpg_parser_new(
                        .max_nesting = opts.max_nesting,
                        .flags = opts.flags,
                        .bytes = opts.bytes,
                        .count = opts.count,
                        .dom = opts.dom);
        if(!p)
                return make_error_return(JSONPG_ERROR_ALLOC, 0);
        else if(p->result.type == JSONPG_ERROR)
                return p->result;

        if(1 != (opts.callbacks != NULL) + (opts.generator != NULL)) {
                opt_error(p);
                return p->result;
        }

        if(opts.callbacks) {
                g = generator_new(0);
                if(!g) {
                        alloc_error(p);
                        return p->result;
                }
                generator_set_callbacks(g, opts.callbacks, opts.ctx);
        } else {
                g = generator_reset(opts.generator);
        }
        
        ParseResult result;
        if(opts.dom)
                result = dom_parse(p, g);
        else
                result = parse(p, g);

        jsonpg_parser_free(p);
        if(opts.callbacks)
                jsonpg_generator_free(g);

        return result;
}
