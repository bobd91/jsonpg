#include <stddef.h>

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
//
// static void mos_reset(MemoryOutputStream mos)
// {
//         mos->count = 0;
// }
//
// static size_t mos_length(MemoryOutputStream mos)
// {
//         return mos->count;
// }
//
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

static inline Bytes mos_reserve(MemoryOutputStream mos, size_t count)
{
        if(count > mos->capacity - mos->count)
                if(!mos_grow(mos, count))
                        return NULL;

        Bytes str = mos->buffer + mos->count;
        mos->count += count;
        return str;
}

static inline bool mos_put(MemoryOutputStream mos, Byte chr)
{
        Bytes s = mos_reserve(mos, 1);
        if(!s)
                return false;
        s[0] = chr;
        return true;
}

static inline bool mos_putn(MemoryOutputStream mos, Byte chr, size_t count)
{
        if(count) {
                Bytes s = mos_reserve(mos, count);
                if(!s)
                        return false;
                memset(s, chr, count);
        }
        return true;
}

static inline bool mos_puts(MemoryOutputStream mos, CBytes string, size_t count)
{
        Bytes s = mos_reserve(mos, count);
        if(!s)
                return false;
        memcpy(s, string, count);
        return true;
}
//
// static Bytes mos_pop(MemoryOutputStream mos)
// {
//         mos->count = 0;
//         return mos->buffer;
// }
//

static inline void mos_adjust(MemoryOutputStream mos, long amount)
{
        ASSERT((long)(mos->count) + amount >= 0);

        // Keeping compilers happy :(
        mos->count = (size_t)((long)(mos->count) + amount);
}

typedef struct json_output_stream_s *JsonOutputStream;

struct json_output_stream_s {
        MemoryOutputStream mos;
        Generator generator;
        unsigned indent;
        unsigned level;
        bool nl;
        bool comma;
        bool key;
};

static JsonOutputStream jos_new(Allocator a, unsigned indent, Generator g)
{       
        JsonOutputStream jos = allocator_alloc(a, sizeof(struct json_output_stream_s));
        if(!jos)
                return NULL;

        jos->mos = mos_new(a, 0);
        if(!jos->mos)
                return NULL;
      
        jos->generator = g;
        jos->indent = indent;
        jos->nl = false;
        jos->comma = false;
        jos->key = false;
        jos->level = 0;

        return jos;
}

static inline bool jos_put(JsonOutputStream jos, Byte chr)
{
        return mos_put(jos->mos, chr);
}
//
// static inline bool jos_putn(JsonOutputStream jos, Byte chr, size_t count)
// {
//         return mos_putn(jos->mos, chr, count);
// }
//
static inline bool jos_puts(JsonOutputStream jos, CBytes string, size_t count)
{
        return mos_puts(jos->mos, string, count);
}

static inline size_t find_next_special(CBytes string, size_t count, size_t start)
{
        size_t i;
        for(i = start ; i < count ; i++) {
                Byte chr = string[i];
                if(chr == '"' || chr == '\\' || chr < 0x20 || chr > 0x7F)
                        return i;
        }
        return i;
}

static inline bool jos_scan_escape(JsonOutputStream jos, CBytes string, size_t count)
{
        static char const * const s_escapes[] = {
                "00", "01", "02", "03",
                "04", "05", "06", "07",
                NULL, NULL, NULL, "0B",
                NULL, NULL, "0E", "0F",
                "10", "11", "12", "13",
                "14", "15", "16", "17",
                "18", "19", "1A", "1B",
                "1C", "1D", "1E", "1F"
        };

        static unsigned char const c_escapes[] = {
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
        while(count > (pmos2 = find_next_special(string, count, pmos1))) {
                chr = string[pmos2];

                if(!mos_puts(mos, string + pmos1, pmos2 - pmos1))
                        return false;
                
                if(chr > 0x7F) {
                        int len = utf8_validate_sequence(string + pmos2, count - pmos2);
                        if(len == -1) {
                                jos->generator->error = make_error(JSONPG_ERROR_UTF8, 0);
                                return false;
                        }
                        if(!mos_puts(mos, string + pmos2, len))
                                return false;
                        pmos1 = pmos2 + len;
                } else {
                        // chr will be < 0x20, '"' or '\\'
                        Byte e = c_escapes[chr];
                        if(e) {
                                s = mos_reserve(mos, 2);
                                if(!s)
                                        return false;
                                s[0] = '\\';
                                s[1] = e;
                        } else {
                                s = mos_reserve(mos, 6);
                                if(!s)
                                        return false;
                                s[0] = '\\';
                                s[1] = 'u';
                                s[2] = '0';
                                s[3] = '0';
                                s[4] = (Byte)s_escapes[chr][0];
                                s[5] = (Byte)s_escapes[chr][1];
                        }
                        pmos1 = pmos2 + 1;
                }
        }

        return mos_puts(mos, string + pmos1, pmos2 - pmos1);
}

static inline bool jos_puti(JsonOutputStream jos, long integer)
{
        static unsigned buf_len = i64toa_min_buffer_length;

        char *s = (char *)mos_reserve(jos->mos, buf_len);
        if(!s)
                return false;
        long count = i64toa(integer, s) - s;

        ASSERT(count <= buf_len);
        mos_adjust(jos->mos, count - buf_len);

        return true;
}

static inline bool jos_putr(JsonOutputStream jos, double real)
{
        static unsigned buf_len = dtoa_min_buffer_length;

        char *s = (char *)mos_reserve(jos->mos, buf_len);
        if(!s)
                return false;

        long count = dtoa(s, real) - s;

        ASSERT(count <= buf_len);
        mos_adjust(jos->mos, count - buf_len);

        return true;
}

static inline bool jos_indent(JsonOutputStream jos)
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

static inline bool jos_prefix(JsonOutputStream jos)
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

static inline bool jos_prefix_start(JsonOutputStream jos)
{
        if(!jos_prefix(jos))
                return false;
        jos->comma = false;
        jos->level++;

        return true;
}

static inline bool jos_prefix_end(JsonOutputStream jos)
{
        jos->level--;

        if(jos->comma) {
                jos->comma = false;
                if(!jos_prefix(jos))
                        return false;
        }
        jos->comma = true;

        return true;
}

static inline bool jos_key_suffix(JsonOutputStream jos)
{
        if(!mos_put(jos->mos, ':'))
                return false;
        if(jos->indent && !mos_put(jos->mos, ' '))
                return false;

        jos->key = true;

        return true;
}




static inline bool print_null(void *ctx)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                && jos_puts(jos, (CBytes)"null", 4);
}

static inline bool print_boolean(void *ctx, bool is_true)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                        && (is_true
                                ? jos_puts(jos, (CBytes)"true", 4)
                                : jos_puts(jos, (CBytes)"false", 5));
}

static inline bool print_string(void *ctx, Bytes bytes, size_t count)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                && jos_put(jos, '"')
                && jos_scan_escape(jos, bytes, count)
                && jos_put(jos, '"');
}

static inline bool print_key(void *ctx, Bytes bytes, size_t count)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                && jos_put(jos, '"')
                && jos_scan_escape(jos, bytes, count)
                && jos_put(jos, '"')
                && jos_key_suffix(jos);
}

static inline bool print_integer(void *ctx, long integer)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                && jos_puti(jos, integer);
}

static inline bool print_real(void *ctx, double real)
{
        JsonOutputStream jos = ctx;

        return jos_prefix(jos)
                && jos_putr(jos, real);
}

static inline bool print_start_object(void *ctx)
{
        JsonOutputStream jos = ctx;

        return jos_prefix_start(jos)
                && jos_put(jos, '{');
}

static inline bool print_end_object(void *ctx)
{
        JsonOutputStream jos = ctx;

        return jos_prefix_end(jos)
                && jos_put(jos, '}');
}

static inline bool print_start_array(void *ctx)
{
        JsonOutputStream jos = ctx;

        return jos_prefix_start(jos)
                && jos_put(jos, '[');
}

static inline bool print_end_array(void *ctx)
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
        JsonOutputStream jos = jos_new(g->allocator, indent, g);
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
