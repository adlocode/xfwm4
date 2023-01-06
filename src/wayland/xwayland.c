#include "xwayland.h"

static void xwayland_toplevel_destroy(
		struct wl_listener *listener, void *data)
{
	View *view = wl_container_of (listener, view, destroy);

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->destroy.link);
	
	wl_list_remove(&view->link);

	free(view);
}

static void xwayland_toplevel_map(struct wl_listener *listener, void *data)
{
	View *view = wl_container_of (listener, view, map);
					   
	view->scene_tree =
		wlr_scene_subsurface_tree_create(&view->server->scene->tree,
			view->xwayland_surface->surface);
	
	view->scene_tree->node.data = view;
}

void handle_new_xwayland_surface(struct wl_listener *listener, void *data)
{
	xfwmServer *server =
		wl_container_of(listener, server, new_xwayland_surface);
	struct wlr_xwayland_surface *xsurface = data;
					  
	View *view =
		calloc(1, sizeof(View));
	view->server = server;
	view->xwayland_surface = xsurface;
					  
	view->destroy.notify = xwayland_toplevel_destroy;
	wl_signal_add(&xsurface->events.destroy, &view->destroy);	
	view->map.notify = xwayland_toplevel_map;
	wl_signal_add(&xsurface->events.map, &view->map);
	
	wl_list_insert (&view->server->views, &view->link);
	
}
	
void init_xwayland(xfwmServer *server)
{
	server->xwayland = wlr_xwayland_create (server->wl_display, server->compositor, true);
	if (!server->xwayland) {
		wlr_log (WLR_ERROR, "Failed to start Xwayland\n");
		unsetenv ("DISPLAY");
	}
	else {		
		server->new_xwayland_surface.notify = handle_new_xwayland_surface;
		wl_signal_add (&server->xwayland->events.new_surface,
		       &server->new_xwayland_surface);
	
		setenv ("DISPLAY", server->xwayland->display_name, true);
	}
}
