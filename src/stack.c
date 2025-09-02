#include <stdint.h>


static size_t stack_bytes_bytes(size_t stack_size)
{
        return stack_size;
}

static int peek_stack_bytes(stack s)
{
        if(s->ptr == 0)
                return -1;
        return s->stack[s->ptr - 1];
}

static int pop_stack_bytes(stack s)
{
        if(s->ptr == s->ptr_min)
                return -1;
        --s->ptr;

        return 0;
}

static int push_stack_bytes(stack s, int type)
{
        if(s->ptr >= s->size)
                return -1;
        s->stack[s->ptr++] = type;
        return 0;
}

static size_t stack_bytes(size_t stack_size)
{
        // 1-8 => 1, 9-16 => 2, etc
        return (stack_size + 7) / 8;
}

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
