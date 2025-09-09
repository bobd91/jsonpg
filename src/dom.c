#include <stdint.h>

#define DOM_MIN_SIZE 8192
#define NODE_SIZE (sizeof(struct dom_node_s))

typedef struct jsonpg_dom_s *Dom;
typedef struct dom_node_s *DomNode;


struct jsonpg_dom_s {
        Allocator allocator;
        Dom next;
        Dom current;
        size_t count;
        size_t size;
};

struct dom_node_s {
        union {
                JsonType type;
                size_t count;
                double real;
                long integer;
                Byte bytes[sizeof(size_t)];
        } is;
};

static DomInfo dom_parser_info(Dom dom)
{
        DomInfo di;
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
static size_t dom_alloc_size(size_t size)
{
        size = size < DOM_MIN_SIZE 
                ? DOM_MIN_SIZE 
                : size;
        return dom_size_align(size);
}

static Dom dom_hdr_new(Allocator a, size_t size)
{
        size = dom_alloc_size(size + sizeof(struct jsonpg_dom_s));

        Dom hdr = allocator_alloc(a, size);

        if(!hdr)
                return NULL;

        hdr->next = NULL;
        hdr->current = NULL;
        hdr->size = size;
        hdr->count = sizeof(struct jsonpg_dom_s);

        return hdr;
}

static DomNode dom_node_next(Dom root, size_t count)
{
        size_t required = dom_size_align(count + 2 * NODE_SIZE);
        Dom hdr = root->current;
        if(required > hdr->size - hdr->count) {
                Dom new = dom_hdr_new(root->allocator, required);

                if(!new)
                        return NULL;

                hdr->next = new;
                root->current = new;
                hdr = new;
        }
        size_t offset = hdr->count;
        hdr->count += required;
        return (DomNode)(offset + (void *)hdr);
}

static DomNode dom_add_type(Dom root, JsonType type, size_t count)
{
        DomNode node = dom_node_next(root, count);
        if(!node)
                return NULL;
        node->is.type = type;
        node++;
        node->is.count = count;

        return node;
}

static DomNode dom_add_integer(Dom root, long integer)
{
        DomNode node = dom_add_type(root, JSONPG_INTEGER, NODE_SIZE);
        if(!node)
                return NULL;

        node++;
        node->is.integer = integer;

        return node;
}

static DomNode dom_add_real(Dom root, double real)
{
        DomNode node = dom_add_type(root, JSONPG_REAL, NODE_SIZE);
        if(!node)
                return NULL;

        node++;
        node->is.real = real;

        return node;
}

static DomNode dom_add_bytes(Dom root, JsonType type, Bytes bytes, size_t count)
{
        DomNode node = dom_add_type(root, type, count);
        if(!node)
                return NULL;

        node++;
        memcpy(node->is.bytes, bytes, count);

        return node;
}

static int dom_boolean(void *ctx, bool is_true)
{
        Dom root = ctx;
        return !dom_add_type(root, is_true ? JSONPG_TRUE : JSONPG_FALSE, 0);
}

static int dom_null(void *ctx)
{
        Dom root = ctx;
        return !dom_add_type(root, JSONPG_NULL, 0);
}

static int dom_integer(void *ctx, long integer)
{
        Dom root = ctx;
        return !dom_add_integer(root, integer);
}

static int dom_real(void *ctx, double real)
{
        Dom root = ctx;
        return !dom_add_real(root, real);
}

static int dom_string(void *ctx, Bytes bytes, size_t count)
{
        Dom root = ctx;
        return !dom_add_bytes(root, JSONPG_STRING, bytes, count);
}

static int dom_key(void *ctx, Bytes bytes, size_t count)
{
        Dom root = ctx;
        return !dom_add_bytes(root, JSONPG_KEY, bytes, count);
}

static int dom_begin_array(void *ctx)
{
        Dom root = ctx;
        return !dom_add_type(root, JSONPG_BEGIN_ARRAY, 0);
}

static int dom_end_array(void *ctx)
{
        Dom root = ctx;
        return !dom_add_type(root, JSONPG_END_ARRAY, 0);
}

static int dom_begin_object(void *ctx)
{
        Dom root = ctx;
        return !dom_add_type(root, JSONPG_BEGIN_OBJECT, 0);
}

static int dom_end_object(void *ctx)
{
        Dom root = ctx;
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

Dom dom_new(Allocator a, size_t size)
{
        Dom root = dom_hdr_new(a, size);
        if(!root)
                return NULL;

        root->allocator = a;
        root->current = root;
        return root;
}

Dom jsonpg_result_dom(Generator g)
{
        return g->ctx;
}

static Generator dom_generator(Generator g)
{
        Dom root = dom_new(g->allocator, 0);
        if(!root)
                return NULL;

        return generator_set_callbacks(g, &dom_callbacks, root);
}

static JsonType dom_parse_next(Parser p)
{
        Dom hdr = p->dom_info.hdr;
        size_t offset = p->dom_info.offset;

        if(offset >= hdr->count) {
                hdr = hdr->next;
                if(!hdr)
                        return JSONPG_EOF;
                offset = sizeof(struct jsonpg_dom_s);
        }

        DomNode node = (DomNode)(offset + (void *)hdr);
        offset += NODE_SIZE;
        JsonType type = node->is.type;
        node++;
        offset += NODE_SIZE;
        size_t count = node->is.count;
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

static ParseResult dom_parse(Parser p, Generator g)
{
        Dom hdr = p->dom_info->hdr;
        size_t offset = sizeof(struct jsonpg_dom_s);
        bool ok = true;

        while(hdr && ok) {
                DomNode node = (DomNode)(offset + (void *)hdr);
                offset += NODE_SIZE;
                JsonType type = node->is.type;
                node++;
                offset += NODE_SIZE;
                size_t count = node->is.count;

                switch(type) {
                case JSONPG_STRING:
                        node++;
                        offset += dom_size_align(count);
                        ok = jsonpg_string(g, node->is.bytes, count);
                        break;

                case JSONPG_KEY:
                        node++;
                        offset += dom_size_align(count);
                        ok = jsonpg_key(g, node->is.bytes, count);
                        break;

                case JSONPG_TRUE:
                case JSNOPG_FALSE:
                        ok = jsonpg_boolean(g, type == JSONPG_TRUE);
                        break;

                case JSONPG_NULL:
                        ok = jsonpg_null(g);
                        break;

                case JSONPG_START_OBJECT:
                        ok = jsonpg_start_object(g);
                        break;

                case JSONPG_END_OBJECT:
                        ok = jsonpg_end_object(g);
                        break;

                case JSONPG_START_ARRAY:
                        ok = jsonpg_start_array(g);
                        break;

                case JSONPG_END_ARRAY:
                        ok = jsonpg_end_array(g);
                        break;

                case JSONPG_INTEGER:
                        node++;
                        offset += NODE_SIZE;
                        ok = jsongpg_integer(g, node->is.integer);
                        break;

                case JSONPG_REAL:
                        node++;
                        offset += NODE_SIZE;
                        ok = jsonpg_real(g, node->is.real);
                        break;

                default:
                        ok = false;
                }
                
                if(offset >= hdr->count && ok) {
                        offset = sizeof(struct jsonpg_dom_s);
                        hdr = hdr->next;
                }
        }

        if(!ok)
                return make_pg_error_return(p, g);

        return (ParseResult) { .type = JSONPG_EOF };

}
