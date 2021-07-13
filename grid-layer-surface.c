#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cairo.h"
#include "client.h"
#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static struct zwlr_layer_shell_v1 *layer_shell;

static int grid_size;

struct wleird_layer_surface {
	struct wleird_surface surface;
	struct zwlr_layer_surface_v1 *layer_surface;
};

static struct wleird_layer_surface layer_surface = {0};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, wl_shm_interface.name) == 0) {
		shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, 4);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		layer_shell = wl_registry_bind(registry, name,
			&zwlr_layer_shell_v1_interface, 2);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// This space is intentionally left blank
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static void render(struct wleird_surface *surface,
		uint32_t grid_width, uint32_t grid_height) {
	struct pool_buffer *buffer = get_next_buffer(shm, surface->buffers,
		surface->width, surface->height);
	if (buffer == NULL) {
		fprintf(stderr, "failed to obtain buffer\n");
		return;
	}

	cairo_t *cairo = buffer->cairo;

	float colors[][4] = {
		{0.0f, 0.0f, 0.0f, 1.0f},
		{1.0f, 0.5f, 0.5f, 1.0f},
	};
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);

	for (uint32_t y = 0; y < grid_height; y++) {
		for (uint32_t x = 0; x < grid_width; x++) {
			float *c = colors[(x + y) % 2];
			cairo_set_source_rgba(cairo, c[0], c[1], c[2], c[3]);
			cairo_rectangle(cairo, x * grid_size, y * grid_size,
				grid_size, grid_size);
			cairo_fill(cairo);
		}
	}

	cairo_restore(cairo);

	wl_surface_attach(surface->wl_surface, buffer->buffer,
		surface->attach_x, surface->attach_y);
	wl_surface_damage_buffer(surface->wl_surface, 0, 0,
		surface->width, surface->height);
	wl_surface_commit(surface->wl_surface);
	buffer->busy = true;
	surface->attach_x = surface->attach_y = 0;
}

static void layer_surface_handle_configure(void *data,
		struct zwlr_layer_surface_v1 *wlr_layer_surface,
		uint32_t serial, uint32_t w, uint32_t h) {
	struct wleird_layer_surface *layer_surface = data;

	uint32_t grid_width = w / grid_size, grid_height = h / grid_size;
	if (grid_width < 1) {
		grid_width = 1;
	}
	if (grid_height < 1) {
		grid_height = 1;
	}

	layer_surface->surface.width = grid_width * grid_size;
	layer_surface->surface.height = grid_height * grid_size;

	zwlr_layer_surface_v1_ack_configure(wlr_layer_surface, serial);
	render(&layer_surface->surface, grid_width, grid_height);
}

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_handle_configure,
};

static int usage(char* bin) {
	fprintf(stderr, "Usage: %s [grid_size] [anchor]\n", bin);
	fprintf(stderr, "grid_size: A grid cell's width/height greater than 0\n");
	fprintf(stderr, "anchor: The layer surface's anchor ('t', 'b', 'l' and 'r'; e.g. 'tlb' for top, left and bottom)\n");
	return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		return usage(argv[0]);
	}

	grid_size = strtol(argv[1], NULL, 10);
	if (grid_size <= 0) {
		return usage(argv[0]);
	}

	uint32_t anchor = 0;
	for (char *c = argv[2]; *c; c++) {
		if (*c == 't') {
			anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
		} else if (*c == 'b') {
			anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
		} else if (*c == 'l') {
			anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
		} else if (*c == 'r') {
			anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
		} else {
			return usage(argv[0]);
		}
	}

	if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT &&
			anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
		layer_surface.surface.width = 0;
	} else {
		layer_surface.surface.width = grid_size;
	}
	if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP &&
			anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
		layer_surface.surface.height = 0;
	} else {
		layer_surface.surface.height = grid_size;
	}

	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	if (shm == NULL || compositor == NULL || layer_shell == NULL) {
		fprintf(stderr, "compositor doesn't support wl_shm, wl_compositor "
			"or wlr-layer-shell\n");
		exit(EXIT_FAILURE);
	}

	layer_surface.surface.wl_surface = wl_compositor_create_surface(compositor);

	layer_surface.layer_surface =
		zwlr_layer_shell_v1_get_layer_surface(layer_shell,
			layer_surface.surface.wl_surface, NULL,
			ZWLR_LAYER_SHELL_V1_LAYER_TOP, "");
	zwlr_layer_surface_v1_add_listener(layer_surface.layer_surface,
		&layer_surface_listener, &layer_surface);

	zwlr_layer_surface_v1_set_size(layer_surface.layer_surface,
		layer_surface.surface.width, layer_surface.surface.height);
	zwlr_layer_surface_v1_set_anchor(layer_surface.layer_surface, anchor);

	wl_surface_commit(layer_surface.surface.wl_surface);

	while (wl_display_dispatch(display) != -1) {
		// This space is intentionally left blank
	}

	return EXIT_SUCCESS;
}
