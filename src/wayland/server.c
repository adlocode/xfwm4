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
wayland_event_source_prepare (GSource *base, int *timeout)
{
  WaylandEventSource *source = (WaylandEventSource *)base;

  *timeout = -1;

  wl_display_flush_clients (source->display);

  return FALSE;
}

static gboolean
wayland_event_source_dispatch (GSource *base,
                               GSourceFunc callback,
                               void *data)
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
  WaylandEventSource *source;
  struct wl_event_loop *loop = wl_display_get_event_loop (display);

  source = (WaylandEventSource *) g_source_new (&wayland_event_source_funcs,
                                                sizeof (WaylandEventSource));
  source->display = display;
  g_source_add_unix_fd (&source->source,
                        wl_event_loop_get_fd (loop),
                        G_IO_IN | G_IO_ERR);

  return &source->source;
}

void
xfwmWaylandInit (void)
{
    GSource *wayland_event_source;
    /* The Wayland display is managed by libwayland. It handles accepting
     * clients from the Unix socket, manging Wayland globals, and so on. */
  
  xfwmWaylandCompositor *compositor = &_xfwm_wayland_compositor;
  
  memset (compositor, 0, sizeof (xfwmWaylandCompositor));
  
    compositor->wl_display = wl_display_create();
    if (compositor->wl_display == NULL) {
        wlr_log(WLR_ERROR, "%s", _("Failed to connect to a Wayland display"));
        return false;
    }

    wayland_event_source = wayland_event_source_new (compositor->wl_display);

    g_source_set_priority (wayland_event_source, GDK_PRIORITY_EVENTS + 1);
    g_source_attach (wayland_event_source, NULL);

    /* The backend is a wlroots feature which abstracts the underlying input and
     * output hardware. The autocreate option will choose the most suitable
     * backend based on the current environment, such as opening an X11 window
     * if an X11 server is running. */
#if ! WLR_CHECK_VERSION(0, 13, 0) || WLR_CHECK_VERSION(0, 17, 0)
    compositor->backend = wlr_backend_autocreate(compositor->wl_display, NULL);
#else
    compositor->backend = wlr_backend_autocreate(compositor->wl_display);
#endif
    if (compositor->backend == NULL) {
        wlr_log(WLR_ERROR, "%s", _("Failed to create backend"));
        return false;
    }

    /* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
         * can also specify a renderer using the WLR_RENDERER env var.
         * The renderer is responsible for defining the various pixel formats it
         * supports for shared memory, this configures that for clients. */
    compositor->renderer = wlr_renderer_autocreate(compositor->backend);
    if (compositor->renderer == NULL) {
        wlr_log(WLR_ERROR, "%s", _("Failed to create renderer"));
        return false;
    }

    wlr_renderer_init_wl_display(compositor->renderer, compositor->wl_display);

        /* Autocreates an allocator for us.
         * The allocator is the bridge between the renderer and the backend. It
         * handles the buffer creation, allowing wlroots to render onto the
         * screen */
        compositor->allocator = wlr_allocator_autocreate(compositor->backend, compositor->renderer);
    if (compositor->allocator == NULL) {
        wlr_log(WLR_ERROR, "%s", _("Failed to create allocator"));
        return false;
    }

    compositor->compositor = wlr_compositor_create(compositor->wl_display,
            compositor->renderer);
    compositor->subcompositor = wlr_subcompositor_create(compositor->wl_display);
    compositor->output_layout = wlr_output_layout_create();
    compositor->seat = seatCreate(compositor);
    compositor->cursor = cursor_create(compositor);

    compositor->output_manager = wlr_xdg_output_manager_v1_create(compositor->wl_display,
                compositor->output_layout);

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

    const char *socket = wl_display_add_socket_auto(compositor->wl_display);
    if (!socket) {
        wlr_backend_destroy(compositor->backend);
        return false;
    }

    if (!wlr_backend_start(compositor->backend)) {
        wlr_log(WLR_ERROR, "%s", _("Failed to start backend"));
        wlr_backend_destroy(compositor->backend);
        wl_display_destroy(compositor->wl_display);
        return false;
    }

    wlr_log(WLR_INFO, "%s: WAYLAND_DISPLAY=%s", _("Running Wayland compositor on Wayland display"), socket);
    setenv("WAYLAND_DISPLAY", socket, true);

    wlr_gamma_control_manager_v1_create(compositor->wl_display);
    wlr_screencopy_manager_v1_create(compositor->wl_display);
    compositor->idle_notifier = wlr_idle_notifier_v1_create(compositor->wl_display);

    wlr_data_device_manager_create(compositor->wl_display);
    wl_list_init(&compositor->views);
    init_xdg_decoration(compositor);
    init_layer_shell(compositor);

    /* Set up the xdg-shell. The xdg-shell is a Wayland protocol which is used
     * for application windows. For more detail on shells, refer to Drew
     * DeVault's article:
     *
     * https://drewdevault.com/2018/07/29/Wayland-shells.html
     */
  
    init_xdg_shell(compositor);
    
    init_xwayland (compositor);	

    return true;
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
