#pragma once

static Generator generator_new(uint16_t, unsigned);
static Generator generator_set_callbacks(Generator, Callbacks *callbacks, void *ctx);
static Generator generator_reset(Generator, unsigned);
