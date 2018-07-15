#ifndef _CLIENT_H
#define _CLIENT_H

#include <wayland-client-protocol.h>
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif
#include "pool-buffer.h"
#include "xdg-shell-client-protocol.h"

extern struct wl_shm *shm;
extern struct wl_compositor *compositor;
extern struct xdg_wm_base *wm_base;

extern struct wl_pointer *pointer;

struct wleird_surface {
	struct wl_surface *wl_surface;

	int width, height;
	struct pool_buffer buffers[2];

	int attach_x, attach_y;
	float color[4];
};

struct wleird_toplevel {
	struct wleird_surface surface;

	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
};

extern struct xdg_surface_listener xdg_surface_listener;
extern struct xdg_toplevel_listener xdg_toplevel_listener;

void noop();

void registry_init(struct wl_display *display);

void surface_init(struct wleird_surface *surface);
void surface_render(struct wleird_surface *surface);

void toplevel_init(struct wleird_toplevel *toplevel);

void default_xdg_surface_handle_configure(void *data,
	struct xdg_surface *xdg_surface, uint32_t serial);

#endif
