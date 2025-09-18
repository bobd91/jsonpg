/*
 * Use the parser to parse JSON values and pass the results to the generator 
 * 
 * Given the nested nature of JSON it would make sense to parse 
 * arrays and objects recursively however that can run into stack problems
 * with deeply nested input.
 *
 * This implementation flattens the parse into a loop with the nesting
 * levels being tracked in a bit stack (1/0 aray/object)
 *
 * The result is fairly complex but the outline of the loop is:
 *
 * - At the top of the loop we are expecting a JSON value, or key:value pair
 *
 * - If we are in an object then parse the key:
 *
 * - We are now expecting a value, the type of which can be determined
 *   by the first character
 *
 * - If we have open array/object then we cannot immediately produce a value 
 *   unless the array/object is empty, so we check for empty straight away.
 *   If it is not empty we go back to the top as we now expect a value or key:value.
 *
 * - We now have a parsed value so need to check for end array/object
 *   and comma separator.  
 *   Need to handle multiple endings such as '... }]], ...'
 * 
 * - Once we have parsed a JSON value we have either finished, if at the
 *   top level, or we need to go round again
 */

static void parse_generate(parser *p, generator *g)
{
        memory_input_stream *const mis = p->mis;
        const unsigned flags = p->flags;
        const bool opt_comments = flags & JSONPG_ALLOW_COMMENTS;
        const bool opt_trailing_commas = flags & JSONPG_ALLOW_TRAILING_COMMAS;
 
        // Easier for us to think in terms of validating rather than allowing invalid
        const bool validate_utf8 = !(flags & JSONPG_ALLOW_INVALID_UTF8_IN);

        byte *bytes;
        size_t count;

        bool more_todo = true;

        // STACK_NONE   - at the base level, not in object or array
        // STACK_OBJECT - in an object
        // STACK_ARRY   - in an array
        int stack_type = STACK_NONE;

        byte b = consume_whitespace(p, opt_comments);

        do {

                if(stack_type == STACK_OBJECT) {
                        if(b != '"')
                                throw_parse_error(p, JSONPG_ERROR_EXPECTED_KEY);

                        count = parse_string(p, &bytes, validate_utf8);
                        b = consume_whitespace(p, opt_comments);
                        if(b != ':')
                                throw_parse_error(p, JSONPG_ERROR_EXPECTED_KEY);

                        if(!jsonpg_key(g, bytes, count)) 
                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        
                        mis_take(mis); // ':'
                        b = consume_whitespace(p, opt_comments);
                }

                switch(b) {
                case '[':
                        stack_type = parse_start_array(p);
                        if(!jsonpg_start_array(g)) 
                                throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                        b = consume_whitespace(p, opt_comments);
                        if(opt_trailing_commas && b == ',') {
                                mis_take(mis); // ','
                                b = consume_whitespace(p, opt_comments);
                                if(b != ']')
                                        throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                        }
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
                        if(opt_trailing_commas && b == ',') {
                                mis_take(mis); // ','
                                b = consume_whitespace(p, opt_comments);
                                if(b != '}')
                                        throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                        }
                        if(b ==  '}') {
                                stack_type = parse_end_object(p);
                                if(!jsonpg_end_object(g))
                                        throw_parse_error(p, JSONPG_ERROR_TERMINATED);
                                break;
                        }
                        b = consume_whitespace(p, opt_comments);
                        continue;

                case '"':
                        count = parse_string(p, &bytes, validate_utf8);
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
                        throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                }

                while(true) {
                        b = consume_whitespace(p, opt_comments);
                        if(b == ',') {
                                mis_take(mis);
                                b = consume_whitespace(p, opt_comments);
                                // Optional comma only if followed by } or ]
                                if(!(opt_trailing_commas && (b == '}' || b == ']')))
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
                        } else if(stack_type == STACK_NONE) {
                                more_todo = false;
                                break;
                        } else {
                                throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                        }

                }

        } while(more_todo);

        consume_whitespace(p, opt_comments);

}

static parse_result parse(parser *p, generator *g)
{
        const bool multiple_values = p->flags & JSONPG_ALLOW_MULTIPLE_VALUES;
        const bool trailing_chars = p->flags & JSONPG_ALLOW_TRAILING_CHARS;

        parse_result val;

        if(0 == setjmp(p->env)) {
                while(true) {
                        parse_generate(p, g);

                        if(!mis_eof(p->mis)) {
                                if(multiple_values)
                                        continue;
                                if(!trailing_chars)
                                        throw_parse_error(p, JSONPG_ERROR_UNEXPECTED);
                        }
                        break;
                }
                val = make_parse_result(p, JSONPG_EOF);
        } else {
                val = make_pg_error_return(p, g);
        }

        return val;
}

parse_result jsonpg_parse_opt(parse_opts opts)
{
        generator *g;
        parser *p;
        
        p = jsonpg_parser_new_opt((parser_opts) {
                        .max_nesting = opts.max_nesting,
                        .allow = opts.allow,
                        .bytes = opts.bytes,
                        .count = opts.count,
                        .dom = opts.dom
                        });
        if(!p)
                return make_error_return(JSONPG_ERROR_ALLOC, 0);
        else if(p->result.type == JSONPG_ERROR)
                return p->result;

        if(1 != (opts.callbacks != NULL) + (opts.generator != NULL)) {
                return make_error_return(JSONPG_ERROR_OPT, 0);
        }

        if(opts.callbacks) {
                g = generator_new(0, p->flags);
                if(!g) {
                        return make_error_return(JSONPG_ERROR_ALLOC, 0);
                }
                generator_set_callbacks(g, opts.callbacks, opts.ctx);
        } else {
                g = generator_reset(opts.generator, p->flags);
        }
        
        parse_result result;
        if(opts.dom)
                result = dom_parse(p, g);
        else
                result = parse(p, g);

        jsonpg_parser_free(p);
        if(opts.callbacks)
                jsonpg_generator_free(g);

        return result;
}
