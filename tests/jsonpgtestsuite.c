#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../include/jsonpg.h"

#include "../include/jsonpg_def_macros.h"

#define test_start() JsonpgGenerator gen = ctx
#define test_end()   return true;

bool test_null(void *ctx)
{
        test_start();

        null();

        test_end();
}

bool test_boolean(void *ctx, bool is_true)
{
        test_start();

        boolean(is_true);
        
        test_end();
}

bool test_integer(void *ctx, long l)
{
        test_start();

        integer(l);

        test_end();
}

bool test_real(void *ctx, double d)
{
        test_start();

        real(d);

        test_end();
}

bool test_string(void *ctx, uint8_t *bytes, size_t count)
{
        test_start();

        string_bytes(bytes, count);

        test_end();
}

bool test_key(void *ctx, uint8_t *bytes, size_t count)
{
        test_start();

        key_bytes(bytes, count);

        test_end();
}

bool test_start_object(void *ctx)
{
        test_start();

        start_object();

        test_end();
}

bool test_end_object(void *ctx)
{
        test_start();

        end_object();

        test_end();
}

bool test_start_array(void *ctx)
{
        test_start();

        start_array();

        test_end();
}

bool test_end_array(void *ctx)
{
        test_start();

        end_array();

        test_end();
}

#include "../include/jsonpg_undef_macros.h"

void *ctx_generator(void) {
        return jsonpg_generator_new(.max_nesting = 0);
}

JsonpgCallbacks test_callbacks = {
        .null = test_null,
        .boolean = test_boolean,
        .integer = test_integer,
        .real = test_real,
        .string = test_string,
        .key = test_key,
        .start_object = test_start_object,
        .end_object = test_end_object,
        .start_array = test_start_array,
        .end_array = test_end_array
};

void fail(char *msg)
{
        fprintf(stderr, msg);
        exit(1);
}

void run_parse_next(JsonpgParser p, JsonpgGenerator g)
{
        bool abort = false;
        JsonpgResult res;
        while(!(abort || JSONPG_EOF == jsonpg_parse_next(p))) {
                res = jsonpg_parse_result(p);
                switch(res.type) {
                case JSONPG_TRUE:
                case JSONPG_FALSE:
                        abort = !jsonpg_boolean(g, res.type == JSONPG_TRUE);
                        break;
                case JSONPG_NULL:
                        abort = !jsonpg_null(g);
                        break;
                case JSONPG_STRING:
                        abort = !jsonpg_string(g, res.string.bytes, res.string.count);
                        break;
                case JSONPG_KEY:
                        abort = !jsonpg_key(g, res.string.bytes, res.string.count);
                        break;
                case JSONPG_INTEGER:
                        abort = !jsonpg_integer(g, res.number.integer);
                        break;
                case JSONPG_REAL:
                        abort = !jsonpg_real(g, res.number.real);
                        break;
                case JSONPG_START_ARRAY:
                        abort = !jsonpg_start_array(g);
                        break;
                case JSONPG_END_ARRAY:
                        abort = !jsonpg_end_array(g);
                        break;
                case JSONPG_START_OBJECT:
                        abort = !jsonpg_start_object(g);
                        break;
                case JSONPG_END_OBJECT:
                        abort = !jsonpg_end_object(g);
                        break;
                default:
                        abort = true;
                }
        }
}


JsonpgResult parse_solution(int soln, FILE *fh)
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
        JsonpgGenerator g = NULL;
        JsonpgGenerator ctx_g = NULL;

        if(soln < 3) {
                create_dom = true;
                g = jsonpg_generator_new(.dom = true);
        } else if (soln < 4) { // not < 5 as 4 is parse_next an it cannot do this
                parse_callback = true;
        } else if (soln < 7) {
                ctx_g = ctx_generator();
                g = jsonpg_generator_new(
                                .callbacks = &test_callbacks,
                                .ctx = ctx_g);
        } else if (soln < 9) {
                g = jsonpg_generator_new(
                                .indent = 4);
        } else if(soln < 11) {
                g = jsonpg_generator_new();
        }

        JsonpgResult res;

        fseek(fh, 0L, SEEK_END);
        long length = ftell(fh);
        rewind(fh);
        uint8_t *buf = malloc(length + 1);
        if(!buf)
                fail("Failed to allocate memory to read file content");

        fread(buf, length, 1, fh);

        if(soln % 2) {
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
        } else {
                JsonpgParser p = NULL;
                if(create_dom) {
                        res = jsonpg_parse(.bytes = buf, .count = length, .generator = g);
                        if(res.type == JSONPG_EOF) {
                                ctx_g = ctx_generator();
                                p = jsonpg_parser_new(.dom = jsonpg_result_dom(g));
                                run_parse_next(p, ctx_g);
                        } else {
                                printf("Returned type: %d\n", res.type);
                                fail("Failed to create DOM\n");
                        }
                } else {
                        p = jsonpg_parser_new(.bytes = buf, .count = length);
                        run_parse_next(p, g);
                }
                res = jsonpg_parse_result(p);
                jsonpg_parser_free(p);
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
        printf("Where solution number (default: 9) is:\n");
        printf("  N - parse/generate route [Stringified | Prettified : Parse | parse Next]\n"); 
        printf("  1 - byte buffer => dom => stdout                [S:P]\n");
        printf("  2 - byte buffer => dom => stdout                [S:N]\n");
        printf("  3 - byte buffer => parse/callback => stdout     [S:P]\n");
        printf("  4 - No Parse Next solution, treat as N = 6      [S:N]\n");
        printf("  5 - byte buffer => generator/callback => stdout [S:P]\n");
        printf("  6 - byte buffer => generator/callback => stdout [S:N]\n");
        printf("  7 - byte buffer => buffer => stdout             [P:P]\n");
        printf("  8 - byte buffer => buffer => stdout             [P:N]\n");
        printf("  9 - byte buffer => buffer => stdout             [S:P]\n");
        printf(" 10 - byte buffer => buffer => stdout             [S:N]\n");
}
                
int main(int argc, char *argv[]) {
        int soln = 0;

        if(2 == argc) {
                if(0 == strcmp("-h", argv[1])) {
                        usage(argv[0]);
                        exit(0);
                } else {
                        soln = 9;
                }
        } else if(4 == argc && 0 == strcmp("-s", argv[1])) {
                long l = strtol(argv[2], NULL, 10);
                if(l > 0 && l < 11)
                        soln = l;
        }

        if(!soln)
                fail("Usage: jsonpg [-s solution (1-10)] infile\n       jsonpg -h\n");


        char *infile = argv[(2 == argc) ? 1 : 3];
        FILE *fh = fopen(infile, "rb");
        if(!fh)
                fail("Failed to open input file\n");



        JsonpgResult v = parse_solution(soln, fh);
        fclose(fh);
        int ret = (v.type == JSONPG_EOF) ? 0 : 1;
        if(ret)
                printf("Type: %d, Returned %d\n", v.type, ret);
        else
                printf("\n");
        return ret;
}


                        



