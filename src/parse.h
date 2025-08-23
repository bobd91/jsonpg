#pragma once

#define BUF_SIZE 4096

#define TOKEN_MAX 3 // string/escape_u/surrogate

#define STATE_INITIAL 0xFE
#define STATE_ERROR   0xFF

#define STACK_SIZE 1024

#define FLAG_COMMENTS                   0x01
#define FLAG_TRAILING_COMMAS            0x02
#define FLAG_SINGLE_QUOTES              0x04
#define FLAG_UNQUOTED_KEYS              0x08
#define FLAG_UNQUOTED_STRINGS           0x10
#define FLAG_ESCAPE_CHARACTERS          0x20
#define FLAG_OPTIONAL_COMMAS            0x40
#define FLAG_IS_OBJECT                  0x80
#define FLAG_IS_ARRAY                   0x100

typedef enum {
        token_null,
        token_true,
        token_false,
        token_string,
        token_key,
        token_sq_string,
        token_sq_key,
        token_nq_string,
        token_nq_key,
        token_integer,
        token_real,
        token_escape,
        token_escape_chars,
        token_escape_u,
        token_surrogate
} token_type;

typedef struct token_s {
        token_type type;
        uint8_t *pos;
} *token;

typedef struct {
        uint8_t *bytes;
        size_t length;
} string_val;

typedef union {
        long integer;
        double real;
} number_val;

typedef union {
        number_val number;
        string_val string;
        jsonpg_error_info error;
} parse_result;


typedef struct str_buf_s *str_buf;
typedef struct reader_s *reader;

struct jsonpg_parser_s {
        uint8_t seen_eof;
        uint8_t token_ptr;
        uint8_t input_is_ours;
        uint8_t state;
        uint8_t push_state;
        uint16_t flags;
        uint32_t input_size;
        uint8_t *input;   
        uint8_t *current;
        uint8_t *last;
        str_buf write_buf;
        reader reader;
        parse_result result;
        struct token_s tokens[TOKEN_MAX];
        struct stack_s stack;
};

typedef struct jsonpg_parser_s *jsonpg_parser;

