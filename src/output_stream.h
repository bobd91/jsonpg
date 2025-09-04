

MemoryOutputStream mos_new(allocator a, size_t initial_capacity)
{
        JSONPG_ASSERT(a);

        MemoryOutputStream os = memory_allocate(a, 
                                        sizeof(struct memory_output_stream_s));
        if(!os)
                return NULL;
        os->allocator = a
        os->initial_capacity = initial_capacity > 0 
                                ? initial_capacity 
                                : JSONPG_DEFAULT_MOS_CAPACITY;
        os->capacity = 0;
        os->count = 0;
        os->buffer = NULL;

        return os;
}

size_t mos_length(MemoryOutputStream os)
{
        JSONPG_ASSERT(os);

        return os->count;
}

bool mos_put(MemoryOutputStream mos, Byte chr)
{
        JSONPG_ASSERT(mos);

        Bytes s = mos_reserve(mos, 1);
        if(!s)
                return false;
        s[0] = chr;
        return true;
}

bool mos_putn(MemoryOutputStream mos, Byte chr, size_t count)
{
        JSONPG_ASSERT(mos);

        Bytes s = mos_reserve(mos, count);
        if(!s)
                return false;
        memset(s, chr, count);
        return true;
}

bool mos_puts(MemoryOutputStream mos, Bytes string, size_t count)
{
        JSONPG_ASSERT(mos);

        Bytes s = mos_reserve(mos, count);
        if(!s)
                return false;
        memcpy(s, string, count);
        return true;
}

Bytes mos_pop(MemoryOutputStream os)
{
        JSONPG_ASSERT(os);

        os->count = 0;
        return os->buffer;
}

void mos_adjust(MemoryOutputStream os, size_t amount)
{
        JSONPG_ASSERT(os);

        os->count += amount;
}

Bytes mos_grow(MemoryOutputStream os, size_t incr)
{
        JSONPG_ASSERT(os);

        size_t size = os->capacity 
                ? os->capacity << 1
                : JSONPG_DEFAULT_BUFFER_SIZE;

        size_t required = os->capacity + incr;

        while(size < required)
                size << 1;

        Bytes new;
        if(os->buffer)
                new = memory_reallocate(os->allocator, buffer, size);
        else
                new = memory_allocate(os->allocator, size);

        if(new) {
                os->buffer = new;
                os->capacity = size;
        }

        return new;
}


Bytes mos_reserve(MemoryOutputStream os, size_t count)
{
        JSONPG_ASSERT(os);

        Bytes str = os->buffer + os->count;

        if(count > os->capacity - os->count)
                if(!mos_grow(os, count))
                        return NULL;
        os->count += count;
        return str;
}

JsonOuputStream jos_new(Allocator a, MemoryOutputStream mos)
{
        JsonOutputStream jos = allocator_alloc(a, sizeof(struct json_output_stream_c));
        if(!jos)
                return NULL;
        jos->mos = mos;
}

bool jos_put(JsonOutputStream jos, Byte chr)
{
        JSONPG_ASSERT(jos);

        return mos_put(jos->mos, chr);
}

bool jos_putn(JsonOutputStream jos, Byte chr, size_t count)
{
        JSONPG_ASSERT(jos);

        return mos_putn(jos->mos, chr, count);
}

bool jos_puts(JsonOutputStream jos, Bytes string, size_t count)
{
        JSONPG_ASSERT(jos);

        return mos_puts(jos->mos, string, count);
}

size_t find_next_escape(Bytes string, size_t count. size_t start)
{
        JSONPG_ASSERT(jos);

        // TODO: SIMD?
        for(int i = start ; i < count ; i++) {
                Byte chr = string[i];
                if(chr == '"' || chr == '\\' | chr < 0x20)
                        return i;
        }
        return i;
}

bool jos_escape(JsonOutputStream jos, Bytes string, size_t count)
{
        JSONPG_ASSERT(jos);

        static char *s_escapes[] = {
                "00", "01", "02", "03",
                "04", "05", "06", "07",
                NULL, NULL, NULL, "0B",
                NULL, NULL, "0E", "0F",
                "10", "11", "12", "13",
                "14", "15", "16", "17",
                "18", "19", "1A", "1B",
                "1C", "1D", "1E", "1F"
        };

        static char c_escapes[] = {
                [0x08] = 'b', [0x09] = 't', [0x0A] = 'n',
                [0x0C] = 'f', [0x0D] = 'r', ['"'] = '"',
                ['\\'] = '\\'
        };

        size_t pos1 = 0;
        size_t pos2 = 0;
        Byte chr;
        while(count > (pos2 = find_next_escape(string, count, pos1))) {
                chr = string[pos2];
                JSONPG_ASSERT(chr <= '\\');

                if(!jos_puts(jos, string + pos1, pos2 - pos1))
                        return false;

                Byte e = c_escapes[chr];
                if(e) {
                        s = mos_reserve(mos, 2);
                        if(!s)
                                return false;
                        s[0] = '\\';
                        s[1] = e;
                } else {
                        JSONPG_ASSERT(chr <= 0x1F);
                        Bytes es = s_escapes[chr];
                        JSONPG_ASSERT(es);
                        if(!jos_puts(jos, "\\u00", 4))
                                return false;
                        if(!jos_puts(jos, es, 2))
                                return false;
                }

                pos1 = pos2 + 1;
        }
        return jos_puts(jos, string + pos1, pos2 - pos1);
}

bool jos_puti(JsonOutputStream jos, long integer)
{
        JSONPG_ASSERT(jos);

        Bytes s = mos_reserve(jos->mos, 20);
        if(!s)
                return false;
        size_t count = pg_ltoa(integer, s);
        mos_adjust(jos->mos, 20 - count);
        return true;
}

bool jos_putr(JsonOutputStream jos, double real)
{
        JSONPG_ASSERT(jos);

        Bytes s = mos_reserve(jos->mos, 25);
        if(!s)
                return false;
        size_t count = pg_dtoa(real, s);
        mos_adjust(jos->mos, 25 - count);
        return true;
}

bool jos_indent(JsonOutputStream jos)
{
        if (jos->comma)
                if(!mos_put(jos->mos, ','))
                        return false;

#ifdef JSONPG_PRETTY_INDENT
        if(jos->newline) {
                if(!jos_put(jos, '\n'))
                        return false;
                if(!jos_putn(jos, ' ', JSONPG_PRETTY_INDENT * jos->level))
                        return false;
        } else if(!jos->empty) {
                return jos_put(jos, ' ');
        }
#endif
        return true;
}

bool print_null(JsonOutputStream jos)
{
        return jos_indent(jos)
                && jos_puts(jos, "null", 4);
}

bool print_boolean(JsonOutputStream jos, bool is_true)
{
        return jos_indent(jos)
                && is_true
                        ? jos_puts(jos, "true,", 5)
                        : jos_puts(jos, "false,", 6);
}

bool print_string_bytes(JsonOutputStream jos, Bytes bytes, size_t count)
{
        return jos_indent(jos)
                && jos_put(jos, '"')
                && jos_escape(jos, bytes, count)
                && jos_put(jos, '"');
}

bool print_key_bytes(JsonOutputStream jos, Bytes bytes, size_t count)
{
        
        return jos_indent_key(jos)
                && jos_put(jos, '"')
                && jos_escape(jos, bytes, count)
                && jos_puts(jos, "\":", 2);
}

bool print_integer(JsonOutputStream jos, long integer)
{
        return jos_indent(jos)
                && jos_puti(jos, integer)
                && jos_put(jos, ',');
}

bool print_double(JsonOutputStream jos, double real)
{
        return jos_indent(jos)
                && jos_putr(jos, real)
                && jos_put(jos, ',');
}

bool void print_start_object(JsonOutputStream jos)
{
        return jos_indent(jos)
                && jos_put(jos, '{');
}

bool print_end_object(JsonOutputStream jos)
{
        return jos_indent_end(jos)
                && jos_put(jos, '}');
}

bool print_start_array(JsonOutputStream jos)
{
        return jos_indent(jos)
                && jos_put(jos, '[');
}

bool print_end_array(JsonOutputStream jos)
{
        return jos_indent_end(jos)
                && jos_put(jos, ']');
}


