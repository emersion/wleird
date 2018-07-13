#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

static const int cursor_size = 50;

static struct wleird_toplevel toplevel = {0};
static struct wleird_surface cursor_surface = {0};
static bool inversed = false;

static void pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t surface_x, wl_fixed_t surface_y) {
	int hotspot = inversed ? cursor_size : 0;
	wl_pointer_set_cursor(wl_pointer, serial, cursor_surface.wl_surface,
		hotspot, hotspot);
}

static void pointer_handle_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface) {
	// No-op
}

static void pointer_handle_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	// No-op
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button,
		uint32_t button_state) {
	if (button_state != WL_POINTER_BUTTON_STATE_RELEASED) {
		return;
	}

	int sign = inversed ? 1 : -1;
	cursor_surface.attach_x = cursor_surface.attach_y = sign * cursor_size;
	inversed = !inversed;
	surface_render(&cursor_surface);
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_handle_enter,
	.leave = pointer_handle_leave,
	.motion = pointer_handle_motion,
	.button = pointer_handle_button,
	.axis = noop,
};


int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	registry_init(display);

	if (pointer != NULL) {
		wl_pointer_add_listener(pointer, &pointer_listener, NULL);
	}

	toplevel_init(&toplevel);
	float color[4] = {1, 1, 1, 1};
	memcpy(toplevel.surface.color, color, sizeof(float[4]));

	surface_init(&cursor_surface);
	cursor_surface.width = cursor_surface.height = cursor_size;
	float cursor_color[4] = {1, 0, 0, 0.5};
	memcpy(cursor_surface.color, cursor_color, sizeof(float[4]));
	surface_render(&cursor_surface);

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	return EXIT_SUCCESS;
}
