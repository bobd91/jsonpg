/*
 * jsonpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * stack.c
 *   maintains a bit stack of nested array/objects
 *   a 1 indicates an array in the stack, a 0 an object
 */

#include <stdint.h>

static inline int stack_peek(stack *s)
{
        if(s->ptr == 0)
                return -1;
        unsigned sp = s->ptr - 1;
        return 0x01 & s->stack[sp >> 3] >> (sp & 0x07);
}

static inline int stack_pop(stack *s) 
{
        if(s->ptr == 0)
                return -1;
        else if(s->ptr == 1) {
                s->ptr = 0;
                return STACK_NONE;
        } else {
                --s->ptr;
                unsigned sp = s->ptr - 1;
                // 0 or 1
                return 0x01 & s->stack[sp >> 3] >> (sp & 0x07);
        }
}

static inline int stack_push(stack *s, int type) 
{
        unsigned sp = s->ptr;
        if(sp >= s->size) 
                return -1;
        byte offset = (byte)(sp >> 3);
        byte mask = 1 << (sp & 0x07);
        
        if(type == STACK_ARRAY)
                s->stack[offset] |= mask;
        else 
                s->stack[offset] &= ~mask;
        s->ptr++;
        return 0;
}
