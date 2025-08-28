#pragma once

static jsonpg_dom dom_generator_ctx(arena a);
static jsonpg_generator dom_generator(jsonpg_generator);

typedef struct dom_info_s {
        jsonpg_dom hdr;
        size_t offset;
} dom_info;
