#ifndef _CLIENT_H
#define _CLIENT_H

#include <wayland-client-protocol.h>
#include "pool-buffer.h"
#include "xdg-shell-client-protocol.h"

extern struct wl_shm *shm;
extern struct wl_compositor *compositor;
extern struct xdg_wm_base *wm_base;

struct wleird_surface {
	struct wl_surface *wl_surface;

	int width, height;
	struct pool_buffer buffers[2];

	float color[4];
};

struct wleird_toplevel {
	struct wleird_surface surface;

	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
};

void registry_init(struct wl_display *display);

void surface_init(struct wleird_surface *surface);
void surface_render(struct wleird_surface *surface);

void toplevel_init(struct wleird_toplevel *toplevel);

#endif
