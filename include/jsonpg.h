#pragma once

#include <stdio.h>
#include <stddef.h>

// Allow some non-strict JSON behaviour
// 'Or' values together where multiple are to be allowed
// Pass in to .allow option when parsing/generating

// Allow C style block and line comments
#define JSONPG_ALLOW_COMMENTS                    0x01

// Allow commas before end of arrays and objects
#define JSONPG_ALLOW_TRAILING_COMMAS             0x02

// Allow trailing characters in input after successful parse
#define JSONPG_ALLOW_TRAILING_CHARS              0x04

// Allow multiple JSON values in the input
// If this is set then ALLOW_TRAILING_CHARS is ignored
#define JSONPG_ALLOW_MULTIPLE_VALUES             0x08 

// Allow invalid UTF8 sequences in the input
#define JSONPG_ALLOW_INVALID_UTF8_IN             0x10

// Allow invalid utf8 sequences in the output
// It is up to a generator whether or not it validates utf8 sequences by default.
// The only supplied generator that does is the default generator for creating 
// JSON output, and it respects this setting
#define JSONPG_ALLOW_INVALID_UTF8_OUT            0x20


typedef enum {
        JSONPG_NONE,
        JSONPG_PULL,
        JSONPG_NULL,
        JSONPG_FALSE,
        JSONPG_TRUE,
        JSONPG_INTEGER,
        JSONPG_REAL,
        JSONPG_STRING,
        JSONPG_KEY,
        JSONPG_START_ARRAY,
        JSONPG_END_ARRAY,
        JSONPG_START_OBJECT,
        JSONPG_END_OBJECT,
        JSONPG_ERROR,
        JSONPG_EOF
} jsonpg_type;

typedef enum {
        JSONPG_ERROR_NONE,
        JSONPG_ERROR_OPT,
        JSONPG_ERROR_ALLOC,
        JSONPG_ERROR_NUMBER,
        JSONPG_ERROR_UTF8,
        JSONPG_ERROR_SURROGATE,
        JSONPG_ERROR_STACK_UNDERFLOW,
        JSONPG_ERROR_STACK_OVERFLOW,
        JSONPG_ERROR_EXPECTED_VALUE,
        JSONPG_ERROR_EXPECTED_KEY,
        JSONPG_ERROR_NO_OBJECT,
        JSONPG_ERROR_NO_ARRAY,
        JSONPG_ERROR_ESCAPE,
        JSONPG_ERROR_UNEXPECTED,
        JSONPG_ERROR_INVALID,
        JSONPG_ERROR_TERMINATED,
        JSONPG_ERROR_EOF
} jsonpg_error_code;

typedef struct {
        const unsigned char *bytes;
        size_t count;
} jsonpg_string_info;

typedef union {
        long integer;
        double real;
} jsonpg_number_info;

typedef struct {
        jsonpg_error_code code;
        const char *text;
} jsonpg_error_info;

typedef struct {
        jsonpg_type type;
        size_t position;
        union {
                jsonpg_number_info number;
                jsonpg_string_info string;
                jsonpg_error_info error;
        };
} jsonpg_result;

typedef struct {
        bool (*boolean)(void *ctx, bool is_true);
        bool (*null)(void *ctx);
        bool (*integer)(void *ctx, long integer);
        bool (*real)(void *ctx, double real);
        bool (*string)(void *ctx, const unsigned char *bytes, size_t length);
        bool (*key)(void *ctx, const unsigned char *bytes , size_t length);
        bool (*start_array)(void *ctx);
        bool (*end_array)(void *ctx);
        bool (*start_object)(void *ctx);
        bool (*end_object)(void *ctx);
} jsonpg_callbacks;

typedef struct jsonpg_parser            jsonpg_parser;
typedef struct jsonpg_generator         jsonpg_generator;
typedef struct jsonpg_dom               jsonpg_dom;


void jsonpg_set_allocators(
                void *(*malloc)(size_t), 
                void *(*realloc)(void *, size_t),
                void (*free)(void *));

typedef struct {
        // required to track array/object nesting
        unsigned max_nesting;

        // mask of JSONPG_ALLOW_... values above
        unsigned allow;

        // Input options, specify one type only
        //
        // All input is JSON bytes except for the 'dom' option
        // which is an in-memeory representation of parsed JSON
        // created by jsonpg_generator_new(.dom = true, ...)
        unsigned char *bytes;           // input bytes, must set count
        size_t count;
        char *string;                   // NULL terminated C string
        jsonpg_dom *dom;

} jsonpg_parser_opts;

// ------------------------------------
// Pull Parsing
// ------------------------------------

// Create a parser for pull parsing
// Not needed for callback / generator parsing as jsonpg_parse
// creates one internally
jsonpg_parser *jsonpg_parser_new_opt(jsonpg_parser_opts);
#define jsonpg_parser_new(...)   jsonpg_parser_new_opt(     \
                (jsonpg_parser_opts){ .max_nesting = 1024,  \
                                       __VA_ARGS__ })     


// Pull parser, get next parse event and result
jsonpg_type jsonpg_parse_next(jsonpg_parser *);
jsonpg_result jsonpg_parse_result(jsonpg_parser *);

// Example, pull parsing from string that has trailing commas in it
//
// p = jsonpg_parser_new( .allow = JSONPG_ALLOW_TRAILING_COMMAS      
//                        .string = "{\"k1\": [12.5, true,],}");
// 
// The comments below indicate what JSON items are parsed
// The actual type of item is returned from jsonpg_parse_next
// Values are recovered from jsonpg_parse_result(p)
//
// Note: true and false have their own jsonpg_type variables
//       there is no jsonpg_type boolean
//
// jsonpg_parse_next(p); // type: begin_object
// jsonpg_parse_next(p); // type: key, value: "k1"
// jsonpg_parse_next(p); // type: begin_array
// jsonpg_parse_next(p); // type: real, value: 12.5
// jsonpg_parse_next(p); // type: true
// jsonpg_parse_next(p); // type: end_array
// jsonpg_parse_next(p); // type: end_object
// jsonpg_parse_next(p); // type: EOF
// jsonpg_parser_free(p);
//

// Free the parser returned from jsonpg_parser_new
void jsonpg_parser_free(jsonpg_parser *);

// ------------------------------------
// Callback and generator Parsing
// ------------------------------------

typedef struct {
        // Options for parser created internally by json_parse
        // The parser will be freed before returning
        // See parser_opts above for desriptions
        unsigned max_nesting;
        unsigned allow;      
        unsigned char *bytes;
        size_t count;
        char *string;
        jsonpg_dom *dom;

        // Optional callbacks and callback ctx for SAX style parsing
        // This is a common use case so providing the options here
        // saves the caller having to create and free a generator themselves
        jsonpg_callbacks *callbacks;
        void *ctx;

        // Optional generator
        // Ignored if callbacks/ctx are specified
        jsonpg_generator *generator;

} jsonpg_parse_opts;

jsonpg_result jsonpg_parse_opt(jsonpg_parse_opts);
#define jsonpg_parse(...)  jsonpg_parse_opt(              \
                (jsonpg_parse_opts){ .max_nesting = 1024, \
                                     __VA_ARGS__ })         

// Example, parse a byte buffer and call callbacks with context
//          allow comments and multiple values in input
//
// jsonpg_parse( .allow = JSONPG_ALLOW_MULTIPLE_VALUES | JSONPG_ALLOW_COMMENTS
//               .bytes = my_bytes, 
//               .count = my_byte_count, 
//               .callbacks = my_callbacks,
//               .ctx = my_context);


// ------------------------------------
// Generating output
// ------------------------------------

typedef struct {
        // Pretty printing is ignored when writing to DOM or callbacks
        // Pretty printing indent, 0 = stringify
        unsigned indent;    
        
        // Currently the only value that effects generators is 
        // JSONPG_ALLOW_INVALID_UTF8_OUT
        unsigned allow;

        // Output options, specify max one type
        // If none are specified then the output will be buffered
        // and results will be available via jsonpg_result_string
        // or jsonpg_result_bytes
        
        // Options 'dom' build an in-memory representation of the parse
        // results which is available via jsonpg_result_dom
        //
        // Note: not much can be done with dom at the moment apart from
        //       providing it as an input to parse
        bool dom;

        jsonpg_callbacks *callbacks;
        void *ctx;

        // The structure of generated JSON is validated via the C assert
        // mechanism so is active during development and testing but
        // will be removed in an NDEBUG build.
        // While validating the correct nesting of arrays and objects
        // jsonpg needs to know the maximum nesting level to support
        // It will default to 1024 which is more than enough for most use cases
        // The setting has no effect in producion builds
        unsigned max_nesting;

} jsonpg_generator_opts;

jsonpg_generator *jsonpg_generator_new_opt(jsonpg_generator_opts);
#define jsonpg_generator_new(...)  jsonpg_generator_new_opt(    \
                (jsonpg_generator_opts){ .max_nesting = 1024, \
                                        __VA_ARGS__ })           


// The lifetime of results is that of their generator.
// A string or dom returned from these functions should not be used
// once their generator has been freed

jsonpg_error_info jsonpg_result_error(jsonpg_generator *);
jsonpg_dom *jsonpg_result_dom(jsonpg_generator *);
char *jsonpg_result_string(jsonpg_generator *);
size_t jsonpg_result_bytes(jsonpg_generator *, unsigned char **);

void jsonpg_generator_free(jsonpg_generator *);

// Write JSON items to a generator
//
// Functions return true if successful, false on error
// The only errors returned will be out of memory or invalid utf8 output
// both of which are quite unlikely, so you can probably get away with
// creating output then checking for errors at the end.
//
// error information can be retrieved from jsonpg_result_error.
// An error code of JSON_ERROR_NONE indicates no errors.
//
// Macros to make building JSON more concise can be found in
// jsonpg_def_macros.h

bool jsonpg_null(jsonpg_generator *);
bool jsonpg_boolean(jsonpg_generator *, bool);
bool jsonpg_integer(jsonpg_generator *, long);
bool jsonpg_real(jsonpg_generator *, double);
bool jsonpg_string(jsonpg_generator *, const unsigned char *, size_t);
bool jsonpg_key(jsonpg_generator *, const unsigned char *, size_t);
bool jsonpg_start_array(jsonpg_generator *);
bool jsonpg_end_array(jsonpg_generator *);
bool jsonpg_start_object(jsonpg_generator *);
bool jsonpg_end_object(jsonpg_generator *);

