/*
 * jsonpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * jsonpg.c
 *   includes all required files for a unity style build
 */

#include "../include/jsonpg.h"
#include "common.h"
#include "types.h"
#include "generate.h"
#include "dom.h"

#include "thirdparty/fast_double_parser.h"
#include "thirdparty/dragonbox.c"
#include "thirdparty/itoa.c"

//#define JSONPG_DEBUG
#include "debug.c"
#include "alloc.c"
#include "utf8.c"
#include "input.c"
#include "error.c"
#include "output.c"
#include "stack.c"
#include "generate.c"
#include "dom.c"
#include "parser.c"
#include "parse.c"
#include "parsenext.c"

