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

#ifndef _XFWM_SERVER_H
#define _XFWM_SERVER_H

#define MAX(a, b) ((a > b) ? (a) : (b))
#define MIN(a, b) ((a < b) ? (a) : (b))
#define TITLEBAR_HEIGHT 8 /* TODO: Get this from the theme */
#include <wlr/version.h>
#define WLR_CHECK_VERSION(major, minor, micro) (WLR_VERSION_NUM >= ((major << 16) | (minor << 8) | (micro)))

#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/xwayland.h>
#include <wlr/util/log.h>

#include <glib.h>
#include <gtk/gtk.h>

#ifdef USE_NLS
#	include <libintl.h>
#	include <locale.h>
#	define _ gettext
#else
#	define _(s) (s)
#endif

#include "cursor.h"
#include "decoration.h"
#include "layer_shell.h"
#include "output.h"
#include "seat.h"

struct _View;
typedef struct _View View;

typedef struct _Client Client;

struct _xfwmWaylandCompositor
{
    struct wl_display *wl_display;

    struct wlr_allocator *allocator;
    struct wlr_backend *backend;
    struct wlr_compositor *compositor;
    struct wlr_idle_notifier_v1 *idle_notifier;
    struct wlr_output_layout *output_layout;
    struct wlr_xdg_output_manager_v1 *output_manager;
    struct wlr_renderer *renderer;
    struct wlr_scene *scene;
    struct wlr_subcompositor *subcompositor;

    xfwmCursor *cursor;
    Seat *seat;

    Client *grabbed_view;
    struct wlr_box grab_geo_box;
    double grab_x, grab_y;
    uint32_t resize_edges;
    struct wl_list views;

    struct wlr_layer_shell_v1 *layer_shell;
    struct wlr_xdg_shell *xdg_shell;
    struct wlr_xwayland *xwayland;

    struct wl_listener new_layer_surface;
    struct wl_listener new_xdg_surface;
    struct wl_listener new_xdg_decoration;
    
    struct wl_listener new_xwayland_surface;
    struct wl_listener xwayland_ready;

    struct wl_listener new_input;
    struct wl_listener new_output;
    struct wl_list outputs; /* output::link */
};

typedef struct _xfwmWaylandCompositor xfwmWaylandCompositor;

void xfwmWaylandInit (void);
xfwmWaylandCompositor * xfwmWaylandGetDefault (void);
gboolean terminate (xfwmWaylandCompositor *server);

#endif /* server.h */
