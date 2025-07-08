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

#ifndef _XFWM_SEAT_H
#define _XFWM_SEAT_H

#include <wlr/types/wlr_seat.h>

struct _xfwmWaylandCompositor;
typedef struct _xfwmWaylandCompositor xfwmWaylandCompositor;

struct _Seat {
	struct wlr_seat *seat;

	struct wlr_layer_surface_v1 *focused_layer;

	struct wl_list keyboards;

	struct wl_listener request_set_primary_selection;
	struct wl_listener request_set_selection;
};

typedef struct _Seat Seat;

struct _Keyboard {
	struct wl_list link;
	xfwmWaylandCompositor *server;
	struct wlr_keyboard *keyboard;

	struct wl_listener destroy;
	struct wl_listener modifiers;
	struct wl_listener key;
};

typedef struct _Keyboard Keyboard;

Seat *seatCreate (xfwmWaylandCompositor *server);
void seat_focus_surface(Seat *seat, struct wlr_surface *surface);
void seat_set_focus_layer(Seat *seat, struct wlr_layer_surface_v1 *layer);
void seatDestroy (Seat *seat);
#endif
