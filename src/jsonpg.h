#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#define JSONPG_FLAG_COMMENTS                   0x01
#define JSONPG_FLAG_TRAILING_COMMAS            0x02
#define JSONPG_FLAG_SINGLE_QUOTES              0x04
#define JSONPG_FLAG_UNQUOTED_KEYS              0x08
#define JSONPG_FLAG_UNQUOTED_STRINGS           0x10
#define JSONPG_FLAG_ESCAPE_CHARACTERS          0x20
#define JSONPG_FLAG_OPTIONAL_COMMAS            0x40
#define JSONPG_FLAG_IS_OBJECT                  0x80
#define JSONPG_FLAG_IS_ARRAY                   0x100

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
        JSONPG_ERROR_PARSE,
        JSONPG_ERROR_NUMBER,
        JSONPG_ERROR_UTF8,
        JSONPG_ERROR_STACK_UNDERFLOW,
        JSONPG_ERROR_STACK_OVERFLOW,
        JSONPG_ERROR_FILE_READ,
        JSONPG_ERROR_FILE_WRITE,
        JSONPG_ERROR_EXPECTED_VALUE,
        JSONPG_ERROR_EXPECTED_KEY,
        JSONPG_ERROR_NO_OBJECT,
        JSONPG_ERROR_NO_ARRAY,
        JSONPG_ERROR_ABORT
} JsonpgErrorCode;

typedef struct jsonpg_string_info_s {
        Bytes *bytes;
        size_t count;
} JsonpgStringInfo;

typedef union jsonpg_number_info_u {
        long integer;
        double real;
} JsonpgNumberInfo;

typedef struct jsonpg_error_result_s {
        JsonpgErrorCode code;
        size_t at;
} JsonpgErrorInfo;

typedef struct jsonpg_result_s {
        JsonpgType type;
        union {
                JsonpgNumberInfo number;
                JsonpgStringInfo string;
                JsonpgErrorInfo error;
        };
} JsonpgResult;

typedef struct {
        int (*boolean)(void *ctx, bool is_true);
        int (*null)(void *ctx);
        int (*integer)(void *ctx, long integer);
        int (*real)(void *ctx, double real);
        int (*string)(void *ctx, uint8_t *bytes, size_t length);
        int (*key)(void *ctx, uint8_t *bytes , size_t length);
        int (*begin_array)(void *ctx);
        int (*end_array)(void *ctx);
        int (*begin_object)(void *ctx);
        int (*end_object)(void *ctx);
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
        uint16_t max_nesting;

        // mask of JSONPG_FLAG_... values above
        uint16_t flags;

        // Input options, specify one type only
        //
        // All input is JSON bytes except for the 'dom' option
        // which is an in-memeory representation of parsed JSON
        // created by jsonpg_generator_new(.dom = true, ...)
        uint8_t *bytes;         // input bytes, must set count
        size_t count;
        char *string;           // NULL terminated C string
        JsonpgDom dom;

} JsonpgParserOpts;

// ------------------------------------
// Pull Parsing
// ------------------------------------

// Create a parser for pull parsing
// Not needed for callback / generator parsing as jsonpg_parse
// creates one internally
jsonpg_parser jsonpg_parser_new_opt(JsonpgParserOpts);
#define jsonpg_parser_new(...)   jsonpg_parser_new_opt(     \
                (JsonpgParserOpts){ .max_nesting = 1024,  \
                                       __VA_ARGS__ })     


// Pull parser, get next parse event and result
JsonpgType jsonpg_parse_next(JsonpgParser);
JsonpgParseResult jsonpg_parse_result(JsonpgParser);

// Example, pull parsing from string
//          allow single quotes to make JSON string creation simpler
//
// p = jsonpg_parser_new(.flags = JSONPG_FLAG_SINGLE_QUOTES);
// jsonpg_parse(.parser = p, .string = "{'k1': [12.5, 'foo']}");
// 
// The comments below indicate what JSON items are parsed
// The actual type of item is returned from jsonpg_parse_next
// Values are recovered from jsonpg_parse_result(p)
//
// jsonpg_parse_next(p); // type: begin_object
// jsonpg_parse_next(p); // type: key, value: "k1"
// jsonpg_parse_next(p); // type: begin_array
// jsonpg_parse_next(p); // type: real, value: 12.5
// jsonpg_parse_next(p); // type: string, value: "foo"
// jsonpg_parse_next(p); // type: end_array
// jsonpg_parse_next(p); // type: end_object
// jsonpg_parse_next(p); // type: EOF
// jsonpg_parser_free(p);
//

// Free the parser returned from jsonpg_parser_new
void jsonpg_parser_free(void *);

// ------------------------------------
// Callback and Generator Parsing
// ------------------------------------

typedef struct {
        // Options for parser created internally by json_parse
        // The parser will be freed before returning
        // See parser_opts above for desriptions
        uint16_t max_nesting;
        uint16_t flags;      
        uint8_t *bytes;
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
// jsonpg_parse(.bytes = my_bytes, 
//              .count = my_byte_count, 
//              .callbacks = my_callbacks,
//              .ctx = my_context);


// ------------------------------------
// Generating output
// ------------------------------------

typedef struct {
        // Pretty printing is ignored when writing to DOM or callbacks
        int indent;             // pretty printing indent, 0 = stringify
        
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
        size_t max_nesting;

} JsonpgGeneratorOpts;

jsonpg_generator jsonpg_generator_new_opt(JsonpgGeneratorOpts);
#define jsonpg_generator_new(...)  jsonpg_generator_new_opt(    \
                (JsonpgGeneratorOpts){ __VA_ARGS__ })           

// The lifetime of results is that of their generator.
// A string or dom returned from these functions should not be used
// once their generator has been freed
JsonpgErrorInfo jsonpg_result_error(JsonpgGenerator);
JsonpgDom jsonpg_result_dom(JsonpgGenerator);
char *jsonpg_result_string(JsonpgGenerator);
size_t jsonpg_result_bytes(JsonpgGenerator, uint8_t **);

void jsonpg_generator_free(void *);

// Write JSON items to a generator
// Functions return true if successful, false on error
// Error information can be retrieved from jsonpg_result_error
//
// Macros to make building JSON more concise can be found in
// jsonpg_def_macros.h
bool jsonpg_null(JsonpgGenerator);
bool jsonpg_boolean(JsonpgGenerator, bool);
bool jsonpg_integer(JsonpgGenerator, long);
bool jsonpg_real(JsonpgGenerator, double);
bool jsonpg_string(JsonpgGenerator, uint8_t *, size_t);
bool jsonpg_key(JsonpgGenerator, uint8_t *, size_t);
bool jsonpg_begin_array(JsonpgGenerator);
bool jsonpg_end_array(JsonpgGenerator);
bool jsonpg_begin_object(JsonpgGenerator);
bool jsonpg_end_object(JsonpgGenerator);

