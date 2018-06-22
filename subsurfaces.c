#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

static struct wl_subcompositor *subcompositor = NULL;


struct wleird_subsurface {
	struct wleird_surface surface;

	struct wl_subsurface *wl_subsurface;
};

static void subsurface_init(struct wleird_subsurface *subsurface,
		struct wl_surface *parent) {
	surface_init(&subsurface->surface);

	subsurface->wl_subsurface = wl_subcompositor_get_subsurface(subcompositor,
		subsurface->surface.wl_surface, parent);
}


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


int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	registry_init(display);

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	if (subcompositor == NULL) {
		fprintf(stderr, "compositor doesn't support wl_subcompositor\n");
		return EXIT_FAILURE;
	}

	struct wleird_toplevel toplevel = {0};
	toplevel_init(&toplevel);

	float color[4] = {1, 1, 1, 1};
	memcpy(toplevel.surface.color, color, sizeof(float[4]));

	struct wleird_subsurface subsurfaces[3] = {0};
	size_t subsurfaces_len = sizeof(subsurfaces) / sizeof(subsurfaces[0]);
	for (size_t i = 0; i < subsurfaces_len; ++i) {
		subsurface_init(&subsurfaces[i], toplevel.surface.wl_surface);

		float color[4] = {i == 0, i == 1, i == 2, 1};
		memcpy(subsurfaces[i].surface.color, color, sizeof(float[4]));

		surface_render(&subsurfaces[i].surface);
	}

	wl_subsurface_set_position(subsurfaces[0].wl_subsurface, 10, 10);
	wl_subsurface_set_position(subsurfaces[1].wl_subsurface, 100, 50);
	wl_subsurface_set_position(subsurfaces[2].wl_subsurface, 50, 100);

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	return EXIT_SUCCESS;
}
