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

#include "server.h"
#include "xdg_shell.h"
#include "xwayland.h"

static xfwmWaylandCompositor _xfwm_wayland_compositor;

typedef struct
{
  GSource source;
  struct wl_display *display;
} WaylandEventSource;

static gboolean
wayland_event_source_prepare (GSource *base,
                              int     *timeout)
{
  WaylandEventSource *source = (WaylandEventSource *)base;

  *timeout = -1;

  wl_display_flush_clients (source->display);

  return FALSE;
}

static gboolean
wayland_event_source_dispatch (GSource    *base,
                               GSourceFunc callback,
                               void       *data)
{
  WaylandEventSource *source = (WaylandEventSource *)base;
  struct wl_event_loop *loop = wl_display_get_event_loop (source->display);

  wl_event_loop_dispatch (loop, 0);

  return TRUE;
}

static GSourceFuncs wayland_event_source_funcs =
{
  wayland_event_source_prepare,
  NULL,
  wayland_event_source_dispatch,
  NULL
};

static GSource *
wayland_event_source_new (struct wl_display *display)
{
  GSource *source;
  WaylandEventSource *wayland_source;
  struct wl_event_loop *loop = wl_display_get_event_loop (display);

  source = g_source_new (&wayland_event_source_funcs,
                         sizeof (WaylandEventSource));
  wayland_source = (WaylandEventSource *) source;
  wayland_source->display = display;
  g_source_add_unix_fd (&wayland_source->source,
                        wl_event_loop_get_fd (loop),
                        G_IO_IN | G_IO_ERR);

  return &wayland_source->source;
}

void
xfwmWaylandInit (void)
{
    GSource *wayland_event_source;
    xfwmWaylandCompositor *compositor = &_xfwm_wayland_compositor;

    memset (compositor, 0, sizeof (xfwmWaylandCompositor));

    /* The Wayland display is managed by libwayland. It handles accepting
     * clients from the Unix socket, manging Wayland globals, and so on. */
    compositor->wl_display = wl_display_create();

    wayland_event_source = wayland_event_source_new (compositor->wl_display);

    g_source_set_priority (wayland_event_source, GDK_PRIORITY_EVENTS + 1);
    g_source_attach (wayland_event_source, NULL);

    /* The backend is a wlroots feature which abstracts the underlying input and
     * output hardware. The autocreate option will choose the most suitable
     * backend based on the current environment, such as opening an X11 window
     * if an X11 server is running. */
    compositor->backend = wlr_backend_autocreate(compositor->wl_display);
    if (compositor->backend == NULL)
    {
        wlr_log(WLR_ERROR, "failed to create wlr_backend");
        return;
    }
    /* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
     * can also specify a renderer using the WLR_RENDERER env var.
     * The renderer is responsible for defining the various pixel formats it
     * supports for shared memory, this configures that for clients. */
    compositor->renderer = wlr_renderer_autocreate(compositor->backend);
    if (compositor->renderer == NULL)
    {
        wlr_log(WLR_ERROR, "failed to create wlr_renderer");
        return;
    }

    wlr_renderer_init_wl_display(compositor->renderer, compositor->wl_display);

    /* Autocreates an allocator for us.
     * The allocator is the bridge between the renderer and the backend. It
     * handles the buffer creation, allowing wlroots to render onto the
     * screen */
     compositor->allocator = wlr_allocator_autocreate(compositor->backend,
        compositor->renderer);
    if (compositor->allocator == NULL)
    {
        wlr_log(WLR_ERROR, "failed to create wlr_allocator");
        return;
    }

    /* This creates some hands-off wlroots interfaces. The compositor is
     * necessary for clients to allocate surfaces, the subcompositor allows to
     * assign the role of subsurfaces to surfaces and the data device manager
     * handles the clipboard. Each of these wlroots interfaces has room for you
     * to dig your fingers in and play with their behavior if you want. Note that
     * the clients cannot set the selection directly without compositor approval,
     * see the handling of the request_set_selection event below.*/
    compositor->compositor = wlr_compositor_create(compositor->wl_display, compositor->renderer);
    compositor->subcompositor = wlr_subcompositor_create(compositor->wl_display);
    wlr_data_device_manager_create(compositor->wl_display);

    /* Creates an output layout, which a wlroots utility for working with an
     * arrangement of screens in a physical layout. */
    compositor->output_layout = wlr_output_layout_create();

    /* Configure a listener to be notified when new outputs are available on the
     * backend. */
    wl_list_init(&compositor->outputs);
    compositor->new_output.notify = new_output_notify;
    wl_signal_add(&compositor->backend->events.new_output, &compositor->new_output);

    /* Create a scene graph. This is a wlroots abstraction that handles all
     * rendering and damage tracking. All the compositor author needs to do
     * is add things that should be rendered to the scene graph at the proper
     * positions and then call wlr_scene_output_commit() to render a frame if
     * necessary.
     */
    compositor->scene = wlr_scene_create();
    wlr_scene_attach_output_layout(compositor->scene, compositor->output_layout);

    compositor->idle_notifier = wlr_idle_notifier_v1_create(compositor->wl_display);

    /* Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
     * used for application windows. For more detail on shells, refer to Drew
     * DeVault's article:
     *
     * https://drewdevault.com/2018/07/29/Wayland-shells.html
     */
    init_xdg_shell (compositor);

    init_xwayland (compositor);

    init_xdg_decoration (compositor);
    init_layer_shell (compositor);    

    /*
     * Configures a seat, which is a single "seat" at which a user sits and
     * operates the computer. This conceptually includes up to one keyboard,
     * pointer, touch, and drawing tablet device. We also rig up a listener to
     * let us know when new input devices are available on the backend.
     */
    compositor->seat = seatCreate (compositor);

    /*
     * Creates a cursor, which is a wlroots utility for tracking the cursor
     * image shown on screen.
     */
    compositor->cursor = cursor_create (compositor);

    /* Add a Unix socket to the Wayland display. */
    const char *socket = wl_display_add_socket_auto(compositor->wl_display);
    if (!socket)
    {
        wlr_backend_destroy(compositor->backend);
        return;
    }

    /* Start the backend. This will enumerate outputs and inputs, become the DRM
     * master, etc */
    if (!wlr_backend_start(compositor->backend))
    {
        wlr_backend_destroy(compositor->backend);
        wl_display_destroy(compositor->wl_display);
        return;
    }

    /* Set the WAYLAND_DISPLAY environment variable to our socket */
    setenv("WAYLAND_DISPLAY", socket, true);
    
    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
            socket);
}

xfwmWaylandCompositor *
xfwmWaylandGetDefault (void)
{
    return &_xfwm_wayland_compositor;
}

gboolean
terminate (xfwmWaylandCompositor* compositor)
{
    cursor_destroy(compositor->cursor);
    wl_list_remove(&compositor->new_xdg_decoration.link); /* decoration destroy */	
    wl_display_destroy_clients(compositor->wl_display);
    wl_display_destroy(compositor->wl_display);
    seatDestroy(compositor->seat);
    wlr_output_layout_destroy(compositor->output_layout);

    wlr_log(WLR_INFO, "%s", _("Display destroyed"));

    return true;
}