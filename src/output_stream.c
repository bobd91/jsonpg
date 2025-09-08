

MemoryOutputStream mos_new(Allocator a, size_t initial_capacity)
{
        MemoryOutputStream mos = memory_allocate(a, 
                                        sizeof(struct memory_output_stream_s));
        if(!mos)
                return NULL;
        mos->allocator = a
        mos->initial_capacity = initial_capacity > 0 
                                ? initial_capacity 
                                : JSONPG_DEFAULT_MOS_CAPACITY;
        mos->capacity = 0;
        mos->count = 0;
        mos->buffer = NULL;

        return mos;
}

void mos_reset(MemoryOutputStream mos)
{
        mos->count = 0;
}

size_t mos_length(MemoryOutputStream mos)
{
        return mos->count;
}

bool mos_put(MemoryOutputStream mos, Byte chr)
{
        Bytes s = mos_reserve(mos, 1);
        if(!s)
                return false;
        s[0] = chr;
        return true;
}

bool mos_putn(MemoryOutputStream mos, Byte chr, size_t count)
{
        Bytes s = mos_reserve(mos, count);
        if(!s)
                return false;
        memset(s, chr, count);
        return true;
}

bool mos_puts(MemoryOutputStream mos, Bytes string, size_t count)
{
        Bytes s = mos_reserve(mos, count);
        if(!s)
                return false;
        memcpy(s, string, count);
        return true;
}

Bytes mos_pop(MemoryOutputStream mos)
{
        mos->count = 0;
        return mos->buffer;
}

void mos_adjust(MemoryOutputStream mos, ssize_t amount)
{
        JSONPG_ASSERT(mos->count + amount >= 0);

        mos->count += amount;
}

Bytes mos_grow(MemoryOutputStream mos, size_t incr)
{
        size_t size = mos->capacity 
                ? mos->capacity << 1
                : mos->initial_capacity;

        size_t required = mos->count + incr;

        while(size < required)
                size << 1;

        Bytes new;
        if(mos->buffer)
                new = memory_reallocate(mos->allocator, buffer, size);
        else
                new = memory_allocate(mos->allocator, size);

        if(new) {
                mos->buffer = new;
                mos->capacity = size;
        }

        return new;
}


Bytes mos_reserve(MemoryOutputStream mos, size_t count)
{
        Bytes str = mos->buffer + mos->count;

        if(count > mos->capacity - mos->count)
                if(!mos_grow(mos, count))
                        return NULL;
        mos->count += count;
        return str;
}

JsonOuputStream jos_new(Allocator a, MemoryOutputStream mos, unsigned indent)
{       
        JsonOutputStream jos = allocator_alloc(a, sizeof(struct json_output_stream_c));
        if(!jos)
                return NULL;
        jos->mos = mos;
        jos->indent = indent;
        jos->nl = false;
        jos->comma = false;
        jos->key = false;
        jos->level = 0;
}

bool jos_put(JsonOutputStream jos, Byte chr)
{
        return mos_put(jos->mos, chr);
}

bool jos_putn(JsonOutputStream jos, Byte chr, size_t count)
{
        return mos_putn(jos->mos, chr, count);
}

bool jos_puts(JsonOutputStream jos, Bytes string, size_t count)
{
        return mos_puts(jos->mos, string, count);
}

size_t find_next_escape(Bytes string, size_t count, size_t start)
{
        for(int i = start ; i < count ; i++) {
                Byte chr = string[i];
                if(chr == '"' || chr == '\\' || chr < 0x20)
                        return i;
        }
        return i;
}

bool jos_escape(JsonOutputStream jos, Bytes string, size_t count)
{
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

        MemoryOutputStream mos = jos->mos;
        size_t pmos1 = 0;
        size_t pmos2 = 0;
        Byte chr;
        Bytes s;

        while(count > (pmos2 = find_next_escape(string, count, pmos1))) {
                chr = string[pmos2];

                if(!jos_puts(jos, string + pmos1, pmos2 - pmos1))
                        return false;
                
                // chr will be < 0x20, '"' or '\\'
                Byte e = c_escapes[chr];
                if(e) {
                        s = mos_reserve(mos, 2);
                        if(!s)
                                return false;
                        s[0] = '\\';
                        s[1] = e;
                } else {
                        Bytes es = s_escapes[chr];
                        Bytes s = mos_reserve(mos, 6);
                        if(!s)
                                return false;
                        s[0] = '\\';
                        s[1] = 'u';
                        s[2] = '0';
                        s[3] = '0';
                        s[4] = es[0];
                        s[5] = es[1];
                }

                pmos1 = pmos2 + 1;
        }
        return jos_puts(jos, string + pmos1, pmos2 - pmos1);
}

bool jos_puti(JsonOutputStream jos, long integer)
{
        Bytes s = mos_reserve(jos->mos, 20);
        if(!s)
                return false;
        size_t count = i64toa(integer, s) - s;

        JSONPG_ASSERT(count <= 20);
        mos_adjust(jos->mos, 20 - count);

        return true;
}

bool jos_putr(JsonOutputStream jos, double real)
{
        Bytes s = mos_reserve(jos->mos, 25);
        if(!s)
                return false;
        size_t count = dtoa(real, s, 0) - s;

        JSONPG_ASSERT(count <= 25);
        mos_adjust(jos->mos, 25 - count);

        return true;
}

static bool jos_indent(JsonOutputStream jos)
{
        // Avoid leading newline
        if(jos->nl) {
                if(!mos_put(jos->mos, '\n'))
                        return false;
        } else {
                jos->nl = true;
        }
                                
        return mos_putn(jos->mos, ' ', jos->indent * jos->level);
}

static bool jos_prefix(JsonOutputStream jos)
{
        if(!jos->key) {
                if(jos->comma && !mos_put(jos->mos, ','))
                        return false;
                if(jos->indent && !jos_indent(jos))
                        return false;
        }
        jos->comma = true;
        jos->key = false;

        return true;
}

static bool jos_prefix_start(JsonOutput_stream jos)
{
        if(!jos_prefix(jos))
                return false;
        jos->comma = false;
        jos->level++;

        return true;
}

static bool jos_prefix_end(JsonOutputStream jos)
{
        jos->level--;
        JSONPG_ASSERT(jos->level >= 0);

        if(jos->comma) {
                jos->comma = false;
                if(!jos_prefix(jos))
                        return false;
        }
        jos->comma = true;

        return true;
}

static bool jos_key_suffix(JsonOutputStream jos)
{
        if(!mos_put(jos->mos, ':'))
                return false;
        if(jos_indent && !mos_put(jos->mos, ' '))
                return false;

        jos->key = true;

        return true;
}




bool print_null(JsonOutputStream jos)
{
        return jos_prefix(jos)
                && jos_puts(jos, "null", 4);
}

bool print_boolean(JsonOutputStream jos, bool is_true)
{
        return jos_prefix(jos)
                        && (is_true
                                ? jos_puts(jos, "true,", 5)
                                : jos_puts(jos, "false,", 6));
}

bool print_string_bytes(JsonOutputStream jos, Bytes bytes, size_t count)
{
        return jos_prefix(jos)
                && jos_put(jos, '"')
                && jos_escape(jos, bytes, count)
                && jos_put(jos, '"');
}

bool print_key_bytes(JsonOutputStream jos, Bytes bytes, size_t count)
{
        return jos_prefix(jos)
                && jos_put(jos, '"')
                && jos_escape(jos, bytes, count)
                && jos_put(jos, '"')
                && jos_key_suffix(jos);
}

bool print_integer(JsonOutputStream jos, long integer)
{
        return jos_prefix(jos)
                && jos_puti(jos, integer)
                && jos_put(jos, ',');
}

bool print_double(JsonOutputStream jos, double real)
{
        return jos_prefix(jos)
                && jos_putr(jos, real)
                && jos_put(jos, ',');
}

bool void print_start_object(JsonOutputStream jos)
{
        return jos_prefix_start(jos)
                && jos_put(jos, '{');
}

bool print_end_object(JsonOutputStream jos)
{
        return jos_prefix_end(jos)
                && jos_put(jos, '}');
}

bool print_start_array(JsonOutputStream jos)
{
        return jos_prefix_start(jos)
                && jos_put(jos, '[');
}

bool print_end_array(JsonOutputStream jos)
{
        return jos_prefix_end(jos)
                && jos_put(jos, ']');
}


