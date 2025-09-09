#pragma once

// Shared project structures and typedefs
//
typedef unsigned char                   Byte;
typedef Byte                            *Bytes;

typedef struct allocator_s              *Allocator;

#define STACK_OBJECT 0
#define STACK_ARRAY 1

typedef struct {
       uint16_t ptr;
       uint16_t size;
       Bytes    stack;
} Stack;

typedef struct {
        Dom     hdr;
        size_t  offset;
} DomInfo;


// Types exposed by library via opaque pointer

struct jsonpg_parser_s {
        Allocator                       allocator;
        uint16_t                        flags;
        MemoryInputStream               mis;
        CowStream                       cow;
        DomInfo                         dom_info;
        ParseResult                     result;
        Stack                           stack;
}

struct jsonpg_generator_s {
        Allocator                       allocator;
        JsonpgCallbacks                 *callbacks;
        void                            *ctx;
        bool                            key_next;
        JsonpgErrorInfo                 error;
        size_t                          count;
        Stack                           stack;
};

struct jsonpg_dom_s {
        Allocator allocator;
        Dom next;
        Dom current;
        size_t count;
        size_t size;
};

// Alias jsonpg stuff so we don't have to prefix stuff in code
// The only jsonpg stuff left in code should be the
// extern functions

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
