#include <pebble.h>

#include "cloud.h"

static Window *s_main_window;
static Layer *s_watch_layer;
static bool s_cloud_ready;
static bool s_logged_first_draw;

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

	char time_text[16];
	clock_copy_time_string(time_text, sizeof(time_text));
	const GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
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
	(void)tick_time;
	(void)units_changed;
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
	tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
	APP_LOG(APP_LOG_LEVEL_INFO, "second tick subscription requested");
	APP_LOG(APP_LOG_LEVEL_INFO, "entering Pebble event loop");
	app_event_loop();
	APP_LOG(APP_LOG_LEVEL_INFO, "Pebble event loop exited");
	tick_timer_service_unsubscribe();
	window_destroy(s_main_window);
	return 0;
}
