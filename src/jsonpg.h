#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

typedef enum {
        JSONPG_NONE,
        JSONPG_ROOT,
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
        JSONPG_ERROR_OPTS,
        JSONPG_ERROR_ALLOC,
        JSONPG_ERROR_PARSE,
        JSONPG_ERROR_NUMBER,
        JSONPG_ERROR_UTF8,
        JSONPG_ERROR_STACKUNDERFLOW,
        JSONPG_ERROR_STACKOVERFLOW,
        JSONPG_ERROR_FILE_READ,
        JSONPG_ERROR_FILE_WRITE,
        JSONPG_ERROR_EXPECTED_VALUE,
        JSONPG_ERROR_EXPECTED_KEY,
        JSONPG_ERROR_NO_OBJECT,
        JSONPG_ERROR_NO_ARRAY
} jsonpg_error_code;

typedef struct {
        uint8_t *bytes;
        size_t length;
} jsonpg_string_val;

typedef union {
        long integer;
        double real;
} jsonpg_number_val;

typedef struct {
        jsonpg_error_code code;
        size_t at;
} jsonpg_error_val;

typedef union {
        jsonpg_number_val number;
        jsonpg_string_val string;
        jsonpg_error_val error;
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
typedef struct str_buf_s          *jsonpg_buffer;


void jsonpg_set_allocators(
                void *(*malloc)(size_t), 
                void *(*realloc)(void *, size_t),
                void (*free)(void *));

typedef struct {
        uint16_t max_nesting;
        bool comments;
        bool trailing_commas;
        bool optional_commas;
        bool single_quotes;
        bool unquoted_keys;
        bool unquoted_strings;
        bool escape_characters;
        bool is_object;
        bool is_array;
} jsonpg_parser_opts;

jsonpg_parser jsonpg_parser_new_opt(jsonpg_parser_opts);
#define jsonpg_parser_new(...)   jsonpg_parser_new_opt(        \
                (jsonpg_parser_opts){ .max_nesting = 1024,     \
                                      .comments = true,        \
                                      __VA_ARGS__ })           \

// jsonpg_parser_new(.max_nesting = 2048, .is_array = true, .unquoted_strings = true);
// jsonpg_parser_new(.comments = false);

typedef struct {
        int fd;
        FILE *stream;
        uint8_t *bytes;
        size_t count;
        char *string;
        jsonpg_callbacks *callbacks;
        void *context;
        jsonpg_dom dom;
        jsonpg_generator generator;
} jsonpg_parse_opts;

jsonpg_type jsonpg_parse_opt(jsonpg_parser, jsonpg_parse_opts);
#define jsonpg_parse(X, ...)  jsonpg_parse_opt((X),           \
                (jsonpg_parse_opts){ .fd = -1,                \
                                      __VA_ARGS__ })         \

// jsonpg_parse(parser, .stream = stdin, .callback = &my_callbacks, .context = &my_ctx));
// jsonpg_parse(parser, .bytes = my_input_buffer, .count = 5364);
// jsonpg_parse(parser, .string = "[ 12, 3.45, true, false, { \"foo\": null }]" );

// Pull parser, get next parse event
jsonpg_type jsonpg_parse_next(jsonpg_parser);

jsonpg_value jsonpg_parse_result(jsonpg_parser);
jsonpg_error_val jsonpg_parse_error(jsonpg_parser);

// Generate parse events from those stored in dom
jsonpg_type jsonpg_dom_parse(jsonpg_dom, jsonpg_generator);

jsonpg_dom jsonpg_dom_new(size_t);
jsonpg_buffer jsnonpg_buffer_new(size_t);

char *jsonpg_buffered_string(jsonpg_buffer);
size_t jsonpg_buffered_bytes(jsonpg_buffer, uint8_t **);


typedef struct {
        int fd;
        bool pretty;
        FILE *stream;
        jsonpg_buffer buffer;
        size_t max_nesting;
} jsonpg_generator_opts;

jsonpg_generator jsonpg_generator_new_opt(jsonpg_generator_opts);
#define jsonpg_generator_new(...)  jsonpg_generator_new_opt(      \
                (jsonpg_generator_opts){ .fd = -1,                \
                                         .max_nesting = 1024      \
                                         __VA_ARGS__ })           \

// jsonpg_generator_new(.stream = stdout, .pretty = true);
// jsonpg_generator_new(.fd = my_file, .max_nesting = 0 );

jsonpg_generator jsonpg_callback(jsonpg_callbacks *, void *);
jsonpg_generator jsonpg_dom_builder(jsonpg_dom);

jsonpg_error_val jsonpg_generator_error(jsonpg_generator);

void jsonpg_parser_free(jsonpg_parser);
void jsonpg_generator_free(jsonpg_generator);
void jsonpg_dom_free(jsonpg_dom);
void jsonpg_buffer_free(jsonpg_buffer);

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

