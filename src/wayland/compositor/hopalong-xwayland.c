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
#include "hopalong-xwayland.h"

static void
hopalong_xwayland_surface_map(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, map);
	hopalong_view_focus(view, view->xwayland_surface->surface);
}

static void
hopalong_xwayland_surface_unmap(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, unmap);
	hopalong_view_unmap(view);
}

static void
hopalong_xwayland_destroy(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, destroy);

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_configure.link);
	wl_list_remove(&view->set_title.link);

	hopalong_view_destroy(view);
}

static void
hopalong_xwayland_set_title(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, set_title);
	view->title_dirty = true;
}

static void
hopalong_xwayland_toplevel_minimize(struct hopalong_view *view)
{
	wlr_log(WLR_INFO, "hopalong_xwayland_toplevel_minimize: not implemented");
}

static void
hopalong_xwayland_toplevel_maximize(struct hopalong_view *view)
{
	wlr_log(WLR_INFO, "hopalong_xwayland_toplevel_maximize: not implemented");
}

static void
hopalong_xwayland_toplevel_close(struct hopalong_view *view)
{
	wlr_xwayland_surface_close(view->xwayland_surface);
}

static const char *
hopalong_xwayland_toplevel_getprop(struct hopalong_view *view, enum hopalong_view_prop prop)
{
	switch (prop)
	{
	case HOPALONG_VIEW_TITLE:
		return view->xwayland_surface->title;
	case HOPALONG_VIEW_APP_ID:
		return view->xwayland_surface->class;
	}

	return NULL;
}

static struct wlr_surface *
hopalong_xwayland_toplevel_get_surface(struct hopalong_view *view)
{
	return view->xwayland_surface->surface;
}

static void
hopalong_xwayland_toplevel_set_activated(struct hopalong_view *view, bool activated)
{
	struct hopalong_server *server = view->server;

	wlr_xwayland_set_seat(server->wlr_xwayland, server->seat);
	wlr_xwayland_surface_activate(view->xwayland_surface, true);
}

static bool
hopalong_xwayland_toplevel_get_geometry(struct hopalong_view *view, struct wlr_box *box)
{
	return_val_if_fail(view->xwayland_surface != NULL, false);

	struct wlr_surface *surface = hopalong_view_get_surface(view);
	if (surface == NULL)
		return false;

	box->x = view->x;
	box->y = view->y;

	box->width = surface->current.width;
	box->height = surface->current.height;
	return true;
}

static void
hopalong_xwayland_toplevel_set_size(struct hopalong_view *view, int width, int height)
{
	wlr_xwayland_surface_configure(view->xwayland_surface, view->x, view->y, width, height);
}

static struct wlr_surface *
hopalong_xwayland_toplevel_surface_at(struct hopalong_view *view, double x, double y, double *sx, double *sy)
{
	struct wlr_surface *surface = hopalong_view_get_surface(view);

	if (surface != NULL)
	{
		int w = surface->current.width;
		int h = surface->current.height;

		if (x >= view->x && x <= view->x + w &&
		    y >= view->y && y <= view->y + h)
		{
			*sx = x - view->x;
			*sy = y - view->y;

			return surface;
		}
	}

	return NULL;
}

static bool
hopalong_xwayland_toplevel_can_move(struct hopalong_view *view)
{
	return true;
}

static bool
hopalong_xwayland_toplevel_can_resize(struct hopalong_view *view)
{
	return true;
}

static const struct hopalong_view_ops hopalong_xwayland_view_ops = {
	.minimize = hopalong_xwayland_toplevel_minimize,
	.maximize = hopalong_xwayland_toplevel_maximize,
	.close = hopalong_xwayland_toplevel_close,
	.getprop = hopalong_xwayland_toplevel_getprop,
	.get_surface = hopalong_xwayland_toplevel_get_surface,
	.set_activated = hopalong_xwayland_toplevel_set_activated,
	.get_geometry = hopalong_xwayland_toplevel_get_geometry,
	.set_size = hopalong_xwayland_toplevel_set_size,
	.surface_at = hopalong_xwayland_toplevel_surface_at,
	.can_move = hopalong_xwayland_toplevel_can_move,
	.can_resize = hopalong_xwayland_toplevel_can_resize,
};

static void
hopalong_xwayland_request_configure(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, request_configure);
	struct wlr_xwayland_surface_configure_event *ev = data;
	struct wlr_xwayland_surface *xsurface = view->xwayland_surface;

	wlr_xwayland_surface_configure(xsurface, ev->x, ev->y, ev->width, ev->height);

	view->x = ev->x + 128;
	view->y = ev->y + 128;

	if (xsurface->surface != NULL)
		hopalong_view_focus(view, xsurface->surface);
}

static void
hopalong_xwayland_new_surface(struct wl_listener *listener, void *data)
{
	struct wlr_xwayland_surface *xwayland_surface = data;

	struct hopalong_server *server = wl_container_of(listener, server, new_xwayland_surface);
	struct hopalong_view *view = calloc(1, sizeof(*view));

	view->xwayland_surface = xwayland_surface;
	view->server = server;
	view->ops = &hopalong_xwayland_view_ops;
	view->layer = HOPALONG_LAYER_MIDDLE;

	if (xwayland_surface->override_redirect)
		view->using_csd = true;

	view->map.notify = hopalong_xwayland_surface_map;
	wl_signal_add(&xwayland_surface->events.map, &view->map);

	view->unmap.notify = hopalong_xwayland_surface_unmap;
	wl_signal_add(&xwayland_surface->events.unmap, &view->unmap);

	view->destroy.notify = hopalong_xwayland_destroy;
	wl_signal_add(&xwayland_surface->events.destroy, &view->destroy);

	view->request_configure.notify = hopalong_xwayland_request_configure;
	wl_signal_add(&xwayland_surface->events.request_configure, &view->request_configure);

	view->set_title.notify = hopalong_xwayland_set_title;
	wl_signal_add(&xwayland_surface->events.set_title, &view->set_title);

	wl_list_insert(&server->views, &view->link);

	view->title_dirty = true;
}

void
hopalong_xwayland_shell_setup(struct hopalong_server *server)
{
	server->wlr_xwayland = wlr_xwayland_create(server->display, server->compositor, true);

	server->new_xwayland_surface.notify = hopalong_xwayland_new_surface;
	wl_signal_add(&server->wlr_xwayland->events.new_surface, &server->new_xwayland_surface);
}

void
hopalong_xwayland_shell_teardown(struct hopalong_server *server)
{
	wlr_xwayland_destroy(server->wlr_xwayland);
}

