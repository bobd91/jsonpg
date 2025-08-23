#include <stdint.h>

        
static int peek_stack(stack s)
{
        if(s->ptr == 0)
                return -1;
        uint16_t sp = s->ptr - 1;
        return 0x01 & s->stack[sp >> 3] >> (sp & 0x07);
}

static int pop_stack(stack s) 
{
        if(s->ptr == s->ptr_min)
                return -1;
        --s->ptr;

        return 0;
}

static int push_stack(stack s, int type) 
{
        uint16_t sp = s->ptr;
        if(sp >= s->size) 
                return -1;
        int offset = sp >> 3;
        int mask = 1 << (sp & 0x07);
        
        if(type == STACK_ARRAY)
                s->stack[offset] |= mask;
        else 
                s->stack[offset] &= ~mask;
        s->ptr++;
        return 0;
}

