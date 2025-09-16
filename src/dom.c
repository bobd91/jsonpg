#include <stdint.h>

#define DOM_MIN_SIZE 8192
#define NODE_SIZE (sizeof(struct dom_node_s))

typedef struct dom_node_s *DomNode;

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
        di.offset = sizeof(dom_s);
        return di;
}

// Ensure all sizes align with structure size
static inline size_t dom_size_align(size_t size)
{
        return NODE_SIZE * (1 + ((size - 1) / NODE_SIZE));
}


// Ensure minimum size
static inline size_t dom_alloc_size(size_t size)
{
        size = size < DOM_MIN_SIZE 
                ? DOM_MIN_SIZE 
                : size;
        return dom_size_align(size);
}

static inline Dom dom_hdr_new(Allocator a, size_t size)
{
        size = dom_alloc_size(size + sizeof(dom_s));

        Dom hdr = allocator_alloc(a, size);

        if(!hdr)
                return NULL;

        hdr->next = NULL;
        hdr->current = NULL;
        hdr->size = size;
        hdr->count = sizeof(dom_s);

        return hdr;
}

static inline DomNode dom_node_next(Dom root, size_t count)
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
        return (DomNode)(offset + (char *)hdr);
}

static inline DomNode dom_add_type(Dom root, JsonType type, size_t count)
{
        DomNode node = dom_node_next(root, count);
        if(!node)
                return NULL;
        node->is.type = type;
        node++;
        node->is.count = count;

        return node;
}

static inline DomNode dom_add_integer(Dom root, long integer)
{
        DomNode node = dom_add_type(root, JSONPG_INTEGER, NODE_SIZE);
        if(!node)
                return NULL;

        node++;
        node->is.integer = integer;

        return node;
}

static inline DomNode dom_add_real(Dom root, double real)
{
        DomNode node = dom_add_type(root, JSONPG_REAL, NODE_SIZE);
        if(!node)
                return NULL;

        node++;
        node->is.real = real;

        return node;
}

static inline DomNode dom_add_bytes(Dom root, JsonType type, const unsigned char *bytes, size_t count)
{
        DomNode node = dom_add_type(root, type, count);
        if(!node)
                return NULL;

        node++;
        memcpy(node->is.bytes, bytes, count);

        return node;
}

static inline bool dom_boolean(void *ctx, bool is_true)
{
        Dom root = ctx;
        return dom_add_type(root, is_true ? JSONPG_TRUE : JSONPG_FALSE, 0);
}

static inline bool dom_null(void *ctx)
{
        Dom root = ctx;
        return dom_add_type(root, JSONPG_NULL, 0);
}

static inline bool dom_integer(void *ctx, long integer)
{
        Dom root = ctx;
        return dom_add_integer(root, integer);
}

static inline bool dom_real(void *ctx, double real)
{
        Dom root = ctx;
        return dom_add_real(root, real);
}

static inline bool dom_string(void *ctx, const unsigned char *bytes, size_t count)
{
        Dom root = ctx;
        return dom_add_bytes(root, JSONPG_STRING, bytes, count);
}

static inline bool dom_key(void *ctx, const unsigned char *bytes, size_t count)
{
        Dom root = ctx;
        return dom_add_bytes(root, JSONPG_KEY, bytes, count);
}

static inline bool dom_start_array(void *ctx)
{
        Dom root = ctx;
        return dom_add_type(root, JSONPG_START_ARRAY, 0);
}

static inline bool dom_end_array(void *ctx)
{
        Dom root = ctx;
        return dom_add_type(root, JSONPG_END_ARRAY, 0);
}

static inline bool dom_start_object(void *ctx)
{
        Dom root = ctx;
        return dom_add_type(root, JSONPG_START_OBJECT, 0);
}

static inline bool dom_end_object(void *ctx)
{
        Dom root = ctx;
        return dom_add_type(root, JSONPG_END_OBJECT, 0);
}


static Callbacks dom_callbacks = {
        .boolean = dom_boolean,
        .null = dom_null,
        .integer = dom_integer,
        .real = dom_real,
        .string = dom_string,
        .key = dom_key,
        .start_array = dom_start_array,
        .end_array = dom_end_array,
        .start_object = dom_start_object,
        .end_object = dom_end_object,
};

static Dom dom_new(Allocator a, size_t size)
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
                if(!hdr) {
                        p->result.type = JSONPG_EOF;
                        return JSONPG_EOF;
                }
                offset = sizeof(struct jsonpg_dom_s);
        }

        DomNode node = (DomNode)(offset + (char *)hdr);
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
                p->result.string.count = count;
                break;
        default:
        }
        p->dom_info.hdr = hdr;
        p->dom_info.offset = offset;

        p->result.type = type;
        return type;
}

static ParseResult dom_parse(Parser p, Generator g)
{
        Dom hdr = p->dom_info.hdr;
        size_t offset = sizeof(dom_s);
        bool ok = true;

        while(hdr && ok) {
                DomNode node = (DomNode)(offset + (char *)hdr);
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
                case JSONPG_FALSE:
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
                        ok = jsonpg_integer(g, node->is.integer);
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
