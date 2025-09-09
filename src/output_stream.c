
#define MOS_DEFAULT_CAPACITY 4096;

typedef struct memory_output_stream_s *MemoryOutputStream;

struct memory_output_stream_s {
        Allocator allocator;
        size_t initial_capacity;
        size_t capacity;
        size_t count;
        Bytes buffer;
};

static MemoryOutputStream mos_new(Allocator a, size_t initial_capacity)
{
        MemoryOutputStream mos = allocator_alloc(a, 
                                        sizeof(struct memory_output_stream_s));
        if(!mos)
                return NULL;

        mos->allocator = a;
        mos->initial_capacity = initial_capacity > 0 
                                ? initial_capacity 
                                : MOS_DEFAULT_CAPACITY;
        mos->capacity = 0;
        mos->count = 0;
        mos->buffer = NULL;

        return mos;
}

static void mos_reset(MemoryOutputStream mos)
{
        mos->count = 0;
}

static size_t mos_length(MemoryOutputStream mos)
{
        return mos->count;
}

static Bytes mos_grow(MemoryOutputStream mos, size_t incr)
{
        size_t size = mos->capacity 
                ? mos->capacity << 1
                : mos->initial_capacity;

        size_t required = mos->count + incr;

        while(size < required)
                size <<= 1;

        Bytes new;
        if(mos->buffer)
                new = allocator_realloc(mos->allocator, mos->buffer, size);
        else
                new = allocator_alloc(mos->allocator, size);

        if(new) {
                mos->buffer = new;
                mos->capacity = size;
        }

        return new;
}

static Bytes mos_reserve(MemoryOutputStream mos, size_t count)
{
        if(count > mos->capacity - mos->count)
                if(!mos_grow(mos, count))
                        return NULL;

        Bytes str = mos->buffer + mos->count;
        mos->count += count;
        return str;
}

static bool mos_put(MemoryOutputStream mos, Byte chr)
{
        Bytes s = mos_reserve(mos, 1);
        if(!s)
                return false;
        s[0] = chr;
        return true;
}

static bool mos_putn(MemoryOutputStream mos, Byte chr, size_t count)
{
        Bytes s = mos_reserve(mos, count);
        if(!s)
                return false;
        memset(s, chr, count);
        return true;
}

static bool mos_puts(MemoryOutputStream mos, Bytes string, size_t count)
{
        Bytes s = mos_reserve(mos, count);
        if(!s)
                return false;
        memcpy(s, string, count);
        return true;
}

static Bytes mos_pop(MemoryOutputStream mos)
{
        mos->count = 0;
        return mos->buffer;
}

static void mos_adjust(MemoryOutputStream mos, ssize_t amount)
{
        ASSERT(mos->count + amount >= 0);

        mos->count += amount;
}

typedef struct json_output_stream_s *JsonOutputStream;

struct json_output_stream_s {
        MemoryOutputStream mos;
        unsigned indent;
        unsigned level;
        bool nl;
        bool comma;
        bool key;
};

static JsonOutputStream jos_new(Allocator a, unsigned indent)
{       
        JsonOutputStream jos = allocator_alloc(a, sizeof(struct json_output_stream_s));
        if(!jos)
                return NULL;

        jos->mos = mos_new(a, 0);
        if(!jos->mos)
                return NULL;
      
        jos->indent = indent;
        jos->nl = false;
        jos->comma = false;
        jos->key = false;
        jos->level = 0;

        return jos;
}

static bool jos_put(JsonOutputStream jos, Byte chr)
{
        return mos_put(jos->mos, chr);
}
//
// static bool jos_putn(JsonOutputStream jos, Byte chr, size_t count)
// {
//         return mos_putn(jos->mos, chr, count);
// }
//
static bool jos_puts(JsonOutputStream jos, Bytes string, size_t count)
{
        return mos_puts(jos->mos, string, count);
}

static size_t find_next_escape(Bytes string, size_t count, size_t start)
{
        int i;
        for(i = start ; i < count ; i++) {
                Byte chr = string[i];
                if(chr == '"' || chr == '\\' || chr < 0x20)
                        return i;
        }
        return i;
}

static bool jos_escape(JsonOutputStream jos, Bytes string, size_t count)
{
        // gcc wont let me initialise unsigned char *[] from literal strings
        // so have to cast later 
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

        static Byte c_escapes[] = {
                [0x08] = 'b', [0x09] = 't', [0x0A] = 'n',
                [0x0C] = 'f', [0x0D] = 'r', ['"'] = '"',
                ['\\'] = '\\'
        };

        MemoryOutputStream mos = jos->mos;
        size_t pmos1 = 0;
        size_t pmos2 = 0;
        Byte chr;
        Bytes s;

        // TODO: find/validate outgoing UTF8 sequences 
        //       as assert or flag?
        while(count > (pmos2 = find_next_escape(string, count, pmos1))) {
                chr = string[pmos2];

                if(!mos_puts(mos, string + pmos1, pmos2 - pmos1))
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
                        Bytes es = (Bytes)s_escapes[chr];
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
        return mos_puts(mos, string + pmos1, pmos2 - pmos1);
}

static bool jos_puti(JsonOutputStream jos, long integer)
{
        char *s = (char *)mos_reserve(jos->mos, 20);
        if(!s)
                return false;
        size_t count = i64toa(integer, s) - s;

        ASSERT(count <= 20);
        mos_adjust(jos->mos, 20 - count);

        return true;
}

static bool jos_putr(JsonOutputStream jos, double real)
{
        char *s = (char *)mos_reserve(jos->mos, 25);
        if(!s)
                return false;
        size_t count = dtoa(real, s, 0) - s;

        ASSERT(count <= 25);
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

static bool jos_prefix_start(JsonOutputStream jos)
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
        ASSERT(jos->level >= 0);

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
        if(jos->indent && !mos_put(jos->mos, ' '))
                return false;

        jos->key = true;

        return true;
}




static bool print_null(void *ctx)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                && jos_puts(jos, (Bytes)"null", 4);
}

static bool print_boolean(void *ctx, bool is_true)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                        && (is_true
                                ? jos_puts(jos, (Bytes)"true,", 5)
                                : jos_puts(jos, (Bytes)"false,", 6));
}

static bool print_string(void *ctx, Bytes bytes, size_t count)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                && jos_put(jos, '"')
                && jos_escape(jos, bytes, count)
                && jos_put(jos, '"');
}

static bool print_key(void *ctx, Bytes bytes, size_t count)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                && jos_put(jos, '"')
                && jos_escape(jos, bytes, count)
                && jos_put(jos, '"')
                && jos_key_suffix(jos);
}

static bool print_integer(void *ctx, long integer)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                && jos_puti(jos, integer)
                && jos_put(jos, ',');
}

static bool print_real(void *ctx, double real)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                && jos_putr(jos, real)
                && jos_put(jos, ',');
}

static bool print_start_object(void *ctx)
{
        JsonOutputStream jos = ctx;

        return jos_prefix_start(jos)
                && jos_put(jos, '{');
}

static bool print_end_object(void *ctx)
{
        JsonOutputStream jos = ctx;

        return jos_prefix_end(jos)
                && jos_put(jos, '}');
}

static bool print_start_array(void *ctx)
{
        JsonOutputStream jos = ctx;

        return jos_prefix_start(jos)
                && jos_put(jos, '[');
}

static bool print_end_array(void *ctx)
{
        JsonOutputStream jos = ctx;

        return jos_prefix_end(jos)
                && jos_put(jos, ']');
}

static Callbacks print_callbacks = {
        .boolean = print_boolean,
        .null = print_null,
        .integer = print_integer,
        .real = print_real,
        .string = print_string,
        .key = print_key,
        .start_object = print_start_object,
        .end_object = print_end_object,
        .start_array = print_start_array,
        .end_array = print_end_array
};


static Generator json_generator(Generator g, unsigned indent)
{
        JsonOutputStream jos = jos_new(g->allocator, indent);
        if(!jos)
                return NULL;

        return generator_set_callbacks(g, &print_callbacks, jos);
}


char *jsonpg_result_string(Generator g)
{
        JsonOutputStream jos = g->ctx;
        return jos_put(jos, '\0')
                ? (char *)jos->mos->buffer
                : NULL;
}

size_t jsonpg_result_bytes(Generator g, Bytes *bytes_result)
{
        JsonOutputStream jos = g->ctx;
        *bytes_result = jos->mos->buffer;
        return jos->mos->count;
}
