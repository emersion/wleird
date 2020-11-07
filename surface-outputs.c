#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "client.h"

struct wleird_subsurface {
	struct wleird_surface surface;

	struct wl_subsurface *wl_subsurface;
};

struct output {
	struct wl_output *wl_output;
	struct wl_list link;

	char *name;
};

#define OUTPUT_TRACKER_CAP 64

struct output_tracker {
	const char *title;
	struct output *outputs[OUTPUT_TRACKER_CAP];
};

static const char ESC = 0x001B;


static struct wl_subcompositor *subcompositor = NULL;
static struct wl_list outputs = {0}; // output.link

static struct wleird_toplevel toplevel = {0};
static struct output_tracker toplevel_tracker = {0};

static struct wleird_subsurface subsurfaces[3] = {0};
static struct output_tracker subsurface_trackers[3] = {0};
const size_t subsurfaces_len = sizeof(subsurfaces) / sizeof(subsurfaces[0]);

static bool warning_printed = false;

static void subsurface_init(struct wleird_subsurface *subsurface,
		struct wl_surface *parent) {
	surface_init(&subsurface->surface);

	subsurface->wl_subsurface = wl_subcompositor_get_subsurface(subcompositor,
		subsurface->surface.wl_surface, parent);
}


static void output_handle_geometry(void *data, struct wl_output *wl_output,
		int32_t x, int32_t y, int32_t phys_width, int32_t phys_height,
		int32_t subpixel, const char *make, const char *model,
		int32_t transform) {
	struct output *output = data;
	int len = snprintf(NULL, 0, "%s %s", make, model);
	output->name = malloc(len + 1);
	snprintf(output->name, len + 1, "%s %s", make, model);
}

static void output_handle_mode(void *data, struct wl_output *wl_output,
		uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	// No-op
}

static const struct wl_output_listener output_listener = {
	.geometry = output_handle_geometry,
	.mode = output_handle_mode,
};

static void output_create(struct wl_output *wl_output) {
	struct output *output = calloc(1, sizeof(*output));
	output->wl_output = wl_output;
	wl_list_insert(&outputs, &output->link);

	wl_output_add_listener(wl_output, &output_listener, output);
}

static struct output *find_output(struct wl_output *wl_output) {
	struct output *output;
	wl_list_for_each(output, &outputs, link) {
		if (output->wl_output == wl_output) {
			return output;
		}
	}
	abort();
}


static void print_state(void);

static void surface_handle_enter(void *data, struct wl_surface *wl_surface,
		struct wl_output *wl_output) {
	struct output_tracker *tracker = data;

	struct output *output = find_output(wl_output);

	ssize_t empty_index = -1;
	for (size_t i = 0; i < OUTPUT_TRACKER_CAP; i++) {
		if (tracker->outputs[i] == NULL) {
			empty_index = i;
		} else if (tracker->outputs[i] == output) {
			fprintf(stderr, "Warning: received duplicate enter event "
				"for output \"%s\" and surface \"%s\"\n",
				output->name, tracker->title);
			warning_printed = true;
			return;
		}
	}
	if (empty_index < 0) {
		fprintf(stderr, "Warning: cannot process wl_surface.enter event: "
			"no output slot available for surface \"%s\"\n", tracker->title);
		warning_printed = true;
		return;
	}
	tracker->outputs[empty_index] = output;

	print_state();
}

static void surface_handle_leave(void *data, struct wl_surface *wl_surface,
		struct wl_output *wl_output) {
	struct output_tracker *tracker = data;

	struct output *output = find_output(wl_output);

	for (size_t i = 0; i < OUTPUT_TRACKER_CAP; i++) {
		if (tracker->outputs[i] == output) {
			tracker->outputs[i] = NULL;
			goto out;
		}
	}

	fprintf(stderr, "Warning: received leave event without an enter event "
		"for output \"%s\" and surface \"%s\"\n", output->name, tracker->title);
	warning_printed = true;

out:
	print_state();
}

static const struct wl_surface_listener surface_listener = {
	.enter = surface_handle_enter,
	.leave = surface_handle_leave,
};


static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
		subcompositor = wl_registry_bind(registry, name,
			&wl_subcompositor_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output *wl_output =
			wl_registry_bind(registry, name, &wl_output_interface, 1);
		output_create(wl_output);
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


static void print_tracker_state(struct output_tracker *tracker) {
	printf("Surface \"%s\": ", tracker->title);

	bool empty = true;
	for (size_t i = 0; i < OUTPUT_TRACKER_CAP; i++) {
		if (tracker->outputs[i] == NULL) {
			continue;
		}
		if (!empty) {
			printf(", ");
		}
		printf("\"%s\"", tracker->outputs[i]->name);
		empty = false;
	}

	printf("\n");
}

static void print_state(void) {
	static bool first = true;
	if (!first && !warning_printed) {
		for (size_t i = 0; i < 1 + subsurfaces_len; i++) {
			printf("%c[1A", ESC); // move up
			printf("%c[2K", ESC); // clear line
		}
	}
	first = false;
	warning_printed = false;

	print_tracker_state(&toplevel_tracker);
	for (size_t i = 0; i < subsurfaces_len; i++) {
		print_tracker_state(&subsurface_trackers[i]);
	}
}

int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	toplevel_tracker.title = "toplevel";
	subsurface_trackers[0].title = "red sub-surface";
	subsurface_trackers[1].title = "green sub-surface";
	subsurface_trackers[2].title = "blue sub-surface";

	wl_list_init(&outputs);
	registry_init(display);

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	if (subcompositor == NULL) {
		fprintf(stderr, "compositor doesn't support wl_subcompositor\n");
		return EXIT_FAILURE;
	}

	toplevel_init(&toplevel);
	wl_surface_add_listener(toplevel.surface.wl_surface, &surface_listener,
		&toplevel_tracker);

	float color[4] = {1, 1, 1, 1};
	memcpy(toplevel.surface.color, color, sizeof(float[4]));

	for (size_t i = 0; i < subsurfaces_len; ++i) {
		subsurface_init(&subsurfaces[i], toplevel.surface.wl_surface);
		wl_surface_add_listener(subsurfaces[i].surface.wl_surface,
			&surface_listener, &subsurface_trackers[i]);

		subsurfaces[i].surface.width = 50;
		subsurfaces[i].surface.height = 50;

		float color[4] = {i == 0, i == 1, i == 2, 1};
		memcpy(subsurfaces[i].surface.color, color, sizeof(float[4]));

		surface_render(&subsurfaces[i].surface);
	}

	wl_subsurface_set_position(subsurfaces[0].wl_subsurface, 10, 10);
	wl_subsurface_set_position(subsurfaces[1].wl_subsurface, 200, 10);
	wl_subsurface_set_position(subsurfaces[2].wl_subsurface, 200, 200);

	print_state();

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	return EXIT_SUCCESS;
}
