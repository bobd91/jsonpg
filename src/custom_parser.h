
#define JSONPG_EMPTY(X)   JSONPG_EMPTY_(X)
#define JSONPG_EMPTY_(X)  JSONPG_EMPTY_##X##_
#define JSONPG_EMPTY__    1

#ifndef JSONPG_PREFIX
#define JSONPG_PREFIX  jsonpg_
#endif

#define JSONPG_NAME(X)     JSONPG_NAME_(JSONPG_PREFIX, X)
#define JSONPG_NAME_(X,Y)  JSONPG_NAME__(X, Y)
#define JSONPG_NAME__(X,Y) X##Y

#ifndef JSONPG_PARSE_NAME
#define JSONPG_PARSE_NAME     JSONPG_NAME(parse)
#endif

#ifndef JSONPG_HANDLER_BOOLEAN
#define JSONPG_HANDLER_BOOLEAN JSONPG_NAME(boolean)
#endif
#ifndef JSONPG_HANDLER_NULL
#define JSONPG_HANDLER_NULL JSONPG_NAME(null)
#endif
#ifndef JSONPG_HANDLER_INTEGER
#define JSONPG_HANDLER_INTEGER JSONPG_NAME(integer)
#endif
#ifndef JSONPG_HANDLER_REAL
#define JSONPG_HANDLER_REAL JSONPG_NAME(real)
#endif
#ifndef JSONPG_HANDLER_STRING
#define JSONPG_HANDLER_STRING JSONPG_NAME(string)
#endif
#ifndef JSONPG_HANDLER_KEY
#define JSONPG_HANDLER_KEY JSONPG_NAME(key)
#endif
#ifndef JSONPG_HANDLER_START_OBJECT
#define JSONPG_HANDLER_START_OBJECT JSONPG_NAME(start_object)
#endif
#ifndef JSONPG_HANDLER_END_OBJECT
#define JSONPG_HANDLER_END_OBJECT JSONPG_NAME(end_object)
#endif
#ifndef JSONPG_HANDLER_START_ARRAY
#define JSONPG_HANDLER_START_ARRAY JSONPG_NAME(start_array)
#endif
#ifndef JSONPG_HANDLER_END_ARRAY
#define JSONPG_HANDLER_END_ARRAY JSONPG_NAME(end_array)
#endif

static void output_boolean(Parser p, bool is_true)
{
#if JSONPG_EMPTY(JSONPG_HANDLER_BOOLEAN) == 0
        if(JSONPG_HANDLER_BOOLEAN(p->ctx, is_true))
                parse_error(p, TERMINATED);
#endif

static void output_null(Parser p)
{
#if JSONPG_EMPTY(JSONPG_HANDLER_NULL) == 0
        if(JSONPG_HANDLER_NULL(p->ctx))
                parse_error(p, TERMINATED);
#endif
}

static void output_integer(Parser p, long integer)
{
#if JSONPG_EMPTY(JSONPG_HANDLER_INTEGER) == 0
        if(JSONPG_HANDLER_INTEGER(p->ctx, integer))
                parse_error(p, TERMINATED);
#endif
}

static void output_real(Parser p, double real)
{
#if JSONPG_EMPTY(JSONPG_HANDLER_REAL) == 0
        if(JSONPG_HANDLER_REAL(p->ctx, real))
                parse_error(p, TERMINATED);
#endif
}

static void output_string(Parser p, Bytes string, size_t count)
{
#if JSONPG_EMPTY(JSONPG_HANDLER_STRING) == 0
        if(JSONPG_HANDLER_STRING(p->ctx, string, count))
                parse_error(p, TERMINATED);
#endif
        return true;
}

static void output_key(Parser p, Bytes key, size_t count)
{
#if JSONPG_EMPTY(JSONPG_HANDLER_KEY) == 0
        if(JSONPG_HANDLER_KEY(p->ctx, key, count))
                parse_error(p, TERMINATED);
#endif
        return true;
}

static void output_start_object(Parser p)
{
#if JSONPG_EMPTY(JSONPG_HANDLER_START_OBJECT) == 0
        if(JSONPG_HANDLER_START_OBJECT(p->ctx))
                parse_error(p, TERMINATED)
#endif
        return true;
}

static void output_end_object(Parser p)
{
#if JSONPG_EMPTY(JSONPG_HANDLER_END_OBJECT) == 0
        if(JSONPG_HANDLER_END_OBJECT(p->ctx))
                parse_error(p, TERMINATED)
#endif
        return true;
}

static void output_start_array(Parser p)
{
#if JSONPG_EMPTY(JSONPG_HANDLER_START_ARRAY) == 0
        if(JSONPG_HANDLER_START_ARRAY(p->ctx))
                parse_error(p, TERMINATED);
#endif
        return true;
}

static void output_end_array(Parser p)
{
#if JSONPG_EMPTY(JSONPG_HANDLER_END_ARRAY) == 0
        if(!JSONPG_HANDLER_END_ARRAY(p->ctx))
                parse_error(p, TERMINATED);
#endif
        return true;
}

static void parse_value(JpgParser p)
{
        Nesting nesting = nesting_new(p->allocator);
        if(!nesting)
                parse_error(p, MEMORY);

        Bytes bytes;
        size_t count;
        int initial_level = nesting->level;
        bool is_key = nesting->type == JSONPG_OBJECT;

        do {
                Byte b = input_peek(p);

                if(is_key) {
                        if(b == '"')
                                count = parse_string(p, &bytes);

#ifdef JSONPG_FLAG_SINGLE_QUOTE
                        else if(b == '\'') {
                                count = parse_squote_string(p, &bytes);
#endif
#ifdef JSONPG_FLAG_KEY_NO_QUOTE
                        else
                                count = parse_nquote_string(p, &bytes);
#else
                        else
                                parse_error(p, KEY);
#endif
                        output_key(p, bytes, count);
                        consume_whitespace(p);
                        if(input_consume(p, ':'))
                                parse_error(p, COLON);
                        consume_whitespace(p);

                        is_key = false;
                        b = input_peek(p);
                }

                switch(b) {
                case '"':
                        count = parse_string(p, &bytes);
                        output_string(p, bytes, count);
                        break;
                case '{': 
                        input_take(p);
                        consume_whitespace(p);
                        nesting = nesting_push(p, JSONPG_OBJECT);
                        output_start_object(p);
                        if(!input_consume(p, '}')) {
                                consume_whitespace(p);
                                is_key = true;
                                continue;
                        }
                        output_end_object(p);
                        nesting = nesting_pop(p);
                        break;
                case '[':
                        input_take(p);
                        consume_whitespace(p);
                        nesting = nesting_push(p, JSONPG_ARRAY);
                        output_start_array(p);
                        if(!input_consume(p,']')) {
                                consume_whitespace(p);
                                continue;
                        }
                        output_end_array(p);
                        nesting = nesting_pop(p);
                        break;
                case 't':
                        parse_true(p);
                        output_boolean(p, true);
                        break;
                case 'f':
                        parse_false(p);
                        output_boolean(p, false)
                        break;
                case 'n':
                        parse_null(p);
                        output_null(p);
                        break;

#ifdef JSONPG_FLAG_SINGLE_QUOTE
                case '\'':
                        count = parse_squote_string(p, &bytes);
                        output_string(p, bytes, count);
                        break;
#endif
#ifdef JSONPG_FLAG_TRAILING_COMMA
                case '}':
                        input_take(p);
                        output_end_object(p);
                        nesting = nesting_pop(p);
                        break;
                case ']':
                        input_take(p);
                        output_end_array(p);
                        nesting = nesting_pop(p);
                        break;
#endif
                default:
                        if(b == '-' || ('0' <= b && b <= '9')) {
                                double d;
                                long l;
                                if(parse_number(p, &d, &l))
                                        output_real(p, d);
                                else
                                        output_integer(p, l)
                                break;
                        }

#ifdef JSONPG_STRING_NO_QUOTE
                        count = parse_nquote_string(p, &bytes);
                        output_string(p, bytes, count);
                        break;
#else
                        parse_error(p, UNEXPECTED);
#endif
                }

                consume_whitespace(p);

                if(nesting->type == JSONPG_OBJECT) {
                        if(input_consume(p, '}')) {
                                consume_whitespace(p);
                                output_end_object(p);
                                nesting = nesting_pop(p);
                        }
                } else if(nesting->type == JSONPG_ARRAY) {
                        if(input_consume(p, ']')) {
                                consume_whitespace(p);
                                output_end_array(p);
                                nesting = nesting_pop(p);
                        }
                }
                if(nesting->level > initial->level) {
                        if(input_consume(p, ',')) {
                                consume_whitespace(p);
                                is_key = nesting->type == JSONPG_OBJECT;
                                continue;
                        }

#ifdef JSONPG_FLAG_OPTIONAL_COMMA
                        is_key = nesting->type == JSONPG_OBJECT;
                        continue;
#endif
                        parse_error(p, UNEXPECTED);
                } 
                break;
        } while(!input_eof(p));

#ifndef JSONPG_FLAG_IGNORE_TRAILING_CONTENT
        consume_whitespace(p);
        if(!parse_eof(p))
                parse_error(p, UNEXPECTED);
#endif
}

ParseResult JSONPG_PARSE_NAME(Bytes bytes, size_t count, void *ctx)
{
        Parser p = parser_new(bytes, count, ctx);
        if(!p)
                return NULL;

        if(0 == setjmp(p->env))
                parse_json(p);

        ParseResult result = p->result;
        parser_free(p);

        return result;
}

#undef JSONPG_EMPTY
#undef JSONPG_EMPTY_
#undef JSONPG_EMPTY__
#undef JSONPG_PREFIX
#undef JSONPG_NAME
#undef JSONPG_NAME_
#undef JSONPG_NAME__
#undef JSONPG_PARSE_NAME
#undef JSONPG_HANDLER_BOOLEAN
#undef JSONPG_HANDLER_NULL
#undef JSONPG_HANDLER_INTEGER
#undef JSONPG_HANDLER_REAL
#undef JSONPG_HANDLER_STRING
#undef JSONPG_HANDLER_KEY
#undef JSONPG_HANDLER_START_OBJECT
#undef JSONPG_HANDLER_END_OBJECT
#undef JSONPG_HANDLER_START_ARRAY
#undef JSONPG_HANDLER_END_ARRAY
