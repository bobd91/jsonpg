#pragma once

#include <stdint.h>

#define STACK_OBJECT    0
#define STACK_ARRAY     1

typedef struct stack_s *stack;

struct stack_s {
        uint8_t  ptr_min;
        uint16_t ptr;
        uint16_t size;
        uint8_t *stack;
};
