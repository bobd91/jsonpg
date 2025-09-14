#include <stdint.h>

static inline int stack_peek(Stack s)
{
        if(s->ptr == 0)
                return -1;
        uint16_t sp = s->ptr - 1;
        return 0x01 & s->stack[sp >> 3] >> (sp & 0x07);
}

static inline int stack_pop(Stack s) 
{
        if(s->ptr == 0)
                return -1;
        else if(s->ptr == 1) {
                s->ptr = 0;
                return STACK_NONE;
        } else {
                --s->ptr;
                uint16_t sp = s->ptr - 1;
                // 0 or 1
                return 0x01 & s->stack[sp >> 3] >> (sp & 0x07);
        }
}

static inline int stack_push(Stack s, int type) 
{
        uint16_t sp = s->ptr;
        if(sp >= s->size) 
                return -1;
        Byte offset = (Byte)(sp >> 3);
        Byte mask = 1 << (sp & 0x07);
        
        if(type == STACK_ARRAY)
                s->stack[offset] |= mask;
        else 
                s->stack[offset] &= ~mask;
        s->ptr++;
        return 0;
}
