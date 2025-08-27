#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "jsonpg.c"

#define SKELETON_FILE "state.skel.c"
#define OUTPUT_FILE   "state.c"

typedef struct gen_state_s *gen_state;
typedef struct gen_class_s *gen_class;
typedef struct gen_rule_s *gen_rule;
typedef struct gen_match_s *gen_match;
typedef struct gen_action_s *gen_action;
typedef struct gen_builtin_s *gen_builtin;
typedef struct gen_range_s gen_range;
typedef struct gen_class_list_s *gen_class_list;
typedef struct gen_rule_list_s *gen_rule_list;
typedef struct gen_match_list_s *gen_match_list;
typedef struct gen_action_list_s *gen_action_list;
typedef struct gen_renderer_s *gen_renderer;

#define MAX_CHARS_IN_CLASS 22
#define CODE_START_LEVEL 3

struct gen_state_s {
        gen_class_list classes;
        gen_rule_list rules;
};

struct gen_class_s {
        char *name;
        uint8_t chars[MAX_CHARS_IN_CLASS];
};

struct gen_rule_s {
        char *name;
        int id;
        gen_match_list matches;
};

typedef enum {
        MATCH_CLASS_NAME,
        MATCH_CLASS,
        MATCH_CHAR,
        MATCH_RANGE,
        MATCH_ANY,
        MATCH_VIRTUAL
} gen_match_type;

struct gen_range_s {
        uint8_t start;
        uint8_t end;
};

struct gen_match_s {
        gen_match_type type;
        union {
                char *class_name;
                gen_class class;
                uint8_t character;
                gen_range range;
        } match;
        gen_action action;
        uint8_t id;
};

typedef enum {
        ACTION_LIST,
        ACTION_COMMAND,
        ACTION_RULE_NAME,
        ACTION_RULE
} gen_action_type;

struct gen_action_s {
        gen_action_type type;
        union {
                gen_action_list actions;
                gen_builtin command;
                char *rule_name;
                gen_rule rule;
        } action;
};

typedef struct gen_map_s{
        char *name;
        int value;
} gen_map;

#define MAPPING(X) gen_map X[] = {
#define MAP(X, Y) { X, Y },
#define MAPPING_END() { NULL, -1 } };

typedef enum {
        TOKEN_FINAL,
        TOKEN_MULTI,
        TOKEN_PARTIAL,
        TOKEN_MARKER
} gen_token_type;

MAPPING(tokens)
MAP("null", TOKEN_FINAL)
MAP("true", TOKEN_FINAL)
MAP("null", TOKEN_FINAL)
MAP("true", TOKEN_FINAL)
MAP("false", TOKEN_FINAL)
MAP("integer", TOKEN_FINAL)
MAP("real", TOKEN_FINAL)
MAP("string", TOKEN_FINAL)
MAP("key", TOKEN_FINAL)
MAP("sq_string", TOKEN_FINAL)
MAP("sq_key", TOKEN_FINAL)
MAP("nq_string", TOKEN_FINAL)
MAP("nq_key", TOKEN_FINAL)
MAP("object", TOKEN_MULTI)
MAP("array", TOKEN_MULTI)
MAP("escape", TOKEN_PARTIAL)
MAP("escape_u", TOKEN_PARTIAL)
MAP("escape_chars", TOKEN_PARTIAL)
MAP("surrogate", TOKEN_MARKER)
MAPPING_END()

// don't need the value but want to validate config names
MAPPING(configs)
MAP("comments", JSONPG_FLAG_COMMENTS)
MAP("single_quotes", JSONPG_FLAG_SINGLE_QUOTES)
MAP("unquoted_strings", JSONPG_FLAG_UNQUOTED_STRINGS)
MAP("unquoted_keys", JSONPG_FLAG_UNQUOTED_KEYS)
MAP("trailing_commas", JSONPG_FLAG_TRAILING_COMMAS)
MAP("optional_commas", JSONPG_FLAG_OPTIONAL_COMMAS)
MAP("escape_characters", JSONPG_FLAG_ESCAPE_CHARACTERS)
MAPPING_END()

typedef enum {
        CMD_PUSH_STATE,
        CMD_POP_STATE,
        CMD_PUSH,
        CMD_IF_PEEK,
        CMD_IFN_PEEK,
        CMD_IF_POP,
        CMD_POP,
        CMD_SWAP,
        CMD_IF_CONFIG
} builtin_type;


typedef enum {
        ARG_STATE,
        ARG_TOKEN,
        ARG_FLAG,
        ARG_OR     // multiple args or'ed together
} arg_type;

static arg_type arg_types[] = {
        ARG_STATE,
        ARG_STATE,
        ARG_TOKEN,
        ARG_TOKEN | ARG_OR,
        ARG_TOKEN | ARG_OR,
        ARG_TOKEN,
        ARG_TOKEN,
        ARG_TOKEN,
        ARG_FLAG | ARG_OR
};

MAPPING(builtins)
MAP("pushstate", CMD_PUSH_STATE)
MAP("popstate", CMD_POP_STATE)
MAP("push", CMD_PUSH)
MAP("ifpeek", CMD_IF_PEEK)
MAP("ifnpeek", CMD_IFN_PEEK)
MAP("ifpop", CMD_IF_POP)
MAP("pop", CMD_POP)
MAP("swap", CMD_SWAP)
MAP("ifconfig", CMD_IF_CONFIG)
MAPPING_END()

typedef struct gen_arg_list_s *arg_list;
struct gen_arg_list_s {
        char *arg;
        arg_list next;
};

struct gen_builtin_s {
        builtin_type type;
        char *name;
        arg_list args;
        gen_token_type token_type;  // multiple tokens, same type
};

struct gen_class_list_s {
        gen_class class;
        gen_class_list next;
};

struct gen_rule_list_s {
        gen_rule rule;
        gen_rule_list next;
};

struct gen_match_list_s {
        gen_match match;
        gen_match_list next;
};

struct gen_action_list_s {
        gen_action action;
        gen_action_list next;
};

struct gen_renderer_s {
        int level;
        int startlevel;
        str_buf sbuf;
};

static uint8_t gen_rule_id = 0;
static uint8_t gen_action_id = 0x80;
static char *gen_hex_chars = "0123456789ABCDEF";

int str_equal(char *s1, char *s2)
{
        return 0 == strcmp(s1, s2);
}

int map_lookup(gen_map *mp, char *name)
{
        gen_map m = *mp++;
        while(m.name) {
                if(str_equal(name, m.name))
                        return m.value;
                m = *mp++;
        }
        return m.value;
}

void warn(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
}

void fail(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
        exit(1);
}


void *fmalloc(size_t size) {
        void *p = malloc(size);
        if(!p)
                fail("Failed to allocate %d bytes", size);
        return p;
}

char result_char(jsonpg_parser p)
{
        return (char)*jsonpg_parse_result(p).string.bytes;
}

char *result_str(jsonpg_parser p)
{
        string_val s = jsonpg_parse_result(p).string;
        char *ptr = fmalloc(1 + s.length);
        memcpy(ptr, s.bytes, s.length);
        ptr[s.length] = '\0';
        return ptr;
}

void expect_type(jsonpg_type t1, jsonpg_type t2)
{
        if(t1 != t2)
                fail("Expected type %d, got %d", t2, t1);
}

void expect_next(jsonpg_type t, jsonpg_parser p)
{
        jsonpg_type t1 = jsonpg_parse_next(p);
        expect_type(t1, t);
}

void append_class(gen_state states, gen_class c)
{
        gen_class_list cl = fmalloc(sizeof(struct gen_class_list_s));
        cl->class = c;
        cl->next = NULL;

        if(!states->classes) {
                states->classes = cl;
                return;
        }

        gen_class_list next = states->classes;
        gen_class_list last = next;
        while(next) {
                if(str_equal(next->class->name, c->name))
                        fail("Duplicate class name '%s'", c->name);
                last = next;
                next = next->next;
        }
        last->next = cl;
}

void append_rule(gen_state states, gen_rule r)
{
        gen_rule_list rl = fmalloc(sizeof(struct gen_rule_list_s));
        rl->rule = r;
        rl->next = NULL;

        if(!states->rules) {
                states->rules = rl;
                return;
        }

        gen_rule_list next = states->rules;
        gen_rule_list last = next;
        while(next) {
                if(str_equal(next->rule->name, r->name))
                        fail("Duplicate rule name '%s'", r->name);
                last = next;
                next = next->next;
        }
        last->next = rl;
}       
                
jsonpg_type parse_args(jsonpg_parser p, gen_builtin b, int have_next)
{
        jsonpg_type type;
        b->args = NULL;
        arg_list *al = &b->args;
        while(have_next || JSONPG_STRING == (type = jsonpg_parse_next(p))) {
                (*al) = fmalloc(sizeof(struct gen_arg_list_s));
                (*al)->next = NULL;
                (*al)->arg = result_str(p);
                al = &(*al)->next;

                have_next = 0;
        }
        return type;
}

gen_builtin parse_builtin(jsonpg_parser p)
{
        // validate here as best to combine multiple ifconfig flags early
        // we have nowhere to put multiple config flag strings
        // but we can or all of the flags into one value

        gen_builtin b = fmalloc(sizeof(struct gen_builtin_s));

        jsonpg_type type = jsonpg_parse_next(p);
        expect_type(type, JSONPG_KEY);
        char *key = result_str(p);

        b->name = key;
        b->type = map_lookup(builtins, key);
        arg_type arg_type = arg_types[b->type];

        type = jsonpg_parse_next(p);
        if(type == JSONPG_BEGIN_ARRAY) {
                if(!(arg_type & ARG_OR))
                        fail("Builtin command does not support multiple arguments");

                type = parse_args(p, b, 0);
                expect_type(type, JSONPG_END_ARRAY);
                type = jsonpg_parse_next(p);
        } else if(type == JSONPG_STRING) {
                type = parse_args(p, b, 1);
        } else {
                fail("Unexpected input token when parsing builtin");
        }
        expect_type(type, JSONPG_END_OBJECT);

        return b;
}

gen_action_list parse_action_list(jsonpg_parser);

gen_action parse_action(jsonpg_parser p)
{
        gen_action act = fmalloc(sizeof(struct gen_action_s));
        jsonpg_type type = jsonpg_parse_next(p);
        switch(type) {
                case JSONPG_STRING:
                        act->type = ACTION_RULE_NAME;
                        act->action.rule_name = result_str(p);
                        break;
                case JSONPG_BEGIN_OBJECT:
                        act->type = ACTION_COMMAND;
                        act->action.command = parse_builtin(p);
                        break;
                case JSONPG_BEGIN_ARRAY:
                        act->type = ACTION_LIST;
                        act->action.actions = parse_action_list(p);
                        break;
                default:
                        return NULL;
        }
        return act;
}

gen_action_list parse_action_list(jsonpg_parser p)
{
        gen_action_list current = NULL;
        gen_action_list head = NULL;
        gen_action_list prev = NULL;
        gen_action act;
        while((act = parse_action(p))) {
                current = fmalloc(sizeof(struct gen_action_list_s));
                if(!head)
                        head = current;
                current->action = act;
                if(prev)
                        prev->next = current;
                prev = current;
        }
        return head;
}

gen_match parse_match(jsonpg_parser p)
{
        jsonpg_type type = jsonpg_parse_next(p);
        if(type == JSONPG_END_OBJECT)
                return NULL;

        char *chars = result_str(p);
        gen_match m = fmalloc(sizeof(struct gen_match_s));
        m->type = -1;
        m->id = 0;
        int n = strlen(chars);
        unsigned r1, r2;
        switch(n) {
        case 0:
                fail("Rule match cannot be empty");
                // never get here
        case 1:
                m->type = MATCH_CHAR;
                m->match.character = *chars;
                break;
        case 4:
                if(1 == sscanf(chars, "0x%02X", &r1)) {
                        m->type = MATCH_CHAR;
                        m->match.character = (uint8_t)r1;
                }
                break;
        case 9:
                if(2 == sscanf(chars, "0x%02X-0x%02X", &r1, &r2)) {
                        if(r1 >= r2)
                                fail("Invalid range '%s'", chars);
                        m->type = MATCH_RANGE;
                        m->match.range.start = r1;
                        m->match.range.end = r2;
                }
                break;
        } 
        if(m->type == -1) {
                if('$' == *chars) {
                        m->type = MATCH_CLASS_NAME;
                        m->match.class_name = chars + 1;
                } else if(str_equal("...", chars)) {
                        m->type = MATCH_ANY;
                } else if(str_equal("???", chars)) {
                        m->type = MATCH_VIRTUAL;
                } else {
                        fail("Invalid match specification '%s'", chars);
                }
        }
        m->action = parse_action(p);
        return m;
}


gen_match_list parse_match_list(jsonpg_parser p) {
        expect_next(JSONPG_BEGIN_OBJECT, p);

        gen_match_list current = NULL;
        gen_match_list head = NULL;
        gen_match_list prev = NULL;
        gen_match m;
        while((m = parse_match(p))) {
                current = fmalloc(sizeof(struct gen_match_list_s));
                if(!head)
                        head = current;
                current->match = m;
                if(prev)
                        prev->next = current;
                prev = current;
        }
        return head;
}




gen_state create_states()
{
        return malloc(sizeof(struct gen_state_s));
}

void parse_class_chars(gen_class c, jsonpg_parser p)
{
        expect_next(JSONPG_BEGIN_ARRAY, p);
        jsonpg_type type = jsonpg_parse_next(p);
        int i = 0;
        while(type == JSONPG_STRING) {
                if(i >= MAX_CHARS_IN_CLASS)
                        fail("Too many characters in character class '%s'", c->name);
                c->chars[i++] = result_char(p);
                type = jsonpg_parse_next(p);
        }
        c->chars[i] = '\0';
        expect_type(type, JSONPG_END_ARRAY);
}

gen_class parse_class(char *name, jsonpg_parser p)
{
        gen_class c = fmalloc(sizeof(struct gen_class_s));
        c->name = name;
        parse_class_chars(c, p);
        return c;
}

void add_class(gen_state states, char *name, jsonpg_parser p)
{
        append_class(states, parse_class(name, p));
}

int rule_is_virtual(gen_rule r)
{
        // rule with single "???" match clause
        return r->matches->match->type == MATCH_VIRTUAL
                && !(r->matches->next);
}

gen_rule parse_rule(char *name, jsonpg_parser p)
{
        gen_rule r = fmalloc(sizeof(struct gen_rule_s));
        r->name = name;
        r->matches = parse_match_list(p);
        r->id = rule_is_virtual(r) ? -1 : gen_rule_id++;

        return r;
}

void add_rule(gen_state states, char *name, jsonpg_parser p)
{
        append_rule(states, parse_rule(name, p));
}





gen_class find_class(gen_state states, char *name)
{
        gen_class_list cl = states->classes;
        while(cl) {
                if(str_equal(name, cl->class->name))
                        return cl->class;
                cl = cl->next;
        }
        return NULL;
}

gen_rule find_rule(gen_state states, char *name)
{
        gen_rule_list rl = states->rules;
        while(rl) {
                if(str_equal(name, rl->rule->name)) {
                        return rl->rule;
                }
                rl = rl->next;
        }
        return NULL;
}

int validate_builtin(gen_builtin b)
{
        if(b->type == -1) {
                warn("Unknown builtin '%s'", b->name);
                return 0;
        }
        arg_list al = b->args;
        switch(b->type) {
        case CMD_PUSH_STATE:
        case CMD_POP_STATE:
                // no validation required
                break;
        case CMD_IF_CONFIG:
                while(al) {
                        if(map_lookup(configs, al->arg) == -1) {
                                warn("Unknown config '%s'", al->arg);
                                return 0;
                        }
                        al = al->next;
                }
                break;
        default:
                arg_list al = b->args;
                gen_token_type tt;
                while(al) {
                        tt = map_lookup(tokens, al->arg);
                        if(tt == -1) {
                                warn("Unknown token '%s'", al->arg);
                                return 0;
                        }

                        if(b->args != al && b->token_type != tt) {
                                warn("Incompatible token type '%s'", al->arg);
                                return 0;
                        }
                        b->token_type = tt;
                        al = al->next;
                }
                break;
        }
        return 1;
}

int validate_action(gen_state states, gen_action a)
{
        int res = 1;
        switch(a->type) {
                case ACTION_LIST:
                        gen_action_list al = a->action.actions;
                        while(al) {
                                res &= validate_action(states, al->action);
                                al = al->next;
                        }
                        break;
                case ACTION_COMMAND:
                        res &= validate_builtin(a->action.command);
                        break;
                case ACTION_RULE_NAME:
                        char *name = a->action.rule_name;
                        gen_rule r = find_rule(states, name);
                        if(!r) {
                                warn("Unknown rule '%s'", name);
                                res = 0;
                        }
                        a->type = ACTION_RULE;
                        a->action.rule = r;
                        break;
                case ACTION_RULE:
                        // will only exist post validation
        }
        return res;
}

int validate_match(gen_state states, gen_match m)
{
        int res = 1;
        if(m->type == MATCH_CLASS_NAME) {
                char *name = m->match.class_name;
                gen_class c = find_class(states, name);
                if(!c) {
                        warn("Unknown class '%s'", name);
                        res = 0;
                }
                m->type = MATCH_CLASS;
                m->match.class = c;
        }
        res &= validate_action(states, m->action);
        return res;
}

int validate_rule(gen_state states, gen_rule r)
{
        int res = 1;
        gen_match_list ml = r->matches;
        while(ml) {
                res &= validate_match(states, ml->match);
                ml = ml->next;
        }
        return res;
}

int validate_states(gen_state states) 
{
        int res = 1;
        gen_rule_list rl = states->rules;
        while(rl) {
                res &= validate_rule(states, rl->rule);
                rl = rl->next;
        }
        return res;
}


gen_state parse_state(jsonpg_parser p)
{
        gen_state states = create_states();

        jsonpg_type type = jsonpg_parse_next(p);
        while(JSONPG_KEY == type) {
                char *key = result_str(p);
                if('$' == *key) {
                        add_class(states, key + 1, p);
                } else {
                        add_rule(states, key, p);
                }
                type = jsonpg_parse_next(p);
        }
        expect_type(JSONPG_END_OBJECT, type);

        return states;
}

void dump_command(gen_builtin command)
{
        printf("  Command: %s\n", command->name);
        arg_list al = command->args;
        while(al) {
                printf("           %s\n", al->arg);
                al = al->next;
        }
}

void dump_action_rule(gen_rule r)
{
        printf("  Action Rule: %s\n", r->name);
}

void dump_action(gen_action a)
{
        switch(a->type) {
                case ACTION_LIST:
                        gen_action_list al = a->action.actions;
                        while(al) {
                                dump_action(al->action);
                                al = al->next;
                        }
                        break;
                case ACTION_COMMAND:
                        dump_command(a->action.command);
                        break;
                case ACTION_RULE:
                        dump_action_rule(a->action.rule);
                        break;
                case ACTION_RULE_NAME:
                        fail("Rule %s not validated", a->action.rule_name);
        }

}

void dump_match(gen_match m)
{
        switch(m->type) {
                case MATCH_CLASS:
                        printf(" Match class %s\n", m->match.class->name);
                        break;
                case MATCH_CHAR:
                        uint8_t c = m->match.character;
                        if(c < 0x20)
                                printf(" Match char 0x%02X\n", (int)c);
                        else
                                printf(" Match char '%c'\n", (int)c);
                        break;
                case MATCH_RANGE:
                        gen_range *r = &m->match.range;
                        printf(" Match range 0x%02X-0x%02X\n", r->start, r->end);
                        break;
                case MATCH_ANY:
                        printf( "Match any\n");
                        break;
                case MATCH_VIRTUAL:
                        printf(" Match virtual\n");
                        break;
                case MATCH_CLASS_NAME:
                        fail("Class %s not validated", m->match.class_name);
        }

        dump_action(m->action);

}

void dump_matches(gen_match_list ml)
{
        while(ml) {
                dump_match(ml->match);
                ml = ml->next;
        }
}

void dump_rule(gen_rule r)
{
        printf("Rule: %s [%d]\n", r->name, (unsigned)r->id);
        dump_matches(r->matches);
        printf("\n");
}

void dump_class(gen_class c)
{
        printf("Class: %s\n", c->name);
        uint8_t *chars = c->chars;
        int len = strlen((char *)chars);
        int first = 1;
        for(int i = 0 ; i < len ; i++) {
                if(first) {
                        printf("Chars: ");
                        first = 0;
                } else {
                        printf(", ");
                }
                if(chars[i] < 0x20)
                        printf("0x%02X", (int)chars[i]);
                else
                        printf("'%c'", (int)chars[i]);
        }
        printf("\n\n");
}

void dump_state(gen_state states) {
        gen_class_list cl = states->classes;
        while(cl) {
                dump_class(cl->class);
                cl = cl->next;
        }
        gen_rule_list rl = states->rules;
        while(rl) {
                dump_rule(rl->rule);
                rl = rl->next;
        }
}

void render(gen_renderer r, char *chars)
{
        str_buf_append_chars(r->sbuf, chars);
}

void render_c(gen_renderer r, uint8_t c)
{
        str_buf_append(r->sbuf, &c, 1);
}

void render_indent(gen_renderer r, char *chars)
{
        static char indent[] = "        ";
        render(r, "\n");
        for(int i = 0 ; i < r->level ; i++)
                render(r, indent);
        render(r, chars);
}

void render_level(gen_renderer r, int inc)
{
        r->level += inc;
}

void render_startlevel(gen_renderer r, int l)
{
        r->level = l;
        r->startlevel = l;
}

void write_renderer(FILE *stream, gen_renderer r)
{
        uint8_t *str;
        int len = str_buf_content(r->sbuf, &str);
        // drop leading newline
        fprintf(stream, "%.*s", len - 1, 1 + (char *)str);
}

void render_x(gen_renderer r, uint8_t x)
{
        render(r, "0x");
        render_c(r, gen_hex_chars[x >> 4]);
        render_c(r, gen_hex_chars[x & 0xF]);
}        

void render_enum(gen_rule r, gen_renderer enums, gen_renderer enum_names, int first) 
{
        if(r->id < 0)
                // will mess up first processing if first entry is virtual,
                // which it isn't at the moment (phew)
                return;

        if(!first) {
                render(enums, ",");
                render(enum_names, ",");
        }

        render_indent(enums, "state_");
        render(enums, r->name);

        render_indent(enum_names, "[state_");
        render(enum_names, r->name);
        render(enum_names, "] = \"state_");
        render(enum_names, r->name);
        render(enum_names, "\"");
}

void render_map_values(gen_rule r, uint8_t *bytes, gen_renderer map, int first_map)
{
        if(r->id < 0)
                return;

        int first = 1;
        if(!first_map)
                render(map, ", ");
        render_indent(map, "{");
        for(int i = 0 ; i < 256 ; i++) {
                if(!first)
                        render(map, ", ");
                first = 0;
                render_x(map, bytes[i]);
        }
        render(map, "}");
}

int is_if_command(gen_builtin command)
{
        return 0 == strncmp("if", command->name, 2);
}

void render_call(char *prefix, char *arg, gen_renderer code)
{
        render_indent(code, prefix);
        render(code, arg);
        render(code, ");");
}

void render_if(int not, char *prefix, arg_list args, char *close, gen_renderer code)
{
        int bracket = not && args->next != NULL;
        render_indent(code, "if(");

        if(not) {
                render(code, "!");
                if(bracket)
                        render(code, "(");
        }
        render(code, prefix);
        render(code, args->arg);
        render(code, close);
        args = args->next;
        while(args) {
                render(code, " || ");
                render(code, prefix);
                render(code, args->arg);
                render(code, close);
                args = args->next;
        }

        if(bracket)
                render(code, ")");

        render(code, ") {");
        render_level(code, 1);
}

void render_ifpeek(int not, gen_builtin command, gen_renderer code)
{
        if(command->token_type == TOKEN_MULTI) {
                render_if(not, "in_", command->args, "()", code);
        } else {
                render_if(not, "ifpeek_token(token_", command->args, ")", code);
        }
}

void render_alloc_error(gen_renderer code)
{
        render(code, " {");
        render_level(code, 1);
        // render_indent(code, "result = alloc_error(p);");
        // render_indent(code, "break;");
        render_indent(code, "return alloc_error(p);");
        render_level(code, -1);
        render_indent(code, "}");
}

void render_builtin(int is_virtual, gen_builtin command, gen_renderer code)
{
        arg_list args = command->args;
        switch(command->type) {
        case CMD_PUSH_STATE:
                render_indent(code, "push_state(state_");
                render(code, args->arg);
                render(code, ");");
                break;
        case CMD_POP_STATE:
                // if(is_virtual)
                //         render_indent(code, "incr = 0;");
                render_indent(code, "new_state = pop_state();");
                render_indent(code, "goto L");
                render(code, is_virtual ? "noinc;" : "inc;");
                break;
        case CMD_IF_CONFIG:
                render_if(0, "if_config(config_", args, ")", code);
                break;
        case CMD_PUSH:
                switch(command->token_type) {
                case(TOKEN_MULTI):
                        render_indent(code, "result = begin_");
                        render(code, args->arg);
                        render(code, "();");
                        break;
                case(TOKEN_PARTIAL):
                        render_indent(code, "if(push_token(token_");
                        render(code, args->arg);
                        render(code, "))");
                        render_alloc_error(code);
                        break;
                default:
                        render_indent(code, "push_token(token_");
                        render(code, args->arg);
                        render(code, ");");
                }
                break;
        case CMD_IF_PEEK:
                render_ifpeek(0, command, code);
                break;
        case CMD_IFN_PEEK:
                render_ifpeek(1, command, code);
                break;
        case CMD_IF_POP:
                render_ifpeek(0, command, code);
                // fallthrough
        case CMD_POP:
                switch(command->token_type) {
                case TOKEN_FINAL:
                        render_indent(code, "result = accept_");
                        render(code, args->arg);
                        render(code, "(pop_token());");
                        break;
                case TOKEN_MULTI:
                        render_indent(code, "result = end_");
                        render(code, args->arg);
                        render(code, "();");
                        break;
                case TOKEN_PARTIAL:
                        render_indent(code, "if(process_");
                        render(code, args->arg);
                        render(code, "(pop_token()))");
                        render_alloc_error(code);
                        break;
                case TOKEN_MARKER:
                        render_indent(code, "pop_token();");
                        break;
                }
                break;
        case CMD_SWAP:
                render_indent(code, "swap_token(token_");
                render(code, args->arg);
                render(code, ");");
                break;
        default:
                fail("Unexpected builtin command '%s' [%d]", command->name, command->type);
 
        }
}

void render_rule_match(int is_virtual, gen_rule r, gen_renderer code)
{
        // if(is_virtual)
        //         render_indent(code, "incr = 0;");
        render_indent(code, "new_state = state_");
        render(code, r->name);
        render(code, ";");

        // if(code->level > 1 + code->startlevel)
                // render_indent(code, "break;");
        render_indent(code, "goto L");
        render(code, is_virtual ? "noinc;" : "inc;");
}

void render_actions(int, gen_action_list, gen_renderer);

void render_action(int is_virtual, gen_action a, gen_renderer code)
{
        switch(a->type) {
                case ACTION_LIST:
                        render_actions(is_virtual, a->action.actions, code);
                        break;
                case ACTION_COMMAND:
                        render_builtin(is_virtual, a->action.command, code);
                        break;
                case ACTION_RULE:
                        gen_rule r = a->action.rule;
                        if(r->id < 0) {
                                render_action(is_virtual, r->matches->match->action, code);
                        } else {
                                render_rule_match(is_virtual, r, code);
                        }
                        break;
                case ACTION_RULE_NAME:
                        fail("Rule '%s' not validated", a->action.rule_name);
        }
}

void render_if_block(int is_virtual, gen_action_list al, gen_renderer code)
{
        render_action(is_virtual, al->action, code);
        al = al->next;
        if(!al)
                fail("If without then");
        render_action(is_virtual, al->action, code);
        render_level(code, -1);
        render_indent(code, "}");
        al = al->next;
        if(al) {
                render(code, " else {");

                render_level(code, 1);
                render_action(is_virtual, al->action, code);
                render_level(code, -1);

                render_indent(code, "}");

                al = al->next;
                if(al)
                        fail("If with too many clauses");
        }
}

int is_if_action(gen_action a)
{
        if(a->type == ACTION_COMMAND)
                return is_if_command(a->action.command);
        else
                return 0;
}

void render_actions(int is_virtual, gen_action_list al, gen_renderer code)
{
        if(is_if_action(al->action)) {
                render_if_block(is_virtual, al, code);
        } else {
                while(al) {
                        render_action(is_virtual, al->action, code);
                        al = al->next;
                }
        }
}

void render_action_comment(
                int is_virtual,
                gen_renderer code)
{
        render_indent(code, "// ");
        if(is_virtual)
                render(code, "[virtual] ");
}

void render_action_desc(gen_rule r, gen_match m, gen_renderer rend, int escape)
{
        render(rend, r->name);
        render(rend, "/");

        switch(m->type) {
                case MATCH_CLASS:
                        render(rend, "$");
                        render(rend, m->match.class->name);
                        break;
                case MATCH_CHAR:
                        render(rend, "'");
                        if(escape &&
                                        ('\\' == m->match.character 
                                         || '\"' == m->match.character))
                                render(rend, "\\");
                        render_c(rend, m->match.character);
                        render(rend, "'");
                        break;
                case MATCH_RANGE:
                        render_x(rend, m->match.range.start);
                        render(rend, "-");
                        render_x(rend, m->match.range.end);
                        break;
                case MATCH_ANY:
                        render(rend, "...");
                        break;
                case MATCH_VIRTUAL:
                        render(rend, "???");
                        break;
                case MATCH_CLASS_NAME:
                        fail("Class '%s' not resolved", m->match.class_name);
        }
}

uint8_t render_new_action(
                int is_virtual,
                gen_rule r,
                gen_match m,
                gen_renderer gotos,
                gen_renderer code, 
                gen_renderer cases)
{
        uint8_t s = gen_action_id++;
        
        render_indent(gotos, "&&L");
        render_x(gotos, s);
        render(gotos, ",");

        render_indent(code, "L");
        // render_indent(code, "case ");
        render_x(code, s);
        render(code, ":");
        render_level(code, 1);

        render_indent(cases, "[");
        render_x(cases, s);
        render(cases, "] = ");

        render_action_comment(is_virtual, code);
        render_action_desc(r, m, code, 0);

        render(cases, "\"");
        render_action_desc(r, m, cases, 1);
        render(cases, "\",");

        return s;
}

int render_previous(gen_renderer r, char *txt) {
        int count = strlen(txt);
        if(r->sbuf->count >= count) {
                return 0 == memcmp(txt, 
                                r->sbuf->bytes + r->sbuf->count - count,
                                count);
        }
        return 0;
}

uint8_t render_match_action(
                int is_virtual,
                gen_rule r,
                gen_match m,
                gen_renderer gotos,
                gen_renderer code,
                gen_renderer cases)
{
        if(m->id)
                return m->id;

        gen_action a = m->action;
        switch(a->type) {
                case ACTION_LIST:
                        m->id = render_new_action(is_virtual, r, m, gotos, code, cases);
                        render_actions(is_virtual, a->action.actions, code);
                        // render_indent(code, "break;");
                        if(!(render_previous(code, "goto Linc;")
                                                || render_previous(code, "goto Lnoinc;")))
                                render_indent(code, "goto Lerror;");
                        render_level(code, -1);
                        break;
                case ACTION_COMMAND:
                        m->id = render_new_action(is_virtual, r, m, gotos, code, cases);
                        render_builtin(is_virtual, a->action.command, code);
                        // render_indent(code, "break;");
                        if(!(render_previous(code, "goto Linc;")
                                                || render_previous(code, "goto Lnoinc;")))
                                render_indent(code, "goto Lerror;");
                        render_level(code, -1);
                        break;
                case ACTION_RULE:
                        gen_rule ar = a->action.rule;
                        if(ar->id < 0) {
                                m->id = render_match_action(
                                                is_virtual, 
                                                ar, 
                                                ar->matches->match, 
                                                gotos, 
                                                code, 
                                                cases);
                        } else {
                                m->id = ar->id;
                        }
                        break;
                case ACTION_RULE_NAME:
                        fail("Rule '%s' not validated", a->action.rule_name);
        }


        return m->id;
}

uint8_t render_rule_default_state(gen_rule r, gen_renderer gotos, gen_renderer code, gen_renderer cases)
{
        gen_match_list ml = r->matches;
        while(ml) {
                gen_match m = ml->match;
                if(m->type == MATCH_ANY) {
                        return render_match_action(false, r, m, gotos, code, cases);
                        break;
                } else if(m->type == MATCH_VIRTUAL) {
                        return render_match_action(true, r, m, gotos, code, cases);
                        break;
                }
                ml = ml->next;
        }

        return -1; //state_error;
}





void render_rule(
                gen_rule r, 
                gen_renderer map, 
                gen_renderer enums, 
                gen_renderer enum_names,
                gen_renderer gotos,
                gen_renderer code, 
                gen_renderer cases,
                int first)
{
        if(r->id < 0)
                return;

        static uint8_t rule_states[256];

        render_enum(r, enums, enum_names, first);
        gen_match_list ml = r->matches;
        uint8_t def_state = render_rule_default_state(r, gotos, code, cases);
        memset(rule_states, def_state, 256);

        while(ml) {
                int len;
                gen_match m = ml->match;
                uint8_t match_state;
                switch(m->type) {
                        case MATCH_CLASS:
                                match_state = render_match_action(false, r, m, gotos, code, cases);
                                uint8_t *chars = m->match.class->chars;
                                len = strlen((char *)chars);
                                for(int i = 0 ; i < len ; i++)
                                        rule_states[chars[i]] = match_state;
                                break;
                        case MATCH_CHAR:
                                match_state = render_match_action(false, r, m, gotos, code, cases);
                                rule_states[m->match.character] = match_state;
                                break;
                        case MATCH_RANGE:
                                match_state = render_match_action(false, r, m, gotos, code, cases);
                                gen_range *rg = &m->match.range;
                                len = rg->end - rg->start;
                                for(int i = 0 ; i <= len ; i++)
                                        rule_states[rg->start + i] = match_state;
                                break;
                        case MATCH_CLASS_NAME:
                                // validation will have changed this to CLASS or failed
                        case MATCH_ANY:
                                // handled as default set above
                        case MATCH_VIRTUAL:
                                // handled as default set above
                }
                ml = ml->next;
        }
        render_map_values(r, rule_states, map, first);
}

gen_renderer renderer_new(int level)
{
        gen_renderer r = fmalloc(sizeof(struct gen_renderer_s));
        r->level = level;
        r->startlevel = level;
        r->sbuf = str_buf_new(0);
        return r;
}

void copy_until(FILE *in, FILE *out, char *tag)
{
        size_t tlen = tag ? strlen(tag) : 0;
        char *line = NULL;
        size_t len = 0;
        ssize_t read;

        while ((read = getline(&line, &len, in)) != -1) {
                if(tag && 0 == strncmp(tag, line, tlen))
                        return;
                fprintf(out, "%s", line);
        }
        if(tag)
                fail("Tag %s not found", tag);
}

void merge_renderer(FILE *in, FILE *out, gen_renderer r, char *tag)
{
        copy_until(in, out, tag);
        write_renderer(out, r);
}

void render_state(gen_state states)
{
        gen_rule_list rl = states->rules;
        gen_renderer map = renderer_new(1);
        gen_renderer enums = renderer_new(1);
        gen_renderer enum_names = renderer_new(1);
        gen_renderer cases = renderer_new(1);
        gen_renderer gotos = renderer_new(CODE_START_LEVEL);
        gen_renderer code = renderer_new(CODE_START_LEVEL);

        int first = 1;
        while(rl) {
                render_rule(rl->rule, map, enums, enum_names, gotos, code, cases, first);
                first = 0;
                rl = rl->next;
        }

        // drop last comma from cases
        cases->sbuf->count--;

        FILE *skelfile = fopen(SKELETON_FILE, "r");
        if(!skelfile)
                fail("Failed to open %s", SKELETON_FILE);

        FILE *cfile = fopen(OUTPUT_FILE, "w");
        if(!cfile)
                fail("Failed to create %s", OUTPUT_FILE);

        merge_renderer(skelfile, cfile, map, "<= map");
        merge_renderer(skelfile, cfile, enums, "<= enums");
        merge_renderer(skelfile, cfile, enum_names, "<= enum_names");
        merge_renderer(skelfile, cfile, cases, "<= cases");
        merge_renderer(skelfile, cfile, gotos, "<= gotos");
        merge_renderer(skelfile, cfile, code, "<= code");
        copy_until(skelfile, cfile, NULL);

        fclose(cfile);
        fclose(skelfile);
}

int main(int argc, char *argv[])
{
        if((argc != 2 && argc != 3) 
                        || (argc == 3 && 0 != strcmp("-d", argv[1]))) {
                printf("Usage: state [-d] <json file>");
                exit(1);
        }

        int dump = (argc == 3);

        FILE *stream = fopen(argv[argc - 1], "rb");
        if(!stream) {
                perror("Failed to open input");
                exit(1);
        }

        jsonpg_parser p = jsonpg_parser_new();
        jsonpg_type type = jsonpg_parse(p, .stream = stream);
        if(type != JSONPG_EOF && type != JSONPG_ERROR) {
                type = jsonpg_parse_next(p);
                if(JSONPG_BEGIN_OBJECT == type) {
                        gen_state s = parse_state(p);
                        if(validate_states(s)) {
                                if(dump) {
                                        dump_state(s);
                                } else {
                                        render_state(s);
                                }
                        } else {
                                printf("Invalid states\n");
                        }
                } else {
                        printf("Expecting begin object, got: %d\n", type);
                        exit(1);
                }
                type = jsonpg_parse_next(p);
        }
        if(type != JSONPG_EOF) {
                if(type == JSONPG_ERROR) 
                        printf("Error %d at %ld\n", 
                                        jsonpg_parse_result(p).error.code, 
                                        jsonpg_parse_result(p).error.at);
                else
                        printf("Expecting EOF, got: %d\n", type);
                exit(1);
        }
}
