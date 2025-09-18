
struct memory_input_stream {
        byte *start;
        byte *string;
        byte *read;
        byte *write;
        byte *mark;
        size_t count;
};

static memory_input_stream *mis_new(allocator *a)
{
        memory_input_stream *mis = allocator_alloc(a, sizeof(memory_input_stream));

        mis->count = 0;
        mis->start = NULL;
        mis->string = NULL;
        mis->read = NULL;
        mis->write = NULL;
        mis->mark = NULL;

        return mis;
}

static void mis_set_bytes(memory_input_stream *mis, byte *bytes, size_t count)
{
        mis->start = bytes;
        mis->read = bytes;
        mis->write = bytes;
        mis->mark = bytes;
        mis->count = count;
}

static inline size_t mis_tell(memory_input_stream *mis)
{
        return (size_t)(mis->read - mis->start);
}

static inline void mis_adjust(memory_input_stream *mis, byte *ptr)
{
        ASSERT(0 <= ptr - mis->start && ptr - mis->start <= (long)mis->count);
        mis->read = ptr;
}

static inline const byte *mis_at(memory_input_stream *mis, size_t pos)
{
        if(pos >= mis->count)
                return NULL;

        return mis->start + pos;
}

static inline bool mis_eof(memory_input_stream *mis)
{
        ASSERT(mis_tell(mis) <= mis->count);

        return mis_tell(mis) == mis->count;
}

static inline byte mis_peek(memory_input_stream *mis)
{
        return *mis->read;
}

static inline byte mis_take(memory_input_stream *mis)
{
        return *mis->read++;
}

static inline byte mis_find(memory_input_stream *mis, byte c)
{
        byte *p = (byte *)strchr((const char *)mis->read,
                        (char)c);
        if(!p) {
                mis->read = mis->start + mis->count;
                return '\0';
        }
        mis->read = p;
        return *p;
}

static inline bool mis_consume(memory_input_stream *mis, byte b)
{
        if(*mis->read != b)
                return false;

        mis->read++;
        return true;
}

static inline bool mis_validate_utf8(memory_input_stream *mis)
{
        // Terminating \0 will halt utf8 validation if <4 chars
        int len = utf8_validate_sequence(mis->read, 4);
        if(len == -1)
                return false;
        mis->read += len;
        return true;
}

static inline void mis_string_start(memory_input_stream *mis)
{
        mis->string = mis->read;
        mis->write = mis->read;
        mis->mark = mis->read;
}

static inline void mis_string_update(memory_input_stream *mis)
{
        if(mis->mark != mis->write) {
                size_t amt = (size_t)(mis->read - mis->mark);
                if(amt) {
                        memmove(mis->write, mis->mark, amt);
                        mis->write += amt;
                }
        } else {
                mis->write = mis->read;
        }
}

static inline void mis_string_restart(memory_input_stream *mis)
{
        mis->mark = mis->read;
}

static inline byte **mis_writer(memory_input_stream *mis)
{
        return &mis->write;
}

static inline void mis_byte_copy(memory_input_stream *mis)
{
        *mis->write++ = *mis->read++;
}

static inline size_t mis_string_complete(memory_input_stream *mis, byte **bytes)
{
        size_t len;

        *bytes = mis->string;
        if(mis->mark == mis->string) {
                // No escapes
                len = (size_t)(mis->read - mis->string);
        } else {
                mis_string_update(mis);
                len = (size_t)(mis->write - mis->string);
        }
        mis->read++;
        return len;
}
