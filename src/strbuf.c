#include <assert.h>
#include <string.h>

typedef struct str_buf_s *str_buf;

struct str_buf_s {
        uint8_t *bytes;
        size_t count;
        size_t size;    
};


static str_buf str_buf_empty()
{
        str_buf sbuf = pg_alloc(sizeof(struct str_buf_s));
        if(!sbuf)
                return NULL;
        sbuf->bytes = NULL;
        sbuf->count = 0;
        sbuf->size = 0;
        return sbuf;
}

static str_buf str_buf_reset(str_buf sbuf)
{
        sbuf->count = 0;
        return sbuf;
}

static str_buf str_buf_alloc(str_buf sbuf, size_t size)
{
        size = size >= BUF_SIZE ? size : BUF_SIZE;

        sbuf->bytes = pg_alloc(size);
        if(!sbuf->bytes) {
                pg_dealloc(sbuf);
                return NULL;
        }
        sbuf->size = size;

        return sbuf;
}

static str_buf str_buf_alloc_new(str_buf sbuf)
{
        return str_buf_alloc(sbuf, BUF_SIZE);
}

static str_buf str_buf_new(size_t size)
{
        str_buf sbuf = str_buf_empty();
        if(!sbuf)
                return NULL;
        return str_buf_alloc(sbuf, size);
}

static void str_buf_free(str_buf sbuf)
{
        if(sbuf) {
                pg_dealloc(sbuf->bytes);
                pg_dealloc(sbuf);
        }
}

static int str_buf_append(str_buf sbuf, uint8_t *bytes, size_t count)
{
        if(!sbuf->bytes && !str_buf_alloc_new(sbuf))
                return -1;

        int new_count = sbuf->count + count;

        if(new_count > sbuf->size) {
                do {
                        sbuf->size <<= 1;
                } while(new_count > sbuf->size);
                uint8_t *b = pg_realloc(sbuf->bytes, sbuf->size);
                if(!b) {
                        str_buf_free(sbuf);
                        return -1;
                }

                sbuf->bytes = b;
        }
        memcpy(sbuf->bytes + sbuf->count, bytes, count);
        sbuf->count += count;

        return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int str_buf_append_chars(str_buf sbuf, char *str)
{
        return str_buf_append(sbuf, (uint8_t *)str, strlen(str));
}
#pragma GCC diagnostic pop

static int str_buf_append_c(str_buf sbuf, char c)
{
        return str_buf_append(sbuf, (uint8_t *)&c, 1);
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
