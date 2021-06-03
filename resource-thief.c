#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <gbm.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wayland-client-protocol.h>

#include "pool-buffer.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"

#define POOL_SIZE 1024
#define POINTS_PER_REGION 300
#define DMABUF_WIDTH 64
#define DMABUF_HEIGHT 64
#define DMABUF_FORMAT GBM_FORMAT_XRGB8888

static void handle_sigint(int sig) {
	(void)sig;
}

/* Different types of resources to trick the compositor into exhausting. */
enum mode {
	CONSUME_NOOP,
	CONSUME_SHMPOOL,
	CONSUME_REGION,
	CONSUME_DMABUF
};

struct connection {
	struct wl_list link;
	uint32_t resource_count;
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_shm *shm;
	struct wl_compositor *compositor;
	struct zwp_linux_dmabuf_v1 *linux_dmabuf;
	struct wl_shm_pool **pools;
	struct wl_region **regions;
	struct wl_buffer **buffers;
};

void destroy_connection(struct connection *conn) {
	if (!conn) {
		return;
	}
	if (conn->pools) {
		for (uint32_t i = 0; i < conn->resource_count; i++) {
			if (conn->pools[i]) {
				wl_shm_pool_destroy(conn->pools[i]);
			}
		}
	}
	if (conn->buffers) {
		for (uint32_t i = 0; i < conn->resource_count; i++) {
			if (conn->buffers[i]) {
				wl_buffer_destroy(conn->buffers[i]);
			}
		}
	}
	if (conn->regions) {
		for (uint32_t i = 0; i < (conn->resource_count / POINTS_PER_REGION + 1); i++) {
			if (conn->regions[i]) {
				wl_region_destroy(conn->regions[i]);
			}
		}
	}
	if (conn->shm) {
		wl_shm_destroy(conn->shm);
	}
	if (conn->compositor) {
		wl_compositor_destroy(conn->compositor);
	}
	if (conn->linux_dmabuf) {
		zwp_linux_dmabuf_v1_destroy(conn->linux_dmabuf);
	}
	if (conn->registry) {
		wl_registry_destroy(conn->registry);
	}
	if (conn->display) {
		wl_display_disconnect(conn->display);
	}
	free(conn->pools);
	free(conn);
}


static void reg_global(void *data, struct wl_registry *wl_registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct connection *conn = data;
	if (strcmp(interface, wl_shm_interface.name) == 0) {
		conn->shm = wl_registry_bind(wl_registry, name,
			&wl_shm_interface, 1);
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		conn->compositor = wl_registry_bind(wl_registry, name,
			&wl_compositor_interface, 1);
	} else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0
			&& version >= 2) {
		conn->linux_dmabuf = wl_registry_bind(wl_registry, name,
			&zwp_linux_dmabuf_v1_interface, 2);
	}
}
static void reg_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
	(void)data;
	(void)wl_registry;
	(void)name;
}

static const struct wl_registry_listener reg_listener = {
	.global = &reg_global,
	.global_remove = &reg_global_remove,
};

struct connection *consume(uint32_t resource_count, enum mode mode, int fd) {
	const char *resource = "unknown things";
	if (mode == CONSUME_DMABUF) {
		resource = "linux dmabuf fds";
	} else if (mode == CONSUME_REGION) {
		resource = "wl_region points";
	} else if (mode == CONSUME_SHMPOOL) {
		resource = "shm pool mappings";
	}

	fprintf(stderr, "Trying to create %"PRIu32" %s\n", resource_count, resource);

	struct connection *conn = calloc(1, sizeof(struct connection));
	if (!conn) {
			return NULL;
	}
	conn->resource_count = resource_count;
	conn->display = wl_display_connect(NULL);
	if (!conn->display) {
		goto fail;
	}
	conn->registry = wl_display_get_registry(conn->display);
	wl_registry_add_listener(conn->registry, &reg_listener, conn);
	wl_display_roundtrip(conn->display);
	// wait for events sent by newly bound globals
	wl_display_roundtrip(conn->display);

	switch (mode) {
	case CONSUME_SHMPOOL:
		if (!conn->shm) {
			fprintf(stderr, "wl_shm global not available\n");
			goto fail;
		}
		conn->pools = calloc(resource_count, sizeof(struct wl_shm_pool *));
		if (!conn->pools) {
			goto fail;
		}
		for (uint32_t i = 0; i < conn->resource_count; i++) {
			conn->pools[i] = wl_shm_create_pool(conn->shm, fd, POOL_SIZE);
			/* the roundtrip checks if this connection broke */
			if (wl_display_roundtrip(conn->display) == -1) {
				goto fail;
			}
		}
		break;
	case CONSUME_DMABUF:
		if (!conn->linux_dmabuf) {
			fprintf(stderr, "zwp_linux_dmabuf_v1 global not available\n");
			goto fail;
		}
		conn->buffers = calloc(resource_count, sizeof(struct wl_buffer *));
		if (!conn->buffers) {
			goto fail;
		}
		for (uint32_t i = 0; i < conn->resource_count; i++) {
			struct zwp_linux_buffer_params_v1 *params =
				zwp_linux_dmabuf_v1_create_params(
					conn->linux_dmabuf);
			zwp_linux_buffer_params_v1_add(params, fd, 0, 0,
				DMABUF_WIDTH * 4, 0, 0);
			conn->buffers[i] = zwp_linux_buffer_params_v1_create_immed(
				params, DMABUF_WIDTH, DMABUF_HEIGHT, DMABUF_FORMAT, 0);
			zwp_linux_buffer_params_v1_destroy(params);

			/* the roundtrip checks if this connection broke */
			if (wl_display_roundtrip(conn->display) == -1) {
				goto fail;
			}
		}
		break;
	case CONSUME_REGION:
		if (!conn->compositor) {
			fprintf(stderr, "wl_compositor global not available\n");
			goto fail;
		}
		conn->regions = calloc(resource_count / POINTS_PER_REGION + 1,
			sizeof(struct wl_region *));
		if (!conn->regions) {
			goto fail;
		}

		/* Because most compositors incur quadratic runtime when
		 * creating regions with many points, divide the points
		 * registered between regions */
		struct wl_region *current_region = NULL;
		uint32_t region_no = 0;
		uint32_t isqrt = 0;
		for (uint32_t i = 0; i < conn->resource_count; i++) {
			if (i >= region_no * POINTS_PER_REGION) {
				current_region = wl_compositor_create_region(conn->compositor);
				conn->regions[region_no++] = current_region;
			}
			/* add pixels in 50% density, pseudorandom pattern */
			uint64_t pos_no = 2*i + ((uint64_t)rand()%2);
			if (pos_no >= (isqrt+1)*(isqrt+1)) {
				isqrt++;
			}
			int32_t x = (int32_t)isqrt, y = (int32_t)(pos_no - isqrt*isqrt);
			wl_region_add(current_region, x, y, 1, 1);

			if (i % 128 == 0) {
				if (wl_display_roundtrip(conn->display) == -1) {
					goto fail;
				}
			}
		}

		if (wl_display_roundtrip(conn->display) == -1) {
			goto fail;
		}
		break;

	default:
		fprintf(stderr, "not implemented\n");
		break;

	}

	return conn;
fail:
	destroy_connection(conn);
	return NULL;
}

static const char program_desc[] =
	"This program creates as many objects associated with a given finite resource\n"
	"as it can, until the compositor cannot accept any more. Since most code is not\n"
	"tested in resource-constrained scenarios, this can make the compositor crash.\n"
	"  shmpool: memory map areas;  dmabuf: file descriptors;  region: memory\n"
	"\n"
	"usage: resource-thief (shmpool|dmabuf|region)\n";

int main(int argc, char *argv[]) {
	enum mode mode = CONSUME_NOOP;
	if (argc == 2) {
		if (!strcmp(argv[1], "shmpool")) {
			mode = CONSUME_SHMPOOL;
		} else if (!strcmp(argv[1], "dmabuf")) {
			mode = CONSUME_DMABUF;
		} else if (!strcmp(argv[1], "region")) {
			mode = CONSUME_REGION;
		}
	}

	if (mode == CONSUME_NOOP) {
		fprintf(stderr, program_desc);
		return EXIT_FAILURE;
	}

	struct wl_list connections;
	wl_list_init(&connections);

	int fd = -1;
	if (mode == CONSUME_SHMPOOL) {
		fd = create_pool_file(POOL_SIZE);
		if (fd == -1) {
			fprintf(stderr, "failed to create pool file.\n");
			return EXIT_FAILURE;
		}
	} else if (mode == CONSUME_DMABUF) {
		/* todo: select device based on linux-dmabuf primary_device,
		 * when it becomes widely available */
		int drm_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
		if (drm_fd == -1) {
			fprintf(stderr, "failed to open drm device /dev/dri/renderD128.\n");
			return EXIT_FAILURE;
		}
		struct gbm_device *gbm = gbm_create_device(drm_fd);
		if (!gbm) {
			fprintf(stderr, "failed to create gbm device.\n");
			return EXIT_FAILURE;
		}
		struct gbm_bo *bo = gbm_bo_create(gbm, DMABUF_WIDTH,
			DMABUF_HEIGHT, DMABUF_FORMAT,
			GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
		if (!bo) {
			fprintf(stderr, "failed to create dmabuf.\n");
			return EXIT_FAILURE;
		}
		fd = gbm_bo_get_fd(bo);
		if (!bo) {
			fprintf(stderr, "failed to export dmabuf to fd.\n");
			return EXIT_FAILURE;
		}
		gbm_bo_destroy(bo);
		gbm_device_destroy(gbm);
	}

	/* Binary search to create as many objects as possible */
	uint64_t total_pools = 0;
	uint32_t block_size = 1;
	while (block_size < (1uLL << 31)) {
		struct connection *c = consume(block_size, mode, fd);
		if (!c) {
			break;
		}
		total_pools += block_size;
		wl_list_insert(&connections, &c->link);

		block_size = 2 * block_size;
	}
	while (block_size) {
		struct connection *c = consume(block_size, mode, fd);
		if (c) {
			total_pools += block_size;
			wl_list_insert(&connections, &c->link);
		}

		block_size = block_size / 2;
	}
	for (int i = 0; i < 10; i++) {
		struct connection *c = consume(1, mode, fd);
		if (c) {
			total_pools++;
			wl_list_insert(&connections, &c->link);
		}
	}
	fprintf(stderr, "Total pools: %"PRIu64"\n", total_pools);

	/* Wait until Ctrl+C, then clean up and exit */
	fprintf(stderr, "Waiting for SIGINT...\n");
	struct sigaction sigact;
	sigact.sa_handler = handle_sigint;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	if (sigaction(SIGINT, &sigact, NULL) == -1) {
		fprintf(stderr, "Failed to set SIGINT handler\n");
	} else {
		pause();

		sigact.sa_handler = SIG_DFL;
		if (sigaction(SIGINT, &sigact, NULL) == -1) {
			fprintf(stderr, "Failed to reset SIGINT handler\n");
		}
	}

	struct connection *cur, *tmp;
	wl_list_for_each_safe(cur, tmp, &connections, link) {
		wl_list_remove(&cur->link);
		destroy_connection(cur);
	}

	if (mode == CONSUME_SHMPOOL || mode == CONSUME_DMABUF) {
		close(fd);
	}
	fprintf(stderr, "\nDone.\n");
	return EXIT_SUCCESS;
}
