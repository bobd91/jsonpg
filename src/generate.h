#pragma once

typedef struct jsonpg_generator_s *jsonpg_generator;

struct jsonpg_generator_s {
        arena arena;
        jsonpg_callbacks *callbacks;
        void *ctx;
        bool key_next;
        jsonpg_error_value error;
        size_t count;
        struct stack_s stack;
};

static jsonpg_generator generator_new(uint16_t);
static jsonpg_generator generator_set_callbacks(jsonpg_generator g, jsonpg_callbacks *callbacks, void *ctx);
static jsonpg_generator generator_reset(jsonpg_generator);
static void set_generator_error(jsonpg_generator, jsonpg_error_code);
