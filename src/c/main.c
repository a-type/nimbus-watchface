#include <pebble.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "cloud.h"

#define PERSIST_KEY_ANALOG_MODE 0
#define APP_MESSAGE_BUFFER_SIZE 64

extern uint32_t MESSAGE_KEY_ANALOG_MODE;

static Window *s_main_window;
static Layer *s_watch_layer;
static bool s_cloud_ready;
static bool s_logged_first_draw;
static bool s_analog_mode;
static struct tm s_current_time;

static void draw_ray_segment(GContext *context, GPoint start, GPoint endpoint, GColor color, uint8_t thickness) {
	const int16_t cap_radius = thickness / 2;
	graphics_context_set_stroke_color(context, color);
	graphics_context_set_stroke_width(context, thickness);
	graphics_draw_line(context, start, endpoint);
	graphics_context_set_fill_color(context, color);
	graphics_fill_circle(context, endpoint, cap_radius);
}

static void draw_ray(GContext *context, GPoint center, float angle, int16_t sun_radius, int16_t end_radius, uint8_t thickness) {
	const int16_t outline_thickness = thickness + 2;
	const int16_t start_radius = sun_radius + outline_thickness / 2 + 1;
	const GPoint start = GPoint(
		center.x + (int16_t)(cosf(angle) * start_radius),
		center.y + (int16_t)(sinf(angle) * start_radius)
	);
	const GPoint endpoint = GPoint(
		center.x + (int16_t)(cosf(angle) * end_radius),
		center.y + (int16_t)(sinf(angle) * end_radius)
	);
	draw_ray_segment(context, start, endpoint, GColorBlack, outline_thickness);
	draw_ray_segment(context, start, endpoint, GColorFromRGB(255, 220, 0), thickness);
}

static void draw_analog_clock(GContext *context, GRect bounds, GPoint center, int16_t sun_radius) {
	const int16_t screen_size = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
	const int16_t numeral_radius = screen_size / 2 - 14;
	const int16_t minute_hand_radius = sun_radius + (numeral_radius - sun_radius) / 2;
	const int16_t hour_hand_radius = sun_radius + (minute_hand_radius - sun_radius) * 3 / 4;
	const GFont numeral_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
	const float pi = 3.14159265f;
	graphics_context_set_text_color(context, GColorBlack);
	for (int8_t hour = 1; hour <= 12; hour++) {
		const float angle = (float)hour * (2.0f * pi / 12.0f) - pi / 2.0f;
		const GPoint numeral_center = GPoint(
			center.x + (int16_t)(cosf(angle) * numeral_radius),
			center.y + (int16_t)(sinf(angle) * numeral_radius)
		);
		char numeral[3];
		snprintf(numeral, sizeof(numeral), "%d", hour);
		graphics_draw_text(
			context,
			numeral,
			numeral_font,
			GRect(numeral_center.x - 12, numeral_center.y - 9, 24, 18),
			GTextOverflowModeWordWrap,
			GTextAlignmentCenter,
			NULL
		);
	}

	const float minute_angle = (float)s_current_time.tm_min * (2.0f * pi / 60.0f) - pi / 2.0f;
	const float hour_angle = ((float)(s_current_time.tm_hour % 12) + (float)s_current_time.tm_min / 60.0f) *
		(2.0f * pi / 12.0f) - pi / 2.0f;
	draw_ray(context, center, hour_angle, sun_radius, hour_hand_radius, 5);
	draw_ray(context, center, minute_angle, sun_radius, minute_hand_radius, 4);
}

static void draw_watchface(Layer *layer, GContext *context) {
	const GRect bounds = layer_get_bounds(layer);
	const int16_t screen_size = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
	const int16_t sun_diameter = screen_size / 3;
	const GPoint sun_center = GPoint(
		bounds.origin.x + bounds.size.w / 2,
		bounds.origin.y + bounds.size.h / 2
	);
	graphics_context_set_fill_color(context, GColorFromRGB(173, 216, 230));
	graphics_fill_rect(context, bounds, 0, GCornerNone);
	graphics_context_set_fill_color(context, GColorFromRGB(255, 220, 0));
	graphics_fill_circle(context, sun_center, sun_diameter / 2);
	graphics_context_set_stroke_color(context, GColorBlack);
	graphics_context_set_stroke_width(context, 1);
	graphics_draw_circle(context, sun_center, sun_diameter / 2);
	if (s_cloud_ready) {
		if (!s_logged_first_draw) APP_LOG(APP_LOG_LEVEL_INFO, "cloud draw started");
		cloud_draw(context);
		if (!s_logged_first_draw) {
			APP_LOG(APP_LOG_LEVEL_INFO, "cloud draw completed");
			s_logged_first_draw = true;
		}
	} else {
		const GFont diagnostic_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
		graphics_context_set_text_color(context, GColorBlack);
		graphics_draw_text(
			context,
			"CLOUD OFFLINE",
			diagnostic_font,
			GRect(0, 4, bounds.size.w, 18),
			GTextOverflowModeWordWrap,
			GTextAlignmentCenter,
			NULL
		);
	}

	if (s_analog_mode) {
		draw_analog_clock(context, bounds, sun_center, sun_diameter / 2);
		return;
	}

	char time_text[16];
	clock_copy_time_string(time_text, sizeof(time_text));
	const GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
	const GSize text_size = graphics_text_layout_get_content_size(
		time_text,
		font,
		GRect(
			sun_center.x - sun_diameter / 2,
			sun_center.y - sun_diameter / 2,
			sun_diameter,
			sun_diameter
		),
		GTextOverflowModeWordWrap,
		GTextAlignmentCenter
	);
	const GRect text_bounds = GRect(
		sun_center.x - text_size.w / 2,
		sun_center.y - text_size.h / 2,
		text_size.w,
		text_size.h
	);
	graphics_context_set_text_color(context, GColorBlack);
	graphics_draw_text(
		context,
		time_text,
		font,
		text_bounds,
		GTextOverflowModeWordWrap,
		GTextAlignmentCenter,
		NULL
	);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
	(void)units_changed;
	if (tick_time) s_current_time = *tick_time;
	if (s_watch_layer) layer_mark_dirty(s_watch_layer);
}

static void inbox_received_handler(DictionaryIterator *iterator, void *context) {
	(void)context;
	const Tuple *mode_tuple = dict_find(iterator, MESSAGE_KEY_ANALOG_MODE);
	if (!mode_tuple) return;
	s_analog_mode = mode_tuple->value->int32 != 0;
	persist_write_bool(PERSIST_KEY_ANALOG_MODE, s_analog_mode);
	if (s_watch_layer) layer_mark_dirty(s_watch_layer);
}

static void main_window_load(Window *window) {
	s_watch_layer = window_get_root_layer(window);
	const GRect bounds = layer_get_bounds(s_watch_layer);
	s_cloud_ready = cloud_init(bounds.size.w, bounds.size.h);
	APP_LOG(
		s_cloud_ready ? APP_LOG_LEVEL_INFO : APP_LOG_LEVEL_ERROR,
		"cloud init %s (%d x %d)",
		s_cloud_ready ? "ok" : "failed",
		bounds.size.w,
		bounds.size.h
	);
	layer_set_update_proc(s_watch_layer, draw_watchface);
	layer_mark_dirty(s_watch_layer);
}

static void main_window_unload(Window *window) {
	(void)window;
	s_watch_layer = NULL;
}

int main(void) {
	APP_LOG(APP_LOG_LEVEL_INFO, "native watchface main entered");
	s_analog_mode = persist_exists(PERSIST_KEY_ANALOG_MODE) &&
		persist_read_bool(PERSIST_KEY_ANALOG_MODE);
	time_t now = time(NULL);
	struct tm *current_time = localtime(&now);
	if (current_time) s_current_time = *current_time;
	app_message_register_inbox_received(inbox_received_handler);
	const AppMessageResult message_result = app_message_open(
		APP_MESSAGE_BUFFER_SIZE,
		APP_MESSAGE_BUFFER_SIZE
	);
	if (message_result != APP_MSG_OK) {
		APP_LOG(APP_LOG_LEVEL_ERROR, "app message open failed: %d", message_result);
	}
	s_main_window = window_create();
	if (!s_main_window) {
		APP_LOG(APP_LOG_LEVEL_ERROR, "window_create failed");
		return 1;
	}
	window_set_window_handlers(s_main_window, (WindowHandlers){
		.load = main_window_load,
		.unload = main_window_unload,
	});
	window_stack_push(s_main_window, true);
	tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
	APP_LOG(APP_LOG_LEVEL_INFO, "minute tick subscription requested");
	APP_LOG(APP_LOG_LEVEL_INFO, "entering Pebble event loop");
	app_event_loop();
	APP_LOG(APP_LOG_LEVEL_INFO, "Pebble event loop exited");
	tick_timer_service_unsubscribe();
	window_destroy(s_main_window);
	return 0;
}
