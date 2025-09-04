
#define JSONPG_EMPTY(X)   JSONPG_EMPTY_(X)
#define JSONPG_EMPTY_(X)  JSONPG_EMPTY_##X##_
#define JSONPG_EMPTY__    1

#ifndef JSONPG_PREFIX
#define JSONPG_PREFIX  gen_
#endif

#define JSONPG_NAME(X)     JSONPG_NAME_(JSONPG_PREFIX, X)
#define JSONPG_NAME_(X,Y)  JSONPG_NAME__(X, Y)
#define JSONPG_NAME__(X,Y) X##Y

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


#define JSONPG_GENERATOR_PRINT  jsonpg_print_
#define JSONPG_GENERATOR_PRETTY jsonpg_pretty_
#define JSONPG_GENERATOR_DOM    jsonpg_dom_

#ifndef JSONPG_GENERATOR
#define JSONPG_GENERATOR        JSONPG_GENERATOR_PRINT
#endif

#ifndef JSONPG_PRETTY_INDENT
#define JSONPG_PRETTY_INDENT    4
#endif

#ifndef JSONPG_CONTEXT_NAME
#define JSONPG_CONTEXT_NAME     JSONPG_NAME(ctx)
#endif

#if JSONPG_EMPTY(JSONPG_HANDLER_BOOLEAN) == 0
void JSONPG_HANDLER_BOOLEAN(void *ctx, bool is_true)
{
        JSONPG_NAME_(JSONPG_GENERATOR, boolean)(ctx, is_true);
}
#endif

#if JSONPG_EMPTY(JSONPG_HANDLER_NULL) == 0
void JSONPG_HANDLER_NULL(void *ctx)
{
        JSONPG_NAME_(JSONPG_GENERATOR, null)(ctx);
}
#endif

#if JSONPG_EMPTY(JSONPG_HANDLER_INTEGER) == 0
void JSONPG_HANDLER_INTEGER(void *ctx, long integer)
{
        JSONPG_NAME_(JSONPG_GENERATOR, integer)(ctx, integer);
}
#endif

#if JSONPG_EMPTY(JSONPG_HANDLER_REAL) == 0
void JSONPG_HANDLER_REAL(void *ctx, double real)
{
        JSONPG_NAME_(JSONPG_GENERATOR, real)(ctx, real);
}
#endif

#if JSONPG_EMPTY(JSONPG_HANDLER_STRING) == 0
void JSONPG_HANDLER_STRING(void *ctx, Bytes bytes, size_t count)
{
        JSONPG_NAME_(JSONPG_GENERATOR, string)(ctx, bytes, count);
}
#endif

#if JSONPG_EMPTY(JSONPG_HANDLER_KEY) == 0
void JSONPG_HANDLER_KEY(void *ctx, Bytes bytes, size_t count)
{
        JSONPG_NAME_(JSONPG_GENERATOR, key)(ctx, bytes, count);
}
#endif

#if JSONPG_EMPTY(JSONPG_HANDLER_START_OBJECT) == 0
void JSONPG_HANDLER_START_OBJECT(void *ctx)
{
        JSONPG_NAME_(JSONPG_GENERATOR, start_object)(ctx);
}
#endif

#if JSONPG_EMPTY(JSONPG_HANDLER_END_OBJECT) == 0
void JSONPG_HANDLER_END_OBJECT(void *ctx)
{
        JSONPG_NAME_(JSONPG_GENERATOR, end_object)(ctx);
}
#endif

#if JSONPG_EMPTY(JSONPG_HANDLER_START_ARRAY) == 0
void JSONPG_HANDLER_START_ARRAY(void *ctx)
{
        JSONPG_NAME_(JSONPG_GENERATOR, start_array)(ctx);
}
#endif

#if JSONPG_EMPTY(JSONPG_HANDLER_END_ARRAY) == 0
void JSONPG_HANDLER_END_ARRAY(void *ctx)
{
        JSONPG_NAME_(JSONPG_GENERATOR, end_array)(ctx);
}
#endif

void *JSONPG_CONTEXT_NAME(void)
{
        return JSONPG_NAME_(JSONPG_GENERATOR, new)();
}

// TODO: static fns, ctx free, get generated string/dom
