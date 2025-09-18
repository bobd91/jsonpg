#pragma once

static generator *generator_new(unsigned, unsigned);
static generator *generator_set_callbacks(generator *, callbacks *callbacks, void *ctx);
static generator *generator_reset(generator *, unsigned);
