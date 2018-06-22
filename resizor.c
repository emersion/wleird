#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

static struct wleird_toplevel toplevel = {0};

struct {
	int x, y;
	int last_x, last_y;
	bool resizing;
} pointer_state;

static void pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t surface_x, wl_fixed_t surface_y) {
	pointer_state.x = wl_fixed_to_int(surface_x);
	pointer_state.y = wl_fixed_to_int(surface_y);
}

static void pointer_handle_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface) {
	// No-op
}

static void pointer_handle_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	pointer_state.x = wl_fixed_to_int(surface_x);
	pointer_state.y = wl_fixed_to_int(surface_y);

	if (pointer_state.resizing) {
		int dx = pointer_state.x - pointer_state.last_x;
		int dy = pointer_state.y - pointer_state.last_y;

		toplevel.surface.attach_x = dx;
		toplevel.surface.attach_y = dy;
		toplevel.surface.width -= dx;
		toplevel.surface.height -= dy;
		surface_render(&toplevel.surface);
	}

	pointer_state.last_x = pointer_state.x;
	pointer_state.last_y = pointer_state.y;
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button,
		uint32_t button_state) {
	pointer_state.resizing = (button_state == WL_POINTER_BUTTON_STATE_PRESSED);

	pointer_state.last_x = pointer_state.x;
	pointer_state.last_y = pointer_state.y;
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

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	return EXIT_SUCCESS;
}
