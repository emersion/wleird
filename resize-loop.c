#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

#define MIN 2
#define MAX 512
#define SPEED 10
static int size = MIN;

static struct wleird_toplevel toplevel = {0};
static const struct wl_callback_listener callback_listener;


static void request_frame_callback(void) {
	struct wl_callback *callback = wl_surface_frame(toplevel.surface.wl_surface);
	wl_callback_add_listener(callback, &callback_listener, NULL);
	wl_surface_commit(toplevel.surface.wl_surface);
}

static void callback_handle_done(void *data, struct wl_callback *callback,
		uint32_t time_ms) {
	if (callback != NULL) {
		wl_callback_destroy(callback);
	}

	toplevel.surface.width = abs(size);
	toplevel.surface.height = abs(size);
	surface_render(&toplevel.surface);

	size += SPEED;
	if (size < 0 && size > -MIN) {
		size = MIN;
	} else if (size > 0 && size > MAX) {
		size = -MAX;
	}
	request_frame_callback();
}

static const struct wl_callback_listener callback_listener = {
	.done = callback_handle_done,
};

static void xdg_toplevel_handle_configure(void *data,
		struct xdg_toplevel *xdg_toplevel, int32_t w, int32_t h,
		struct wl_array *states) {
	if (w == 0 || h == 0) {
		return;
	}

	toplevel.surface.width = abs(size);
	toplevel.surface.height = abs(size);
}

int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	xdg_toplevel_listener.configure = xdg_toplevel_handle_configure;

	registry_init(display);
	toplevel_init(&toplevel);

	float color[4] = {1, 0, 0, 1};
	memcpy(toplevel.surface.color, color, sizeof(float[4]));

	request_frame_callback();

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	return EXIT_SUCCESS;
}
