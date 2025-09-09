
#include <assert.h>

#define ASSERT(X)   assert(X)

#ifdef __has_builtin
#define HAS_BUILTIN(X)  __has_builtin(X)
#else
#define HAS_BUILTIN(X)  0
#endif
