#ifndef _XFWM_SEAT_H
#define _XFWM_SEAT_H

#include <wlr/types/wlr_seat.h>

struct _xfwmServer;
typedef struct _xfwmServer xfwmServer;

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
	xfwmServer *server;
	struct wlr_keyboard *keyboard;

	struct wl_listener destroy;
	struct wl_listener modifiers;
	struct wl_listener key;
};

typedef struct _Keyboard Keyboard;

Seat *seatCreate (xfwmServer *server);
void seat_focus_surface(Seat *seat, struct wlr_surface *surface);
void seat_set_focus_layer(Seat *seat, struct wlr_layer_surface_v1 *layer);
void seatDestroy (Seat *seat);
#endif
