/* Copyright (C) 2018 adlo
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>
 */

#define _GNU_SOURCE
#include <wayland-server.h>
#include <libweston-4/compositor.h>
#include <compositor-drm.h>
#include <compositor-wayland.h>
#include <libweston-desktop.h>
#include <libinput.h>
#include <string.h>
#include <windowed-output-api.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include "server.h"
#include "shell.h"



static int vlog (const char *fmt,
                 va_list     ap)
{
  return vfprintf (stderr, fmt, ap);
}

static int vlog_continue (const char *fmt,
                          va_list     argp)
{
  return vfprintf (stderr, fmt, argp);
}

static void new_output_notify_drm (struct wl_listener *listener,
                            void               *data)
{
  struct weston_output *output = data;
  ScreenInfoWayland *server = wl_container_of (listener, server, new_output);

  server->api.drm->set_mode (output, WESTON_DRM_BACKEND_OUTPUT_PREFERRED, NULL);
  server->api.drm->set_gbm_format (output, NULL);
  server->api.drm->set_seat (output, NULL);
  weston_output_set_scale (output, 1);
  weston_output_set_transform (output, WL_OUTPUT_TRANSFORM_NORMAL);
  weston_output_enable (output);

  weston_layer_init (&server->background_layer, server->compositor);
  weston_layer_set_position (&server->background_layer, WESTON_LAYER_POSITION_BACKGROUND);
  server->background = weston_surface_create (server->compositor);
  weston_surface_set_size (server->background, output->width, output->height);
  weston_surface_set_color (server->background, 0, 0.25, 0.5, 1);
  server->background_view = weston_view_create (server->background);
  weston_layer_entry_insert (&server->background_layer.view_list, &server->background_view->layer_link);
}

static void new_output_notify_wayland (struct wl_listener *listener,
                            void               *data)
{
  struct weston_output *output = data;
  ScreenInfoWayland *server = wl_container_of (listener, server, new_output);

  weston_output_set_scale (output, 1);
  weston_output_set_transform (output, WL_OUTPUT_TRANSFORM_NORMAL);
  server->api.windowed->output_set_size (output, 800, 600);
  weston_output_enable (output);

  weston_layer_init (&server->background_layer, server->compositor);
  weston_layer_set_position (&server->background_layer, WESTON_LAYER_POSITION_BACKGROUND);
  server->background = weston_surface_create (server->compositor);
  weston_surface_set_size (server->background, output->width, output->height);
  weston_surface_set_color (server->background, 0, 0.25, 0.5, 1);
  server->background_view = weston_view_create (server->background);
  weston_layer_entry_insert (&server->background_layer.view_list, &server->background_view->layer_link);

}

static int load_drm_backend (ScreenInfoWayland *server)
{
  struct weston_drm_backend_config config = {{ 0, }};
  int ret = 0;

  config.base.struct_version = WESTON_DRM_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof (struct weston_drm_backend_config);

  ret = weston_compositor_load_backend (server->compositor, WESTON_BACKEND_DRM, &config.base);

  server->api.drm = weston_drm_output_get_api (server->compositor);
  if (server->api.drm == NULL)
    return 1;
  server->new_output.notify = new_output_notify_drm;

  return ret;
}

static int load_wayland_backend (ScreenInfoWayland *server)
{

  struct weston_wayland_backend_config config = {{ 0, }};
  int ret = 0;

	config.base.struct_version = WESTON_WAYLAND_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof (struct weston_wayland_backend_config);

	config.cursor_size = 32;
	config.display_name = 0;
	config.use_pixman = 0;
	config.sprawl = 0;
	config.fullscreen = 0;
	config.cursor_theme = NULL;


	ret = weston_compositor_load_backend (server->compositor, WESTON_BACKEND_WAYLAND, &config.base);

  server->api.windowed = weston_windowed_output_get_api (server->compositor);
  server->new_output.notify = new_output_notify_wayland;

  server->api.windowed->output_create (server->compositor, "W1");

  return ret;
}

int main (int    argc,
          char **argv)
{
	struct wl_display *display;
	struct weston_compositor *ec = NULL;
	int ret = 0;
  const char *socket_name = NULL;
  ScreenInfoWayland *server;
  struct weston_output *output;

  server = malloc (sizeof(ScreenInfoWayland));

	display = wl_display_create ();
	server->compositor = weston_compositor_create (display, NULL);
  weston_compositor_set_xkb_rule_names (server->compositor, NULL);

	if (!server->compositor)
		return 0;

  weston_log_set_handler (vlog, vlog_continue);

	server->compositor->default_pointer_grab = NULL;
	server->compositor->vt_switching = true;

	server->compositor->repaint_msec = 16;
	server->compositor->idle_time = 300;

  server->compositor->kb_repeat_rate = 40;
  server->compositor->kb_repeat_delay = 400;

  enum weston_compositor_backend backend = WESTON_BACKEND_DRM;
  if (getenv("WAYLAND_DISPLAY") || getenv("WAYLAND_SOCKET"))
		backend = WESTON_BACKEND_WAYLAND;

  switch (backend)
    {
    case WESTON_BACKEND_DRM:
      ret = load_drm_backend (server);
      if (ret != 0)
        return ret;
      break;
    case WESTON_BACKEND_WAYLAND:
    ret = load_wayland_backend (server);
      if (ret != 0)
        return ret;
      break;
    default:
      return 1;
    }

  wl_signal_add (&server->compositor->output_pending_signal, &server->new_output);
  weston_pending_output_coldplug (server->compositor);



  xfway_server_shell_init (server);

  socket_name = wl_display_add_socket_auto (display);
  if (socket_name)
  {
    weston_log ("Compositor running on %s", socket_name);
    setenv ("WAYLAND_DISPLAY", socket_name, 1);
    unsetenv ("DISPLAY");
  }

  weston_compositor_wake (server->compositor);
  wl_display_run (display);

  weston_compositor_destroy (server->compositor);

	return 0;
}
