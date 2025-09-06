/*
 * See include/jsonpg/LICENSE
 */

/*
 * To create a custom jsonpg parser with associated handler functions
 * create a new .c file
 *
 * Define any of the following macros:
 *
 * JSONPG_PARSE_NAME                    The name to give the generated parse function
 *                                      (default: parse)
 *
 * JSONPG_PARSE_STATIC                  If defined the parse function will be static
 *
 * JSONPG_HANDLE_<type>                 See // comment section below on how to determine
 *                                      which handler functions will be called
 *                                      and what their names should be
 *
 * Parse options can be specified by defining the following macros
 * 
 * JSONPG_OPTION_COMMENTS               Treat C style comments as whitespace
 * JSONPG_OPTION_TRAILING_COMMA         Allow a trailing comma in arrays/objects
 *                                      [1,2,3,] or {"key": value,}
 * JSONPG_OPTION_OPTIONAL_COMMA         Whitespace can be used instead of commas in arrays/objects
 *                                      [1 2 3] or {"k1":v1 "k2":v2}
 * JSONPG_OPTION_SINGLE_QUOTE           Keys and strings can be enclosed in single quotes
 * JSONPG_OPTION_KEY_NO_QUOTE           Keys can be provided without quotes, terminated with a space
 * JSONPG_OPTION_STRING_NO_QUOTE        Strings can be provided without quotes, terminated with a space
 *                                      Note: escapes are handled differently in unquoted keys/strings
 *                                      Any characters can be escaped by prefixing with \
 *                                      For example: \null will be parsed as "null" rather than "\null"
 * JSONPG_OPTION_IGNORE_TRAILING        Parsing terminates immediately after a successful parse
 *                                      Additonal characters in the input are ignored
 *                              
 * Then include this file
 * #include <jsonpg/custom_parser.h>
 *
 * Provide implementations of your chosen handler functions
 * Your handler funcions can be statically defined within your parser file
 * or externally defined elsewhere
 *
 *
 * To parse JSON, load the string into a buffer and call parse.
 *
 * With event based parsing you often need an object to hold context
 * information to tie multiple events together.  
 * For example you could track the key value of the latest key event
 * so the next value event can determine which key it applies to.
 * To facilitate this, any object you pass as the final argument to the 
 * parse function will be passed as the first argument to each handler function.
 *
 * JsonpgParseResult result = parse(json_buffer, no_of_bytes_buffer, parser_context);
 *
 * result->type == JSONPG_EOF or JSONPG_ERROR
 * if JSONPG_ERROR
 * result->type.error.code   see include/jsonpg/error.h for error values
 * result->type.error.at     position in the input where the error occurred
 */

// Copy this comment section into your parser file above the
// #include <jsonpg/custom_parser.h> line
//
// For each parse event you wish to handle
// uncomment and optionally change the name of the handler function name
//
// The required function signatures are given above each define
// The first argument to each is the value given as the final argument to the parse function
// Functions should return true to continue parsing, false to terminate
//
// bool <handler>(void *, bool)
// #define JSONPG_HANDLE_BOOLEAN                        handle_boolean
//
// bool <handler>(void *)
// #define JSONPG_HANDLE_NULL                           handle_null
//
// bool <handler>(void *, long)
// #define JSONPG_HANDLE_INTEGER                        handle_integer
//
// bool <handler>(void *, double)
// #define JSONPG_HANDLE_REAL                           handle_real
//
// bool <handler>(void *, unsigned char *, size_t)
// #define JSONPG_HANDLE_STRING                         handle_string
//
// bool <handler>(void *, unsigned char *, size_t)
// #define JSONPG_HANDLE_KEY                            handle_key
//
// bool <handler>(void *)
// #define JSONPG_HANDLE_START_OBJECT                   handle_start_object
//
// bool <handler>(void *)
// #define JSONPG_HANDLE_END_OBJECT                     handle_end_object
//
// bool <handler>(void *)
// #define JSONPG_HANDLE_START_ARRAY                    handle_start_array
//
// bool <handler>(void *)
// #define JSONPG_HANDLE_END_ARRAY                      handle_end_array

#include <jsonpg/parse.h>

#ifndef JSONPG_PARSE_NAME
#define JSONPG_PARSE_NAME       parse
#endif

#ifndef JSONPG_PARSE_STATIC
#define JSONPG_PARSE_STATIC     
#else
#undef JSONPG_PARSE_STATIC
#define JSONPG_PARSE_STATIC     static
#endif

static inline void jsonpg_handle_boolean(JsonpgParser p, bool is_true)
{
#ifdef JSONPG_HANDLE_BOOLEAN
        if(!JSONPG_HANDLE_BOOLEAN(p->ctx, is_true))
                jsonpg_parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_handle_null(JsonpgParser p)
{
#ifdef JSONPG_HANDLE_NULL
        if(!JSONPG_HANDLE_NULL(p->ctx))
                jsonpg_parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_handle_integer(JsonpgParser p, long integer)
{
#ifdef JSONPG_HANDLE_INTEGER
        if(!JSONPG_HANDLE_INTEGER(p->ctx, integer))
                jsonpg_parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_handle_real(JsonpgParser p, double real)
{
#ifdef JSONPG_HANDLE_REAL
        if(!JSONPG_HANDLE_REAL(p->ctx, real))
                jsonpg_parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_handle_string(JsonpgParser p, unsigned char *string, size_t count)
{
#ifdef JSONPG_HANDLE_STRING
        if(!JSONPG_HANDLE_STRING(p->ctx, string, count))
                jsonpg_parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_handle_key(JsonpgParser p, unsigned char *key, size_t count)
{
#ifdef JSONPG_HANDLE_KEY
        if(!JSONPG_HANDLE_KEY(p->ctx, key, count))
                jsonpg_parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_handle_start_object(JsonpgParser p)
{
#ifdef JSONPG_HANDLE_START_OBJECT
        if(!JSONPG_HANDLE_START_OBJECT(p->ctx))
                jsonpg_parse_error(p, TERMINATED)
#endif
}

static inline void jsonpg_handle_end_object(JsonpgParser p)
{
#ifdef JSONPG_HANDLE_END_OBJECT
        if(!JSONPG_HANDLE_END_OBJECT(p->ctx))
                jsonpg_parse_error(p, TERMINATED)
#endif
}

static inline void jsonpg_handle_start_array(JsonpgParser p)
{
#ifdef JSONPG_HANDLE_START_ARRAY
        if(!JSONPG_HANDLE_START_ARRAY(p->ctx))
                jsonpg_parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_handle_end_array(JsonpgParser p)
{
#ifdef JSONPG_HANDLE_END_ARRAY
        if(!JSONPG_HANDLE_END_ARRAY(p->ctx))
                jsonpg_parse_error(p, TERMINATED);
#endif
}

static inline void jsonpg_consume_whitespace(JsonpgParser p)
{
#ifdef JSONPG_OPTION_COMMENTS
        jsonpg_consume_whitespace_comments(p);
#else
        jsonpg_consume_whitespace_only(p);
#endif
}

static void jsonpg_parse_value(JsonpgParser p)
{
        unsigned char *bytes;
        size_t count;
        bool is_key = false;;

        do {
                unsigned char b = jsonpg_parser_peek(p);

                if(is_key) {
                        if(b == '"')
                                count = jsonpg_parse_string(p, &bytes);

#ifdef JSONPG_OPTION_SINGLE_QUOTE
                        else if(b == '\'') {
                                count = jsonpg_parse_sqstring(p, &bytes);
#endif
#ifdef JSONPG_OPTION_KEY_NO_QUOTE
                        else
                                count = jsonpg_parse_nqstring(p, &bytes);
#else
                        else
                                jsonpg_parse_error(p, KEY);
#endif
                        jsonpg_handle_key(p, bytes, count);
                        
                        jsonpg_consume_whitespace(p);
                        if(jsonpg_parser_consume(p, ':'))
                                jsonpg_parse_error(p, COLON);
                        jsonpg_consume_whitespace(p);
                        
                        is_key = false;
                        b = jsonpg_parser_peek(p);
                }

                switch(b) {
                case '"':
                        count = jsonpg_parse_string(p, &bytes);
                        jsonpg_handle_string(p, bytes, count);
                        break;

                case '{': 
                        jsonpg_parse_start_object(p);
                        jsonpg_handle_start_object(p);

                        jsonpg_consume_whitespace(p);
                        if(jsonpg_parser_peek(p, '}')) {
                                jsonpg_parse_end_object(p);
                                jsonpg_handle_end_object();
                                break;
                        }

                        jsonpg_consume_whitespace(p);
                        is_key = true;
                        continue;

                case '[':
                        jsonpg_parse_start_array(p);
                        jsonpg_handle_start_array(p);

                        jsonpg_consume_whitespace(p);
                        if(jsonpg_parser_peek(p, ']')) {
                                jsonpg_parse_end_array(p);
                                jsonpg_handle_end_array(p);
                                break;
                        }

                        jsonpg_consume_whitespace(p);
                        continue;

                case 't':
                        jsonpg_parse_true(p);
                        jsonpg_handle_boolean(p, true);
                        break;

                case 'f':
                        jsonpg_parse_false(p);
                        jsonpg_handle_boolean(p, false)
                        break;

                case 'n':
                        jsonpg_parse_null(p);
                        jsonpg_handle_null(p);
                        break;

#ifdef JSONPG_OPTION_SINGLE_QUOTE
                case '\'':
                        count = jsonpg_parse_sqstring(p, &bytes);
                        jsonpg_handle_string(p, bytes, count);
                        break;
#endif
#ifdef JSONPG_OPTION_TRAILING_COMMA
                case '}':
                        jsonpg_parse_end_object(p);
                        jsonpg_handle_end_object(p);
                        break;

                case ']':
                        jsonpg_parse_end_array(p);
                        jsonpg_handle_end_array(p);
                        break;
#endif
                default:
                        if(b == '-' || ('0' <= b && b <= '9')) {
                                double d;
                                long l;
                                if(JSONPG_REAL == jsonpg_parse_number(p, &d, &l))
                                        jsonpg_handle_real(p, d);
                                else
                                        jsonpg_handle_integer(p, l)
                                break;
                        }

#ifdef JSONPG_STRING_NO_QUOTE
                        count = jsonpg_parse_nqstring(p, &bytes);
                        jsonpg_handle_string(p, bytes, count);
                        break;
#else
                        jsonpg_parse_error(p, UNEXPECTED);
#endif
                }

                jsonpg_consume_whitespace(p);

                if(jsonpg_parser_in_object(p)) {
                        if(jsonpg_parser_peek(p, '}')) {
                                jsonpg_parse_end_object(p);
                                jsonpg_handle_end_object(p);
                                jsonpg_consume_whitespace(p);
                        }
                } else if(jsonpg_parser_in_array(p)) {
                        if(jsonpg_parser_peek(p, ']')) {
                                jsonpg_parse_end_array(p);
                                jsonpg_handle_end_array(p);
                                jsonpg_consume_whitespace(p);
                        }
                }
                if(jsonpg_parser_in_any(p)) {
                        if(jsonpg_parser_consume(p, ',')) {
                                jsonpg_consume_whitespace(p);
                                is_key = jsonpg_parser_in_object(p);
                                continue;
                        }

#ifdef JSONPG_OPTION_OPTIONAL_COMMA
                        is_key = jsonpg_parser_in_object(p);
                        continue;
#endif
                        jsonpg_parse_error(p, UNEXPECTED);
                } 
                break;
        } while(!jsonpg_parser_eof(p));
}

static void jsonpg_parse_json(JsonpgParser p)
{
        jsonpg_parse_value(p);

#ifndef JSONPG_OPTION_IGNORE_TRAILING
        jsonpg_consume_whitespace(p);
        if(!jsonpg_parser_eof(p))
                jsonpg_parse_error(p, UNEXPECTED);
#endif
}


JSONPG_PARSE_STATIC JsonpgParseResult JSONPG_PARSE_NAME(
                unsigned char *bytes, 
                size_t count, 
                void *ctx)
{
        JsonpgParser p = jsonpg_parser_new(bytes, count, ctx);
        if(!p)
                return NULL;

        if(0 == setjmp(p->env))
                jsonpg_parse_json(p);

        JsonpgParseResult result = p->result;
        jsonpg_parser_free(p);

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
