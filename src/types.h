#pragma once

#include <stdint.h>
#include <setjmp.h>

// Alias jsonpg types so we don't have to prefix our code
// The only jsonpgs left in code should be 
//  - the names of extern functions
//  - the members of the JsonpgType and JsonpgErrorCode enums

typedef JsonpgType                      JsonType;
typedef JsonpgErrorCode                 ErrorCode;
typedef JsonpgResult                    ParseResult;
typedef JsonpgErrorInfo                 ErrorInfo;
typedef JsonpgCallbacks                 Callbacks;
typedef struct jsonpg_parser_s          parser_s;
typedef JsonpgParser                    Parser;
typedef struct jsonpg_generator_s       generator_s;
typedef JsonpgGenerator                 Generator;
typedef struct jsonpg_dom_s             dom_s;
typedef JsonpgDom                       Dom;
typedef JsonpgParserOpts                ParserOpts;
typedef JsonpgParseOpts                 ParseOpts;
typedef JsonpgGeneratorOpts             GeneratorOpts;


// Shared project structures and typedefs

typedef unsigned char                   Byte;
typedef unsigned char                   *Bytes;
typedef const unsigned char             *CBytes;

typedef struct allocator_s              *Allocator;
typedef struct memory_input_stream_s    *MemoryInputStream;
typedef struct memory_output_stream_s   *MemoryOutputStream;
typedef struct cow_stream_s             *CowStream;
typedef struct stack_s                  *Stack;

#define STACK_OBJECT 0
#define STACK_ARRAY  1
#define STACK_NONE   2

struct stack_s {
       uint16_t ptr;
       uint16_t size;
       Bytes    stack;
};

typedef struct dom_info_s {
        Dom     hdr;
        size_t  offset;
} DomInfo;

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
} ParseState;

// Types exposed by library via opaque pointer

struct jsonpg_parser_s {
        unsigned                        flags;
        Allocator                       allocator;
        MemoryInputStream               mis;
        ParseState                      state;
        DomInfo                         dom_info;
        ParseResult                     result;
        jmp_buf                         env;
        struct stack_s                  stack;
};

struct jsonpg_generator_s {
        Allocator                       allocator;
        JsonpgCallbacks                 *callbacks;
        void                            *ctx;
        bool                            key_next;
        JsonpgErrorInfo                 error;
        size_t                          count;
        struct stack_s                  stack;
};

struct jsonpg_dom_s {
        Allocator allocator;
        Dom next;
        Dom current;
        size_t count;
        size_t size;
};
