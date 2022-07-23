/*
 * Hopalong - a friendly Wayland compositor
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <stdlib.h>
#include "hopalong-server.h"
#include "hopalong-output.h"
#include "hopalong-xdg.h"
#include "hopalong-cursor.h"
#include "hopalong-seat.h"
#include "hopalong-xwayland.h"
#include "hopalong-keybinding.h"

#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/backend/wayland.h>

static void
hopalong_server_new_output(struct wl_listener *listener, void *data)
{
	struct hopalong_server *server = wl_container_of(listener, server, new_output);
	return_if_fail(server != NULL);

	struct wlr_output *wlr_output = data;
	return_if_fail(wlr_output != NULL);

 	if (!wlr_output_init_render(wlr_output, server->allocator, server->renderer)) {
		return;
 	}

	/* TODO: Right now if we are using a rendering backend that has modes, we
	 * choose the preferred mode.  It would be better if this were configurable
	 * to allow the user to choose the resolution of her preference.
	 */
	if (!wl_list_empty(&wlr_output->modes))
	{
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);

		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);

		if (!wlr_output_commit(wlr_output))
			return;
	}

	struct hopalong_output *output = hopalong_output_new_from_wlr_output(server, wlr_output);
	return_if_fail(output != NULL);

	/* TODO: configure position of output in the layout */
	wlr_output_layout_add_auto(server->output_layout, wlr_output);
}

static bool
hopalong_server_initialize(struct hopalong_server *server, const struct hopalong_server_options *options)
{
	return_val_if_fail(server != NULL, false);

	/* set up the display server */
	server->display = wl_display_create();
	return_val_if_fail(server->display != NULL, false);

	/* set up the backend */
	server->backend = wlr_backend_autocreate(server->display);
	return_val_if_fail(server->backend != NULL, false);

	/* set up a GLES2 renderer */
 	server->renderer = wlr_renderer_autocreate(server->backend);
 	return_val_if_fail(server->renderer != NULL, false);

 	/* set up an allocator */
 	server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
 	return_val_if_fail(server->allocator != NULL, false);

	/* start hooking up wlroots stuff */
	wlr_renderer_init_wl_display(server->renderer, server->display);
	server->compositor = wlr_compositor_create(server->display, server->renderer);
	wlr_data_device_manager_create(server->display);

	/* set up output layout manager */
	server->output_layout = wlr_output_layout_create();
	return_val_if_fail(server->output_layout != NULL, false);

	/* listen for output layout changes */
	wl_list_init(&server->outputs);
	server->new_output.notify = hopalong_server_new_output;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	/* set up output layout manager protocol */
	server->xdg_output_manager = wlr_xdg_output_manager_v1_create(server->display, server->output_layout);

	/* initialize view lists */
	wl_list_init(&server->views);

	/* initialize the layers */
	for (size_t i = 0; i < HOPALONG_LAYER_COUNT; i++)
		wl_list_init(&server->mapped_layers[i]);

	/* set up cursor */
	hopalong_cursor_setup(server);

	/* set up keybindings */
	hopalong_keybinding_setup(server);

	/* set up XDG shell */
	hopalong_xdg_shell_setup(server);

	/* set up XWayland shell */
	hopalong_xwayland_shell_setup(server);

	/* set up layer shell */
	hopalong_layer_shell_setup(server);

	/* set up the seat */
	hopalong_seat_setup(server);

	/* add useful extensions */
	wlr_screencopy_manager_v1_create(server->display);
	wlr_export_dmabuf_manager_v1_create(server->display);
	wlr_data_control_manager_v1_create(server->display);
	wlr_gamma_control_manager_v1_create(server->display);
	wlr_primary_selection_v1_device_manager_create(server->display);
  
  server->is_windowed = FALSE;
  if (getenv ("WAYLAND_DISPLAY") || getenv ("WAYLAND_SOCKET"))
    server->is_windowed = TRUE;

	/* set up style */
	if (options->style_name != NULL)
		server->style = hopalong_style_load(options->style_name);
	else
		server->style = hopalong_style_get_default();

	return true;
}

/*
 * Initialize the Hopalong server.
 */
struct hopalong_server *
hopalong_server_new(const struct hopalong_server_options *options)
{
	struct hopalong_server *server = calloc(1, sizeof(*server));

	if (!hopalong_server_initialize(server, options))
	{
		hopalong_server_destroy(server);
		return NULL;
	}

	return server;
}

/*
 * Run the compositor event loop.  This does not return until we are
 * terminating.
 */
bool
hopalong_server_run(struct hopalong_server *server)
{
	return_val_if_fail(server != NULL, false);
	return_val_if_fail(server->display != NULL, false);

	if (!wlr_backend_start(server->backend))
		return false;

	wl_display_run(server->display);

	return true;
}

/*
 * Destroy the Hopalong server.
 */
void
hopalong_server_destroy(struct hopalong_server *server)
{
	return_if_fail(server != NULL);

	hopalong_seat_teardown(server);
	hopalong_cursor_teardown(server);
	hopalong_layer_shell_teardown(server);
	hopalong_xwayland_shell_teardown(server);
	hopalong_xdg_shell_teardown(server);
	hopalong_keybinding_teardown(server);

	if (server->output_layout)
		wlr_output_layout_destroy(server->output_layout);

	if (server->backend)
		wlr_backend_destroy(server->backend);

	if (server->display)
		wl_display_destroy(server->display);

	free(server);
}

/*
 * Add a listening socket for clients.
 */
const char *
hopalong_server_add_socket(struct hopalong_server *server)
{
	return_val_if_fail(server != NULL, NULL);

	return wl_display_add_socket_auto(server->display);
}
