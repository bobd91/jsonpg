#include <stdint.h>

#define DOM_MIN_SIZE 8192
#define NODE_SIZE (sizeof(struct dom_node_s))

typedef struct jsonpg_dom_s *dom_hdr;
typedef struct dom_node_s *dom_node;
typedef struct dom_type_s dom_type;


struct jsonpg_dom_s {
        dom_hdr next;
        dom_hdr current;
        size_t count;
        size_t size;
};

struct dom_type_s {
        jsonpg_type type;
        size_t count;
};

struct dom_node_s {
        union {
                dom_type type;
                double real;
                long integer;
                uint8_t bytes[8];
        } is;
};

dom_info dom_parser_info(jsonpg_dom dom)
{
        dom_info di;
        di.hdr = dom;
        di.offset = sizeof(struct jsonpg_dom_s);
        return di;
}

// Ensure all sizes align with structure size
static size_t dom_size_align(size_t size)
{
        return NODE_SIZE * (1 + ((size - 1) / NODE_SIZE));
}

// Ensure minimum size
static size_t dom_node_size(size_t size)
{
        size = size < DOM_MIN_SIZE 
                ? DOM_MIN_SIZE 
                : size;
        return dom_size_align(size);
}

static dom_hdr dom_hdr_new(size_t size)
{
        size = dom_node_size(size + sizeof(struct jsonpg_dom_s));

        dom_hdr hdr = pg_alloc(size);

        if(!hdr)
                return NULL;

        hdr->next = NULL;
        hdr->current = NULL;
        hdr->size = size;
        hdr->count = sizeof(struct jsonpg_dom_s);

        return hdr;
}

static dom_node dom_node_next(dom_hdr root, size_t count)
{
        size_t required = dom_size_align(count + NODE_SIZE);
        dom_hdr hdr = root->current;
        if(required > hdr->size - hdr->count) {
                // Should we always make new or sometimes realloc
                // to avoid lots of wasted space?
                // can't realloc root due to risk of invalidating ptr
                // handed to us.
                // Leave for now and accept memory loss
                dom_hdr new = dom_hdr_new(required);

                if(!new)
                        return NULL;

                hdr->next = new;
                root->current = new;
                hdr = new;
        }
        size_t offset = hdr->count;
        hdr->count += required;
        return (dom_node)(offset + (void *)hdr);


}

static dom_node dom_add_type(dom_hdr root, jsonpg_type type, size_t count)
{
        dom_node node = dom_node_next(root, count);
        if(!node)
                return NULL;
        node->is.type.type = type;
        node->is.type.count = count;

        return node;
}

static dom_node dom_add_integer(dom_hdr root, long integer)
{
        dom_node node = dom_add_type(root, JSONPG_INTEGER, NODE_SIZE);
        if(!node)
                return NULL;

        node++;
        node->is.integer = integer;

        return node;
}

static dom_node dom_add_real(dom_hdr root, double real)
{
        dom_node node = dom_add_type(root, JSONPG_REAL, NODE_SIZE);
        if(!node)
                return NULL;

        node++;
        node->is.real = real;

        return node;
}

static dom_node dom_add_bytes(dom_hdr root, jsonpg_type type, uint8_t *bytes, size_t count)
{
        dom_node node = dom_add_type(root, type, count);
        if(!node)
                return NULL;

        node++;
        memcpy(node->is.bytes, bytes, count);

        return node;
}

static int dom_boolean(void *ctx, bool is_true)
{
        dom_hdr root = ctx;
        return !dom_add_type(root, is_true ? JSONPG_TRUE : JSONPG_FALSE, 0);
}

static int dom_null(void *ctx)
{
        dom_hdr root = ctx;
        return !dom_add_type(root, JSONPG_NULL, 0);
}

static int dom_integer(void *ctx, long integer)
{
        dom_hdr root = ctx;
        return !dom_add_integer(root, integer);
}

static int dom_real(void *ctx, double real)
{
        dom_hdr root = ctx;
        return !dom_add_real(root, real);
}

static int dom_string(void *ctx, uint8_t *bytes, size_t count)
{
        dom_hdr root = ctx;
        return !dom_add_bytes(root, JSONPG_STRING, bytes, count);
}

static int dom_key(void *ctx, uint8_t *bytes, size_t count)
{
        dom_hdr root = ctx;
        return !dom_add_bytes(root, JSONPG_KEY, bytes, count);
}

static int dom_begin_array(void *ctx)
{
        dom_hdr root = ctx;
        return !dom_add_type(root, JSONPG_BEGIN_ARRAY, 0);
}

static int dom_end_array(void *ctx)
{
        dom_hdr root = ctx;
        return !dom_add_type(root, JSONPG_END_ARRAY, 0);
}

static int dom_begin_object(void *ctx)
{
        dom_hdr root = ctx;
        return !dom_add_type(root, JSONPG_BEGIN_OBJECT, 0);
}

static int dom_end_object(void *ctx)
{
        dom_hdr root = ctx;
        return !dom_add_type(root, JSONPG_END_OBJECT, 0);
}


static jsonpg_callbacks dom_callbacks = {
        .boolean = dom_boolean,
        .null = dom_null,
        .integer = dom_integer,
        .real = dom_real,
        .string = dom_string,
        .key = dom_key,
        .begin_array = dom_begin_array,
        .end_array = dom_end_array,
        .begin_object = dom_begin_object,
        .end_object = dom_end_object,
};

jsonpg_dom dom_new(size_t size)
{
        jsonpg_dom dom = dom_hdr_new(size);
        if(!dom)
                return NULL;

        dom->current = dom;
        return dom;
}

void dom_free(void *p)
{
        jsonpg_dom dom = p;
        if(!dom) 
                return;

        jsonpg_dom next;
        while(dom) {
                next = dom->next;
                pg_dealloc(dom);
                dom = next;
        }
}

jsonpg_dom jsonpg_result_dom(jsonpg_generator g)
{
        return g->ctx;
}

jsonpg_generator dom_generator(void)
{
        jsonpg_dom dom = dom_new(0);
        if(!dom)
                return NULL;

        return generator_new(&dom_callbacks, dom, dom_free, 0);
}

jsonpg_type dom_parse_next(jsonpg_parser p)
{
        jsonpg_dom hdr = p->dom_info.hdr;
        size_t offset = p->dom_info.offset;

        if(offset >= hdr->count) {
                hdr = hdr->next;
                if(!hdr)
                        return JSONPG_EOF;
                offset = sizeof(struct jsonpg_dom_s);
        }

        dom_node node = (dom_node)(offset + (void *)hdr);
        offset += NODE_SIZE;
        jsonpg_type type = node->is.type.type;
        size_t count = node->is.type.count;
        switch(type) {
        case JSONPG_INTEGER:
                node++;
                offset += NODE_SIZE;
                p->result.number.integer = node->is.integer;
                break;
        case JSONPG_REAL:
                node++;
                offset += NODE_SIZE;
                p->result.number.real = node->is.real;
                break;
        case JSONPG_STRING:
        case JSONPG_KEY:
                node++;
                offset += dom_size_align(count);
                p->result.string.bytes = node->is.bytes;
                p->result.string.length = count;
                break;
        default:
        }
        p->dom_info.hdr = hdr;
        p->dom_info.offset = offset;
        return type;
}
