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

#include "hopalong-cursor.h"
#include "hopalong-server.h"
#include "hopalong-shell.h"

static const char * cursor_images[HOPALONG_VIEW_FRAME_AREA_COUNT] = {
	[HOPALONG_VIEW_FRAME_AREA_TOP]		= "top_side",
	[HOPALONG_VIEW_FRAME_AREA_BOTTOM]	= "bottom_side",
	[HOPALONG_VIEW_FRAME_AREA_LEFT]		= "left_side",
	[HOPALONG_VIEW_FRAME_AREA_RIGHT]	= "right_side",
	[HOPALONG_VIEW_FRAME_AREA_TITLEBAR]	= "left_ptr",
	[HOPALONG_VIEW_FRAME_AREA_MINIMIZE]	= "left_ptr",
	[HOPALONG_VIEW_FRAME_AREA_MAXIMIZE]	= "left_ptr",
	[HOPALONG_VIEW_FRAME_AREA_CLOSE]	= "left_ptr",
};

static void
process_cursor_move(struct hopalong_server *server, uint32_t time)
{
	server->grabbed_view->x = server->cursor->x - server->grab_x;
	server->grabbed_view->y = server->cursor->y - server->grab_y;

	wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "grabbing", server->cursor);
}

static void
process_cursor_resize(struct hopalong_server *server, uint32_t time)
{
	struct hopalong_view *view = server->grabbed_view;

	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP)
	{
		new_top = border_y;

		if (new_top >= new_bottom)
			new_top = new_bottom - 1;
	}
	else if (server->resize_edges & WLR_EDGE_BOTTOM)
	{
		new_bottom = border_y;

		if (new_bottom <= new_top)
			new_bottom = new_top + 1;
	}

	if (server->resize_edges & WLR_EDGE_LEFT)
	{
		new_left = border_x;

		if (new_left >= new_right)
			new_left = new_right - 1;
	}
	else if (server->resize_edges & WLR_EDGE_RIGHT)
	{
		new_right = border_x;

		if (new_right <= new_left)
			new_right = new_left + 1;
        }

	struct wlr_box geo_box;
	hopalong_view_get_geometry(view, &geo_box);
	view->x = new_left - geo_box.x;
	view->y = new_top - geo_box.y;

	int new_width = new_right - new_left;
	int new_height = new_bottom - new_top;
	hopalong_view_set_size(view, new_width, new_height);
}

static void
process_cursor_motion(struct hopalong_server *server, uint32_t time)
{
        if (server->cursor_mode == HOPALONG_CURSOR_MOVE)
	{
		process_cursor_move(server, time);
		return;
	}
	else if (server->cursor_mode == HOPALONG_CURSOR_RESIZE)
	{
		process_cursor_resize(server, time);
		return;
	}

	/* Otherwise, find the view under the pointer and send the event along. */
	double sx, sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	struct hopalong_view *view = hopalong_shell_desktop_view_at(server,
		server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (view == NULL || view->frame_area == -1 || surface != NULL)
		wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr", server->cursor);
	else if (view->frame_area != -1 && view->frame_area < HOPALONG_VIEW_FRAME_AREA_COUNT)
		wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, cursor_images[view->frame_area], server->cursor);

	if (surface != NULL)
	{
		bool focus_changed = seat->pointer_state.focused_surface != surface;

		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);

		if (!focus_changed)
			wlr_seat_pointer_notify_motion(seat, time, sx, sy);
		else
		{
			view->frame_area = -1;
			view->frame_area_edges = WLR_EDGE_NONE;
		}
	}
	else
		wlr_seat_pointer_clear_focus(seat);
}

static void
cursor_motion(struct wl_listener *listener, void *data)
{
	struct hopalong_server *server = wl_container_of(listener, server, cursor_motion);
	struct wlr_event_pointer_motion *event = data;

	wlr_cursor_move(server->cursor, event->device, event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

static void
cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	struct hopalong_server *server = wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;

	wlr_cursor_warp_absolute(server->cursor, event->device, event->x, event->y);
	process_cursor_motion(server, event->time_msec);
}

static void
cursor_button(struct wl_listener *listener, void *data)
{
	struct hopalong_server *server = wl_container_of(listener, server, cursor_button);
	struct wlr_event_pointer_button *event = data;

	wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);

	double sx, sy;
	struct wlr_surface *surface;
	struct hopalong_view *view = hopalong_shell_desktop_view_at(server, server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (event->state == WLR_BUTTON_RELEASED)
	{
		server->cursor_mode = HOPALONG_CURSOR_PASSTHROUGH;

		wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr", server->cursor);

		server->resize_edges = WLR_EDGE_NONE;
	}
	else if (view != NULL)
	{
		int frame_area = view->frame_area;
		int frame_area_edges = view->frame_area_edges;

		hopalong_view_focus(view, surface);

		if (frame_area == HOPALONG_VIEW_FRAME_AREA_TITLEBAR && hopalong_view_can_move(view))
		{
			server->cursor_mode = HOPALONG_CURSOR_MOVE;
			server->grabbed_view = view;
			server->grab_x = server->cursor->x - view->x;
			server->grab_y = server->cursor->y - view->y;

			hopalong_view_get_geometry(view, &server->grab_geobox);
			server->grab_geobox.x = view->x;
			server->grab_geobox.y = view->y;

			return;
		}
		else if (frame_area == HOPALONG_VIEW_FRAME_AREA_CLOSE)
		{
			hopalong_view_close(view);
			return;
		}
		else if (frame_area == HOPALONG_VIEW_FRAME_AREA_MAXIMIZE)
		{
			hopalong_view_maximize(view);
			return;
		}
		else if (frame_area == HOPALONG_VIEW_FRAME_AREA_MINIMIZE)
		{
			hopalong_view_minimize(view);
			return;
		}

		if (frame_area_edges != WLR_EDGE_NONE && hopalong_view_can_resize(view))
		{
			server->cursor_mode = HOPALONG_CURSOR_RESIZE;

			server->grabbed_view = view;
			server->resize_edges = frame_area_edges;

			server->grab_x = 0;
			server->grab_y = 0;

			hopalong_view_get_geometry(view, &server->grab_geobox);
			server->grab_geobox.x = view->x;
			server->grab_geobox.y = view->y;

			return;
		}
	}
}

static void
cursor_axis(struct wl_listener *listener, void *data)
{
	struct hopalong_server *server = wl_container_of(listener, server, cursor_axis);
	struct wlr_event_pointer_axis *event = data;

	wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation,
		event->delta, event->delta_discrete, event->source);
}

static void
cursor_frame(struct wl_listener *listener, void *data)
{
	struct hopalong_server *server = wl_container_of(listener, server, cursor_frame);
	wlr_seat_pointer_notify_frame(server->seat);
}

void
hopalong_cursor_setup(struct hopalong_server *server)
{
	server->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server->cursor, server->output_layout);

	server->cursor_mgr = wlr_xcursor_manager_create("breeze_cursors", 48);
	wlr_xcursor_manager_load(server->cursor_mgr, 1);
	wlr_xcursor_manager_load(server->cursor_mgr, 2);

	server->cursor_motion.notify = cursor_motion;
	wl_signal_add(&server->cursor->events.motion, &server->cursor_motion);

	server->cursor_motion_absolute.notify = cursor_motion_absolute;
	wl_signal_add(&server->cursor->events.motion_absolute, &server->cursor_motion_absolute);

	server->cursor_button.notify = cursor_button;
	wl_signal_add(&server->cursor->events.button, &server->cursor_button);

	server->cursor_axis.notify = cursor_axis;
	wl_signal_add(&server->cursor->events.axis, &server->cursor_axis);

	server->cursor_frame.notify = cursor_frame;
	wl_signal_add(&server->cursor->events.frame, &server->cursor_frame);
}

void
hopalong_cursor_teardown(struct hopalong_server *server)
{
	if (server->cursor_mgr)
		wlr_xcursor_manager_destroy(server->cursor_mgr);

	if (server->cursor)
		wlr_cursor_destroy(server->cursor);
}
