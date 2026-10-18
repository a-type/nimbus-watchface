#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cloud.h"

#define TEST_WIDTH 260
#define TEST_HEIGHT 260

static GColor s_pixels[TEST_HEIGHT][TEST_WIDTH];
static GColor s_color;
static uint32_t s_draw_calls;
static uint32_t s_color_calls[4];
static bool s_out_of_bounds;

void graphics_context_set_fill_color(GContext *context, GColor color) {
	context->fill_color = color;
	s_color = color;
}

void graphics_fill_rect(GContext *context, GRect rect, uint16_t radius, uint8_t corners) {
	(void)context;
	(void)radius;
	(void)corners;
	if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height != 1 ||
		rect.x + rect.width > TEST_WIDTH || rect.y >= TEST_HEIGHT) {
		s_out_of_bounds = true;
		return;
	}
	for (int16_t x = rect.x; x < rect.x + rect.width; x++) {
		s_pixels[rect.y][x] = s_color;
	}
	s_draw_calls++;
	if (s_color == GColorWhite) s_color_calls[0]++;
	else if (s_color == GColorFromRGB(80, 80, 80)) s_color_calls[1]++;
	else if (s_color == GColorBlack) s_color_calls[2]++;
	else if (s_color == GColorFromRGB(50, 50, 50)) s_color_calls[3]++;
}

static void clear_output(void) {
	memset(s_pixels, 0, sizeof(s_pixels));
	memset(s_color_calls, 0, sizeof(s_color_calls));
	s_color = GColorBlack;
	s_draw_calls = 0;
	s_out_of_bounds = false;
}

static void verify_renderer(int16_t width, int16_t height, uint32_t seed) {
	GContext context = {0};
	GColor expected[TEST_HEIGHT][TEST_WIDTH];
	clear_output();
	assert(cloud_init(width, height, seed));
	cloud_draw(&context);
	assert(!s_out_of_bounds);
	assert(s_draw_calls > 0);
	assert(s_color_calls[0] > 0);
	assert(s_color_calls[1] > 0);
	assert(s_color_calls[2] > 0);
	assert(s_color_calls[3] > 0);
	memcpy(expected, s_pixels, sizeof(expected));

	clear_output();
	assert(cloud_init(width, height, seed));
	cloud_draw(&context);
	assert(!s_out_of_bounds);
	assert(memcmp(expected, s_pixels, sizeof(expected)) == 0);

	clear_output();
	assert(cloud_init(width, height, seed + 1u));
	cloud_draw(&context);
	assert(!s_out_of_bounds);
	assert(memcmp(expected, s_pixels, sizeof(expected)) != 0);
}

int main(void) {
	verify_renderer(200, 228, 20261018u);
	verify_renderer(228, 228, 20261019u);
	assert(!cloud_init(TEST_WIDTH + 1, 228, 20261018u));
	puts("native cloud renderer checks passed");
	return 0;
}
