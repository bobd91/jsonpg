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
} JsonpgType;

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
} JsonpgErrorCode;

typedef struct {
        const unsigned char *bytes;
        size_t count;
} JsonpgStringInfo;

typedef union {
        long integer;
        double real;
} JsonpgNumberInfo;

typedef struct {
        JsonpgErrorCode code;
        const char *text;
} JsonpgErrorInfo;

typedef struct {
        JsonpgType type;
        size_t position;
        union {
                JsonpgNumberInfo number;
                JsonpgStringInfo string;
                JsonpgErrorInfo error;
        };
} JsonpgResult;

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
} JsonpgCallbacks;

typedef struct jsonpg_parser_s    *JsonpgParser;
typedef struct jsonpg_generator_s *JsonpgGenerator;
typedef struct jsonpg_dom_s       *JsonpgDom;


void jsonpg_set_allocators(
                void *(*malloc)(size_t), 
                void *(*realloc)(void *, size_t),
                void (*free)(void *));

typedef struct {
        // required to track array/object nesting
        unsigned short max_nesting;

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
        JsonpgDom dom;

} JsonpgParserOpts;

// ------------------------------------
// Pull Parsing
// ------------------------------------

// Create a parser for pull parsing
// Not needed for callback / generator parsing as jsonpg_parse
// creates one internally
JsonpgParser jsonpg_parser_new_opt(JsonpgParserOpts);
#define jsonpg_parser_new(...)   jsonpg_parser_new_opt(     \
                (JsonpgParserOpts){ .max_nesting = 1024,  \
                                       __VA_ARGS__ })     


// Pull parser, get next parse event and result
JsonpgType jsonpg_parse_next(JsonpgParser);
JsonpgResult jsonpg_parse_result(JsonpgParser);

// Example, pull parsing from string that has trailing commas in it
//
// p = jsonpg_parser_new( .allow = JSONPG_ALLOW_TRAILING_COMMAS      
//                        .string = "{\"k1\": [12.5, true,],}");
// 
// The comments below indicate what JSON items are parsed
// The actual type of item is returned from jsonpg_parse_next
// Values are recovered from jsonpg_parse_result(p)
//
// Note: true and false have their own JsonpgType variables
//       there is no JsonpgType boolean
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
void jsonpg_parser_free(JsonpgParser);

// ------------------------------------
// Callback and Generator Parsing
// ------------------------------------

typedef struct {
        // Options for parser created internally by json_parse
        // The parser will be freed before returning
        // See parser_opts above for desriptions
        unsigned short max_nesting;
        unsigned allow;      
        unsigned char *bytes;
        size_t count;
        char *string;
        JsonpgDom dom;

        // Optional callbacks and callback ctx for SAX style parsing
        // This is a common use case so providing the options here
        // saves the caller having to create and free a generator themselves
        JsonpgCallbacks *callbacks;
        void *ctx;

        // Optional generator
        // Ignored if callbacks/ctx are specified
        JsonpgGenerator generator;

} JsonpgParseOpts;

JsonpgResult jsonpg_parse_opt(JsonpgParseOpts);
#define jsonpg_parse(...)  jsonpg_parse_opt(              \
                (JsonpgParseOpts){ .max_nesting = 1024, \
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

        JsonpgCallbacks *callbacks;
        void *ctx;

        // The structure of generated JSON is validated via the C assert
        // mechanism so is active during development and testing but
        // will be removed in an NDEBUG build.
        // While validating the correct nesting of arrays and objects
        // jsonpg needs to know the maximum nesting level to support
        // It will default to 1024 which is more than enough for most use cases
        // The setting has no effect in producion builds
        unsigned short max_nesting;

} JsonpgGeneratorOpts;

JsonpgGenerator jsonpg_generator_new_opt(JsonpgGeneratorOpts);
#define jsonpg_generator_new(...)  jsonpg_generator_new_opt(    \
                (JsonpgGeneratorOpts){ .max_nesting = 1024, \
                                        __VA_ARGS__ })           


// The lifetime of results is that of their generator.
// A string or dom returned from these functions should not be used
// once their generator has been freed

JsonpgErrorInfo jsonpg_result_error(JsonpgGenerator);
JsonpgDom jsonpg_result_dom(JsonpgGenerator);
char *jsonpg_result_string(JsonpgGenerator);
size_t jsonpg_result_bytes(JsonpgGenerator, unsigned char **);

void jsonpg_generator_free(JsonpgGenerator);

// Write JSON items to a generator
//
// Functions return true if successful, false on error
// The only errors returned will be out of memory or invalid utf8 output
// both of which are quite unlikely, so you can probably get away with
// creating output then checking for errors at the end.
//
// Error information can be retrieved from jsonpg_result_error.
// An error code of JSON_ERROR_NONE indicates no errors.
//
// Macros to make building JSON more concise can be found in
// jsonpg_def_macros.h

bool jsonpg_null(JsonpgGenerator);
bool jsonpg_boolean(JsonpgGenerator, bool);
bool jsonpg_integer(JsonpgGenerator, long);
bool jsonpg_real(JsonpgGenerator, double);
bool jsonpg_string(JsonpgGenerator, const unsigned char *, size_t);
bool jsonpg_key(JsonpgGenerator, const unsigned char *, size_t);
bool jsonpg_start_array(JsonpgGenerator);
bool jsonpg_end_array(JsonpgGenerator);
bool jsonpg_start_object(JsonpgGenerator);
bool jsonpg_end_object(JsonpgGenerator);

