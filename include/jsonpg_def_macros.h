/*
 * Macros to make writing JSON via a generator more concise
 *
 * // This define specifies the name of your generator variable
 * // defaults to 'gen' 
 * #define JSONPG_GEN  my_gen
 * jsonpg_generator my_gen = jsonpg_generator_new(...);
 *
 * #include <libjsonpg/jsonpg_def_macros.h>
 *
 * start_array();
 * object(
 *      key("k1"), real(12.5),
 *      key("k2"), array(boolean(true), boolean(false), null()),
 *      key("k3"), string("Value 3")
 *      );
 * end_array();
 *
 * #include <libjsonpg/jsonpg_undef_macros.h>
 *
 * Produces:
 *
 * [
 *     {
 *         "k1": 12.5,
 *         "k2": [
 *             true,
 *             false,
 *             null
 *         ],
 *         "k3": "Value 3"
 *    }
 * ]
 *
 * Note: object() and array() macros expand to a single C statement
 *       so generating a large structure may hit compiler limits
 */

#include <string.h>

#ifndef JSONPG_GEN
#define JSONPG_GEN              gen
#endif

#ifndef JSONPG_MACROS
#define JSONPG_MACROS

#define object(...)             start_object(), __VA_ARGS__, end_object()
#define array(...)              start_array(), __VA_ARGS__, end_array()

#define key(S)                  jsonpg_key((JSONPG_GEN), (unsigned char *)(S), strlen(S))
#define string(S)               jsonpg_string((JSONPG_GEN), (unsigned char *)(S), strlen(S))
#define boolean(B)              jsonpg_boolean((JSONPG_GEN), (B))
#define null()                  jsonpg_null((JSONPG_GEN))
#define integer(I)              jsonpg_integer((JSONPG_GEN), (I))
#define real(R)                 jsonpg_real((JSONPG_GEN), (R))
#define key_bytes(B, C)         jsonpg_key((JSONPG_GEN), (B), (C))
#define string_bytes(B, C)      jsonpg_string((JSONPG_GEN), (B), (C))
#define start_object()          jsonpg_start_object((JSONPG_GEN))
#define end_object()            jsonpg_end_object((JSONPG_GEN))
#define start_array()           jsonpg_start_array((JSONPG_GEN))
#define end_array()             jsonpg_end_array((JSONPG_GEN))

#endif

