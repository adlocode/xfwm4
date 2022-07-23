/*
 * Copyright (C) 2018 - 2022 adlo
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>
 */

/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
//#ifndef WLR_USE_UNSTABLE
//#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
//#endif

#ifndef XFWM_TYPES_XFWM_SHELL
#define XFWM_TYPES_XFWM_SHELL

#include <wayland-server.h>
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
#include <protocol/xfway-shell-protocol.h>
//#include <wlr/types/wlr_output.h>

struct xfwm_shell {
	struct wl_event_loop *event_loop;
	struct wl_global *global;
	struct wl_list resources;
	struct wl_list toplevels; // xfwm_shell_window::link

	struct wl_listener display_destroy;

	struct {
		struct wl_signal destroy;
	} events;

	void *data;
  
  struct wl_display *display;
  
  struct {
		struct wl_client *client;
		struct wl_resource *desktop_shell;
		struct wl_listener client_destroy_listener;

		unsigned deathcount;
		struct timespec deathstamp;
	} child;
};

/*struct xfwm_shell_window_output {
	struct wl_list link; // xfwm_shell_window::outputs
	struct wl_listener output_destroy;
	struct wlr_output *output;

	struct xfwm_shell_window *toplevel;
};*/

struct xfwm_shell_window {
	struct xfwm_shell *manager;
	struct wl_list resources;
	struct wl_list link;
	struct wl_event_source *idle_source;

	char *title;
	char *app_id;
	struct wl_list outputs; // wlr_foreign_toplevel_v1_output
	uint32_t state; // wlr_foreign_toplevel_v1_state

	struct {
		// xfwm_shell_window_maximized_event
		struct wl_signal request_maximize;
		//xfwm_shell_window_minimized_event
		struct wl_signal request_minimize;
		//xfwm_shell_window_activated_event
		struct wl_signal request_activate;
		//xfwm_shell_window_fullscreen_event
		struct wl_signal request_fullscreen;
		struct wl_signal request_close;

		//xfwm_shell_window_set_rectangle_event
		struct wl_signal set_rectangle;
		struct wl_signal destroy;
    
    struct wl_signal shell_request_focus;
    struct wl_signal shell_request_raise;
	} events;

	void *data;
};

enum xfwm_shell_window_state {
	XFWM_SHELL_WINDOW_STATE_MAXIMIZED = (1 << 0),
	XFWM_SHELL_WINDOW_STATE_MINIMIZED = (1 << 1),
	XFWM_SHELL_WINDOW_STATE_ACTIVATED = (1 << 2),
	XFWM_SHELL_WINDOW_STATE_FULLSCREEN = (1 << 3),
};

struct xfwm_shell_window_maximized_event {
	struct xfwm_shell_window *toplevel;
	bool maximized;
};

struct xfwm_shell_window_minimized_event {
	struct xfwm_shell_window *toplevel;
	bool minimized;
};

struct xfwm_shell_window_activated_event {
	struct xfwm_shell_window *toplevel;
	struct wlr_seat *seat;
};

/*struct xfwm_shell_window_fullscreen_event {
	struct xfwm_shell_window *toplevel;
	bool fullscreen;
	struct wlr_output *output;
};*/

struct xfwm_shell_window_set_rectangle_event {
	struct xfwm_shell_window *toplevel;
	struct wlr_surface *surface;
	int32_t x, y, width, height;
};

struct xfwm_shell_window_focus_event {
	struct xfwm_shell_window *toplevel;
	struct wlr_seat *seat;
};

struct xfwm_shell_window_raise_event {
	struct xfwm_shell_window *toplevel;
	struct wlr_seat *seat;
};

struct xfwm_shell *xfwm_shell_create(
	struct wl_display *display);
void xfwm_shell_destroy(
	struct xfwm_shell *manager);

struct xfwm_shell_window *xfwm_shell_window_create(
	struct xfwm_shell *manager);
void xfwm_shell_window_destroy(
	struct xfwm_shell_window *toplevel);

void xfwm_shell_window_set_title(
	struct xfwm_shell_window *toplevel, const char *title);
void xfwm_shell_window_set_app_id(
	struct xfwm_shell_window *toplevel, const char *app_id);

//void xfwm_shell_window_output_enter(
	//struct xfwm_shell_window *toplevel, struct wlr_output *output);
//void xfwm_shell_window_output_leave(
	//struct xfwm_shell_window *toplevel, struct wlr_output *output);

void xfwm_shell_window_set_maximized(
	struct xfwm_shell_window *toplevel, bool maximized);
void xfwm_shell_window_set_minimized(
	struct xfwm_shell_window *toplevel, bool minimized);
void xfwm_shell_window_set_activated(
	struct xfwm_shell_window *toplevel, bool activated);
void xfwm_shell_window_set_fullscreen(
	struct xfwm_shell_window* toplevel, bool fullscreen);

#endif
