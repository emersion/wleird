#include "client.h"
#include "pool-buffer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum pattern {
	PATTERN_FINE,
	PATTERN_FAT,
	PATTERN_FAT_HORIZ,
	PATTERN_SNOW,
	PATTERN_SNOW2,
	PATTERN_OVERCOPY,
	PATTERN_NORMAL,
	PATTERN_CIRCLE,
	PATTERN_BLOCKNORMAL,
	PATTERN_VSTACK,
	PATTERN_RING,
	PATTERN_ENDPOINTS,
	PATTERN_WRAPAROUND,
	PATTERN_UNKNOWN
};
static struct {
	enum pattern pat;
	const char *desc;
} options[] = {
	{PATTERN_BLOCKNORMAL, "blocknormal"},
	{PATTERN_CIRCLE, "circle"},
	{PATTERN_ENDPOINTS, "endpoints"},
	{PATTERN_FAT, "fat-grid"},
	{PATTERN_FAT_HORIZ, "fat-grid-h"},
	{PATTERN_FINE, "fine-grid"},
	{PATTERN_NORMAL, "normal"},
	{PATTERN_OVERCOPY, "overcopy"},
	{PATTERN_VSTACK, "vstack"},
	{PATTERN_RING, "ring"},
	{PATTERN_SNOW, "snow"},
	{PATTERN_SNOW2, "snow2"},
	{PATTERN_WRAPAROUND, "wraparound"},
	{PATTERN_UNKNOWN, NULL},
};
static int usage() {
	fprintf(stderr, "usage: ./damage-paint [pattern]\n");
	fprintf(stderr, "patterns:");
	for (int i = 0; options[i].desc; i++) {
		fprintf(stderr, " %s", options[i].desc);
	}
	fprintf(stderr, "\n");
	return EXIT_FAILURE;
}

static struct wl_callback *callback = NULL;
static struct wl_display *display = NULL;
static struct wleird_toplevel toplevel = {0};
static int counter = 0;
static enum pattern pattern = PATTERN_UNKNOWN;

static void call_render(void *data, struct wl_callback *wl_callback,
	uint32_t callback_data);
static struct wl_callback_listener callback_listener = {call_render};

static int randint(int max) {
	/* not uniform */
	return (int)((uint32_t)rand() % (uint32_t)max);
}

// damage_render paints a buffer entirely in a new color, and then only damages
// certain parts of it. This reveals whether the compositor is currently
// copying all buffer content or only the parts that have been damaged.
//
// Note that this is not correct usage of buffer damage, and is only done here
// to reveal the behavior of the compositor. A compositor will sometimes need
// to ignore the buffer damage and read the full buffer content, such as if an
// obscured surface is unobscured.
static void damage_render(struct wleird_surface *surface) {
	struct pool_buffer *buffer = get_next_buffer(
	    shm, surface->buffers, surface->width, surface->height);
	if (buffer == NULL) {
		fprintf(stderr, "failed to obtain buffer\n");
		return;
	}

	cairo_t *cairo = buffer->cairo;

	// Colormap
	counter++;
	int stage = (counter / 23) % 3;
	float phase = (counter % 23) / 23.0;
	float *color = surface->color;
	switch (stage) {
	case 0:
		color[0] = 0.;
		color[1] = phase;
		color[2] = 1 - phase;
		break;
	case 1:
		color[0] = phase;
		color[1] = 1 - phase;
		color[2] = 0;
		break;
	case 2:
		color[0] = 1 - phase;
		color[1] = 0;
		color[2] = phase;
		break;
	}

	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cairo, color[0], color[1], color[2], color[3]);
	cairo_paint(cairo);
	cairo_restore(cairo);

	wl_surface_attach(surface->wl_surface, buffer->buffer,
		surface->attach_x, surface->attach_y);

	const int nholes = 50;
	const int nlines = 53;
	int hole_size = 3;
	int hole_xspacing = (surface->width + nholes - 1) / nholes;
	int hole_yspacing = (surface->height + nholes - 1) / nholes;

	double snow_density = 0.03;
	int nsnowflakes =
	    (int)(surface->width * surface->height * snow_density);
	// avoid overflowing destination buffer, lest
	// libwayland go wl_abort on us :-(
	nsnowflakes = nsnowflakes > 1000 ? 1000 : nsnowflakes;

	int nblocks = 33;

	switch (pattern) {
	case PATTERN_SNOW:;
		for (int i = 0; i < nsnowflakes; i++) {
			wl_surface_damage_buffer(surface->wl_surface,
				randint(surface->width),
				randint(surface->height), 1, 1);
		}
		break;
	case PATTERN_SNOW2:;
		for (int i = 0; i < nsnowflakes; i++) {
			wl_surface_damage_buffer(surface->wl_surface,
				randint(surface->width - 1),
				randint(surface->height - 1), 2, 2);
		}
		break;
	case PATTERN_FINE:;
		for (int i = 0; i < nlines; i++) {
			int xc = (i * surface->width) / nlines;
			xc = (xc + counter) % surface->width;
			wl_surface_damage_buffer(surface->wl_surface, xc, 0, 3,
				surface->height);
		}
		for (int i = 0; i < nlines; i++) {
			int yc = (i * surface->height) / nlines;
			yc = (yc + counter) & surface->height;
			wl_surface_damage_buffer(surface->wl_surface, 0, yc,
				surface->width, 2);
		}
		break;
	case PATTERN_FAT:;
		for (int i = 0; i < nholes; i++) {
			wl_surface_damage_buffer(surface->wl_surface,
				i * hole_xspacing, 0, hole_xspacing - hole_size,
				surface->height);
			wl_surface_damage_buffer(surface->wl_surface,
				0, i * hole_yspacing, surface->width,
				hole_yspacing - hole_size);
		}
		break;
	case PATTERN_FAT_HORIZ:;
		// basically the same pattern as horiz, except with intervals
		// recompiled for improved locality
		for (int i = 0; i < nholes; i++) {
			wl_surface_damage_buffer(surface->wl_surface, 0,
				i * hole_yspacing, surface->width,
				hole_yspacing - hole_size);
			for (int j = 0; j < nholes; j++) {
				wl_surface_damage_buffer(surface->wl_surface,
					j * hole_xspacing,
					i * hole_yspacing + hole_yspacing -
						hole_size,
					hole_xspacing - hole_size, hole_size);
			}
		}
		break;
	case PATTERN_OVERCOPY:;
		for (int i = 0; i < 1000; i++) {
			int xo = i % 31;
			int yo = i % 37;
			wl_surface_damage_buffer(surface->wl_surface, xo, yo,
				surface->width, surface->height);
		}
		break;
	case PATTERN_CIRCLE:;
		int cr = (surface->width > surface->height
			? surface->height : surface->width) * 0.45;
		int cx = surface->width / 2;
		int cy = surface->height / 2;

		for (int i = 0, k = 0; i < 100000 && k < 1000; i++) {
			// Uniformly randomly rejection sample from rectangles
			// contained in the circle
			int x2 = randint(surface->width - 1) + 1;
			int x1 = randint(x2);
			int y2 = randint(surface->height - 1) + 1;
			int y1 = randint(y2);

			int sx1 = (x1 - cx) * (x1 - cx);
			int sx2 = (x2 - cx) * (x2 - cx);
			int sy1 = (y1 - cy) * (y1 - cy);
			int sy2 = (y2 - cy) * (y2 - cy);
			int sx = sx1 > sx2 ? sx1 : sx2;
			int sy = sy1 > sy2 ? sy1 : sy2;
			if (sx + sy < cr * cr) {
				wl_surface_damage_buffer(surface->wl_surface,
					x1, y1, x2 - x1, y2 - y1);
				k++;
			}
		}
		break;
	case PATTERN_ENDPOINTS:;
		int cbs = 10;
		wl_surface_damage_buffer(surface->wl_surface, 0, 0, cbs, cbs);
		wl_surface_damage_buffer(surface->wl_surface,
			surface->width - cbs, surface->height - cbs, cbs, cbs);
		break;
	case PATTERN_WRAPAROUND:;
		// Because the memory layout of shm is not a rectangle, but a
		// torus section
		int cyw = 10;
		wl_surface_damage_buffer(surface->wl_surface, 0, 0, cyw,
			surface->height);
		wl_surface_damage_buffer(surface->wl_surface,
			surface->width - cyw, 0, cyw, surface->height);
		break;
	case PATTERN_RING:;
		int rr = (surface->width > surface->height
			? surface->height : surface->width) * 0.45;
		int br = rr / 100;
		br = br > 3 ? br : 3;

		// A convolution of a box and a ring
		for (int i = 0; i < 2000; i++) {
			float u = randint(0x1000000) / (float)0x1000000;
			float twopi = 6.283185307179586f;
			int x = cosf(u * twopi) * rr + surface->width / 2;
			int y = sinf(u * twopi) * rr + surface->height / 2;

			wl_surface_damage_buffer(surface->wl_surface, x - br,
				y - br, 2 * br, 2 * br);
		}
		break;
	case PATTERN_BLOCKNORMAL:;
		for (int x = 0; x < nblocks; x++) {
			for (int y = 0; y < nblocks; y++) {
				int xlow = (x * surface->width) / nblocks;
				int xhigh = ((x + 1) * surface->width) / nblocks;
				int ylow = (y * surface->height) / nblocks;
				int yhigh = ((y + 1) * surface->height) / nblocks;
				wl_surface_damage_buffer(surface->wl_surface,
					xlow, ylow, xhigh - xlow, yhigh - ylow);
			}
		}
		break;
	case PATTERN_VSTACK:;
		int vblocks = 21;
		for (int y = 0; y < vblocks; y++) {
			int ylow = (y * surface->height) / vblocks,
			    yhigh = ((y + 1) * surface->height) / vblocks;
			wl_surface_damage_buffer(surface->wl_surface,
				2 * surface->width / 7 +
					counter % (2 * surface->width / 7),
				ylow, surface->width / 7 + 5 * (y % 3 == 1),
				yhigh - ylow);
		}
		break;

	case PATTERN_NORMAL:
	default:
		wl_surface_damage_buffer(surface->wl_surface, 0, 0,
			surface->width, surface->height);
	}

	// Request frame advice callback
	if (callback)
		wl_callback_destroy(callback);

	callback = wl_surface_frame(surface->wl_surface);
	wl_callback_add_listener(callback, &callback_listener, surface);

	wl_surface_commit(surface->wl_surface);
	buffer->busy = true;
	surface->attach_x = surface->attach_y = 0;
}
static void call_render(void *data, struct wl_callback *wl_callback,
		uint32_t callback_data) {
	damage_render((struct wleird_surface *)data);
}

void damage_xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	struct wleird_toplevel *toplevel = data;

	xdg_surface_ack_configure(xdg_surface, serial);
	damage_render(&toplevel->surface);
}

int main(int argc, char **argv) {
	if (argc <= 1) {
		return usage();
	}
	for (int i = 0; options[i].desc; i++) {
		if (!strcmp(options[i].desc, argv[1])) {
			pattern = options[i].pat;
			break;
		}
	}

	if (pattern == PATTERN_UNKNOWN) {
		return usage();
	}

	display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	registry_init(display);

	xdg_surface_listener.configure = damage_xdg_surface_handle_configure;
	toplevel_init(&toplevel);

	float color[4] = {1, 1, 0, 1};
	memcpy(toplevel.surface.color, color, sizeof(float[4]));

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	wl_display_disconnect(display);
}
