#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../src/jsonpg.h"

#include "../src/jsonpg_def_macros.h"

#define test_start() jsonpg_generator gen = ctx
#define test_end()   return 0;

int test_null(void *ctx)
{
        test_start();

        null();

        test_end();
}

int test_boolean(void *ctx, bool is_true)
{
        test_start();

        if(is_true)
                true();
        else
                false();
        
        test_end();
}

int test_integer(void *ctx, long l)
{
        test_start();

        integer(l);

        test_end();
}

int test_real(void *ctx, double d)
{
        test_start();

        real(d);

        test_end();
}

int test_string(void *ctx, uint8_t *bytes, size_t count)
{
        test_start();

        str_bytes(bytes, count);

        test_end();
}

int test_key(void *ctx, uint8_t *bytes, size_t count)
{
        test_start();

        key_bytes(bytes, count);

        test_end();
}

int test_begin_object(void *ctx)
{
        test_start();

        begin_object();

        test_end();
}

int test_end_object(void *ctx)
{
        test_start();

        end_object();

        test_end();
}

int test_begin_array(void *ctx)
{
        test_start();

        begin_array();

        test_end();
}

int test_end_array(void *ctx)
{
        test_start();

        end_array();

        test_end();
}

#include "../src/jsonpg_undef_macros.h"

void *ctx_generator(void) {
        return jsonpg_generator_new(.max_nesting = 0);
}

jsonpg_callbacks test_callbacks = {
        .null = test_null,
        .boolean = test_boolean,
        .integer = test_integer,
        .real = test_real,
        .string = test_string,
        .key = test_key,
        .begin_object = test_begin_object,
        .end_object = test_end_object,
        .begin_array = test_begin_array,
        .end_array = test_end_array
};

void fail(char *msg)
{
        fprintf(stderr, msg);
        exit(1);
}

jsonpg_value parse_solution(int soln, FILE *fh)
{
        // Input - 
        //      buffer
        //      dom (covered by dom output tests below)
        //
        // Output (not JSON, with/without validation) -
        //      dom (1 - 2)
        //      callback (specified in parse) (3 - 4)
        //      callback (generator) (5 - 6)
        //
        // Output (pretty/not pretty, with/without validation) -
        //         buffer (7 - 10)
        //
        bool create_dom = false;
        bool parse_callback = false;
        jsonpg_generator g = NULL;
        jsonpg_generator ctx_g = NULL;

        if(soln == 1) {
                create_dom = true;
                g = jsonpg_generator_new(.dom = true);
        } else if(soln == 2) {
                create_dom = true;
                g = jsonpg_generator_new(.dom = true, .max_nesting = 0);
        } else if (soln < 5) {
                parse_callback = true;
        } else if (soln == 5) {
                ctx_g = ctx_generator();
                g = jsonpg_generator_new(
                                .callbacks = &test_callbacks,
                                .ctx = ctx_g);
        } else if (soln == 6) {
                ctx_g = ctx_generator();
                g = jsonpg_generator_new(
                                .callbacks = &test_callbacks,
                                .ctx = ctx_g,
                                .max_nesting = 0);
        } else if (soln == 7) {
                g = jsonpg_generator_new(
                                .indent = 4);
        } else if(soln == 8) {
                g = jsonpg_generator_new();
        } else if(soln == 9) {
                g = jsonpg_generator_new(
                                .indent = 4,
                                .max_nesting = 0);
        } else {
                g = jsonpg_generator_new(
                                .max_nesting = 0);
        }

        jsonpg_value res;

        fseek(fh, 0L, SEEK_END);
        long length = ftell(fh);
        rewind(fh);
        uint8_t *buf = malloc(length + 1);
        if(!buf)
                fail("Failed to allocate memory to read file content");

        fread(buf, length, 1, fh);

        if(create_dom) {
                res = jsonpg_parse(.bytes = buf, .count = length, .generator = g);
                ctx_g = ctx_generator();
                if(res.type == JSONPG_EOF) {
                        res = jsonpg_parse(
                                        .dom = jsonpg_result_dom(g),
                                        .generator = ctx_g);
                }
        } else if(parse_callback) {
                ctx_g = ctx_generator();
                res = jsonpg_parse(.bytes = buf, .count = length,
                                .callbacks = &test_callbacks,
                                .ctx = ctx_g);
        } else {
                res = jsonpg_parse(.bytes = buf, 
                                .count = length, 
                                .generator = g);
        }
        free(buf);
        if(ctx_g)
                printf("%s", jsonpg_result_string(ctx_g));
        else
                printf("%s", jsonpg_result_string(g));

        jsonpg_generator_free(g);
        jsonpg_generator_free(ctx_g);

        return res;     
}

void usage(char *progname)
{       
        printf("%s [-s <solution number>] <json filename>\n\n", progname);
        printf("Where solution number (default: 8) is:\n");
        printf("  N - parse/generate route [Stringified | Prettified : Validated | Not Validated]\n"); 
        printf("  1 - byte buffer => dom => stdout                [S:V]\n");
        printf("  2 - byte buffer => dom => stdout                [S:N]\n");
        printf("  3 - byte buffer => parse/callback => stdout     [S:V]\n");
        printf("  4 - byte buffer => parse/callback => stdout     [S:N]\n");
        printf("  5 - byte buffer => generator/callback => stdout [S:V]\n");
        printf("  6 - byte buffer => generator/callback => stdout [S:N]\n");
        printf("  7 - byte buffer => buffer => stdout             [P:V]\n");
        printf("  8 - byte buffer => buffer => stdout             [S:V]\n");
        printf("  9 - byte buffer => buffer => stdout             [P:N]\n");
        printf(" 10 - byte buffer => buffer => stdout             [S:N]\n");
}
                
int main(int argc, char *argv[]) {
        int soln = 0;

        if(2 == argc) {
                if(0 == strcmp("-h", argv[1])) {
                        usage(argv[0]);
                        exit(0);
                } else {
                        soln = 8;
                }
        } else if(4 == argc && 0 == strcmp("-s", argv[1])) {
                long l = strtol(argv[2], NULL, 10);
                if(l > 0 && l < 11)
                        soln = l;
        }

        if(!soln)
                fail("Usage: jsonpg [-s solution] infile\n");


        char *infile = argv[(2 == argc) ? 1 : 3];
        FILE *fh = fopen(infile, "rb");
        if(!fh)
                fail("Failed to open input file\n");



        jsonpg_value v = parse_solution(soln, fh);
        fclose(fh);
        int ret = (v.type == JSONPG_EOF) ? 0 : 1;
        if(ret)
                printf("Type: %d, Returned %d\n", v.type, ret);
        else
                printf("\n");
        return ret;
}


                        



