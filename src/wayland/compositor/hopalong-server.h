/*
 * Hopalong - a friendly Wayland compositor
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef HOPALONG_COMPOSITOR_SERVER_H
#define HOPALONG_COMPOSITOR_SERVER_H

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/xwayland.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "hopalong-macros.h"
#include "hopalong-view.h"
#include "hopalong-xwayland.h"
#include "hopalong-style.h"
#include "hopalong-layer-shell.h"

#include "xfwm-shell.h"

enum hopalong_cursor_mode {
	HOPALONG_CURSOR_PASSTHROUGH,
	HOPALONG_CURSOR_MOVE,
	HOPALONG_CURSOR_RESIZE
};

struct hopalong_server {
	struct wl_display *display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
 	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;

	struct wl_list views;
	struct wl_list mapped_layers[HOPALONG_LAYER_COUNT];

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;

	enum hopalong_cursor_mode cursor_mode;
	struct hopalong_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;

	struct wlr_xdg_decoration_manager_v1 *xdg_deco_mgr;
	struct wl_listener new_toplevel_decoration;

	struct wlr_server_decoration_manager *wlr_deco_mgr;

	struct wlr_xwayland *wlr_xwayland;
	struct wl_listener new_xwayland_surface;

	const struct hopalong_style *style;

	struct wlr_xdg_output_manager_v1 *xdg_output_manager;
	struct wlr_layer_shell_v1 *wlr_layer_shell;
	struct wl_listener new_layer_surface;

	struct wl_list keybindings;  
  
  struct xfwm_shell *shell;
};

struct hopalong_server_options {
	const char *style_name;
};

extern struct hopalong_server *hopalong_server_new(const struct hopalong_server_options *options);
extern bool hopalong_server_run(struct hopalong_server *server);
extern void hopalong_server_destroy(struct hopalong_server *server);
extern const char *hopalong_server_add_socket(struct hopalong_server *server);

#endif
