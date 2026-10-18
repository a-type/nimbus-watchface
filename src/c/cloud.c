#include <pebble.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cloud.h"

#define CLOUD_GENERATIONS 4
#define CLOUD_LOBE_COUNT 781
#define CLOUD_OVERLAY_GENERATIONS (CLOUD_GENERATIONS - 1)
#define CLOUD_OVERLAY_LOBE_COUNT 156
#define CLOUD_OVERLAY_CONTOUR_START 31
#define CLOUD_OVERLAY_CONTOUR_COUNT 125
#define CLOUD_OVERLAY_ERASE_START 6
#define CLOUD_OVERLAY_ERASE_COUNT 25
#define CLOUD_MAX_WIDTH 260
#define CLOUD_SCANLINE_COUNT 5
#define OVERLAY_CONTOUR 0x01
#define OVERLAY_WHITE 0x02
#define OVERLAY_SHADOW 0x04

typedef struct {
	float x;
	float y;
	float radius_x;
	float radius_y;
	float heading;
} CloudLobe;

static const uint16_t s_depth_starts[] = {0, 1, 6, 31, 156};
static const uint16_t s_depth_ends[] = {1, 6, 31, 156, 781};
static const float s_root_headings[] = {-1.57f, -0.12f, 3.02f, -2.26f, -0.78f};
static CloudLobe s_lobes[CLOUD_LOBE_COUNT];
static CloudLobe s_overlay_lobes[CLOUD_OVERLAY_LOBE_COUNT];
static bool s_lobe_shaded[CLOUD_LOBE_COUNT];
static bool s_overlay_contour[CLOUD_OVERLAY_CONTOUR_COUNT];
static bool s_overlay_erase[CLOUD_OVERLAY_ERASE_COUNT];
static bool s_overlay_shaded[CLOUD_OVERLAY_ERASE_COUNT];
static uint8_t s_scanlines[CLOUD_SCANLINE_COUNT][CLOUD_MAX_WIDTH];
static uint8_t s_overlay_scanlines[CLOUD_SCANLINE_COUNT][CLOUD_MAX_WIDTH];
static int16_t s_width;
static int16_t s_height;
static int16_t s_left;
static int16_t s_right;
static int16_t s_floor;
static float s_fit;
static float s_offset_x;
static float s_offset_y;
static float s_base_top;
static float s_base_left;
static float s_base_right;
static float s_corner;
static uint32_t s_random_state;
static bool s_ready;

static float random_unit(void) {
	s_random_state += 0x6d2b79f5u;
	uint32_t value = (s_random_state ^ (s_random_state >> 15)) * (1u | s_random_state);
	value ^= value + ((value ^ (value >> 7)) * (61u | value));
	return (float)((value ^ (value >> 14)) / 4294967296.0);
}

static float random_between(float minimum, float maximum) {
	return minimum + random_unit() * (maximum - minimum);
}

static float vertical_shape_influence(
	float origin_y,
	float floor,
	float height,
	float radius_y,
	float lateralness
) {
	const float fraction = fminf(1.0f, fmaxf(0.0f, (floor - origin_y) / fmaxf(1.0f, height)));
	const float lower = fmaxf(0.0f, (0.5f - fraction) / 0.5f);
	const float upper = fmaxf(0.0f, (fraction - 0.5f) / 0.5f);
	const float lateral_squared = lateralness * lateralness;
	return radius_y * 1.42f * (0.58f * lateral_squared * lateral_squared * lower - 0.18f * upper);
}

static void generate_lobes(float width, float height, float floor, float root_x,
	CloudLobe *lobes, float size_multiplier, uint8_t max_depth, uint32_t seed) {
	s_random_state = seed;
	const float root_radius = floorf(fminf(width, height) * 0.25f * size_multiplier + 0.5f);
	lobes[0] = (CloudLobe){root_x, floor - root_radius, root_radius, root_radius, -1.57f};
	float branch_headings[5];

	for (uint16_t depth = 1; depth <= max_depth; depth++) {
		uint16_t child_index = s_depth_starts[depth];
		for (uint16_t parent_index = s_depth_starts[depth - 1];
			parent_index < s_depth_ends[depth - 1]; parent_index++) {
			const CloudLobe parent = lobes[parent_index];
			if (depth != 1) {
				for (uint8_t branch = 0; branch < 5; branch++) {
					branch_headings[branch] = random_between(parent.heading - 0.64f, parent.heading + 0.64f);
				}
			}
			for (uint8_t branch = 0; branch < 5; branch++) {
				const float direction = depth == 1 ? s_root_headings[branch] : branch_headings[branch];
				const float turbulence = random_between(-1.0f, 1.0f);
				const float scale = random_between(0.39f, 0.53f);
				const float origin_x = depth == 1 ? root_x : parent.x;
				const float origin_y = depth == 1 ? floor : parent.y;
				const float cosine = cosf(direction);
				const float upwardness = fmaxf(0.0f, -sinf(direction));
				const float projected_x = origin_x + cosine * parent.radius_x * 0.6f;
				const float alignment = fmaxf(0.0f, 1.0f - fabsf(projected_x - root_x) / (width * 0.38f));
				const float velocity = 0.32f + 0.68f * alignment * upwardness * (depth == 1 ? 0.55f : 1.0f);
				const float branch_scale = (0.32f + 0.4f * velocity) * scale * 1.7f * 1.42f * size_multiplier;
				const float radius_x = fmaxf(8.0f, floorf(parent.radius_x * branch_scale + 0.5f));
				const float radius_y = fmaxf(7.0f, floorf(parent.radius_y * branch_scale + 0.5f));
				const float lateralness = fabsf(cosine);
				const float reach = parent.radius_x * (0.55f + 0.66f * lateralness + turbulence * 0.06f) * 1.42f;
				const float lift = parent.radius_y * velocity * (0.72f + turbulence * 0.05f) * (1.15f + 0.55f * upwardness);
				const float settling = parent.radius_y * (1.0f - velocity) * (0.36f + turbulence * 0.04f);
				const float influence = vertical_shape_influence(origin_y, floor, height, parent.radius_y, lateralness);
				const float center_x = fmaxf(radius_x, floorf(origin_x + cosine * reach + 0.5f));
				const float center_y = fminf(floor - radius_y, floorf(origin_y - lift + settling + influence + 0.5f));
				lobes[child_index++] = (CloudLobe){center_x, center_y, radius_x, radius_y, direction};
			}
		}
	}
}

static void fit_lobes(int16_t width, int16_t height, uint32_t seed) {
	const float floor = floorf(height * 0.98f + 0.5f);
	const float root_x = floorf(width * 0.48f + 0.5f);
	generate_lobes(width, height, floor, root_x, s_lobes, 1.0f, CLOUD_GENERATIONS, seed);
	generate_lobes(width, height, floor, root_x, s_overlay_lobes, 0.92f, CLOUD_OVERLAY_GENERATIONS, seed);
	float left = INFINITY;
	float top = floor - 38.0f;
	float right = -INFINITY;
	for (uint16_t i = 0; i < CLOUD_LOBE_COUNT; i++) {
		const CloudLobe *lobe = &s_lobes[i];
		left = fminf(left, lobe->x - lobe->radius_x);
		top = fminf(top, lobe->y - lobe->radius_y);
		right = fmaxf(right, lobe->x + lobe->radius_x);
	}
	for (uint16_t i = 0; i < CLOUD_OVERLAY_LOBE_COUNT; i++) {
		const CloudLobe *lobe = &s_overlay_lobes[i];
		left = fminf(left, lobe->x - lobe->radius_x);
		top = fminf(top, lobe->y - lobe->radius_y);
		right = fmaxf(right, lobe->x + lobe->radius_x);
	}
	const float margin = fminf(4.0f, fminf((width - 1) / 2.0f, (height - 1) / 2.0f));
	s_fit = fminf(1.0f, fminf((width - 2.0f * margin) / (right - left), (height - 2.0f * margin) / (floor - top)));
	s_offset_x = (width - (right - left) * s_fit) / 2.0f;
	s_offset_y = height - margin - (floor - top) * s_fit;
	s_floor = (int16_t)(height - margin);
	for (uint16_t i = 0; i < CLOUD_LOBE_COUNT; i++) {
		CloudLobe *lobe = &s_lobes[i];
		lobe->x = s_offset_x + (lobe->x - left) * s_fit;
		lobe->y = s_offset_y + (lobe->y - top) * s_fit;
		lobe->radius_x *= s_fit;
		lobe->radius_y *= s_fit;
	}
	for (uint16_t i = 0; i < CLOUD_OVERLAY_LOBE_COUNT; i++) {
		CloudLobe *lobe = &s_overlay_lobes[i];
		lobe->x = s_offset_x + (lobe->x - left) * s_fit;
		lobe->y = s_offset_y + (lobe->y - top) * s_fit;
		lobe->radius_x *= s_fit;
		lobe->radius_y *= s_fit;
	}
	s_base_top = s_floor - 38.0f * s_fit;
	s_base_left = s_lobes[0].x - s_lobes[0].radius_x;
	s_base_right = s_lobes[0].x + s_lobes[0].radius_x;
	s_corner = fminf(18.0f * s_fit, fminf(s_lobes[0].radius_x, 19.0f * s_fit));
	s_left = (int16_t)fmaxf(0.0f, floorf(s_offset_x) - 2.0f);
	s_right = (int16_t)fminf(width, ceilf(s_offset_x + (right - left) * s_fit) + 2.0f);
}

bool cloud_init(int16_t width, int16_t height, uint32_t seed) {
	s_ready = false;
	if (width <= 0 || width > CLOUD_MAX_WIDTH || height <= 0) return false;
	s_width = width;
	s_height = height;
	fit_lobes(width, height, seed);
	s_random_state = seed + 1u;
	memset(s_lobe_shaded, 0, sizeof(s_lobe_shaded));
	for (uint16_t i = s_depth_starts[CLOUD_GENERATIONS - 1]; i < s_depth_ends[CLOUD_GENERATIONS]; i++) {
		s_lobe_shaded[i] = random_unit() < (2.0f / 3.0f);
	}
	for (uint16_t i = 0; i < CLOUD_OVERLAY_CONTOUR_COUNT; i++) {
		const CloudLobe *lobe = &s_overlay_lobes[CLOUD_OVERLAY_CONTOUR_START + i];
		const bool selected = random_unit() < 0.75f;
		s_overlay_contour[i] = selected && lobe->y < s_floor - 20.0f;
	}
	for (uint16_t i = 0; i < CLOUD_OVERLAY_ERASE_COUNT; i++) {
		const CloudLobe *lobe = &s_overlay_lobes[CLOUD_OVERLAY_ERASE_START + i];
		const bool eligible = lobe->y < s_floor - 20.0f;
		s_overlay_erase[i] = eligible;
		s_overlay_shaded[i] = eligible && random_unit() < (2.0f / 3.0f);
	}
	s_ready = true;
	return true;
}

static void paint_ellipse_row(uint8_t *row, int16_t y, float center_x, float center_y,
	float radius_x, float radius_y, uint8_t mode) {
	const float relative_y = ((float)y + 0.5f - center_y) / radius_y;
	if (fabsf(relative_y) > 1.0f) return;
	const float reach = radius_x * sqrtf(fmaxf(0.0f, 1.0f - relative_y * relative_y));
	const int16_t left = (int16_t)fmaxf(0.0f, ceilf(center_x - reach - 0.5f));
	const int16_t right = (int16_t)fminf(s_width - 1, floorf(center_x + reach - 0.5f));
	for (int16_t x = left; x <= right; x++) {
		if (mode == 1) row[x] = 1;
		else if (mode == 2) row[x] = ((x % 4 == 0 && y % 4 == 0) || (x % 4 == 2 && y % 4 == 2)) ? 2 : 1;
	}
}

static void paint_overlay_contour_row(uint8_t *row, int16_t y, const CloudLobe *lobe) {
	const float relative_y = ((float)y + 0.5f - lobe->y) / lobe->radius_y;
	if (fabsf(relative_y) > 1.0f) return;
	const float reach = lobe->radius_x * sqrtf(fmaxf(0.0f, 1.0f - relative_y * relative_y));
	const int16_t left = (int16_t)fmaxf(0.0f, ceilf(lobe->x - reach - 0.5f));
	const int16_t right = (int16_t)fminf(s_width - 1, floorf(lobe->x + reach - 0.5f));
	for (int16_t x = left; x <= right; x++) row[x] |= OVERLAY_CONTOUR;
}

static bool lobe_contains_pixel(const CloudLobe *lobe, int16_t x, int16_t y) {
	const float dx = (x + 0.5f - lobe->x) / lobe->radius_x;
	const float dy = (y + 0.5f - lobe->y) / lobe->radius_y;
	return dx * dx + dy * dy <= 1.0f;
}

static void paint_overlay_ellipse_row(uint8_t *overlay, const uint8_t *base, int16_t y,
	const CloudLobe *clip_lobe, float center_y, float radius_x, float radius_y,
	uint8_t color, bool dither) {
	const float relative_y = ((float)y + 0.5f - center_y) / radius_y;
	if (fabsf(relative_y) > 1.0f) return;
	const float reach = radius_x * sqrtf(fmaxf(0.0f, 1.0f - relative_y * relative_y));
	const int16_t left = (int16_t)fmaxf(0.0f, ceilf(clip_lobe->x - reach - 0.5f));
	const int16_t right = (int16_t)fminf(s_width - 1, floorf(clip_lobe->x + reach - 0.5f));
	for (int16_t x = left; x <= right; x++) {
		if (!base[x] || !lobe_contains_pixel(clip_lobe, x, y)) continue;
		if (dither && !((x % 4 == 0 && y % 4 == 0) || (x % 4 == 2 && y % 4 == 2))) continue;
		overlay[x] = (overlay[x] & OVERLAY_CONTOUR) | color;
	}
}

static void paint_base_row(uint8_t *row, int16_t y) {
	if (y + 0.5f < s_base_top || y + 0.5f > s_floor) return;
	const int16_t left = (int16_t)fmaxf(0.0f, floorf(s_base_left));
	const int16_t right = (int16_t)fminf(s_width - 1, ceilf(s_base_right));
	for (int16_t x = left; x <= right; x++) {
		const float sample_x = x + 0.5f;
		const float sample_y = y + 0.5f;
		if (sample_x < s_base_left || sample_x > s_base_right || sample_y < s_base_top || sample_y > s_floor) continue;
		const float dx = fmaxf(s_base_left + s_corner - sample_x,
			fmaxf(0.0f, sample_x - (s_base_right - s_corner)));
		const float dy = fmaxf(s_base_top + s_corner - sample_y,
			fmaxf(0.0f, sample_y - (s_floor - s_corner)));
		if (dx * dx + dy * dy <= s_corner * s_corner) row[x] = 1;
	}
}

static void render_scanline(int16_t y, uint8_t *row) {
	memset(row, 0, s_width);
	paint_base_row(row, y);
	for (uint16_t i = 0; i < CLOUD_LOBE_COUNT; i++) {
		const CloudLobe *lobe = &s_lobes[i];
		paint_ellipse_row(row, y, lobe->x, lobe->y, lobe->radius_x, lobe->radius_y, 1);
	}
	for (int8_t depth = CLOUD_GENERATIONS; depth >= 0; depth--) {
		for (uint16_t i = s_depth_starts[depth]; i < s_depth_ends[depth]; i++) {
			const CloudLobe *lobe = &s_lobes[i];
			const bool shade = s_lobe_shaded[i];
			if (shade) paint_ellipse_row(row, y, lobe->x, lobe->y, lobe->radius_x, lobe->radius_y, 2);
			const float upper_radius_x = fmaxf(1.0f, floorf((lobe->radius_x / s_fit) * 1.08f + 0.5f)) * s_fit;
			const float upper_radius_y = fmaxf(1.0f, floorf((lobe->radius_y / s_fit) * 0.64f + 0.5f)) * s_fit;
			const float upper_center_y = lobe->y - lobe->radius_y + upper_radius_y;
			paint_ellipse_row(row, y, lobe->x, upper_center_y, upper_radius_x, upper_radius_y, 1);
		}
	}
	uint8_t *overlay = s_overlay_scanlines[y % CLOUD_SCANLINE_COUNT];
	memset(overlay, 0, s_width);
	for (uint16_t i = 0; i < CLOUD_OVERLAY_CONTOUR_COUNT; i++) {
		if (s_overlay_contour[i]) {
			paint_overlay_contour_row(overlay, y, &s_overlay_lobes[CLOUD_OVERLAY_CONTOUR_START + i]);
		}
	}
	for (uint16_t i = 0; i < CLOUD_OVERLAY_ERASE_COUNT; i++) {
		const CloudLobe *lobe = &s_overlay_lobes[CLOUD_OVERLAY_ERASE_START + i];
		if (s_overlay_erase[i]) {
			paint_overlay_ellipse_row(overlay, row, y, lobe, lobe->y,
				lobe->radius_x, lobe->radius_y, OVERLAY_WHITE, false);
		}
		if (!s_overlay_shaded[i]) continue;
		paint_overlay_ellipse_row(overlay, row, y, lobe, lobe->y,
			lobe->radius_x, lobe->radius_y, OVERLAY_SHADOW, true);
		const float upper_radius_x = fmaxf(1.0f, floorf((lobe->radius_x / s_fit) * 1.08f + 0.5f)) * s_fit;
		const float upper_radius_y = fmaxf(1.0f, floorf((lobe->radius_y / s_fit) * 0.64f + 0.5f)) * s_fit;
		const float upper_center_y = lobe->y - lobe->radius_y + upper_radius_y;
		paint_overlay_ellipse_row(overlay, row, y, lobe, upper_center_y,
			upper_radius_x, upper_radius_y, OVERLAY_WHITE, false);
	}
}

static bool pixel_occupied(int16_t x, int16_t y) {
	const uint8_t base = s_scanlines[y % CLOUD_SCANLINE_COUNT][x];
	const uint8_t overlay = s_overlay_scanlines[y % CLOUD_SCANLINE_COUNT][x];
	return base || (overlay & (OVERLAY_CONTOUR | OVERLAY_WHITE | OVERLAY_SHADOW));
}

static bool contour_edge(int16_t x, int16_t y) {
	for (int8_t dy = -1; dy <= 1; dy++) {
		const int16_t sample_y = y + dy;
		if (sample_y < 0 || sample_y >= s_height) continue;
		const uint8_t *row = s_overlay_scanlines[sample_y % CLOUD_SCANLINE_COUNT];
		for (int8_t dx = -1; dx <= 1; dx++) {
			const int16_t sample_x = x + dx;
			if (sample_x < 0 || sample_x >= s_width) continue;
			if (!(row[sample_x] & OVERLAY_CONTOUR)) return true;
		}
	}
	return false;
}

static uint8_t outlined_color(int16_t x, int16_t y) {
	uint8_t color = s_scanlines[y % CLOUD_SCANLINE_COUNT][x];
	const uint8_t overlay = s_overlay_scanlines[y % CLOUD_SCANLINE_COUNT][x];
	if (overlay & OVERLAY_WHITE) color = 1;
	else if (overlay & OVERLAY_SHADOW) color = 2;
	else if (overlay & OVERLAY_CONTOUR) color = contour_edge(x, y) ? 4 : 1;
	if (!color) return 0;
	for (int8_t dy = -1; dy <= 1; dy++) {
		const int16_t sample_y = y + dy;
		if (sample_y < 0 || sample_y >= s_height) return 3;
		for (int8_t dx = -1; dx <= 1; dx++) {
			const int16_t sample_x = x + dx;
			if (sample_x < 0 || sample_x >= s_width || !pixel_occupied(sample_x, sample_y)) return 3;
		}
	}
	return color;
}

static GColor color_for_pixel(uint8_t color) {
	if (color == 1) return GColorWhite;
	if (color == 2) return GColorFromRGB(80, 80, 80);
	if (color == 4) return GColorFromRGB(50, 50, 50);
	return GColorBlack;
}

static void draw_scanline(GContext *context, int16_t y) {
	int16_t x = s_left;
	while (x < s_right) {
		const uint8_t color = outlined_color(x, y);
		if (!color) {
			x++;
			continue;
		}
		const int16_t start = x++;
		while (x < s_right && outlined_color(x, y) == color) x++;
		graphics_context_set_fill_color(context, color_for_pixel(color));
		graphics_fill_rect(context, GRect(start, y, x - start, 1), 0, GCornerNone);
	}
}

void cloud_draw(GContext *context) {
	if (!s_ready) return;
	memset(s_scanlines[3], 0, s_width);
	memset(s_scanlines[4], 0, s_width);
	memset(s_overlay_scanlines[3], 0, s_width);
	memset(s_overlay_scanlines[4], 0, s_width);
	for (int16_t y = 0; y < s_height + 2; y++) {
		uint8_t *row = s_scanlines[y % CLOUD_SCANLINE_COUNT];
		if (y < s_height) {
			render_scanline(y, row);
		} else {
			memset(row, 0, s_width);
			memset(s_overlay_scanlines[y % CLOUD_SCANLINE_COUNT], 0, s_width);
		}
		if (y >= 2) draw_scanline(context, y - 2);
	}
}
