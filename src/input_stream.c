
typedef struct memory_input_stream_s *MemoryInputStream;

struct memory_input_stream_s {
        Bytes start;
        Bytes string;
        Bytes read;
        Bytes write;
        Bytes mark;
        size_t count;
};

static MemoryInputStream mis_new(Allocator a)
{
        MemoryInputStream mis = allocator_alloc(a, sizeof(struct memory_input_stream_s));

        mis->count = 0;
        mis->start = NULL;
        mis->string = NULL;
        mis->read = NULL;
        mis->write = NULL;
        mis->mark = NULL;

        return mis;
}

static void mis_set_bytes(MemoryInputStream mis, Bytes bytes, size_t count)
{
        mis->start = bytes;
        mis->read = bytes;
        mis->write = bytes;
        mis->mark = bytes;
        mis->count = count;
}

static inline size_t mis_tell(MemoryInputStream mis)
{
        return mis->read - mis->start;
}

static inline void mis_adjust(MemoryInputStream mis, Bytes ptr)
{
        ASSERT(0 <= ptr - mis->start && ptr - mis->start <= mis->count);
        mis->read = ptr;
}

static inline Bytes mis_at(MemoryInputStream mis, size_t pos)
{
        if(pos >= mis->count)
                return NULL;

        return mis->start + pos;
}
//
// static size_t mis_length(MemoryInputStream mis)
// {
//         return mis->count;
// }
//
static inline bool mis_eof(MemoryInputStream mis)
{
        ASSERT(mis_tell(mis) <= mis->count);

        return mis_tell(mis) == mis->count;
}

static inline Byte mis_peek(MemoryInputStream mis)
{
        // if(mis_eof(mis))
        //         return '\0';

        return *mis->read;
}

static inline Byte mis_take(MemoryInputStream mis)
{
        // if(mis_eof(mis))
        //         return '\0';

        return *mis->read++;
}

static inline Byte mis_find(MemoryInputStream mis, Byte c)
{
        Bytes p = (Bytes)strchr((const char *)mis->read,
                        (char)c);
        if(!p) {
                mis->read = mis->start + mis->count;
                return '\0';
        }
        mis->read = p;
        return *p;
}

static inline bool mis_consume(MemoryInputStream mis, Byte b)
{
        if(*mis->read != b)
                return false;

        mis->read++;
        return true;
}

static inline void mis_string_start(MemoryInputStream mis)
{
        mis->string = mis->read;
        mis->write = mis->read;
        mis->mark = mis->read;
}

static inline void mis_string_update(MemoryInputStream mis)
{
        if(mis->mark != mis->write) {
                size_t amt = mis->read - mis->mark;
                if(amt) {
                        memmove(mis->write, mis->mark, amt);
                        mis->write += amt;
                }
        } else {
                mis->write = mis->read;
        }
}

static inline void mis_string_restart(MemoryInputStream mis)
{
        mis->mark = mis->read;
}

static inline Bytes *mis_writer(MemoryInputStream mis)
{
        return &mis->write;
}

static inline void mis_byte_copy(MemoryInputStream mis)
{
        *mis->write++ = *mis->read++;
}

static inline size_t mis_string_complete(MemoryInputStream mis, Bytes *bytes)
{
        size_t len;

        *bytes = mis->string;
        if(mis->mark == mis->string) {
                // No escapes
                len = mis->read - mis->string;
        } else {
                mis_string_update(mis);
                len = mis->write - mis->string;
        }
        mis->read++;
        return len;
}
