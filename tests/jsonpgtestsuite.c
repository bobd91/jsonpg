#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../src/jsonpg.h"

int main(int argc, char *argv[]) {
        if(2 != argc) 
                return 2;

        FILE *fh = fopen(argv[1], "rb");
        if(fh) {
                fseek(fh, 0L, SEEK_END);
                long length = ftell(fh);
                rewind(fh);
                uint8_t *buf = malloc(length + 1);
                if(buf) {
                        fread(buf, length, 1, fh);
                        jsonpg_generator g = jsonpg_generator_new(.fd = fileno(stdout), .max_nesting = 0);
                        jsonpg_value v = jsonpg_parse(.bytes = buf, .count = length, .generator = g);
                        jsonpg_generator_free(g);
                        free(buf);
                        int ret = (v.type == JSONPG_EOF) ? 0 : 1;
                        if(ret)
                                printf("Type: %d, Returned %d\n", v.type, ret);
                        else
                                printf("\n");
                        return ret;
                }
                fclose(fh);
        }
        return 2;
}

                        



