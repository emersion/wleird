#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

#include <wayland-server-core.h>

static struct wleird_toplevel toplevel = {0};
struct wl_event_loop *loop = NULL;

int unmap(void *data) {
	wl_surface_attach(toplevel.surface.wl_surface, NULL, 0, 0);
	wl_surface_commit(toplevel.surface.wl_surface);
	return 0;
}

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	default_xdg_surface_handle_configure(data, xdg_surface, serial);

	struct wl_event_source *source = wl_event_loop_add_timer(loop, unmap, NULL);
	wl_event_source_timer_update(source, 1000);
}

static int handle_display_readable(int fd, uint32_t mask, void *data) {
	struct wl_display *display = data;
	if (mask & WL_EVENT_HANGUP) {
		exit(EXIT_FAILURE);
	}
	if (wl_display_dispatch(display) < 0) {
		fprintf(stderr, "failed to read Wayland events\n");
		exit(EXIT_FAILURE);
	}
	return 0;
}

int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	xdg_surface_listener.configure = xdg_surface_handle_configure;

	registry_init(display);

	toplevel_init(&toplevel);

	float color[4] = {1, 1, 1, 1};
	memcpy(toplevel.surface.color, color, sizeof(float[4]));

	loop = wl_event_loop_create();
	wl_event_loop_add_fd(loop, wl_display_get_fd(display), WL_EVENT_READABLE,
		handle_display_readable, display);

	wl_display_flush(display);
	while (wl_event_loop_dispatch(loop, -1) >= 0) {
		wl_display_flush(display);
	}

	return EXIT_SUCCESS;
}
