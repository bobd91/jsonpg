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
        JSONPG_FALSE,
        JSONPG_NULL,
        JSONPG_TRUE,
        JSONPG_INTEGER,
        JSONPG_REAL,
        JSONPG_STRING,
        JSONPG_KEY,
        JSONPG_BEGIN_ARRAY,
        JSONPG_END_ARRAY,
        JSONPG_BEGIN_OBJECT,
        JSONPG_END_OBJECT,
        JSONPG_ERROR,
        JSONPG_EOF
} jsonpg_type;

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
} jsonpg_error_code;

typedef struct {
        uint8_t *bytes;
        size_t length;
} jsonpg_string_value;

typedef union {
        long integer;
        double real;
} jsonpg_number_value;

typedef struct {
        jsonpg_error_code code;
        size_t at;
} jsonpg_error_value;

typedef struct {
        jsonpg_type type;
        union {
                jsonpg_number_value number;
                jsonpg_string_value string;
                jsonpg_error_value error;
        };
} jsonpg_value;

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
        int (*error)(void *ctx, jsonpg_error_code code, size_t at);
} jsonpg_callbacks;

typedef struct jsonpg_parser_s    *jsonpg_parser;
typedef struct jsonpg_generator_s *jsonpg_generator;
typedef struct jsonpg_dom_s       *jsonpg_dom;


void jsonpg_set_allocators(
                void *(*malloc)(size_t), 
                void *(*realloc)(void *, size_t),
                void (*free)(void *));

typedef struct {
        uint16_t max_nesting;   // required to track array/object nesting
        uint16_t flags;          // mask of JSONPG_FLAG_... values above
} jsonpg_parser_opts;

jsonpg_parser jsonpg_parser_new_opt(jsonpg_parser_opts);
#define jsonpg_parser_new(...)   jsonpg_parser_new_opt(     \
                (jsonpg_parser_opts){ .max_nesting = 1024,  \
                                       __VA_ARGS__ })           

// Example: create a parser that will permit comments and trailing commas
// jsonpg_parser_new(.flags = JSONPG_FLAG_COMMENTS 
//                              | JSONPG_FLAG_TRAILING_COMMAS);


// Read JSON from somewhere custom
typedef struct jsonpg_reader_s *jsonpg_reader;
struct jsonpg_reader_s {
        // Like standard read function except first argument is supplied ctx
        ssize_t (*read)(void *, void *, size_t);
        void *ctx;
};

typedef struct {
        // Optional parser, required for pull parsing
        jsonpg_parser parser;

        // If no parser is supplied then one will be created using these options
        // The parser will be freed before returning
        // See parser_opts above for desriptions
        uint16_t max_nesting;
        uint16_t flags;      

        // Input options, specify one type only
        // If none are supplied then fd = 0 (stdin) is used
        //
        // All input is JSON bytes except for the 'dom' option
        // which is an in-memeory representation of parsed JSON
        // created by jsonpg_generator_new(.dom = true, ...)
        int fd;                 // file descriptor
        uint8_t *bytes;         // input bytes, must set count
        size_t count;
        char *string;           // NULL terminated C string
        jsonpg_reader reader;
        jsonpg_dom dom;

        // Optional callbacks and callback ctx for SAX style parsing
        // This is a common use case so providing the options here
        // saves the caller having to create and free a generator themselves
        // Ignored if a parser option is specified
        jsonpg_callbacks *callbacks;
        void *ctx;

        // Optional generator
        // Ignored if callbacks/ctx or parser options are specified
        jsonpg_generator generator;

} jsonpg_parse_opts;

jsonpg_value jsonpg_parse_opt(jsonpg_parse_opts);
#define jsonpg_parse(...)  jsonpg_parse_opt(              \
                (jsonpg_parse_opts){ .max_nesting = 1024, \
                                     __VA_ARGS__ })         

// Example, parse a byte buffer and call callbacks with context
// jsonpg_parse(.bytes = my_bytes, 
//              .count = my_byte_count, 
//              .callbacks = my_fns,
//              .ctx = my_context);



// Pull parser, get next parse event and result
jsonpg_type jsonpg_parse_next(jsonpg_parser);
jsonpg_value jsonpg_parse_result(jsonpg_parser);

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


// Write generated JSON to a custom location
typedef struct jsonpg_writer_s *jsonpg_writer;
struct jsonpg_writer_s {
        // Like standard write function except first argument is supplied ctx
        ssize_t (*write)(void *, const void *, size_t);
        void *ctx;
};

typedef struct {
        // Pretty printing is ignored when writing to DOM or callbacks
        int indent;             // pretty printing indent, 0 = stringify
        
        // Output options, specify one type
        // Options 'buffer' and 'dom' collect the generated results
        // These results are available from jsonpg_result_string,
        // jsonpg_result_bytes or jsonpg_result_dom (see below)
        bool buffer;
        bool dom;
        int fd;
        jsonpg_writer writer;
        jsonpg_callbacks *callbacks;
        void *ctx;

        // Validation of JSON format, the correct nesting of arrays/objects
        // And the correct positioning of keys requires the nesting of
        // these items to be tracked
        // Set to 0 to disable this tracking
        // Validation of numerics and UTF8 sequences cannot be disabled
        size_t max_nesting;

} jsonpg_generator_opts;

jsonpg_generator jsonpg_generator_new_opt(jsonpg_generator_opts);
#define jsonpg_generator_new(...)  jsonpg_generator_new_opt(    \
                (jsonpg_generator_opts){ .max_nesting = 1024,   \
                                         __VA_ARGS__ })           

// The lifetime of results is that of their generator.
// A string or dom returned from these functions should not be used
// once their generator has been freed
jsonpg_error_value jsonpg_result_error(jsonpg_generator);
jsonpg_dom jsonpg_result_dom(jsonpg_generator);
char *jsonpg_result_string(jsonpg_generator);
size_t jsonpg_result_bytes(jsonpg_generator, uint8_t **);

void jsonpg_parser_free(jsonpg_parser);
void jsonpg_generator_free(jsonpg_generator);

// Write JSON items to a generator
// Macros to make this more concise can be found in
// jsonpg_def_macros.h
int jsonpg_null(jsonpg_generator);
int jsonpg_boolean(jsonpg_generator, bool);
int jsonpg_integer(jsonpg_generator, long);
int jsonpg_real(jsonpg_generator, double);
int jsonpg_string(jsonpg_generator, uint8_t *, size_t);
int jsonpg_key(jsonpg_generator, uint8_t *, size_t);
int jsonpg_begin_array(jsonpg_generator);
int jsonpg_end_array(jsonpg_generator);
int jsonpg_begin_object(jsonpg_generator);
int jsonpg_end_object(jsonpg_generator);

