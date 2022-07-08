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

#include <stdlib.h>
#include "hopalong-shell.h"
#include "hopalong-server.h"
#include "hopalong-decoration.h"

static const int resize_edges[] = {
	WLR_EDGE_TOP,
	WLR_EDGE_BOTTOM,
	WLR_EDGE_LEFT,
	WLR_EDGE_RIGHT,
};

bool
hopalong_shell_view_at(struct hopalong_view *view,
	double lx, double ly, struct wlr_surface **surface, double *sx, double *sy)
{
	double _sx, _sy;
	struct wlr_surface *_surface = hopalong_view_surface_at(view, lx, ly, &_sx, &_sy);

	if (_surface != NULL)
	{
		*surface = _surface;
		*sx = _sx;
		*sy = _sy;
		return true;
	}

	/* check for frame areas */
	view->frame_area = -1;
	view->frame_area_edges = WLR_EDGE_NONE;

	for (size_t i = 0; i < HOPALONG_VIEW_FRAME_AREA_COUNT; i++)
	{
		struct wlr_box *box = &view->frame_areas[i];

		if (!box->width && !box->height)
			continue;

		if (lx >= box->x && lx <= box->x + box->width &&
		    ly >= box->y && ly <= box->y + box->height)
		{
			view->frame_area = i;

			if (i < HOPALONG_VIEW_FRAME_AREA_TITLEBAR)
				view->frame_area_edges |= resize_edges[i];
		}
	}

	return view->frame_area != -1;
}

struct hopalong_view *
hopalong_shell_desktop_view_at(struct hopalong_server *server, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy)
{
	struct hopalong_view *view;

	for (size_t i = 0; i < HOPALONG_LAYER_COUNT; i++)
	{
		wl_list_for_each(view, &server->mapped_layers[i], mapped_link)
		{
			if (hopalong_shell_view_at(view, lx, ly, surface, sx, sy))
				return view;
		}
	}

	return NULL;
}
