#pragma once

#define GENERATOR_EXPECTED_VALUE        1
#define GENERATOR_EXPECTED_KEY          2
#define GENERATOR_STACK_OVERFLOW        3
#define GENERATOR_STACK_UNDERFLOW       4
#define GENERATOR_NO_OBJECT             5
#define GENERATOR_NO_ARRAY              6

typedef struct jsonpg_generator_s *jsonpg_generator;

struct jsonpg_generator_s {
        jsonpg_callbacks *callbacks;
        void *ctx;
        int key_next;
        int error;
        struct stack_s stack;
};

static jsonpg_generator generator_new(jsonpg_callbacks *, void *, uint16_t);
