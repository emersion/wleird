#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

#define FRAME_DELAY 32

struct configure {
	uint32_t serial;
	uint32_t width, height;
};

static bool acked_first_configure = false;
static struct configure current_configure = { 0 }, next_configure = { 0 };
static uint32_t countdown = 0;
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

	countdown--;
	if (countdown > 0) {
		request_frame_callback();
		return;
	}

	fprintf(stderr, "acking configure %d, width: %d, height: %d\n",
		current_configure.serial, current_configure.width, current_configure.height);
	toplevel.surface.width = current_configure.width;
	toplevel.surface.height = current_configure.height;
	xdg_surface_ack_configure(toplevel.xdg_surface, current_configure.serial);
	surface_render(&toplevel.surface);

	if (next_configure.serial == current_configure.serial) {
		return;
	}

	countdown = FRAME_DELAY;
	current_configure = next_configure;
	request_frame_callback();
}

static const struct wl_callback_listener callback_listener = {
	.done = callback_handle_done,
};

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	struct wleird_toplevel *toplevel = data;

	if (!acked_first_configure) {
		xdg_surface_ack_configure(toplevel->xdg_surface, serial);
		surface_render(&toplevel->surface);
		acked_first_configure = true;
		return;
	}

	next_configure.serial = serial;

	if (countdown == 0) {
		current_configure = next_configure;
		countdown = FRAME_DELAY;
		request_frame_callback();
	}
}

static void xdg_toplevel_handle_configure(void *data,
		struct xdg_toplevel *xdg_toplevel, int32_t w, int32_t h,
		struct wl_array *states) {
	if (w == 0 || h == 0) {
		return;
	}

	next_configure.width = w;
	next_configure.height = h;
}

int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	xdg_surface_listener.configure = xdg_surface_handle_configure;
	xdg_toplevel_listener.configure = xdg_toplevel_handle_configure;

	registry_init(display);
	toplevel_init(&toplevel);

	float color[4] = {1, 0, 0, 1};
	memcpy(toplevel.surface.color, color, sizeof(float[4]));

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	return EXIT_SUCCESS;
}
