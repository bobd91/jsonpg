#include <assert.h>
#include <string.h>

typedef struct str_buf_s *str_buf;

struct str_buf_s {
        arena arena;
        uint8_t *bytes;
        size_t count;
        size_t size;    
};

static void str_buf_reset(str_buf sbuf)
{
        sbuf->count = 0;
}

static str_buf str_buf_new(arena a, size_t size)
{
        str_buf sbuf = arena_alloc(a, sizeof(struct str_buf_s));
        if(!sbuf) {
                return NULL;
        }
        sbuf->arena = a;
        sbuf->count = 0;
        size = size >= BUF_SIZE ? size : BUF_SIZE;
        sbuf->bytes = arena_alloc(sbuf->arena, size);
        if(!sbuf->bytes) {
                arena_free(sbuf->arena);
                return NULL;
        }
        sbuf->size = size;
        return sbuf;
}

static int str_buf_realloc(str_buf sbuf, int new_count)
{
        size_t new_size = sbuf->size;
        do {
                new_size <<= 1;
        } while(new_count > new_size);
        uint8_t *b = arena_realloc(
                        sbuf->arena, 
                        sbuf->bytes, 
                        new_size);
        if(!b)
                return -1;

        sbuf->size = new_size;
        sbuf->bytes = b;
        return 0;
}

static int str_buf_append(str_buf sbuf, const uint8_t *bytes, size_t count)
{
        int new_count = sbuf->count + count;

        if(new_count > sbuf->size) {
                if(-1 == str_buf_realloc(sbuf, new_count))
                                return -1;
        }
        memcpy(sbuf->bytes + sbuf->count, bytes, count);
        sbuf->count += count;

        return 0;
}

static int str_buf_append_chars(str_buf sbuf, char *str)
{
        return str_buf_append(sbuf, (uint8_t *)str, strlen(str));
}

static int str_buf_append_c(str_buf sbuf, char c)
{
        if(sbuf->count == sbuf->size) {
                if(-1 == str_buf_realloc(sbuf, sbuf->size))
                        return -1;
        }
        sbuf->bytes[sbuf->count++] = c;
        return 0;
}

static size_t str_buf_content(str_buf sbuf, uint8_t **bytes)
{
        if(sbuf->count) {
                *bytes = sbuf->bytes;
                return sbuf->count;
        } else {
                *bytes = NULL;
                return 0;
        }
}

static char *str_buf_content_str(str_buf sbuf)
{
        if(sbuf->count) {
                if(sbuf->bytes[sbuf->count])
                        str_buf_append_c(sbuf, '\0');
                return (char *)sbuf->bytes;
        }
        return "";
}
