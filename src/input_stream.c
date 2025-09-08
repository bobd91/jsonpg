

MemoryInputStream mis_new(Allocator a)
{
        return memory_allocate(a, sizeof(struct memory_input_stream_s));
}

void mis_set_bytes(Bytes bytes, size_t count)
{
        is->bytes = bytes;
        is->count = count;
        is->current = 0;
}

size_t mis_tell(MemoryInputStream mis)
{
        return mis->current;
}

Bytes mis_at(MemoryInputStream mis, size_t pos)
{
        if(pos >= mis->count)
                return NULL;

        return mis->bytes + pos;
}

size_t mis_length(MemoryInputStream mis)
{
        return mis->count;
}

static bool mis_eof(MemoryInputStream mis)
{
        JSONPG_ASSERT(mis->current <= mis->count);

        return mis->current == mis->count;
}

Byte mis_peek(MemoryInputStream mis)
{
        if(mis_eof(mis))
                return '\0';

        return mis->bytes[mis->current];
}

Byte mis_take(MemoryInputStream mis)
{
        if(mes_eof(mis))
                return '\0';

        return mis->bytes[mis->current++];
}

static bool mis_consume(MemoryInputStream mis, Byte b)
{
        if(mis_peek(mis) != b)
                return false;

        mis_take(mis);
        return true;
}

