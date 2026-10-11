#pragma once

#include <pebble.h>

bool cloud_init(int16_t width, int16_t height);
void cloud_draw(GContext *context);