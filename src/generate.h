#pragma once

typedef struct jsonpg_generator_s *jsonpg_generator;

struct jsonpg_generator_s {
        jsonpg_callbacks *callbacks;
        bool ctx_is_ours;
        void *ctx;
        int key_next;
        jsonpg_error_val error;
        size_t count;
        struct stack_s stack;
};

static jsonpg_generator generator_new(jsonpg_callbacks *, void *, uint16_t);
static void generator_reset(jsonpg_generator);
static void set_generator_error(jsonpg_generator, jsonpg_error_code);
