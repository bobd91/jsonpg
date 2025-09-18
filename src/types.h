#pragma once

#include <stdint.h>
#include <setjmp.h>

// Alias jsonpg types so we don't have to prefix our code
// The only jsonpgs left in code should be 
//  - the names of extern functions
//  - the members of the jsonpg_type and jsonpg_error_code enums

typedef jsonpg_type                     json_type;
typedef jsonpg_error_code               error_code;
typedef jsonpg_result                   parse_result;
typedef jsonpg_error_info               error_info;
typedef jsonpg_callbacks                callbacks;
typedef struct jsonpg_parser            parser;
typedef struct jsonpg_generator         generator;
typedef struct jsonpg_dom               dom;
typedef jsonpg_parser_opts              parser_opts;
typedef jsonpg_parse_opts               parse_opts;
typedef jsonpg_generator_opts           generator_opts;


// Shared project structures and typedefs

typedef unsigned char                   byte;

typedef struct allocator                allocator;
typedef struct memory_input_stream      memory_input_stream;
typedef struct memory_output_stream     memory_output_stream;
typedef struct json_output_stream       json_output_stream;
typedef struct stack                    stack;
typedef struct dom_info                 dom_info;

#define STACK_OBJECT 0
#define STACK_ARRAY  1
#define STACK_NONE   2

struct stack {
       unsigned ptr;
       unsigned size;
       byte     *stack;
};

struct dom_info {
        dom     *hdr;
        size_t  offset;
};

// For pull parser to keep track of where it is up to
typedef enum {
        STATE_START,
        STATE_OBJECT,
        STATE_KEY,
        STATE_KEY_VALUE,
        STATE_OBJECT_COMMA,
        STATE_ARRAY,
        STATE_ARRAY_VALUE,
        STATE_ARRAY_COMMA,
        STATE_DONE,
        STATE_EOF
} parse_state;

// Types exposed by library via opaque pointer

struct jsonpg_parser {
        unsigned                        flags;
        allocator                       *allocator;
        memory_input_stream             *mis;
        parse_state                     state;
        dom_info                        dom_info;
        parse_result                    result;
        jmp_buf                         env;
        stack                           stack;
};

struct jsonpg_generator {
        allocator                       *allocator;
        callbacks                       *callbacks;
        void                            *ctx;
        bool                            validate_utf8;
        bool                            key_next;
        error_info                      error;
        size_t                          count;
        stack                           stack;
};

struct jsonpg_dom {
        allocator                       *allocator;
        dom                             *next;
        dom                             *current;
        size_t                          count;
        size_t                          size;
};
