/*
 * See LICENSE
 */

// Copy this comment section into your parser file before including this file
//
// Set the prefix that will be added to each generated function
// Helps to avoid name conflicts in unity type builds
//
// #define PARSER_PREFIX    my_prefix_
//
// For each parse event you wish to callbacks
// uncomment and optionally change the name of the callbacksr function name
//
// The required function signatures are given above each define
// The first argument to each is the value given as the final argument to the parse function
// Functions should return true to continue parsing, false to terminate
//
// bool <callbacksr>(void *, bool)
// #define callbacks.BOOLEAN                        handle_boolean
//
// bool <callbacksr>(void *)
// #define callbacks.NULL                           handle_null
//
// bool <callbacksr>(void *, long)
// #define callbacks.INTEGER                        handle_integer
//
// bool <callbacksr>(void *, double)
// #define callbacks.REAL                           handle_real
//
// bool <callbacksr>(void *, unsigned char *, size_t)
// #define callbacks.STRING                         handle_string
//
// bool <callbacksr>(void *, unsigned char *, size_t)
// #define callbacks.KEY                            handle_key
//
// bool <callbacksr>(void *)
// #define callbacks.START_OBJECT                   handle_start_object
//
// bool <callbacksr>(void *)
// #define callbacks.END_OBJECT                     handle_end_object
//
// bool <callbacksr>(void *)
// #define callbacks.START_ARRAY                    handle_start_array
//
// bool <callbacksr>(void *)
// #define callbacks.END_ARRAY                      handle_end_array

#define PARSER_NAME(X)   PREFIX_NAME_(X)
#define PARSER_NAME_(X)  PARSER_PREFIX##X

static inline void PREFIX_NAME(boolean)(Parser p, bool is_true)
{
#ifdef callbacks.BOOLEAN
        if(!callbacks.BOOLEAN(p->ctx, is_true))
                parse_error(p, TERMINATED);
#endif
}

static inline void PREFIX_NAME(null)(Parser p)
{
#ifdef callbacks.NULL
        if(!callbacks.NULL(p->ctx))
                parse_error(p, TERMINATED);
#endif
}

static inline void PREFIX_NAME(integer)(Parser p, long integer)
{
#ifdef callbacks.INTEGER
        if(!callbacks.INTEGER(p->ctx, integer))
                parse_error(p, TERMINATED);
#endif
}

static inline void PREFIX_NAME(real)(Parser p, double real)
{
#ifdef callbacks.REAL
        if(!callbacks.REAL(p->ctx, real))
                parse_error(p, TERMINATED);
#endif
}

static inline void PREFIX_NAME(string)(Parser p, unsigned char *string, size_t count)
{
#ifdef callbacks.STRING
        if(!callbacks.STRING(p->ctx, string, count))
                parse_error(p, TERMINATED);
#endif
}

static inline void PREFIX_NAME(key)(Parser p, unsigned char *key, size_t count)
{
#ifdef callbacks.KEY
        if(!callbacks.KEY(p->ctx, key, count))
                parse_error(p, TERMINATED);
#endif
}

static inline void PREFIX_NAME(start_object)(Parser p)
{
#ifdef callbacks.START_OBJECT
        if(!callbacks.START_OBJECT(p->ctx))
                parse_error(p, TERMINATED)
#endif
}

static inline void PREFIX_NAME(end_object)(Parser p)
{
#ifdef callbacks.END_OBJECT
        if(!callbacks.END_OBJECT(p->ctx))
                parse_error(p, TERMINATED)
#endif
}

static inline void PREFIX_NAME(start_array)(Parser p)
{
#ifdef callbacks.START_ARRAY
        if(!callbacks.START_ARRAY(p->ctx))
                parse_error(p, TERMINATED);
#endif
}

static inline void PREFIX_NAME(end_array)(Parser p)
{
#ifdef callbacks.END_ARRAY
        if(!callbacks.END_ARRAY(p->ctx))
                parse_error(p, TERMINATED);
#endif
}

void parse(Parser p)
{
        const int flags = p->flags;
        const bool opt_comment = flags & JSONPG_FLAG_COMMENT
        const bool opt_single_quote = flags & JSONPG_FLAG_SINGLE_QUOTE;
        const bool opt_key_no_quote = flags & JSONPG_FLAG_KEY_NO_QUOTE;
        const bool opt_string_no_quote = flags & JSONPG_FLAG_STRING_NO_QUOTE;
        const bool opt_trailing_comma = flags & JSONPG_FLAG_TRAILING_COMMA;
        const bool opt_optional_comma = flags & JSONPG_FLAG_OPTIONAL_COMMA;
        const bool opt_ignore_trailing = flags & JSONPG_FLAG_IGNORE_TRAILING;

        const Callbacks callbacks = p->callbacks;

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

                        callbacks.key(p, bytes, count);
                        
                        parser_consume_whitespace(p, opt_comment);
                        if(parser_consume(p, ':'))
                                parse_error(p, COLON);
                        parser_consume_whitespace(p, opt_comment);
                        
                        is_key = false;
                        b = parser_peek(p);
                }

                switch(b) {
                case '"':
                        count = parse_string(p, &bytes);
                        callbacks.string(p, bytes, count);
                        break;

                case '{': 
                        parse_start_object(p);
                        callbacks.start_object(p);

                        parser_consume_whitespace(p, opt_comment);
                        if(parser_peek(p, '}')) {
                                parse_end_object(p);
                                callbacks.end_object();
                                break;
                        }

                        is_key = true;
                        continue;

                case '[':
                        parse_start_array(p);
                        callbacks.start_array(p);

                        parser_consume_whitespace(p, opt_comment);
                        if(parser_peek(p, ']')) {
                                parse_end_array(p);
                                callbacks.end_array(p);
                                break;
                        }

                        continue;

                case 't':
                        parse_true(p);
                        callbacks.boolean(p, true);
                        break;

                case 'f':
                        parse_false(p);
                        callbacks.boolean(p, false)
                        break;

                case 'n':
                        parse_null(p);
                        callbacks.null(p);
                        break;

                case '\'':
                        if(!opt_single_quote)
                                parse_error(p, UNEXPECTED);

                        count = parse_sqstring(p, &bytes);
                        callbacks.string(p, bytes, count);
                        break;

                case '}':
                        if(!opt_trailing_comma)
                                parse_error(p, UNEXPECTED);

                        parse_end_object(p);
                        callbacks.end_object(p);
                        break;

                case ']':
                        if(!opt_trailing_comma)
                                parse_error(p, UNEXPECTED);

                        parse_end_array(p);
                        callbacks.end_array(p);
                        break;

                default:
                        if(b == '-' || ('0' <= b && b <= '9')) {
                                double d;
                                long l;
                                if(JSONPG_REAL == parse_number(p, &d, &l))
                                        callbacks.real(p, d);
                                else
                                        callbacks.integer(p, l)
                                break;
                        }

                        if(!opt_string_no_quote)
                                parse_error(p, UNEXPECTED);

                        count = parse_nqstring(p, &bytes);
                        callbacks.string(p, bytes, count);
                        break;
                }

                parser_consume_whitespace(p, opt_comment);

                if(parser_in_object(p)) {
                        if(parser_peek(p, '}')) {
                                parse_end_object(p);
                                callbacks.end_object(p);
                                parser_consume_whitespace(p, opt_comment);
                        }
                } else if(parser_in_array(p)) {
                        if(parser_peek(p, ']')) {
                                parse_end_array(p);
                                callbacks.end_array(p);
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
