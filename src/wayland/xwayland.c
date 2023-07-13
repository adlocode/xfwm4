/* Copyright (c) 2018 - 2023 adlo
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "xwayland.h"
#include "client.h"

static void
xwayland_toplevel_destroy (struct wl_listener *listener, void *data)
{
    Client *view = wl_container_of (listener, view, destroy);

    wl_list_remove(&view->map.link);
    wl_list_remove(&view->destroy.link);

    clientUnframe (view, FALSE);
}

static void
xwayland_toplevel_map (struct wl_listener *listener, void *data)
{
    Client *view = wl_container_of (listener, view, map);
                       
    view->scene_tree =
        wlr_scene_subsurface_tree_create(&view->server->scene->tree,
            view->xwayland_surface->surface);
    
    view->scene_tree->node.data = view;
}

void
handle_new_xwayland_surface (struct wl_listener *listener, void *data)
{
    xfwmWaylandCompositor *server =
        wl_container_of(listener, server, new_xwayland_surface);
    struct wlr_xwayland_surface *xsurface = data;
    DisplayInfo *display_info = myDisplayGetDefault ();

    if ((Window) xsurface->window_id == None)
    {
        return;
    }
                      
    Client *c = clientFrameX11 (display_info, (Window) xsurface->window_id, FALSE);

    if (!c)
    {
        return;
    }		

    c->server = server;
    c->xwayland_surface = xsurface;
                      
    c->destroy.notify = xwayland_toplevel_destroy;
    wl_signal_add(&xsurface->events.destroy, &c->destroy);
    c->map.notify = xwayland_toplevel_map;
    wl_signal_add(&xsurface->events.map, &c->map);
    
}

void
handle_xwayland_ready (struct wl_listener *listener, void *data)
{
    xfwmWaylandCompositor *compositor = wl_container_of (listener, compositor, xwayland_ready);

    /* At this point xwayland is all setup to start accepting
     * connections so we can quit the transient initialization mainloop
     * and unblock xfwmWaylandInit() to continue initializing xfwm4. */
    gtk_main_quit ();
}
    
void
init_xwayland (xfwmWaylandCompositor *server)
{
    server->xwayland = wlr_xwayland_create (server->wl_display, server->compositor, false);
    if (!server->xwayland) {
        wlr_log (WLR_ERROR, "Failed to start Xwayland\n");
        unsetenv ("DISPLAY");
    }
    else {		
        server->new_xwayland_surface.notify = handle_new_xwayland_surface;
        wl_signal_add (&server->xwayland->events.new_surface,
               &server->new_xwayland_surface);
        server->xwayland_ready.notify = handle_xwayland_ready;
        wl_signal_add (&server->xwayland->events.ready,
                       &server->xwayland_ready);
    
        setenv ("DISPLAY", server->xwayland->display_name, true);
    }
    
    /* We need to run a mainloop until xwayland is ready to
     * start accepting connections */
    gtk_main ();
}
