#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "client.h"

static double factor = 0.0;
static struct wleird_toplevel toplevel = {0};
static void xdg_toplevel_handle_configure(void *data,
		struct xdg_toplevel *xdg_toplevel, int32_t w, int32_t h,
		struct wl_array *states) {
	struct wleird_toplevel *toplevel = data;
	if (w == 0 || h == 0) {
		return;
	}

	toplevel->surface.width = fmax((double)w * factor, 1);
	toplevel->surface.height = fmax((double)h * factor, 1);
}

static int usage(char* bin) {
	fprintf(stderr, "Usage: %s [size_factor]\n", bin);
	fprintf(stderr, "size_factor: A floating point factor greater than 0.0 to apply to the requested size\n");
	return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		return usage(argv[0]);
	}

	factor = strtof(argv[1], NULL);
	if (factor <= 0.0) {
		return usage(argv[0]);
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

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	return EXIT_SUCCESS;
}
