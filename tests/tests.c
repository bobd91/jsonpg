#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "../src/jsonpg.c"


/*
*       static jsonpg_generator generator = {
*               .callbacks = &callbacks,
*               .ctx = &pctx
*       };
*/      

void *talloc(size_t s)
{
        void *p = malloc(s);
        printf("\nAlloc  : %p\n", p);
        return p;
}

void tfree(void *p)
{
        printf("\nDealloc: %p\n", p);
        free(p);
        return;
}


jsonpg_parser pp;

char *type_name(jsonpg_type type) {
        static char *names[] = {
                "None",
                "Root",
                "False",
                "Null",
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

void dom_validate(jsonpg_dom dom) {

        while(dom) {
                printf("Header: count = %ld, size = %ld\n", dom->count, dom->size);
                size_t offset = sizeof(struct jsonpg_dom_s);
                while(offset < dom->count) {
                        dom_node node = (dom_node)(offset + (void *)dom);
                        jsonpg_type type = node->is.type.type;
                        size_t count = node->is.type.count;
                        printf("  Node: offset = %ld, type = %s, count = %ld\n",
                                        offset,
                                        type_name(type),
                                        count);
                        offset += NODE_SIZE;
                        char *stype = NULL;
                        switch(type) {
                        case JSONPG_INTEGER:
                                node++;
                                printf("    Integer: offset = %ld, value = %ld\n",
                                                offset,
                                                node->is.integer);
                                offset += NODE_SIZE;
                                break;
                        case JSONPG_REAL:
                                node++;
                                printf("    Real: offset = %ld, value = %g\n",
                                                offset,
                                                node->is.real);
                                offset += NODE_SIZE;
                                break;
                        case JSONPG_STRING:
                                stype = "String";
                        case JSONPG_KEY:
                                if(stype == NULL)
                                        stype = "Key";
                                node++;
                                
                                printf("    %s: offset = %ld, value = ", stype, offset);
                                if(count > 8)
                                        printf("\"%.8s...\"\n", (char *)node->is.bytes);
                                else
                                        printf("\"%s\"\n", (char *)node->is.bytes);
                                offset += dom_size_align(count);
                                break;
                        default:
                        }
                }
                dom = dom->next;
        }

}

int main(int argc, char *argv[])
{
        //jsonpg_set_allocator(talloc, tfree);

        jsonpg_parser p = jsonpg_parser_new(); //&c);

        jsonpg_type res = JSONPG_NONE;

        //      jsonpg_generator generator = jsonpg_stream_printer(stdout, 1);
        // str_buf sbuf = str_buf_new();
        // jsonpg_generator g = jsonpg_buffer_printer(sbuf, 0);

        jsonpg_dom dom = jsonpg_dom_new(0);
        jsonpg_generator g = jsonpg_dom_generator(dom);

        if(argc == 2) {
                int fd = open(argv[1], O_RDONLY, "rb");
                if(fd == -1) {
                        perror("Failed to open file");
                        exit(1);
                }

                g = jsonpg_generator_new(.stream = stdout, .max_nesting = 0);
                g->error.code = 0;
                g->error.at = 0;
                res = jsonpg_parse(p, .fd = fd, .generator = g);
                // if(JSONPG_ERROR == res) {
                //         printf("DOM Parse res: %d\n", res);
                //         exit(1);
                // }
                // //dom_validate(dom);
                // jsonpg_parser_free(p);
                // p = NULL;
                //
                // g = jsonpg_generator_new(.stream = stdout, .pretty = true, .max_nesting = 0);
                // g->error.code = 0;
                // g->error.at = 0;
                // res = jsonpg_dom_parse(dom, g);
        
        } else if(argc == 3) {
                if(0 == strcmp("-e", argv[1])) {
                        puts(argv[2]);
                        uint8_t buf[1024];
                        char *json = argv[2];
                        size_t len = strlen(json);
                        memcpy(buf, json, len + 1);
                        res = jsonpg_parse(p, .bytes = buf, .count = len, .generator = g);
                        if(JSONPG_ERROR == res) {
                                printf("DOM Parse res: %d\n", res);
                                exit(1);
                        }
                        g = jsonpg_generator_new(.stream = stdout, .pretty = true, .max_nesting = 0);
                        res = jsonpg_dom_parse(dom, g);
                }
        } else if(argc == 4) {
                if(0 == strcmp("-t", argv[1])) {
                        errno = 0;
                        long times = strtol(argv[2], NULL, 10);
                        if(errno) {
                                perror("Not a number");
                                exit(1);
                        }
                        jsonpg_type t; 
                        FILE *fh = fopen(argv[3], "rb");
                        if(fh) {
                                fseek(fh, 0L, SEEK_END);
                                long length = ftell(fh);
                                rewind(fh);
                                uint8_t *buf = malloc(length + 1);
                                if(buf) {
                                        fread(buf, length, 1, fh);
                                        t = JSONPG_ERROR;
                                        for(int i = 0 ; i < times ; i++) {
                                                dom = jsonpg_dom_new(0);
                                                // t = jsonpg_parse(p, buf, length, NULL);
                                                // while(t != JSONPG_EOF
                                                //                 && t != JSONPG_ERROR) {
                                                //         t = jsonpg_parse_next(p);
                                                // }
                                                t = jsonpg_parse(p, .dom = dom, .bytes = buf, .count = length);
                                                if(t == JSONPG_ERROR) {
                                                        perror("Parse failed");
                                                        exit(1);
                                                }
                                                jsonpg_dom_free(dom);
                                        }
                                        jsonpg_parser_free(p);
                                        free(buf);
                                        int ret = (t == JSONPG_EOF) ? 0 : 1;
                                        printf("Type: %d, Returned %d\n", t, ret);
                                        return ret;
                                } else {
                                        perror("Failed to allocate buffer");
                                        exit(1);
                                }
                                fclose(fh);
                        }
                }
        }

        // uint8_t *bytes = (uint8_t *)"";
        // size_t count = str_buf_content(sbuf, &bytes);
        // printf("%.*s\n", (int)count, (bytes ? (char *)bytes : ""));

        if(res == JSONPG_EOF)
                printf("\n\nResult EOF: %d\n", res);
        else {
                jsonpg_error_code code = 0;
                size_t at = 0;
                if(p) {
                        code = p->result.error.code;
                        at = p->result.error.at;
                } else if(g) {
                        code = g->error.code;
                        at = g->error.at;
                }
                printf("\n\nResult : %d (%d[%ld])\n", res, code, at);
        }

        jsonpg_generator_free(g);
        jsonpg_parser_free(p);
        jsonpg_dom_free(dom);


       
}


