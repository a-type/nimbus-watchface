#include <pebble.h>

#include "cloud.h"

static Window *s_main_window;
static Layer *s_watch_layer;

static void draw_watchface(Layer *layer, GContext *context) {
	const GRect bounds = layer_get_bounds(layer);
	graphics_context_set_fill_color(context, GColorWhite);
	graphics_fill_rect(context, bounds, 0, GCornerNone);
	cloud_draw(context);

	char time_text[16];
	clock_copy_time_string(time_text, sizeof(time_text));
	const GFont font = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
	const GSize text_size = graphics_text_layout_get_content_size(
		time_text,
		font,
		bounds,
		GTextOverflowModeWordWrap,
		GTextAlignmentCenter
	);
	const GRect text_bounds = GRect(
		bounds.origin.x,
		bounds.origin.y + (bounds.size.h - text_size.h) / 2,
		bounds.size.w,
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
	(void)tick_time;
	(void)units_changed;
	if (s_watch_layer) layer_mark_dirty(s_watch_layer);
}

static void main_window_load(Window *window) {
	s_watch_layer = window_get_root_layer(window);
	const GRect bounds = layer_get_bounds(s_watch_layer);
	cloud_init(bounds.size.w, bounds.size.h);
	layer_set_update_proc(s_watch_layer, draw_watchface);
}

static void main_window_unload(Window *window) {
	(void)window;
	s_watch_layer = NULL;
}

int main(void) {
	s_main_window = window_create();
	window_set_window_handlers(s_main_window, (WindowHandlers){
		.load = main_window_load,
		.unload = main_window_unload,
	});
	window_stack_push(s_main_window, true);
	tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
}

void deinit(void) {
	tick_timer_service_unsubscribe();
	window_destroy(s_main_window);
}