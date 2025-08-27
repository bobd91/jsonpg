/*
 * Macros to make writing JSON via a generator more concise
 *
 * // This define specifies the name of your generator variable
 * // defaults to 'gen' 
 * #define JSONPG_GEN  my_gen
 * jsonpg_generator my_gen = jsonpg_generator_new(...);
 *
 * #include jsonpg_def_macros.h
 *
 * begin_array();
 * object(
 *      kv("k1", real(12.5)),
 *      kv("k2", array(true(), false(), null())),
 *      kv("k3", str("Value 3"))
 *      );
 * end_array();
 *
 * #include jsonpg_undef_macros.h
 *
 * Produces:
 *
 * [ { "k1": 12.5, "k2": [true, false, null], "k3": "Value 3" } ]
 *
 */

#ifndef JSONPG_GEN
#define JSONPG_GEN      gen
#endif

#ifndef JSONPG_MACROS
#define JSONPG_MACROS

#define object(...)     begin_object(), __VA_ARGS__, end_object()
#define array(...)      begin_array(), __VA_ARGS__, end_array()

#define kv(S,...)       key(S), __VA_ARGS__
#define key(S)          jsonpg_key((JSONPG_GEN), (uint8_t *)(S), strlen(S))
#define str(S)          jsonpg_string((JSONPG_GEN), (uint8_t *)(S), strlen(S))
#define true()          jsonpg_boolean((JSONPG_GEN), true)
#define false()         jsonpg_boolean((JSONPG_GEN), false)
#define null()          jsonpg_null((JSONPG_GEN))
#define integer(I)      jsonpg_integer((JSONPG_GEN), (I))
#define real(R)         jsonpg_real((JSONPG_GEN), (R))
#define key_bytes(B, C) jsonpg_key((JSONPG_GEN), (B), (C))
#define str_bytes(B, C) jsonpg_string((JSONPG_GEN), (B), (C))
#define begin_object()  jsonpg_begin_object((JSONPG_GEN))
#define end_object()    jsonpg_end_object((JSONPG_GEN))
#define begin_array()   jsonpg_begin_array((JSONPG_GEN))
#define end_array()     jsonpg_end_array((JSONPG_GEN))

#endif

