#pragma once

#include <pebble.h>

bool cloud_init(int16_t width, int16_t height, uint32_t seed);
void cloud_set_shadow_dither(bool enabled);
void cloud_draw(GContext *context);
