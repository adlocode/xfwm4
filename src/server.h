/* Copyright (C) 2018 - 2019 adlo
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
#ifndef XFWAY_SERVER_H
#define XFWAY_SERVER_H

#define _GNU_SOURCE
#include <wayland-server.h>
#include <wayland-util.h>
#include <libweston/libweston.h>
#include <libweston/backend-drm.h>
#include <libweston/backend-wayland.h>
#include <libweston-desktop/libweston-desktop.h>
#include <libinput.h>
#include <string.h>
#include <libweston/windowed-output-api.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>
#include <protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.h>
#include "wlr_foreign_toplevel_management_v1.h"

struct weston_window_switcher;

struct _xfwmDisplay
{
  struct weston_compositor *compositor;
  struct wl_listener heads_changed_listener;
  bool init_failed;
	struct wl_list layoutput_list;	/**< wet_layoutput::compositor_link */
  union
    {
      const struct weston_drm_output_api *drm;
      const struct weston_windowed_output_api *windowed;
    } api;
  bool is_windowed;

  XfconfChannel *channel;

  struct weston_layer black_background_layer;
  struct weston_layer background_layer;
  struct weston_surface *background;
  struct weston_view *background_view;
  struct weston_layer bottom_layer;
  struct weston_layer surfaces_layer;
  struct weston_layer top_layer;
  struct weston_layer overlay_layer;

  struct wl_list outputs;

  int (*simple_output_configure)(struct weston_output *output);

  GdkDisplay *gdisplay;

};

typedef struct _xfwmDisplay xfwmDisplay;

#endif
