#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "client.h"
#include "pool-buffer.h"

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener sigbus_xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	registry_init(display);

	int width = 512, height = 512, stride = width * 4;
	size_t size = stride * height;
	int fd = create_pool_file(size);
	if (fd < 0) {
		fprintf(stderr, "failed to create shm file\n");
		return EXIT_FAILURE;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
		width, height, stride, WL_SHM_FORMAT_ARGB8888);

	wl_display_roundtrip(display);

	// Shrink the file
	size = 42;
	if (ftruncate(fd, size) < 0) {
		perror("ftruncate failed");
		return EXIT_FAILURE;
	}

	struct wl_surface *surface = wl_compositor_create_surface(compositor);

	struct xdg_surface *xdg_surface =
		xdg_wm_base_get_xdg_surface(wm_base, surface);
	xdg_surface_add_listener(xdg_surface, &sigbus_xdg_surface_listener, NULL);
	xdg_surface_get_toplevel(xdg_surface);
	wl_surface_commit(surface);

	// Wait for the xdg_surface.configure event
	wl_display_roundtrip(display);

	wl_surface_attach(surface, buffer, 0, 0);
	wl_surface_damage_buffer(surface, 0, 0, width, height);
	wl_surface_commit(surface);

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	close(fd);

	return 0;
}
