#define _XOPEN_SOURCE 500
#include <cairo/cairo.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "pool-buffer.h"

static bool set_cloexec(int fd) {
	long flags = fcntl(fd, F_GETFD);
	if (flags == -1) {
		return false;
	}

	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
		return false;
	}

	return true;
}

int create_pool_file(size_t size) {
	static const char template[] = "wleird-XXXXXX";
	const char *path = getenv("XDG_RUNTIME_DIR");
	if (path == NULL) {
		fprintf(stderr, "XDG_RUNTIME_DIR is not set\n");
		return -1;
	}

	size_t name_size = strlen(template) + 1 + strlen(path) + 1;
	char *name = malloc(name_size);
	if (name == NULL) {
		fprintf(stderr, "allocation failed\n");
		return -1;
	}
	snprintf(name, name_size, "%s/%s", path, template);

	int fd = mkstemp(name);
	if (fd < 0) {
		free(name);
		return -1;
	}

	// unlink asap; the file stays valid until all references close
	unlink(name);
	free(name);

	if (!set_cloexec(fd)) {
		close(fd);
		return -1;
	}

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer) {
	struct pool_buffer *buffer = data;
	buffer->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_handle_release,
};

static const enum wl_shm_format wl_fmt = WL_SHM_FORMAT_ARGB8888;
static const cairo_format_t cairo_fmt = CAIRO_FORMAT_ARGB32;

struct pool_buffer *create_buffer(struct wl_shm *shm,
		struct pool_buffer *buf, int32_t width, int32_t height) {
	uint32_t stride = cairo_format_stride_for_width(cairo_fmt, width);
	size_t size = stride * height;

	void *data = NULL;
	if (size > 0) {
		buf->poolfd = create_pool_file(size);
		if (buf->poolfd == -1) {
			return NULL;
		}

		data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
			buf->poolfd, 0);
		if (data == MAP_FAILED) {
			return NULL;
		}

		buf->pool = wl_shm_create_pool(shm, buf->poolfd, size);
		buf->buffer = wl_shm_pool_create_buffer(buf->pool, 0,
			width, height, stride, wl_fmt);
		wl_buffer_add_listener(buf->buffer, &buffer_listener, buf);
	}

	buf->data = data;
	buf->size = size;
	buf->width = width;
	buf->height = height;
	buf->surface = cairo_image_surface_create_for_data(data, cairo_fmt, width,
		height, stride);
	buf->cairo = cairo_create(buf->surface);
	return buf;
}

static void resize_buffer(struct pool_buffer *buf,
		int32_t width, int32_t height) {
	uint32_t stride = cairo_format_stride_for_width(cairo_fmt, width);
	int32_t size = (int32_t)stride * height;

	if (buf->cairo) {
		cairo_destroy(buf->cairo);
		buf->cairo = NULL;
	}
	if (buf->surface) {
		cairo_surface_destroy(buf->surface);
		buf->surface = NULL;
	}
	if (buf->data) {
		munmap(buf->data, buf->size);
		buf->data = NULL;
	}

	if (ftruncate(buf->poolfd, size) == -1) {
		finish_buffer(buf);
		return;
	}
	void *data = NULL;
	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
		buf->poolfd, 0);
	if (data == MAP_FAILED) {
		finish_buffer(buf);
		return;
	}

	wl_buffer_destroy(buf->buffer);
	wl_shm_pool_resize(buf->pool, size);
	buf->buffer = wl_shm_pool_create_buffer(buf->pool, 0,
		width, height, stride, wl_fmt);
	wl_buffer_add_listener(buf->buffer, &buffer_listener, buf);

	buf->size = size;
	buf->width = width;
	buf->height = height;
	buf->surface = cairo_image_surface_create_for_data(data, cairo_fmt, width,
		height, stride);
	buf->cairo = cairo_create(buf->surface);
}

void finish_buffer(struct pool_buffer *buf) {
	if (buf->pool) {
		wl_shm_pool_destroy(buf->pool);
	}
	if (buf->buffer) {
		wl_buffer_destroy(buf->buffer);
	}
	if (buf->cairo) {
		cairo_destroy(buf->cairo);
	}
	if (buf->surface) {
		cairo_surface_destroy(buf->surface);
	}
	if (buf->data) {
		munmap(buf->data, buf->size);
	}
	close(buf->poolfd);
	memset(buf, 0, sizeof(struct pool_buffer));
}

struct pool_buffer *get_next_buffer(struct wl_shm *shm,
		struct pool_buffer pool[static 2], uint32_t width, uint32_t height) {
	struct pool_buffer *buffer = NULL;
	for (size_t i = 0; i < 2; ++i) {
		if (pool[i].busy) {
			continue;
		}
		buffer = &pool[i];
	}
	if (!buffer) {
		return NULL;
	}

	int buf_stride = cairo_format_stride_for_width(cairo_fmt, (int)buffer->width);
	int stride = cairo_format_stride_for_width(cairo_fmt, (int)width);

	int buf_size = buf_stride * (int)buffer->height;
	int size = stride * (int)height;

	if (buf_size > size) {
		finish_buffer(buffer);
	} else if (buf_size > 0 && buf_size < size) {
		// resize, because wl_shm_pool_resize is underused by toolkits
		resize_buffer(buffer, width, height);
	}

	if (!buffer->buffer) {
		if (!create_buffer(shm, buffer, width, height)) {
			return NULL;
		}
	}
	return buffer;
}
