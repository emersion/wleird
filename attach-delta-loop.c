#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

#define AMPLIFICATION 20

double scale = 20;
double cnt = 0.0;
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

	toplevel.surface.attach_x = sin(cnt) * scale;
	toplevel.surface.attach_y = cos(cnt) * scale;
	surface_render(&toplevel.surface);

	cnt += 0.1;
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

	toplevel.surface.width = 100;
	toplevel.surface.height = 100;
}

int main(int argc, char *argv[]) {
	if (argc > 1) {
		scale = strtof(argv[1], NULL);
		if (scale <= 0.0) {
			fprintf(stderr, "invalid scale argument\n");
			return EXIT_FAILURE;
		}
	}

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
