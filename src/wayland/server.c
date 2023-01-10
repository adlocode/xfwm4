/* Copyright (c) 2018 - 2023 adlo
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

gboolean
serverInit (xfwmServer* server)
{
	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	server->wl_display = wl_display_create();
	if (server->wl_display == NULL) {
		wlr_log(WLR_ERROR, "%s", _("Failed to connect to a Wayland display"));
		return false;
	}

	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. */
#if ! WLR_CHECK_VERSION(0, 13, 0) || WLR_CHECK_VERSION(0, 17, 0)
	server->backend = wlr_backend_autocreate(server->wl_display, NULL);
#else
	server->backend = wlr_backend_autocreate(server->wl_display);
#endif
	if (server->backend == NULL) {
		wlr_log(WLR_ERROR, "%s", _("Failed to create backend"));
		return false;
	}

	/* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
         * can also specify a renderer using the WLR_RENDERER env var.
         * The renderer is responsible for defining the various pixel formats it
         * supports for shared memory, this configures that for clients. */
	server->renderer = wlr_renderer_autocreate(server->backend);
	if (server->renderer == NULL) {
		wlr_log(WLR_ERROR, "%s", _("Failed to create renderer"));
		return false;
	}

	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

        /* Autocreates an allocator for us.
         * The allocator is the bridge between the renderer and the backend. It
         * handles the buffer creation, allowing wlroots to render onto the
         * screen */
        server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
	if (server->allocator == NULL) {
		wlr_log(WLR_ERROR, "%s", _("Failed to create allocator"));
		return false;
	}

	server->compositor = wlr_compositor_create(server->wl_display,
			server->renderer);
	server->subcompositor = wlr_subcompositor_create(server->wl_display);
	server->output_layout = wlr_output_layout_create();
	server->seat = seatCreate(server);
	server->cursor = cursor_create(server);

	server->output_manager = wlr_xdg_output_manager_v1_create(server->wl_display,
				server->output_layout);

	wl_list_init(&server->outputs);

	server->new_output.notify = new_output_notify;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	/* Create a scene graph. This is a wlroots abstraction that handles all
	 * rendering and damage tracking. All the compositor author needs to do
	 * is add things that should be rendered to the scene graph at the proper
	 * positions and then call wlr_scene_output_commit() to render a frame if
	 * necessary.
	 */
	server->scene = wlr_scene_create();
	wlr_scene_attach_output_layout(server->scene, server->output_layout);

	const char *socket = wl_display_add_socket_auto(server->wl_display);
	if (!socket) {
		wlr_backend_destroy(server->backend);
		return false;
	}

	if (!wlr_backend_start(server->backend)) {
		wlr_log(WLR_ERROR, "%s", _("Failed to start backend"));
		wlr_backend_destroy(server->backend);
		wl_display_destroy(server->wl_display);
		return false;
	}

	wlr_log(WLR_INFO, "%s: WAYLAND_DISPLAY=%s", _("Running Wayland compositor on Wayland display"), socket);
	setenv("WAYLAND_DISPLAY", socket, true);

	wlr_gamma_control_manager_v1_create(server->wl_display);
	wlr_screencopy_manager_v1_create(server->wl_display);
	server->idle_notifier = wlr_idle_notifier_v1_create(server->wl_display);

	wlr_data_device_manager_create(server->wl_display);
	wl_list_init(&server->views);
	init_xdg_decoration(server);
	init_layer_shell(server);

	/* Set up the xdg-shell. The xdg-shell is a Wayland protocol which is used
	 * for application windows. For more detail on shells, refer to Drew
	 * DeVault's article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	init_xdg_shell(server);
	
	init_xwayland (server);

	return true;
}

gboolean
terminate (xfwmServer* server)
{
	cursor_destroy(server->cursor);
	wl_list_remove(&server->new_xdg_decoration.link); /* wb_decoration_destroy */	
	wl_display_destroy_clients(server->wl_display);
	wl_display_destroy(server->wl_display);
	seatDestroy(server->seat);
	wlr_output_layout_destroy(server->output_layout);

	wlr_log(WLR_INFO, "%s", _("Display destroyed"));

	return true;
}
