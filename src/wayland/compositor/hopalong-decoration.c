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
#include "hopalong-decoration.h"

#include <wlr/types/wlr_xdg_decoration_v1.h>

struct hopalong_deco_state {
	struct hopalong_server *server;
	struct wlr_xdg_toplevel_decoration_v1 *wlr_deco;

	struct wl_listener request_mode;
	struct wl_listener destroy;
};

static void
hopalong_xdg_decoration_request_mode(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_decoration_v1 *wlr_deco = data;
	wlr_xdg_toplevel_decoration_v1_set_mode(wlr_deco,
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void
hopalong_xdg_decoration_destroy(struct wl_listener *listener, void *data)
{
	struct hopalong_deco_state *d = wl_container_of(listener, d, destroy);

	wl_list_remove(&d->destroy.link);
	wl_list_remove(&d->request_mode.link);
	free(d);
}

static void
hopalong_xdg_toplevel_new_decoration(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);
	return_if_fail(data != NULL);

	struct hopalong_server *server = wl_container_of(listener, server, new_toplevel_decoration);
	return_if_fail(server != NULL);

	struct wlr_xdg_toplevel_decoration_v1 *wlr_deco = data;

	struct hopalong_deco_state *deco_state = calloc(1, sizeof(*deco_state));

	deco_state->server = server;
	deco_state->wlr_deco = wlr_deco;

	deco_state->request_mode.notify = hopalong_xdg_decoration_request_mode;
	wl_signal_add(&wlr_deco->events.request_mode, &deco_state->request_mode);

	deco_state->destroy.notify = hopalong_xdg_decoration_destroy;
	wl_signal_add(&wlr_deco->events.destroy, &deco_state->destroy);

	hopalong_xdg_decoration_request_mode(&deco_state->request_mode, wlr_deco);
}

void
hopalong_decoration_setup(struct hopalong_server *server)
{
  server->xdg_deco_mgr = wlr_xdg_decoration_manager_v1_create(server->display);
	server->new_toplevel_decoration.notify = hopalong_xdg_toplevel_new_decoration;
	wl_signal_add(&server->xdg_deco_mgr->events.new_toplevel_decoration, &server->new_toplevel_decoration);

	server->wlr_deco_mgr = wlr_server_decoration_manager_create(server->display);

	wlr_server_decoration_manager_set_default_mode(server->wlr_deco_mgr, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
}

void
hopalong_decoration_teardown(struct hopalong_server *server)
{
}
