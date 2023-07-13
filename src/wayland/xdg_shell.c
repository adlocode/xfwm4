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

#include "xdg_shell.h"
#include "display.h"
#include "client.h"
#include "stacking.h"

Client *
get_view_at (xfwmWaylandCompositor *server, double lx, double ly,
             struct wlr_surface **surface, double *sx, double *sy)
{
    /* This returns the topmost node in the scene at the given layout coords.
     * we only care about surface nodes as we are specifically looking for a
     * surface in the surface tree of a Client. */
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
    /* Find the node corresponding to the Client at the root of this
     * surface tree, it is the only one for which we set the data field. */
    struct wlr_scene_tree *tree = node->parent;
    while (tree != NULL && tree->node.data == NULL) {
        tree = tree->node.parent;
    }
    return tree->node.data;
}

void
focus_view (Client *c, struct wlr_surface *surface)
{
    /* Note: this function only deals with keyboard focus. */
    if (c == NULL || surface == NULL || !(wlr_surface_is_xdg_surface(surface)||
        wlr_surface_is_xwayland_surface(surface))) {
        return;
    }

    if (wlr_surface_is_xdg_surface(surface)) {
        struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_from_wlr_surface(surface);
        if (xdg_surface)
            wlr_log(WLR_INFO, "%s: %s", _("Keyboard focus is now on surface"),
                xdg_surface->toplevel->app_id);
    }

    xfwmWaylandCompositor *server = c->server;
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
    clientRaise (c, None);
    if (wlr_surface_is_xdg_surface(surface)) {
        /* Activate the new surface */
        wlr_xdg_toplevel_set_activated(c->xdg_toplevel, true);
        /*
         * Tell the seat to have the keyboard enter this surface. wlroots will keep
         * track of this and automatically send key events to the appropriate
         * clients without additional work on your part.
         */
        seat_focus_surface(server->seat, c->xdg_toplevel->base->surface);
    }
    else if (wlr_surface_is_xwayland_surface(surface)) {
        wlr_xwayland_surface_activate(c->xwayland_surface, true);
        seat_focus_surface(server->seat, c->xwayland_surface->surface);
    }
}

struct wlr_output *
get_active_output (Client *c)
{
    double closest_x, closest_y;
    struct wlr_output *output = NULL;
    wlr_output_layout_closest_point(c->server->output_layout, output,
            c->x + c->width / 2,
            c->y + c->height / 2,
            &closest_x, &closest_y);
    return wlr_output_layout_output_at(c->server->output_layout, closest_x, closest_y);
}

static GdkRectangle
get_usable_area (Client *c)
{
    struct wlr_output *output = get_active_output(c);
    GdkRectangle usable_area = {0};
    wlr_output_effective_resolution(output, &usable_area.width, &usable_area.height);
    return usable_area;
}

static void
xdg_toplevel_map (struct wl_listener *listener, void *data)
{
    /* Called when the surface is mapped, or ready to display on-screen. */
    Client *c = wl_container_of(listener, c, map);
    if (c->xdg_toplevel->base->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;

    struct wlr_box geo_box = {0};
    GdkRectangle usable_area = get_usable_area(c);
    wlr_xdg_surface_get_geometry(c->xdg_toplevel->base, &geo_box);
    
        c->height = MIN(geo_box.height, usable_area.height);
        c->width = MIN(geo_box.width, usable_area.width);
        c->x = 0;
        c->y = 0;	

        wlr_xdg_toplevel_set_size(c->xdg_toplevel,
                c->width, c->height);
        focus_view(c, c->xdg_toplevel->base->surface);

    wlr_scene_node_set_position(&c->scene_tree->node,
            c->x, c->y);
}

static void
xdg_toplevel_unmap (struct wl_listener *listener, void *data)
{
    /* Called when the surface is unmapped, and should no longer be shown. */
    Client *c = wl_container_of(listener, c, unmap);
    if (c->xdg_toplevel->base->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;
    reset_cursor_mode(c->server);

}

static void
xdg_toplevel_destroy (struct wl_listener *listener, void *data)
{
    /* Called when the surface is destroyed and should never be shown again. */
    Client *c = wl_container_of(listener, c, destroy);

    wl_list_remove(&c->map.link);
    wl_list_remove(&c->unmap.link);
    wl_list_remove(&c->destroy.link);
    wl_list_remove(&c->new_popup.link);

    if (c->xdg_toplevel->base->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        wl_list_remove(&c->request_fullscreen.link);
        wl_list_remove(&c->request_minimize.link);
        wl_list_remove(&c->request_maximize.link);
        wl_list_remove(&c->request_move.link);
        wl_list_remove(&c->request_resize.link);		
    }
                                                                             
  clientUnframe (c, FALSE);
}

static void
xdg_toplevel_request_maximize (struct wl_listener *listener, void *data)
{
    /* This event is raised when a client would like to maximize itself,
     * typically because the user clicked on the maximize button on
     * client-side decorations.
     */
    Client *c = wl_container_of(listener, c, request_maximize);
    GdkRectangle usable_area = get_usable_area(c);

    bool is_maximized = c->xdg_toplevel->current.maximized;
    if (!is_maximized) {		
        c->saved_geometry.x = c->x;
        c->saved_geometry.y = c->y;
        c->saved_geometry.width = c->width;
        c->saved_geometry.height = c->height;
        
            c->x = 0;
            c->y = 0;		
    } else {
        usable_area = c->saved_geometry;
        c->x = c->saved_geometry.x;
        c->y = c->saved_geometry.y;
    }
    wlr_xdg_toplevel_set_size(c->xdg_toplevel, usable_area.width, usable_area.height);
    wlr_xdg_toplevel_set_maximized(c->xdg_toplevel, !is_maximized);
    wlr_scene_node_set_position(&c->scene_tree->node,
            c->x, c->y);
}

static void
xdg_toplevel_request_minimize (struct wl_listener *listener, void *data)
{
    Client *c = wl_container_of(listener, c, request_minimize);
    bool minimize_requested = c->xdg_toplevel->requested.minimized;
    if (minimize_requested) {
        c->saved_geometry.x = c->x;
        c->saved_geometry.y = c->y;
        c->saved_geometry.width = c->width;
        c->saved_geometry.height = c->height;
        c->y = -c->height;
    } else {
        c->x = c->saved_geometry.x;
        c->y = c->saved_geometry.y;
        c->width = c->saved_geometry.width;
        c->height = c->saved_geometry.height;
    }

    wlr_scene_node_set_position(&c->scene_tree->node,
            c->x, c->y);
}

static void
xdg_toplevel_request_fullscreen (struct wl_listener *listener, void *data)
{	
    Client *c =
        wl_container_of(listener, c, request_fullscreen);
    wlr_xdg_surface_schedule_configure(c->xdg_toplevel->base);
}

static void
begin_interactive (Client *c, enum CursorMode mode, uint32_t edges)
{
    /* This function sets up an interactive move or resize operation, where the
     * compositor stops propegating pointer events to clients and instead
     * consumes them itself, to move or resize windows. */
    xfwmWaylandCompositor *server = c->server;
    struct wlr_surface *focused_surface =
        server->seat->seat->pointer_state.focused_surface;
    if (focused_surface &&
        c->xdg_toplevel->base->surface != wlr_surface_get_root_surface(focused_surface)) {
        /* Deny move/resize requests from unfocused clients. */
        return;
    }
    server->grabbed_view = c;
    server->cursor->cursor_mode = mode;

    if (mode == XFWM_CURSOR_MOVE) {
        server->grab_x = server->cursor->cursor->x - c->x;
        server->grab_y = server->cursor->cursor->y - c->y;
    } else {
        struct wlr_box geo_box;
        wlr_xdg_surface_get_geometry(c->xdg_toplevel->base, &geo_box);

        double border_x = (c->x + geo_box.x) +
            ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
        double border_y = (c->y + geo_box.y) +
            ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
        server->grab_x = server->cursor->cursor->x - border_x;
        server->grab_y = server->cursor->cursor->y - border_y;

        server->grab_geobox = geo_box;
        server->grab_geobox.x += c->x;
        server->grab_geobox.y += c->y;

        server->resize_edges = edges;
    }
}

static void
xdg_toplevel_request_move(struct wl_listener *listener, void *data)
{
    /* This event is raised when a client would like to begin an interactive
     * move, typically because the user clicked on their client-side
     * decorations. Note that a more sophisticated compositor should check the
     * provided serial against a list of button press serials sent to this
     * client, to prevent the client from requesting this whenever they want. */
    Client *c = wl_container_of(listener, c, request_move);
    begin_interactive(c, XFWM_CURSOR_MOVE, 0);
}

static void
xdg_toplevel_request_resize (struct wl_listener *listener, void *data)
{
    /* This event is raised when a client would like to begin an interactive
     * resize, typically because the user clicked on their client-side
     * decorations. Note that a more sophisticated compositor should check the
     * provided serial against a list of button press serials sent to this
     * client, to prevent the client from requesting this whenever they want. */
    struct wlr_xdg_toplevel_resize_event *event = data;
    Client *c = wl_container_of(listener, c, request_resize);
    begin_interactive(c, XFWM_CURSOR_RESIZE, event->edges);
}

static void
handle_new_popup (struct wl_listener *listener, void *data)
{
    struct wlr_xdg_popup *popup = data;
    Client *c = wl_container_of(listener, c, new_popup);

    struct wlr_output *wlr_output = wlr_output_layout_output_at(
            c->server->output_layout,
            c->x + popup->current.geometry.x,
            c->y + popup->current.geometry.y);

    if (!wlr_output) return;
    Output *output = wlr_output->data;

    int top_margin = 0;
    struct wlr_box output_toplevel_box = {
        .x = output->geometry.x - c->x,
        .y = output->geometry.y - c->y,
        .width = output->geometry.width,
        .height = output->geometry.height - top_margin,
    };
    wlr_xdg_popup_unconstrain_from_box(popup, &output_toplevel_box);
}

Client *
clientFrameWayland (DisplayInfo *display_info,
                    struct wlr_xdg_surface *xdg_surface,                    
                    gboolean recapture)
{
    ScreenInfo *screen_info;
    XWindowAttributes attrs;
    Client *c = NULL;

    myDisplayGrabServer (display_info);
    myDisplayErrorTrapPush (display_info);

    if (!display_info)
    {
        goto out;
    }

    screen_info = myDisplayGetDefaultScreen (display_info);

    if (!screen_info)
    {
        goto out;
    }

    attrs.x = 0;
    attrs.y = 0;
    attrs.width = 0;
    attrs.height = 0;
    attrs.border_width = 0;
    attrs.depth = 24;
    attrs.visual = NULL;
    attrs.root = screen_info->xroot;
    attrs.class = InputOutput;
    attrs.bit_gravity = NorthWestGravity;
    attrs.win_gravity = NorthWestGravity;
    attrs.backing_store = 0;
    attrs.backing_planes = ~0;
    attrs.backing_pixel = 0;
    attrs.save_under = 0;
    attrs.colormap = 0;
    attrs.map_installed = 1;
    attrs.map_state = IsUnmapped;
    attrs.all_event_masks = ~0;
    attrs.your_event_mask = 0;
    attrs.do_not_propagate_mask = 0;
    attrs.override_redirect = 0;
    attrs.screen = screen_info->xscreen;

    c = _clientFrame (display_info,
                      screen_info,
                      XFWM_CLIENT_TYPE_WAYLAND,
                      xdg_surface,                      
                      None,
                      &attrs,
                      FALSE);

    out:
    /* Window is reparented now, so we can safely release the grab
     * on the server
     */
    myDisplayErrorTrapPopIgnored (display_info);
    myDisplayUngrabServer (display_info);

    return c;
}

static void
handle_new_xdg_surface (struct wl_listener *listener, void *data)
{
    /* This event is raised when wlr_xdg_shell receives a new xdg surface from a
     * client, either a toplevel (application window) or popup. */
    xfwmWaylandCompositor *server =
        wl_container_of(listener, server, new_xdg_surface);
    struct wlr_xdg_surface *xdg_surface = data;
    DisplayInfo *display_info = myDisplayGetDefault ();

    /* We must add xdg popups to the scene graph so they get rendered. The
     * wlroots scene graph provides a helper for this, but to use it we must
     * provide the proper parent scene node of the xdg popup. To enable this,
     * we always set the user data field of xdg_surfaces to the corresponding
     * scene node. */
    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP)
    {
        if (wlr_surface_is_xdg_surface (xdg_surface->popup->parent))
        {
            struct wlr_xdg_surface *parent = wlr_xdg_surface_from_wlr_surface(
                xdg_surface->popup->parent);
            struct wlr_scene_tree *parent_tree = parent->data;
            xdg_surface->data = wlr_scene_xdg_surface_create(
                parent_tree, xdg_surface);
        }
        return;
    }
    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_NONE)
    {
        return;
    }

    /* Allocate a Client for this surface */
    Client *c = clientFrameWayland (display_info, xdg_surface, FALSE);
    if (!c)
    {
        return;
    }
    c->server = server;

    /* Listen to the various events it can emit */
    c->map.notify = xdg_toplevel_map;
    wl_signal_add(&xdg_surface->events.map, &c->map);
    c->unmap.notify = xdg_toplevel_unmap;
    wl_signal_add(&xdg_surface->events.unmap, &c->unmap);
    c->destroy.notify = xdg_toplevel_destroy;
    wl_signal_add(&xdg_surface->events.destroy, &c->destroy);
    c->new_popup.notify = handle_new_popup;
    wl_signal_add(&xdg_surface->events.new_popup, &c->new_popup);

    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL)
    {
        c->scene_tree = wlr_scene_xdg_surface_create(
                &c->server->scene->tree, c->xdg_toplevel->base);
        c->scene_tree->node.data = c;
        xdg_surface->data = c->scene_tree;

        /* cotd */
        struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
        c->request_move.notify = xdg_toplevel_request_move;
        wl_signal_add(&toplevel->events.request_move, &c->request_move);
        c->request_resize.notify = xdg_toplevel_request_resize;
        wl_signal_add(&toplevel->events.request_resize, &c->request_resize);
        c->request_maximize.notify = xdg_toplevel_request_maximize;
        wl_signal_add(&toplevel->events.request_maximize,
            &c->request_maximize);
        c->request_minimize.notify = xdg_toplevel_request_minimize;
        wl_signal_add(&toplevel->events.request_minimize, &c->request_minimize);
        c->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
        wl_signal_add(&toplevel->events.request_fullscreen,
            &c->request_fullscreen);
    }
}

void
init_xdg_shell (xfwmWaylandCompositor *server)
{
    /* Set up xdg-shell version 3.*/
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display, 3);
	server->new_xdg_surface.notify = handle_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface,
			&server->new_xdg_surface);
}