/*
 * See LICENSE
 */

/*
 * To create a custom parser with associated handler functions
 * create a new file, .c or .h
 *
 * Define any of the following macros:
 *
 * JSONPG_PARSE_NAME            The name of the parse function to create
 *                              (default: parse)
 * JSONPG_PARSE_EXTERN          If defined the parse function will be extern
 *                              If undefined the function will be static
 * JSONPG_HANDLE_function       See comment section below on how to determine
 *                              which handler functions will be called
 *                              and what their names should be
 *
 * Then #include this file
 *
 * Your handler funcions can be statically defined within your parser file
 * or externally defined elsewhere
 */


#ifndef JSONPG_PARSE_NAME
#define JSONPG_PARSE_NAME       parse
#endif

#ifndef JSONPG_PARSE_EXTERN
#define JSONPG_PARSE_STATIC     static
#else
#define JSONPG_PARSE_STATIC
#endif

// Copy this comment section into your parser file above the
// #include "custom_parser.h" line
//
// For the types of data you wish to handle
// uncomment and optionally change the name of the handler function name
//
// The required function signatures are given above each define
// The (void *) first argument is the parse context given to
// the parse function
// Functions should return true to keep parsing, false to
// terminate parsing
//
// bool <handler>(void *, bool)
// #define JSONPG_HANDLE_BOOLEAN               handle_boolean
//
// bool <handler>(void *)
// #define JSONPG_HANDLE_NULL                  handle_null
//
// bool <handler>(void *, long)
// #define JSONPG_HANDLE_INTEGER               handle_integer
//
// bool <handler>(void *, double)
// #define JSONPG_HANDLE_REAL                  handle_real
//
// bool <handler>(void *, unsigned char *, size_t)
// #define JSONPG_HANDLE_STRING                handle_string
//
// bool <handler>(void *, unsigned char *, size_t)
// #define JSONPG_HANDLE_KEY                   handle_key
//
// bool <handler>(void *)
// #define JSONPG_HANDLE_START_OBJECT          handle_start_object
//
// bool <handler>(void *)
// #define JSONPG_HANDLE_END_OBJECT            handle_end_object
//
// bool <handler>(void *)
// #define JSONPG_HANDLE_START_ARRAY           handle_start_array
//
// bool <handler>(void *)
// #define JSONPG_HANDLE_END_ARRAY             handle_end_array

static inline void jsonpg_output_boolean(Parser p, bool is_true)
{
#ifdef JSONPG_HANDLE_BOOLEAN
        if(!JSONPG_HANDLE_BOOLEAN(p->ctx, is_true))
                parse_error(p, TERMINATED);
#endif

static inline void jsonpg_output_null(Parser p)
{
#ifdef JSONPG_HANDLE_NULL
        if(!JSONPG_HANDLE_NULL(p->ctx))
                parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_output_integer(Parser p, long integer)
{
#ifdef JSONPG_HANDLE_INTEGER
        if(!JSONPG_HANDLE_INTEGER(p->ctx, integer))
                parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_output_real(Parser p, double real)
{
#ifdef JSONPG_HANDLE_REAL
        if(!JSONPG_HANDLE_REAL(p->ctx, real))
                parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_output_string(Parser p, Bytes string, size_t count)
{
#ifdef JSONPG_HANDLE_STRING
        if(!JSONPG_HANDLE_STRING(p->ctx, string, count))
                parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_output_key(Parser p, Bytes key, size_t count)
{
#ifdef JSONPG_HANDLE_KEY
        if(!JSONPG_HANDLE_KEY(p->ctx, key, count))
                parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_output_start_object(Parser p)
{
#ifdef JSONPG_HANDLE_START_OBJECT
        if(!JSONPG_HANDLE_START_OBJECT(p->ctx))
                parse_error(p, TERMINATED)
#endif
}

static inline void jsonpg_output_end_object(Parser p)
{
#ifdef JSONPG_HANDLE_END_OBJECT
        if(!JSONPG_HANDLE_END_OBJECT(p->ctx))
                parse_error(p, TERMINATED)
#endif
}

static inline void jsonpg_output_start_array(Parser p)
{
#ifdef JSONPG_HANDLE_START_ARRAY
        if(!JSONPG_HANDLE_START_ARRAY(p->ctx))
                parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_output_end_array(Parser p)
{
#ifdef JSONPG_HANDLE_END_ARRAY
        if(!JSONPG_HANDLE_END_ARRAY(p->ctx))
                parse_error(p, TERMINATED);
#endif
}

static void jsonpg_parse_value(Parser p)
{
        Bytes bytes;
        size_t count;
        bool is_key = false;;

        do {
                Byte b = input_peek(p);

                if(is_key) {
                        if(b == '"')
                                count = parse_string(p, &bytes, '"');

#ifdef JSONPG_FLAG_SINGLE_QUOTE
                        else if(b == '\'') {
                                count = parse_string(p, &bytesm '\'');
#endif
#ifdef JSONPG_FLAG_KEY_NO_QUOTE
                        else
                                count = parse_string(p, &bytes, ' ');
#else
                        else
                                parse_error(p, KEY);
#endif
                        jsonpg_output_key(p, bytes, count);
                        consume_whitespace(p);
                        if(input_consume(p, ':'))
                                parse_error(p, COLON);
                        consume_whitespace(p);

                        is_key = false;
                        b = input_peek(p);
                }

                switch(b) {
                case '"':
                        count = parse_string(p, &bytes, '"');
                        jsonpg_output_string(p, bytes, count);
                        break;
                case '{': 
                        input_take(p);
                        consume_whitespace(p);
                        stack_push(p, JSONPG_OBJECT);
                        jsonpg_output_start_object(p);
                        if(!input_consume(p, '}')) {
                                consume_whitespace(p);
                                is_key = true;
                                continue;
                        }
                        jsonpg_output_end_object(p);
                        stack_pop(p);
                        break;
                case '[':
                        input_take(p);
                        consume_whitespace(p);
                        stack_push(p, JSONPG_ARRAY);
                        jsonpg_output_start_array(p);
                        if(!input_consume(p,']')) {
                                consume_whitespace(p);
                                continue;
                        }
                        jsonpg_output_end_array(p);
                        stack_pop(p);
                        break;
                case 't':
                        parse_true(p);
                        jsonpg_output_boolean(p, true);
                        break;
                case 'f':
                        parse_false(p);
                        jsonpg_output_boolean(p, false)
                        break;
                case 'n':
                        parse_null(p);
                        jsonpg_output_null(p);
                        break;

#ifdef JSONPG_FLAG_SINGLE_QUOTE
                case '\'':
                        count = parse_string(p, &bytes, '\'');
                        jsonpg_output_string(p, bytes, count);
                        break;
#endif
#ifdef JSONPG_FLAG_TRAILING_COMMA
                case '}':
                        input_take(p);
                        jsonpg_output_end_object(p);
                        stack_pop(p);
                        break;
                case ']':
                        input_take(p);
                        jsonpg_output_end_array(p);
                        stack_pop(p);
                        break;
#endif
                default:
                        if(b == '-' || ('0' <= b && b <= '9')) {
                                double d;
                                long l;
                                if(parse_number(p, &d, &l))
                                        jsonpg_output_real(p, d);
                                else
                                        jsonpg_output_integer(p, l)
                                break;
                        }

#ifdef JSONPG_STRING_NO_QUOTE
                        count = parse_string(p, &bytes, ' ');
                        jsonpg_output_string(p, bytes, count);
                        break;
#else
                        parse_error(p, UNEXPECTED);
#endif
                }

                consume_whitespace(p);

                if(stack_peek(p) == JSONPG_OBJECT) {
                        if(input_consume(p, '}')) {
                                consume_whitespace(p);
                                jsonpg_output_end_object(p);
                                stack_pop(p);
                        }
                } else if(stack_peek(p) == JSONPG_ARRAY) {
                        if(input_consume(p, ']')) {
                                consume_whitespace(p);
                                jsonpg_output_end_array(p);
                                stack_pop(p);
                        }
                }
                if(stack_depth(p) > 0) {
                        if(input_consume(p, ',')) {
                                consume_whitespace(p);
                                is_key = stack_peek(p) == JSONPG_OBJECT;
                                continue;
                        }

#ifdef JSONPG_FLAG_OPTIONAL_COMMA
                        is_key = stack_peek() == JSONPG_OBJECT;
                        continue;
#endif
                        parse_error(p, UNEXPECTED);
                } 
                break;
        } while(!input_eof(p));
}

static void jsonpg_parse_json(Parser p)
{
        parse_value(p);

#ifndef JSONPG_FLAG_IGNORE_TRAILING_CONTENT
        consume_whitespace(p);
        if(!parse_eof(p))
                parse_error(p, UNEXPECTED);
#endif
}


JSONPG_PARSE_STATIC ParseResult JSONPG_PARSE_NAME(Bytes bytes, size_t count, void *ctx)
{
        Parser p = parser_new(bytes, count, ctx);
        if(!p)
                return NULL;

        if(0 == setjmp(p->env))
                parse_json(p);

        ParseResult result = p->result;
        parser_free(p);

        return result;
}

#undef JSONPG_PARSE_NAME
#undef JSONPG_PARSE_EXTERN
#undef JSONPG_PARSE_STATIC
#undef JSONPG_HANDLE_BOOLEAN
#undef JSONPG_HANDLE_NULL
#undef JSONPG_HANDLE_INTEGER
#undef JSONPG_HANDLE_REAL
#undef JSONPG_HANDLE_STRING
#undef JSONPG_HANDLE_KEY
#undef JSONPG_HANDLE_START_OBJECT
#undef JSONPG_HANDLE_END_OBJECT
#undef JSONPG_HANDLE_START_ARRAY
#undef JSONPG_HANDLE_END_ARRAY
