
typedef struct memory_input_stream_s *MemoryInputStream;

struct memory_input_stream_s {
        Bytes bytes;
        size_t count;
        size_t current;
};

static MemoryInputStream mis_new(Allocator a)
{
        return allocator_alloc(a, sizeof(struct memory_input_stream_s));
}

static void mis_set_bytes(MemoryInputStream mis, Bytes bytes, size_t count)
{
        mis->bytes = bytes;
        mis->count = count;
        mis->current = 0;
}

static size_t mis_tell(MemoryInputStream mis)
{
        return mis->current;
}

static Bytes mis_at(MemoryInputStream mis, size_t pos)
{
        if(pos >= mis->count)
                return NULL;

        return mis->bytes + pos;
}

static size_t mis_length(MemoryInputStream mis)
{
        return mis->count;
}

static bool mis_eof(MemoryInputStream mis)
{
        ASSERT(mis->current <= mis->count);

        return mis->current == mis->count;
}

static Byte mis_peek(MemoryInputStream mis)
{
        if(mis_eof(mis))
                return '\0';

        return mis->bytes[mis->current];
}

static Byte mis_take(MemoryInputStream mis)
{
        if(mis_eof(mis))
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

