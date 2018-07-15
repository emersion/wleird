#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

struct wl_shm *shm = NULL;
struct wl_compositor *compositor = NULL;
struct xdg_wm_base *wm_base = NULL;

struct wl_pointer *pointer = NULL;

void noop() {
	// This space is intentionally left blank
}


void surface_render(struct wleird_surface *surface) {
	struct pool_buffer *buffer = get_next_buffer(shm, surface->buffers,
		surface->width, surface->height);
	if (buffer == NULL) {
		fprintf(stderr, "failed to obtain buffer\n");
		return;
	}

	cairo_t *cairo = buffer->cairo;

	float *color = surface->color;
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cairo, color[0], color[1], color[2], color[3]);
	cairo_paint(cairo);
	cairo_restore(cairo);

	wl_surface_attach(surface->wl_surface, buffer->buffer,
		surface->attach_x, surface->attach_y);
	wl_surface_damage_buffer(surface->wl_surface, 0, 0,
		surface->width, surface->height);
	wl_surface_commit(surface->wl_surface);
	buffer->busy = true;
	surface->attach_x = surface->attach_y = 0;
}

void surface_init(struct wleird_surface *surface) {
	surface->wl_surface = wl_compositor_create_surface(compositor);
	surface->width = 300;
	surface->height = 400;
}


void default_xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	struct wleird_toplevel *toplevel = data;

	xdg_surface_ack_configure(xdg_surface, serial);
	surface_render(&toplevel->surface);
}

struct xdg_surface_listener xdg_surface_listener = {
	.configure = default_xdg_surface_handle_configure,
};

static void xdg_toplevel_handle_configure(void *data,
		struct xdg_toplevel *xdg_toplevel, int32_t w, int32_t h,
		struct wl_array *states) {
	struct wleird_toplevel *toplevel = data;
	if (w == 0 || h == 0) {
		return;
	}
	toplevel->surface.width = w;
	toplevel->surface.height = h;
}

static void xdg_toplevel_handle_close(void *data,
		struct xdg_toplevel *xdg_toplevel) {
	exit(EXIT_SUCCESS);
}

struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
	.close = xdg_toplevel_handle_close,
};

void toplevel_init(struct wleird_toplevel *toplevel) {
	surface_init(&toplevel->surface);

	toplevel->xdg_surface =
		xdg_wm_base_get_xdg_surface(wm_base, toplevel->surface.wl_surface);
	xdg_surface_add_listener(toplevel->xdg_surface, &xdg_surface_listener,
		toplevel);
	toplevel->xdg_toplevel = xdg_surface_get_toplevel(toplevel->xdg_surface);
	xdg_toplevel_add_listener(toplevel->xdg_toplevel, &xdg_toplevel_listener,
		toplevel);

	wl_surface_commit(toplevel->surface.wl_surface);
}


static void seat_handle_capabilities(void *data, struct wl_seat *seat,
		uint32_t capabilities) {
	if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
		// This client isn't multiseat aware for the sake of simplicity
		if (pointer != NULL) {
			return;
		}
		pointer = wl_seat_get_pointer(seat);
	}
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
};


static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, wl_shm_interface.name) == 0) {
		shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, 4);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		struct wl_seat *seat =
			wl_registry_bind(registry, name, &wl_seat_interface, 1);
		wl_seat_add_listener(seat, &seat_listener, NULL);
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

void registry_init(struct wl_display *display) {
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	if (shm == NULL || compositor == NULL || wm_base == NULL) {
		fprintf(stderr, "compositor doesn't support wl_shm, wl_compositor "
			"or xdg-shell\n");
		exit(EXIT_FAILURE);
	}
}
