#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

static char number_buffer[32];

typedef struct print_ctx_s *print_ctx;
typedef ssize_t (*write_fn)(void *, const void *, size_t);

struct print_ctx_s {
        int level;
        int comma;
        int key;
        int pretty;
        int nl;
        char *indent;
        write_fn write;
        void *write_ctx;
        jsonpg_generator g;
};

static void set_print_error(print_ctx ctx, jsonpg_error_code code)
{
        set_generator_error(ctx->g, code);
}

static int write_utf8(print_ctx ctx, uint8_t *bytes, size_t count) 
{
        uint8_t *s = bytes;
        uint8_t *last_s = s;

        char e[3] = "\\_";
        char u[7] = "\\u0000";
        char *print_p;
        int print_w = 0;

        while((s - bytes) < count) {
                if(*s < 0x20) {
                        print_p = e;
                        print_w = 2;
                        switch(*s) {
                        case '\b':
                                e[1] = 'b';
                                break;
                        case '\t':
                                e[1] = 't';
                                break;
                        case '\n':
                                e[1] = 'n';
                                break;
                        case '\f':
                                e[1] = 'f';
                                break;
                        case '\r':
                                e[1] = 'r';
                                break;
                        default:
                                snprintf(u + 4, 3, "%.2X", (int)*s);
                                print_p = u;
                                print_w = 6;
                        }
                        s++;
                } else if(*s < 0x80) {
                        print_p = e;
                        print_w = 2;
                        switch(*s) {
                        case '"':
                                e[1] = '"';
                                break;
                        case '\\':
                                e[1] = '\\';
                                break;
                        default:
                                print_p = NULL;
                        }
                        s++;
                } else {
                        print_p = NULL;
                        int v = valid_utf8_sequence(s, count - (s - bytes));
                        if(!v) {
                                set_print_error(ctx, JSONPG_ERROR_UTF8);
                                return -1;
                        }
                        s += v;
                }
                if(print_p) {
                        // We have to print an escape sequence 
                        // first print stuff we skipped
                        if(ctx->write(ctx->write_ctx, last_s, s - last_s - 1))
                                return -1;
                        last_s = s;
                        if(ctx->write(ctx->write_ctx, print_p, print_w))
                                return -1;
                }
        }
        return ctx->write(ctx->write_ctx, last_s, s - last_s);
}

static int write_c(print_ctx ctx, char c)
{
        return ctx->write(ctx->write_ctx, &c, 1);
}

static int write_s(print_ctx ctx, char *s)
{
        return ctx->write(ctx->write_ctx, s, strlen(s));
}

static int print_indent(print_ctx ctx)
{
        // Avoid leading newline
        if(ctx->nl) {
                if(write_c(ctx, '\n'))
                        return -1;
        } else {
                ctx->nl = 1;
        }

        for(int i = 0 ; i < ctx->level ; i++)
                if(write_s(ctx, ctx->indent))
                        return -1;

        return 0;
}

static int print_prefix(print_ctx ctx)
{
        if(!ctx->key) {
                if(ctx->comma && write_c(ctx, ','))
                        return -1;
                if(ctx->pretty && print_indent(ctx))
                        return -1;
                
        }
        ctx->comma = 1;
        ctx->key = 0;

        return 0;
}

static int print_begin_prefix(print_ctx ctx)
{
        if(print_prefix(ctx))
                return -1;
        ctx->comma = 0;
        ctx->level++;

        return 0;
}

static int print_end_prefix(print_ctx ctx)
{
        ctx->level--;
        if(ctx->comma) {
                ctx->comma = 0; // no trailing comma
                if(print_prefix(ctx))
                        return -1;
        }
        ctx->comma = 1;

        return 0;
}

static int print_key_suffix(print_ctx ctx)
{
        if(write_c(ctx, ':'))
                return -1;
        if(ctx->pretty && write_c(ctx, ' '))
                return -1;
        ctx->key = 1;

        return 0;
}

static int print_boolean(void *ctx, bool is_true) 
{
        if(print_prefix(ctx)
                        || write_s(ctx, is_true ? "true" : "false"))
                return -1;
        return 0;
}

static int print_null(void *ctx) 
{
        if(print_prefix(ctx)
                        || write_s(ctx, "null"))
                return -1;
        return 0;
}

static int print_integer(void *ctx, long l) 
{
        print_prefix(ctx);
        int r = snprintf(number_buffer, sizeof(number_buffer), "%ld", l);
        if(r < 0) {
                set_print_error(ctx, JSONPG_ERROR_NUMBER);
                return -1;
        }

        write_s(ctx, number_buffer);
        return 0;
}

static int print_real(void *ctx, double d) 
{
        if(!(d == 0 || isnormal(d))) {
                set_print_error(ctx, JSONPG_ERROR_NUMBER);
                return -1;
        }
        print_prefix(ctx);
        int r = snprintf(number_buffer, sizeof(number_buffer), "%.16g", d);
        if(r < 0) {
                set_print_error(ctx, JSONPG_ERROR_NUMBER);
                return -1;
        }

        if(write_s(ctx, number_buffer))
                return -1;

        // real number without decimal point or exponent
        // add .0 at the end to preserve type at next JSON decode
        if(r == strcspn(number_buffer, ".e"))
                if(write_s(ctx, ".0"))
                        return -1;

        return 0;
}

static int print_string(void *ctx, uint8_t *bytes, size_t length)
{
        if(print_prefix(ctx)
                        || write_c(ctx, '"')
                        || write_utf8(ctx, bytes, length) 
                        || write_c(ctx, '"'))
                return -1;
        return 0;
}

static int print_key(void *ctx, uint8_t *bytes, size_t length) 
{
        if(print_string(ctx, bytes, length)
                        || print_key_suffix(ctx))
                return -1;
        return 0;
}

static int print_begin_array(void *ctx)
{
        if(print_begin_prefix(ctx) || write_c(ctx, '['))
                return -1;
        return 0;
}

static int print_end_array(void *ctx)
{
        if(print_end_prefix(ctx) || write_c(ctx, ']'))
               return -1;
        return 0;
}

static int print_begin_object(void *ctx) 
{
        if(print_begin_prefix(ctx) || write_c(ctx, '{'))
                return -1;
        return 0;
}

static int print_end_object(void *ctx) 
{
        if(print_end_prefix(ctx) || write_c(ctx, '}'))
                return -1;
        return 0;
}

static int print_error(void *ctx, jsonpg_error_code code, size_t at)
{
        fprintf(stderr, "\nError: %d [%ld]", code, at);
        return -1;
}


static jsonpg_callbacks printer_callbacks = {
        .boolean = print_boolean,
        .null = print_null,
        .integer = print_integer,
        .real = print_real,
        .string = print_string,
        .key = print_key,
        .begin_array = print_begin_array,
        .end_array = print_end_array,
        .begin_object = print_begin_object,
        .end_object = print_end_object,
        .error = print_error
};

static jsonpg_generator print_generator(
                jsonpg_generator g,
                write_fn write, 
                void *write_ctx, 
                int indent)
{
        print_ctx ctx = arena_alloc(
                        g->arena,
                        sizeof(struct print_ctx_s) + indent + 1);
        if(!ctx) {
                jsonpg_generator_free(g);
                return NULL;
        }

        g->callbacks = &printer_callbacks;
        g->ctx = ctx;
        
        ctx->level = 0;
        ctx->comma = 0;
        ctx->key = 0;
        if(indent) {
                ctx->pretty = true;
                ctx->indent = sizeof(struct print_ctx_s) + (void *)ctx;
                for(int i = 0; i < indent ;i++)
                        ctx->indent[i] = ' ';
                ctx->indent[indent] = '\0';
        } else {
                ctx->pretty = false;
        }
        ctx->nl = 0;
        ctx->write = write;
        ctx->write_ctx = write_ctx;

        // For reporting generator errors
        ctx->g = g;

        return g;
}

static ssize_t write_fd(void *ctx, const void *bytes, size_t count)
{
        int fd = CTX_TO_INT(ctx);
        const uint8_t *start = bytes;
        size_t size = count;
        while(size) {
                size_t w = write(fd, start, size);
                if(w < 0) {
                        set_print_error(ctx, JSONPG_ERROR_FILE_WRITE);
                        return -1;
                }
                start += w;
                size -= w;
        }
        return 0;
}

char *jsonpg_result_string(jsonpg_generator g)
{
        print_ctx ctx = g->ctx;
        return str_buf_content_str(ctx->write_ctx);
}

size_t jsonpg_result_bytes(jsonpg_generator g, uint8_t **bytes)
{
        return str_buf_content(g->ctx, bytes);
}

static ssize_t write_buffer(void *ctx, const void *bytes, size_t count)
{
        str_buf sbuf = ctx;
        return str_buf_append(sbuf, bytes, count);
}

static jsonpg_generator file_printer(jsonpg_generator g, int fd, int indent)
{
        return print_generator(g, write_fd, INT_TO_CTX(fd), indent);
}

static jsonpg_generator buffer_printer(jsonpg_generator g, int indent)
{
        str_buf sbuf = str_buf_new(g->arena, 0);
        if(!sbuf)
                return NULL;

        return print_generator(g, write_buffer, sbuf, indent);
}

static jsonpg_generator write_printer(jsonpg_generator g, jsonpg_writer writer, int indent)
{
        return print_generator(g, writer->write, writer->ctx, indent);

}

