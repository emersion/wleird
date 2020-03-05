#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

static struct wleird_toplevel toplevel = {0};
static uint32_t last_time_ms = 0;

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

	fprintf(stderr, "received frame (time=%"PRIu32"ms, delta=%"PRIu32"ms)\n",
		time_ms, time_ms - last_time_ms);
	last_time_ms = time_ms;

	request_frame_callback();
}

static const struct wl_callback_listener callback_listener = {
	.done = callback_handle_done,
};

int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

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
