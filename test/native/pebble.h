#pragma once

#include <stdint.h>

typedef uint32_t GColor;
typedef struct {
	int16_t x;
	int16_t y;
	int16_t width;
	int16_t height;
} GRect;
typedef struct {
	GColor fill_color;
} GContext;

#define GColorWhite ((GColor)0xFFFFFF)
#define GColorBlack ((GColor)0x000000)
#define GColorFromRGB(red, green, blue) \
	((GColor)(((red) << 16) | ((green) << 8) | (blue)))
#define GRect(x, y, width, height) ((GRect){(x), (y), (width), (height)})
#define GCornerNone 0

void graphics_context_set_fill_color(GContext *context, GColor color);
void graphics_fill_rect(GContext *context, GRect rect, uint16_t radius, uint8_t corners);
