#pragma once

#define BUF_SIZE 4096

#define TOKEN_MAX 3 // string/escape_u/surrogate

#define STATE_INITIAL 0xFE
#define STATE_ERROR   0xFF

#define STACK_SIZE 1024

#define CTX_TO_INT(X) ((int)(int64_t)X)
#define INT_TO_CTX(X)   ((void *)(int64_t)X)

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

typedef struct str_buf_s *str_buf;
typedef struct jsonpg_reader_s reader;
typedef struct dom_info_s dom_info;

struct jsonpg_parser_s {
        uint8_t seen_eof;
        uint8_t token_ptr;
        uint8_t state;
        uint8_t push_state;
        uint16_t flags;
        bool input_is_ours;
        uint32_t input_size;
        uint8_t *input;   
        uint8_t *current;
        uint8_t *last;
        size_t processed;
        str_buf write_buf;
        ssize_t (*read_fn)(void *, void *, size_t);
        void *read_ctx;
        dom_info dom_info;
        jsonpg_value result;
        struct token_s tokens[TOKEN_MAX];
        struct stack_s stack;
};

typedef struct jsonpg_parser_s *jsonpg_parser;

