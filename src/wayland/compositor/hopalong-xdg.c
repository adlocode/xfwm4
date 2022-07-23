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
#include "hopalong-xdg.h"
#include "hopalong-server.h"
#include "hopalong-decoration.h"
#include "xfwm-shell.h"

static struct hopalong_view *active_view = NULL;

static void
hopalong_xdg_surface_map(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);

	struct hopalong_view *view = wl_container_of(listener, view, map);
  
  wlr_log (WLR_INFO, "\nmap\n");
  
  view->shell_window = xfwm_shell_window_create (view->server->shell);

	hopalong_view_focus(view, view->xdg_surface->surface);
}

static void
hopalong_xdg_surface_unmap(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);

	struct hopalong_view *view = wl_container_of(listener, view, unmap);
  
  xfwm_shell_window_destroy (view->shell_window);
  
	hopalong_view_unmap(view);
}

static void
hopalong_xdg_surface_destroy(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);

	struct hopalong_view *view = wl_container_of(listener, view, destroy);
  
  if (active_view == view)
    active_view = NULL;

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->set_title.link);
	wl_list_remove(&view->surface_commit.link);

	hopalong_view_destroy(view);
}

static void
hopalong_xdg_toplevel_set_title(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, set_title);
	view->title_dirty = true;
  
  char title[4096] = {};

	const char *title_data = hopalong_view_getprop(view, HOPALONG_VIEW_TITLE);
	if (title_data == NULL)
		title_data = hopalong_view_getprop(view, HOPALONG_VIEW_APP_ID);
	if (title_data != NULL)
		strlcpy(title, title_data, sizeof title);
  
  if (view->shell_window)
    xfwm_shell_window_set_title (view->shell_window, title);
}

static void
hopalong_xdg_begin_drag(struct hopalong_view *view, enum hopalong_cursor_mode mode, uint32_t edges)
{
	struct hopalong_server *server = view->server;
	return_if_fail(server != NULL);

	struct wlr_surface *focused_surface = server->seat->pointer_state.focused_surface;
	return_if_fail(focused_surface != NULL);

	if (view->xdg_surface->surface != focused_surface)
		return;

	server->grabbed_view = view;
	server->cursor_mode = mode;

	if (mode == HOPALONG_CURSOR_MOVE)
	{
		server->grab_x = server->cursor->x - view->x;
		server->grab_y = server->cursor->y - view->y;
	}
	else if (mode == HOPALONG_CURSOR_RESIZE)
	{
		struct wlr_box geo_box;

		wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);

		double border_x = (view->x + geo_box.x) + ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y = (view->y + geo_box.y) + ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);

		server->grab_x = server->cursor->x - border_x;
		server->grab_y = server->cursor->y - border_y;

		server->grab_geobox = geo_box;
		server->grab_geobox.x += view->x;
		server->grab_geobox.y += view->y;

		server->resize_edges = edges;
	}
}

static void
hopalong_xdg_toplevel_request_move(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);

	struct hopalong_view *view = wl_container_of(listener, view, request_move);
	hopalong_xdg_begin_drag(view, HOPALONG_CURSOR_MOVE, 0);
}

static void
hopalong_xdg_toplevel_request_resize(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);
	return_if_fail(data != NULL);

	struct wlr_xdg_toplevel_resize_event *event = data;
	struct hopalong_view *view = wl_container_of(listener, view, request_resize);
	hopalong_xdg_begin_drag(view, HOPALONG_CURSOR_RESIZE, event->edges);
}

static void
hopalong_xdg_toplevel_minimize(struct hopalong_view *view)
{
	wlr_log(WLR_INFO, "hopalong_xdg_toplevel_minimize: not implemented");
}

static void
hopalong_xdg_toplevel_maximize(struct hopalong_view *view)
{
	wlr_log(WLR_INFO, "hopalong_xdg_toplevel_maximize: not implemented");
}

static void
hopalong_xdg_toplevel_close(struct hopalong_view *view)
{
	struct wlr_xdg_surface *surface = view->xdg_surface;

	if (surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL || surface->toplevel == NULL)
		return;

	wlr_xdg_toplevel_send_close(surface);
}

static const char *
hopalong_xdg_toplevel_getprop(struct hopalong_view *view, enum hopalong_view_prop prop)
{
	switch (prop)
	{
	case HOPALONG_VIEW_TITLE:
		return view->xdg_surface->toplevel->title;
	case HOPALONG_VIEW_APP_ID:
		return view->xdg_surface->toplevel->app_id;
	}

	return NULL;
}

static struct wlr_surface *
hopalong_xdg_toplevel_get_surface(struct hopalong_view *view)
{
	return view->xdg_surface->surface;
}

static void
hopalong_xdg_toplevel_set_activated(struct hopalong_view *view, bool activated)
{
	wlr_xdg_toplevel_set_activated(view->xdg_surface, activated);
  
  if (active_view)
    xfwm_shell_window_set_activated (active_view->shell_window, false);
  
  xfwm_shell_window_set_activated (view->shell_window, activated);
  
  active_view = view;
}

static bool
hopalong_xdg_toplevel_get_geometry(struct hopalong_view *view, struct wlr_box *box)
{
	wlr_xdg_surface_get_geometry(view->xdg_surface, box);
	return true;
}

static void
hopalong_xdg_toplevel_set_size(struct hopalong_view *view, int width, int height)
{
	wlr_xdg_toplevel_set_size(view->xdg_surface, width, height);
}

static struct wlr_surface *
hopalong_xdg_toplevel_surface_at(struct hopalong_view *view, double x, double y, double *sx, double *sy)
{
	double view_sx = x - view->x;
	double view_sy = y - view->y;

	double _sx = 0.0, _sy = 0.0;
	struct wlr_surface *surface = wlr_xdg_surface_surface_at(view->xdg_surface, view_sx, view_sy, &_sx, &_sy);

	if (surface != NULL)
	{
		*sx = _sx;
		*sy = _sy;
	}

	return surface;
}

static bool
hopalong_xdg_toplevel_can_move(struct hopalong_view *view)
{
	return true;
}

static bool
hopalong_xdg_toplevel_can_resize(struct hopalong_view *view)
{
	return true;
}

static const struct hopalong_view_ops hopalong_xdg_view_ops = {
	.minimize = hopalong_xdg_toplevel_minimize,
	.maximize = hopalong_xdg_toplevel_maximize,
	.close = hopalong_xdg_toplevel_close,
	.getprop = hopalong_xdg_toplevel_getprop,
	.get_surface = hopalong_xdg_toplevel_get_surface,
	.set_activated = hopalong_xdg_toplevel_set_activated,
	.get_geometry = hopalong_xdg_toplevel_get_geometry,
	.set_size = hopalong_xdg_toplevel_set_size,
	.surface_at = hopalong_xdg_toplevel_surface_at,
	.can_move = hopalong_xdg_toplevel_can_move,
	.can_resize = hopalong_xdg_toplevel_can_resize,
};

static void
hopalong_xdg_surface_commit(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, surface_commit);

	/* GTK3 insists on using CSD, we can detect this because the surface given to the
	 * compositor is actually a sub-surface and has an offset in its geometry.
	 */
	view->using_csd = false;
	if (view->xdg_surface->current.geometry.x || view->xdg_surface->current.geometry.y)
		view->using_csd = true;
  
  view->using_csd = true;
}

static void
hopalong_xdg_new_surface(struct wl_listener *listener, void *data)
{
	struct hopalong_server *server = wl_container_of(listener, server, new_xdg_surface);
	return_if_fail(server != NULL);

	struct wlr_xdg_surface *xdg_surface = data;
	return_if_fail(xdg_surface != NULL);

	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
		return;

	struct hopalong_view *view = calloc(1, sizeof(*view));
	view->server = server;
	view->xdg_surface = xdg_surface;
	view->ops = &hopalong_xdg_view_ops;
	view->layer = HOPALONG_LAYER_MIDDLE;
  
  view->shell_window = NULL;

	view->x = view->y = 64;

	/* hook up our xdg_surface events */
	view->map.notify = hopalong_xdg_surface_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);

	view->unmap.notify = hopalong_xdg_surface_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);

	view->destroy.notify = hopalong_xdg_surface_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	/* hook up our xdg_toplevel events */
	struct wlr_xdg_toplevel *xdg_toplevel = xdg_surface->toplevel;
	return_if_fail(xdg_toplevel != NULL);

	view->request_move.notify = hopalong_xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move, &view->request_move);

	view->request_resize.notify = hopalong_xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize, &view->request_resize);

	view->title_dirty = true;
	view->set_title.notify = hopalong_xdg_toplevel_set_title;
	wl_signal_add(&xdg_toplevel->events.set_title, &view->set_title);

	view->surface_commit.notify = hopalong_xdg_surface_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &view->surface_commit);

	/* add to the list of views */
	wl_list_insert(&server->views, &view->link);
}

/*
 * Sets up resources related to XDG shell support.
 */
void
hopalong_xdg_shell_setup(struct hopalong_server *server)
{
	return_if_fail(server != NULL);

	server->xdg_shell = wlr_xdg_shell_create(server->display);
	return_if_fail(server->xdg_shell);

	server->new_xdg_surface.notify = hopalong_xdg_new_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface, &server->new_xdg_surface);

  //This code commented out as a workaround to a bug
	//hopalong_decoration_setup(server);
}

/*
 * Tears down resources related to XDG shell support.
 */
void
hopalong_xdg_shell_teardown(struct hopalong_server *server)
{
	return_if_fail(server != NULL);

	hopalong_decoration_teardown(server);
}
