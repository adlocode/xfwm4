#include "xdg_shell.h"

View *get_view_at(
		xfwmServer *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	/* This returns the topmost node in the scene at the given layout coords.
	 * we only care about surface nodes as we are specifically looking for a
	 * surface in the surface tree of a wb_view. */
	struct wlr_scene_node *node =
		wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	/* Find the node corresponding to the wb_view at the root of this
	 * surface tree, it is the only one for which we set the data field. */
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	return tree->node.data;
}

void focus_view(View *view, struct wlr_surface *surface) {
	/* Note: this function only deals with keyboard focus. */
	if (view == NULL || surface == NULL || !(wlr_surface_is_xdg_surface(surface)||
		wlr_surface_is_xwayland_surface(surface))) {
		return;
	}

	if (wlr_surface_is_xdg_surface(surface)) {
		struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_from_wlr_surface(surface);
		if (xdg_surface)
			wlr_log(WLR_INFO, "%s: %s", _("Keyboard focus is now on surface"),
				xdg_surface->toplevel->app_id);
	}

	xfwmServer *server = view->server;
	struct wlr_seat *seat = server->seat->seat;
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		/* Don't re-focus an already focused surface. */
		return;
	}
	if (prev_surface && wlr_surface_is_xdg_surface(prev_surface)) {
		/*
		 * Deactivate the previously focused surface. This lets the client know
		 * it no longer has focus and the client will repaint accordingly, e.g.
		 * stop displaying a caret.
		 */
		struct wlr_xdg_surface *previous =
			wlr_xdg_surface_from_wlr_surface(prev_surface);
		wlr_xdg_toplevel_set_activated(previous->toplevel, false);
	}
	else if (prev_surface && wlr_surface_is_xwayland_surface(prev_surface)) {
		struct wlr_xwayland_surface *previous =
			wlr_xwayland_surface_from_wlr_surface(prev_surface);
		wlr_xwayland_surface_activate(previous, false);
	}
	/* Move the view to the front */
	if (!server->seat->focused_layer) {
		wlr_scene_node_raise_to_top(&view->scene_tree->node);
	}
	wl_list_remove(&view->link);
	wl_list_insert(&server->views, &view->link);
	if (wlr_surface_is_xdg_surface(surface)) {
		/* Activate the new surface */
		wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);
		/*
		 * Tell the seat to have the keyboard enter this surface. wlroots will keep
		 * track of this and automatically send key events to the appropriate
		 * clients without additional work on your part.
		 */
		seat_focus_surface(server->seat, view->xdg_toplevel->base->surface);
	}
	else if (wlr_surface_is_xwayland_surface(surface)) {
		wlr_xwayland_surface_activate(view->xwayland_surface, true);
		seat_focus_surface(server->seat, view->xwayland_surface->surface);
	}
}

struct wlr_output *get_active_output(View *view) {
	double closest_x, closest_y;
	struct wlr_output *output = NULL;
	wlr_output_layout_closest_point(view->server->output_layout, output,
			view->geometry.x + view->geometry.width / 2,
			view->geometry.y + view->geometry.height / 2,
			&closest_x, &closest_y);
	return wlr_output_layout_output_at(view->server->output_layout, closest_x, closest_y);
}

static struct wlr_box get_usable_area(View *view) {
	struct wlr_output *output = get_active_output(view);
	struct wlr_box usable_area = {0};
	wlr_output_effective_resolution(output, &usable_area.width, &usable_area.height);
	return usable_area;
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	View *view = wl_container_of(listener, view, map);
	if (view->xdg_toplevel->base->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
		return;

	struct wlr_box geo_box = {0};
	struct wlr_box usable_area = get_usable_area(view);
	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);
	
		view->geometry.height = MIN(geo_box.height, usable_area.height);
		view->geometry.width = MIN(geo_box.width, usable_area.width);
		view->geometry.x = 0;
		view->geometry.y = 0;	

	/* A view no larger than a title bar shouldn't be sized or focused */
	if (view->geometry.height > TITLEBAR_HEIGHT &&
			view->geometry.height > TITLEBAR_HEIGHT *
			(usable_area.width / usable_area.height)) {
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
				view->geometry.width, view->geometry.height);
		focus_view(view, view->xdg_toplevel->base->surface);
	}

	wlr_scene_node_set_position(&view->scene_tree->node,
			view->geometry.x, view->geometry.y);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown. */
	View *view = wl_container_of(listener, view, unmap);
	if (view->xdg_toplevel->base->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
		return;
	reset_cursor_mode(view->server);

	/* Focus the next view, if any. */
	View *next_view = wl_container_of(view->link.next, next_view, link);
	if (next_view && next_view->scene_tree && next_view->scene_tree->node.enabled) {
		wlr_log(WLR_INFO, "%s: %s", _("Focusing next view"),
				next_view->xdg_toplevel->app_id);
		focus_view(next_view, next_view->xdg_toplevel->base->surface);
	}
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	/* Called when the surface is destroyed and should never be shown again. */
	View *view = wl_container_of(listener, view, destroy);

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->new_popup.link);

	if (view->xdg_toplevel->base->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		wl_list_remove(&view->request_fullscreen.link);
		wl_list_remove(&view->request_minimize.link);
		wl_list_remove(&view->request_maximize.link);
		wl_list_remove(&view->request_move.link);
		wl_list_remove(&view->request_resize.link);
		wl_list_remove(&view->link);
	}
                                                                             
  g_free (view);
}

static void xdg_toplevel_request_fullscreen(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to set itself to
	 * fullscreen. waybox currently doesn't support fullscreen, but to
	 * conform to xdg-shell protocol we still must send a configure.
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply.
	 */
	View *view =
		wl_container_of(listener, view, request_fullscreen);
	wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on
	 * client-side decorations.
	 */
	View *view = wl_container_of(listener, view, request_maximize);
	struct wlr_box usable_area = get_usable_area(view);

	bool is_maximized = view->xdg_toplevel->current.maximized;
	if (!is_maximized) {		
		view->previous_geometry = view->geometry;
		
			view->geometry.x = 0;
			view->geometry.y = 0;		
	} else {
		usable_area = view->previous_geometry;
		view->geometry.x = view->previous_geometry.x;
		view->geometry.y = view->previous_geometry.y;
	}
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, usable_area.width, usable_area.height);
	wlr_xdg_toplevel_set_maximized(view->xdg_toplevel, !is_maximized);
	wlr_scene_node_set_position(&view->scene_tree->node,
			view->geometry.x, view->geometry.y);
}

static void xdg_toplevel_request_minimize(struct wl_listener *listener, void *data) {
	View *view = wl_container_of(listener, view, request_minimize);
	bool minimize_requested = view->xdg_toplevel->requested.minimized;
	if (minimize_requested) {
		view->previous_geometry = view->geometry;
		view->geometry.y = -view->geometry.height;

		View *next_view = wl_container_of(view->link.next, next_view, link);
		if (wl_list_length(&view->link) > 1)
			focus_view(next_view, next_view->xdg_toplevel->base->surface);
		else
			focus_view(view, view->xdg_toplevel->base->surface);
	} else {
		view->geometry = view->previous_geometry;
	}

	wlr_scene_node_set_position(&view->scene_tree->node,
			view->geometry.x, view->geometry.y);
}

static void begin_interactive(View *view,
		enum CursorMode mode, uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propagating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	xfwmServer *server = view->server;
	struct wlr_surface *focused_surface =
		server->seat->seat->pointer_state.focused_surface;
	if (view->xdg_toplevel->base->surface != wlr_surface_get_root_surface(focused_surface)) {
		/* Deny move/resize requests from unfocused clients. */
		return;
	}
	server->grabbed_view = view;
	server->cursor->cursor_mode = mode;

	if (mode == XFWM_CURSOR_MOVE) {
		server->grab_x = server->cursor->cursor->x - view->geometry.x;
		server->grab_y = server->cursor->cursor->y - view->geometry.y;
	} else if (mode == XFWM_CURSOR_RESIZE) {
		struct wlr_box geo_box;
		wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);

		double border_x = (view->geometry.x + geo_box.x) +
			((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y = (view->geometry.y + geo_box.y) +
			((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
		server->grab_x = server->cursor->cursor->x - border_x;
		server->grab_y = server->cursor->cursor->y - border_y;

		server->grab_geo_box = geo_box;
		server->grab_geo_box.x += view->geometry.x;
		server->grab_geo_box.y += view->geometry.y;

		server->resize_edges = edges;
	}
}

static void xdg_toplevel_request_move(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. */
	View *view = wl_container_of(listener, view, request_move);
	begin_interactive(view, XFWM_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. */
	struct wlr_xdg_toplevel_resize_event *event = data;
	View *view = wl_container_of(listener, view, request_resize);
	begin_interactive(view, XFWM_CURSOR_RESIZE, event->edges);
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
	struct wlr_xdg_popup *popup = data;
	View *view = wl_container_of(listener, view, new_popup);

	struct wlr_output *wlr_output = wlr_output_layout_output_at(
			view->server->output_layout,
			view->geometry.x + popup->current.geometry.x,
			view->geometry.y + popup->current.geometry.y);

	if (!wlr_output) return;
	Output *output = wlr_output->data;

	int top_margin = 0;
	struct wlr_box output_toplevel_box = {
		.x = output->geometry.x - view->geometry.x,
		.y = output->geometry.y - view->geometry.y,
		.width = output->geometry.width,
		.height = output->geometry.height - top_margin,
	};
	wlr_xdg_popup_unconstrain_from_box(popup, &output_toplevel_box);
}

static void handle_new_xdg_surface(struct wl_listener *listener, void *data) {
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup. */
	xfwmServer *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;

	/* We must add xdg popups to the scene graph so they get rendered. The
	 * wlroots scene graph provides a helper for this, but to use it we must
	 * provide the proper parent scene node of the xdg popup. To enable this,
	 * we always set the user data field of xdg_surfaces to the corresponding
	 * scene node. */
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		if (wlr_surface_is_xdg_surface(xdg_surface->popup->parent)) {
			struct wlr_xdg_surface *parent = wlr_xdg_surface_from_wlr_surface(
				xdg_surface->popup->parent);
			struct wlr_scene_tree *parent_tree = parent->data;
			xdg_surface->data = wlr_scene_xdg_surface_create(
				parent_tree, xdg_surface);
		}
		/* The scene graph doesn't currently unconstrain popups, so keep going */
		/* return; */
	}
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_NONE)
		return;

	/* Allocate a wb_view for this surface */
	View *view = g_new0 (View, 1);
	view->server = server;
	view->xdg_toplevel = xdg_surface->toplevel;

	/* Listen to the various events it can emit */
	view->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);
	view->new_popup.notify = handle_new_popup;
	wl_signal_add(&xdg_surface->events.new_popup, &view->new_popup);

	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		view->scene_tree = wlr_scene_xdg_surface_create(
			&view->server->scene->tree, view->xdg_toplevel->base);
		view->scene_tree->node.data = view;
		xdg_surface->data = view->scene_tree;

		struct wlr_xdg_toplevel *toplevel = view->xdg_toplevel;
		view->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
		wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen);
		view->request_maximize.notify = xdg_toplevel_request_maximize;
		wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);
		view->request_minimize.notify = xdg_toplevel_request_minimize;
		wl_signal_add(&toplevel->events.request_minimize, &view->request_minimize);
		view->request_move.notify = xdg_toplevel_request_move;
		wl_signal_add(&toplevel->events.request_move, &view->request_move);
		view->request_resize.notify = xdg_toplevel_request_resize;
		wl_signal_add(&toplevel->events.request_resize, &view->request_resize);

		wl_list_insert(&view->server->views, &view->link);
	}
}

void init_xdg_shell(xfwmServer *server) {
	/* xdg-shell version 3 */
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display, 3);
	server->new_xdg_surface.notify = handle_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface, &server->new_xdg_surface);
}
