#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

/* Reproduces the test pattern described in [1]. This can help reveal whether
 * the blending done by the compositor is gamma-correct.
 *
 * [1]: http://blog.johnnovak.net/2016/09/21/what-every-coder-should-know-about-gamma/#alpha-blending--compositing
 */

static struct wl_subcompositor *subcompositor = NULL;

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);
}

struct xdg_surface_listener _xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static void xdg_toplevel_handle_close(void *data,
		struct xdg_toplevel *xdg_toplevel) {
	exit(EXIT_SUCCESS);
}

static const struct xdg_toplevel_listener _xdg_toplevel_listener = {
	.configure = noop,
	.close = xdg_toplevel_handle_close,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
		subcompositor = wl_registry_bind(registry, name,
			&wl_subcompositor_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// Who cares?
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static void render_main(struct pool_buffer *buffer) {
	cairo_t *cairo = buffer->cairo;

	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cairo, 1.0, 1.0, 1.0, 1.0);
	cairo_paint(cairo);

	const float colors[][3] = {
		{ 0.094, 0.918, 0.133 },
		{ 0.918, 0.91, 0.094 },
		{ 0.094, 0.918, 0.784 },
		{ 0.933, 0.275, 0.651 },
	};

	size_t colors_len = sizeof(colors) / sizeof(colors[0]);
	int w = buffer->width / (colors_len + 2);
	int h = buffer->height;
	int padding = 10;
	for (size_t i = 0; i < colors_len; i++) {
		cairo_set_source_rgba(cairo, colors[i][0], colors[i][1], colors[i][2], 1.0);
		cairo_rectangle(cairo, (i + 1) * w + padding, 0, w - padding, h);
		cairo_fill(cairo);
	}
}

static void render_overlay(struct pool_buffer *buffer) {
	cairo_t *cairo = buffer->cairo;

	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cairo, 0.0, 0.0, 0.0, 0.0);
	cairo_paint(cairo);

	const float colors[][3] = {
		{ 1.0, 0.0, 0.0 },
		{ 0.0, 0.498, 1.0 },
		{ 0.576, 1.0, 0.0 },
	};

	size_t colors_len = sizeof(colors) / sizeof(colors[0]);
	int w = buffer->width;
	int h = buffer->height / colors_len / 4;
	for (size_t i = 0; i < colors_len; i++) {
		cairo_set_source_rgba(cairo, colors[i][0], colors[i][1], colors[i][2], 1.0);
		cairo_rectangle(cairo, 0, (3 * i + 2) * h, w, h);
		cairo_fill(cairo);

		cairo_set_source_rgba(cairo, colors[i][0], colors[i][1], colors[i][2], 0.5);
		cairo_rectangle(cairo, 0, (3 * i + 3) * h, w, h);
		cairo_fill(cairo);
	}
}

int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	registry_init(display);

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);
	wl_registry_destroy(registry);

	if (subcompositor == NULL) {
		fprintf(stderr, "compositor doesn't support wl_subcompositor\n");
		return EXIT_FAILURE;
	}

	struct wl_surface *main_surface = wl_compositor_create_surface(compositor);
	struct wl_surface *overlay_surface = wl_compositor_create_surface(compositor);

	struct xdg_surface *xdg_surface =
		xdg_wm_base_get_xdg_surface(wm_base, main_surface);
	xdg_surface_add_listener(xdg_surface, &_xdg_surface_listener, NULL);
	struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
	xdg_toplevel_add_listener(xdg_toplevel, &_xdg_toplevel_listener, NULL);
	wl_surface_commit(main_surface);

	// Wait for the xdg_surface.configure event
	wl_display_roundtrip(display);

	wl_subcompositor_get_subsurface(subcompositor, overlay_surface, main_surface);

	struct pool_buffer main_buffer = {0};
	create_buffer(shm, &main_buffer, 400, 400);

	struct pool_buffer overlay_buffer = {0};
	create_buffer(shm, &overlay_buffer, main_buffer.width, main_buffer.height);

	render_main(&main_buffer);
	render_overlay(&overlay_buffer);

	wl_surface_attach(overlay_surface, overlay_buffer.buffer, 0, 0);
	wl_surface_damage_buffer(overlay_surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(overlay_surface);

	wl_surface_attach(main_surface, main_buffer.buffer, 0, 0);
	wl_surface_damage_buffer(main_surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(main_surface);

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	return EXIT_SUCCESS;
}
