

static void parse_generate(Parser p, Generator g)
{
        const MemoryInputStream mis = p->mis;
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
                Byte b = mis_peek(mis);

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
                        if(mis_consume(mis, ':'))
                                parse_error(p, COLON);
                        parser_consume_whitespace(p, opt_comment);
                        
                        if(!jsonpg_key(g, bytes, count)) 
                                parse_error(p, JSONPG_ERROR_TERMINATED);
                        
                        is_key = false;
                        b = mis_peek(mis);
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
                b = mis_peek(mis);

                if(parser_in_object(p)) {
                        if(b == '}') {
                                parse_end_object(p);
                                if(!jsonpg_end_object(g)) 
                                        parse_error(p, JSONPG_ERROR_TERMINATED);
                                parser_consume_whitespace(p, opt_comment);
                        }
                } else if(parser_in_array(p)) {
                        if(b == ']') {
                                parse_end_array(p);
                                if(!jsonpg_end_array(g)) 
                                        parse_error(p, JSONPG_ERROR_TERMINATED);
                                parser_consume_whitespace(p, opt_comment);
                        }
                }
                if(parser_in_any(p)) {
                        if(b == ',') {
                                mis_take(mis);
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
        } while(!mis_eof(mis));

        if(!opt_ignore_trailing) {
                parser_consume_whitespace(p, opt_comment);
                if(!mis_eof(mis))
                        parse_error(p, UNEXPECTED);
        }
}

ParseResult parse(Parse p, Generator g)
{
        ParseResult val;

        if(0 == setjmp(p->env)) {
                parse_generate(p, p);
                val.type = JSONPG_EOF;
        } else {
                val = p->result;
                if(val.type == JSONPG_ERROR) {
                        // Terminations come from generator
                        // Which MAY have set error info
                        if(val.error.code == JSONPG_TERMINATED
                                        && g->error.code) {
                                val.error = g->error;
                        }
                } else {
                        val = make_error(JSONPG_ERROR_UNEXPECTED, 0);
                }
        }

        return val;
}

ParseResult jsonpg_parse_opt(jsonpg_parse_opts opts)
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
        
        ParseResult result = parse(p, g);

        jsonpg_parser_free(p);
        if(opts.callbacks)
                jsonpg_generator_free(g);

        return result;
}
