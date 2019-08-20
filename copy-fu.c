#define _POSIX_C_SOURCE 200809L
#include "client.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

enum copyfu_mode {
	DEFAULT, CAT_RANDOM, BAD_SERIAL, ZERO_SINK, STEAL_SERIAL, RECV_FILE,
	STEAL_SYNC, RECV_FLOOD, RECV_SOCKETPAIR, RECV_EPIPE
};

struct cli_option {
	enum copyfu_mode mode;
	const char *name;
	const char *description;
};

enum send_type {
	SEND_NOT,
	SEND_TEXT,
	SEND_OCTET_STREAM
};

enum recv_type {
	RECV_NOT,
	RECV_TEXT,
	RECV_OCTET_STREAM
};

enum sync_steal_state {
	SYNC_STEAL_READY,
	SYNC_STEAL_WAITING,
	SYNC_STEAL_DONE
};

struct fd_extra {
	enum send_type send_type;
	enum recv_type recv_type;
	int refcount;
	int write_counter, read_counter;
};

struct fd_queue {
	int nfds;
	/* The first fd to be polled is always the main event loop */
	struct pollfd *pfds;
	struct fd_extra *meta;
};

struct mimetype_offer {
	struct wl_list link;
	char *val;
};

static const struct cli_option cli_options[] = {
	{DEFAULT, "default", "Receive on kbd focus enter, copy on click"},
	{CAT_RANDOM, "cat-rand", "On click, offer /dev/urandom as text/plain"},
	{BAD_SERIAL, "bad-serial", "On click, offer with huge serial number"},
	{STEAL_SERIAL, "steal-serial", "Periodically try a range of copy serials"},
	{ZERO_SINK, "zero-sink", "Receive to /dev/null"},
	{RECV_FILE, "recv-file", "Receive to paste_result.txt"},
	{STEAL_SYNC, "steal-sync", "Periodically try the wl_display::sync serial"},
	{RECV_FLOOD, "recv-flood", "Receive very many times to /dev/null"},
	{RECV_SOCKETPAIR, "recv-sockpair", "Receive to a socketpair"},
	{RECV_EPIPE, "recv-epipe", "Receive to a readerless pipe"}
};

static enum copyfu_mode mode = DEFAULT;
static struct wleird_toplevel toplevel = {0};
static struct wl_data_device *data_device = NULL;
static struct wl_data_source *data_source = NULL;
static struct wl_data_offer *data_offer = NULL;
static struct wl_display *display = NULL;
static int devnull = -1;
static uint32_t last_serial = 0;

static struct wl_list offer_list = {&offer_list, &offer_list};
static struct fd_queue fd_set = {0};
static const struct wl_data_source_listener data_source_listener;
static const struct wl_data_offer_listener data_offer_listener;
static enum sync_steal_state steal_state = SYNC_STEAL_READY;

static void add_set_fd(int fd, enum send_type stype, enum recv_type rtype) {
	int idx =fd_set.nfds++;
	fd_set.pfds = realloc(fd_set.pfds, fd_set.nfds * sizeof(struct pollfd));
	fd_set.meta = realloc(fd_set.meta, fd_set.nfds * sizeof(struct fd_extra));

	fd_set.pfds[idx].fd = fd;
	fd_set.pfds[idx].events = (stype == SEND_NOT ? 0 : POLLOUT) | (rtype == RECV_NOT ? 0 : POLLIN);
	fd_set.meta[idx].send_type = stype;
	fd_set.meta[idx].recv_type = rtype;
	fd_set.meta[idx].refcount = (stype != SEND_NOT) + (rtype != RECV_NOT);
	fd_set.meta[idx].write_counter = 0;
	fd_set.meta[idx].read_counter = 0;
}

static void remove_done_fds(void) {
	int iw = 0;
	for (int ir=0;ir<fd_set.nfds;ir++) {
		if (fd_set.meta[ir].refcount > 0) {
			fd_set.meta[iw] = fd_set.meta[ir];
			fd_set.pfds[iw] = fd_set.pfds[ir];
			iw++;
		} else {
			/* no more references, can close the fd */
			close(fd_set.pfds[ir].fd);
		}
	}
	if (iw < fd_set.nfds) {
		fd_set.nfds = iw;
	}
	fd_set.pfds = realloc(fd_set.pfds, fd_set.nfds * sizeof(struct pollfd));
	fd_set.meta = realloc(fd_set.meta, fd_set.nfds * sizeof(struct fd_extra));
}

static void clear_offer_stack(void) {
	struct mimetype_offer *offer = NULL, *tmp_offer = NULL;
	wl_list_for_each_safe(offer, tmp_offer, &offer_list, link) {
		wl_list_remove(&offer->link);
		free(offer);
	}
}

static void data_source_target(void *data,
		struct wl_data_source *wl_data_source, const char *mime_type) {
}

static void data_source_send(void *data, struct wl_data_source *wl_data_source,
		const char *mime_type, int32_t fd) {
	enum send_type stype = SEND_TEXT;
	if (!strcmp(mime_type, "application/octet-stream")) {
		stype = SEND_OCTET_STREAM;
	}
	add_set_fd(fd, stype, RECV_NOT);
}
static void data_source_cancelled(void *data,
		struct wl_data_source *wl_data_source) {
	wl_data_source_destroy(data_source);
	data_source = NULL;
}

static void data_source_dnd_drop_performed(void *data,
		struct wl_data_source *wl_data_source) {
}
static void data_source_dnd_finished(void *data,
		struct wl_data_source *wl_data_source) {
}

static void data_source_action(void *data, struct wl_data_source *wl_data_source,
		uint32_t dnd_action) {
}


static void data_offer_offer(void *data, struct wl_data_offer *wl_data_offer,
		const char *mime_type) {
	if (wl_data_offer != data_offer) {
		fprintf(stderr, "Warning: data offer mismatch\n");
	}

	struct mimetype_offer *e = calloc(1, sizeof(struct mimetype_offer));
	e->val = strdup(mime_type);
	wl_list_insert(&offer_list, &e->link);

	printf("Received offer #%d for %s\n", wl_list_length(&offer_list), mime_type);
}

static void data_offer_source_actions(void *data,
		struct wl_data_offer *wl_data_offer, uint32_t source_actions) {
}

static void data_offer_action(void *data, struct wl_data_offer *wl_data_offer,
		uint32_t dnd_action) {
}


static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button,
		uint32_t button_state) {
	/* Change color when button depressed */
	if (button_state == WL_POINTER_BUTTON_STATE_RELEASED) {
		float color[4] = {1.f, 0.3f, 1.f, 1.f};
		memcpy(toplevel.surface.color, color, sizeof(float[4]));
	} else {
		float color[4] = {1.f, 0.7f, 1.f, 1.f};
		memcpy(toplevel.surface.color, color, sizeof(float[4]));
	}

	/* Attempt copy selection on click */
	if (button_state == WL_POINTER_BUTTON_STATE_PRESSED) {
		if (data_source) {
			wl_data_source_destroy(data_source);
		}

		data_source = wl_data_device_manager_create_data_source(data_device_manager);
		wl_data_source_add_listener(data_source,
			&data_source_listener, NULL);
		if (mode == DEFAULT || mode == BAD_SERIAL ||
				mode == STEAL_SERIAL || mode == STEAL_SYNC) {
			printf("Sending a data source offer\n");
			wl_data_source_offer(data_source,
				"text/plain;charset=utf-8");
			wl_data_source_offer(data_source,
				"application/octet-stream");
		} else if (mode == CAT_RANDOM) {
			printf("Sending a data source offer\n");
			/* because clients will fall for this format */
			wl_data_source_offer(data_source,
				"text/plain;charset=utf-8");
		}

		/* Adding a large number to the serial can break other client's
		 * copy/paste attempts, if not guarded against */
		if (mode == BAD_SERIAL) {
			serial += 10000000;
		}
		wl_data_device_set_selection(data_device, data_source, serial);
		last_serial = serial;
	}

	surface_render(&toplevel.surface);
}

static void data_device_data_offer(void *data,
		struct wl_data_device *wl_data_device,
		struct wl_data_offer *id) {
	if (data_offer) {
		wl_data_offer_destroy(data_offer);
		clear_offer_stack();

	}
	data_offer = id;

	wl_data_offer_add_listener(id, &data_offer_listener, NULL);
}

static void receive_offer(const char *mimetype, bool via_sockpair, bool send_unpaired) {
	int32_t fds[2];
	int ret = -1;
	if (via_sockpair) {
		ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
	} else {
		ret = pipe(fds);
	}

	if (ret < 0) {
		printf("Failed to create %s, %s",
			via_sockpair ? "socketpair" : "pipe", strerror(errno));
		return;
	} else if (send_unpaired) {
		printf("Receiving offer for %s, and closing read end\n", mimetype);
		close(fds[0]);
		wl_data_offer_receive(data_offer, mimetype, fds[1]);
		close(fds[1]);
	} else {
		wl_data_offer_receive(data_offer, mimetype, fds[1]);
		close(fds[1]);

		enum recv_type rtype = RECV_TEXT;
		if (!strcmp(mimetype, "application/octet-stream")) {
			rtype = RECV_OCTET_STREAM;
		}
		add_set_fd(fds[0], SEND_NOT, rtype);
		printf("Receiving offer for %s\n", mimetype);
	}
}

static void data_device_selection(void *data,
		struct wl_data_device *wl_data_device,
		struct wl_data_offer *id) {

	if (id == NULL) {
		printf("No selection\n");
		return;
	}
	printf("Updated selection\n");
	struct mimetype_offer *offer = NULL;
	wl_list_for_each(offer, &offer_list, link) {
		if (mode == DEFAULT) {
			receive_offer(offer->val, false, false);
		} else if (mode == RECV_SOCKETPAIR) {
			receive_offer(offer->val, true, false);
		} else if (mode == RECV_EPIPE) {
			receive_offer(offer->val, false, true);
		} else if (mode == ZERO_SINK) {
			printf("Receiving offer for %s, to /dev/null\n", offer->val);
			wl_data_offer_receive(data_offer, offer->val, devnull);
		} else if (mode == RECV_FILE) {
			int fd = open("paste_result.txt",
				O_CREAT | O_RDWR | O_APPEND, 0644);
			if (fd != -1) {
				printf("Receiving offer for %s, to file\n", offer->val);
				wl_data_offer_receive(data_offer, offer->val, fd);
			} else {
				printf("Creating paste file failed: %s\n",
					strerror(errno));
			}
		} else if (mode == RECV_FLOOD) {
			// Sending many file descriptors; this can kill
			// the source program if its limits are too low...
			printf("Making %d receive requests...\n", 100);
			for (int i=0;i<100;i++) {
				wl_data_offer_receive(data_offer, offer->val, devnull);
			}
		}
	}
}

static void steal_done(void *data, struct wl_callback *wl_callback,
		uint32_t callback_data) {
	printf("Attempting selection with serial=%u\n", callback_data);
	wl_data_device_set_selection(data_device, data_source, callback_data);
	wl_callback_destroy(wl_callback);
	steal_state = SYNC_STEAL_DONE;
}


static const struct wl_data_source_listener data_source_listener = {
	.target = data_source_target,
	.send = data_source_send,
	.cancelled = data_source_cancelled,
	.dnd_drop_performed = data_source_dnd_drop_performed,
	.dnd_finished = data_source_dnd_finished,
	.action = data_source_action,
};
static const struct wl_data_offer_listener data_offer_listener = {
	.offer = data_offer_offer,
	.source_actions = data_offer_source_actions,
	.action = data_offer_action
};
static const struct wl_pointer_listener pointer_listener = {
	.enter = noop,
	.leave = noop,
	.motion = noop,
	.button = pointer_handle_button,
	.axis = noop,
};

static const struct wl_data_device_listener data_device_listener = {
	.data_offer = data_device_data_offer,
	.enter = noop,
	.leave = noop,
	.motion = noop,
	.drop = noop,
	.selection = data_device_selection,
};

static const struct wl_callback_listener steal_callback_listener = {
	.done = steal_done,
};

int main(int argc, char *argv[]) {
	if (argc > 1) {
		mode = (enum copyfu_mode)-1;
		size_t nopts = sizeof(cli_options) / sizeof(cli_options[0]);
		for (size_t i = 0; i < nopts; i++) {
			if (!strcmp(cli_options[i].name,argv[1])) {
				mode = cli_options[i].mode;
				break;
			}
		}
		if (mode == (enum copyfu_mode)-1) {
			printf("Usage: ./copy-fu [MODE=default]\n");
			for (size_t i = 0; i < nopts; i++) {
				printf("%15s %s\n", cli_options[i].name,
					cli_options[i].description);
			}
			return EXIT_FAILURE;
		}
	}

	wl_list_init(&offer_list);

	display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	registry_init(display);

	if (data_device_manager == NULL) {
		fprintf(stderr, "no data device manager\n");
		return EXIT_FAILURE;
	}

	if (pointer == NULL) {
		fprintf(stderr, "no pointer\n");
		return EXIT_FAILURE;
	}

	wl_pointer_add_listener(pointer, &pointer_listener, NULL);

	data_device = wl_data_device_manager_get_data_device(
		data_device_manager, seat);
	wl_data_device_add_listener(data_device, &data_device_listener, NULL);


	toplevel_init(&toplevel);
	float color[4] = {1.f, 0.3f, 1.f, 1.f};
	memcpy(toplevel.surface.color, color, sizeof(float[4]));

	int wlfd = wl_display_get_fd(display);
	fd_set.nfds = 1;
	fd_set.pfds = calloc(1, sizeof(struct pollfd));
	fd_set.pfds[0].fd = wlfd;
	/* it is assumed that the connection is always writeable */
	fd_set.pfds[0].events = POLLIN;
	fd_set.meta = calloc(1, sizeof(struct fd_extra));
	fd_set.meta[0].recv_type = RECV_NOT;
	fd_set.meta[0].send_type = SEND_NOT;
	fd_set.meta[0].refcount = 1;

	int urandom = mode == CAT_RANDOM ? open("/dev/urandom", O_RDONLY) : -1;
	if (mode == ZERO_SINK || mode == RECV_FLOOD) {
		devnull = open("/dev/null", O_WRONLY);
	}

	while (1) {
		if (wl_display_dispatch_pending(display) == -1) {
			fprintf(stderr, "dispatch-pending failed\n");
			break;
		}

		int ret = wl_display_flush(display);
		if (ret < 0 && errno != EAGAIN) {
			printf("flush error\n");
			break;
		}

		// set timeout if we have a periodic event
		int limit = -1;
		if (mode == STEAL_SERIAL || mode == STEAL_SYNC) {
			limit = 1000;
		}

		int nr = poll(fd_set.pfds, (nfds_t)fd_set.nfds, limit);
		if (nr < 0 && (errno == EAGAIN || errno == EINTR)) {
			continue;
		} else if (nr < 0){
			fprintf(stderr, "poll failure\n");
			break;
		}

		if (fd_set.pfds[0].revents & POLLIN) {
			wl_display_prepare_read(display);
			wl_display_read_events(display);
		}
		for (int i=1;i<fd_set.nfds;i++) {
			short rev = fd_set.pfds[i].revents;
			int fd = fd_set.pfds[i].fd;
			struct fd_extra *meta = &fd_set.meta[i];
			if (meta->recv_type != RECV_NOT && (rev & POLLIN)) {
				char buf[4096];
				int nr = (int)read(fd, buf, sizeof(buf));

				/* then actually read, print results */
				printf("Received from fd=%d: %.*s\n", fd, nr, buf);
			}
			if (meta->send_type != SEND_NOT && (rev & POLLOUT)) {
				printf("Writing to %d, already wrote %d\n", fd, meta->write_counter);
				if (mode == CAT_RANDOM) {
					char buf[4096];
					int nr = (int)read(urandom, buf, sizeof(buf));
					if (nr > 0) {
						write(fd, buf, (size_t)nr);
						meta->write_counter += nr;
					} else if (nr < 0) {
						meta->refcount--;
					}
				} else {
					if (meta->send_type == SEND_OCTET_STREAM) {
						uint64_t magic = 0x049a7b1504ec38ed;
						write(fd, &magic, sizeof(magic));
					} else if (meta->send_type == SEND_TEXT) {
						const char msg[] = "A text-type message";
						write(fd, msg, strlen(msg));
					}
					meta->refcount--;
				}
			}

			if (rev & POLLHUP) {
				if (meta->refcount > 0) {
					meta->refcount--;
				}
			}
		}
		remove_done_fds();

		if (mode == STEAL_SERIAL ||
				(mode == STEAL_SYNC && steal_state == SYNC_STEAL_READY)) {
			if (data_source) {
				wl_data_source_destroy(data_source);
			}

			/* try a new data source, in case the old one was cancelled */
			data_source = wl_data_device_manager_create_data_source(
				data_device_manager);
			wl_data_source_add_listener(data_source,
				&data_source_listener, NULL);
			wl_data_source_offer(data_source,
				"text/plain;charset=utf-8");

			if (mode == STEAL_SERIAL) {
				printf("Trying to select with serials %u through %u\n",
					last_serial - 50, last_serial + 100);
				for (int i = -50; i < 100; i++) {
					uint32_t serial = last_serial + (uint32_t)i;
					if (data_source) {
						wl_data_device_set_selection(
							data_device, data_source, serial);
					}
				}
			} else {
				printf("Asking for current serial\n");
				struct wl_callback *cb = wl_display_sync(display);
				wl_callback_add_listener(cb,
					&steal_callback_listener, NULL);
				steal_state = SYNC_STEAL_WAITING;
			}
		}
		if (steal_state == SYNC_STEAL_DONE) {
			steal_state = SYNC_STEAL_READY;
		}
	}

	if (urandom != -1) {
		close(urandom);
	}
	if (devnull != -1) {
		close(devnull);
	}
	return EXIT_SUCCESS;
}
