#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/jsonpg.h"


char *type_name(jsonpg_type type) {
        static char *names[] = {
                "None",
                "Pull",
                "Null",
                "False",
                "True",
                "Integer",
                "Real",
                "String",
                "Key",
                "[",
                "]",
                "{",
                "}",
                "Error",
                "EOF"
        };
        return names[type];
}

int main(int argc, char *argv[])
{
        jsonpg_generator *g;
        jsonpg_result res;
        if(argc == 3) {
                if(0 == strcmp("-e", argv[1])) {
                        puts(argv[2]);
                        uint8_t buf[1024];
                        char *json = argv[2];
                        size_t len = strlen(json);
                        memcpy(buf, json, len + 1);
                        g = jsonpg_generator_new(.indent = 2);
                        res = jsonpg_parse(.bytes = buf, .count = len, .generator = g);
                        char *s = jsonpg_result_string(g);
                        printf(s);
                        jsonpg_generator_free(g);
                        if(JSONPG_ERROR == res.type) {
                                printf("Error: %d\n", res.error.code);
                                exit(1);
                        }
                }
        } else if(argc == 4) {
                if(0 == strcmp("-t", argv[1])) {
                        errno = 0;
                        long times = strtol(argv[2], NULL, 10);
                        if(errno) {
                                perror("Not a number");
                                exit(1);
                        }
                        FILE *fh = fopen(argv[3], "rb");
                        if(fh) {
                                fseek(fh, 0L, SEEK_END);
                                long length = ftell(fh);
                                rewind(fh);
                                uint8_t *buf = malloc(length + 1);
                                if(buf) {
                                        fread(buf, length, 1, fh);
                                        res = (jsonpg_result){};
                                        for(int i = 0 ; i < times ; i++) {
                                                jsonpg_generator *g = jsonpg_generator_new(.dom = true, .max_nesting = 0);
                                                res = jsonpg_parse(.bytes = buf, .count = length, .generator = g);
                                                if(res.type == JSONPG_ERROR) {
                                                        printf("Parse failed: %d at %ld\n", res.error.code, res.position);
                                                        return 1;
                                                }
                                                //char *s = jsonpg_result_string(g);
                                                //puts(s);
                                                //printf("JSON length: %ld\n", strlen(s));
                                                jsonpg_generator_free(g);
                                        }
                                        free(buf);
                                        int ret = (res.type == JSONPG_EOF) ? 0 : 1;
                                        printf("Type: %d, Returned %d\n", res.type, ret);
                                        return ret;
                                } else {
                                        perror("Failed to allocate buffer");
                                        exit(1);
                                }
                                fclose(fh);
                        }
                }
        } else {
                printf("tests2 -e <json> or tests2 -t <num> <json file>\n");
                return 1;
        }

        if(res.type == JSONPG_EOF)
                printf("\n\nResult EOF: %d\n", res.type);
        else
                printf("\n\nResult : %d (%d[%ld])\n", res.type, res.error.code, res.position);
}


