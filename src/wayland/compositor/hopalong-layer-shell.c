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

#include <wlr/util/log.h>

#include "hopalong-server.h"
#include "hopalong-layer-shell.h"

static void
hopalong_layer_shell_surface_map(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, map);
	wlr_surface_send_enter(view->layer_surface->surface, view->layer_surface->output);
}

static void
hopalong_layer_shell_surface_unmap(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, unmap);
	hopalong_view_unmap(view);
}

static void
hopalong_layer_shell_destroy(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, destroy);

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->surface_commit.link);

	hopalong_view_destroy(view);
}

static enum hopalong_layer layer_mapping[HOPALONG_LAYER_COUNT] = {
	[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]	= HOPALONG_LAYER_BACKGROUND,
	[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]	= HOPALONG_LAYER_BOTTOM,
	[ZWLR_LAYER_SHELL_V1_LAYER_TOP]		= HOPALONG_LAYER_TOP,
	[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]	= HOPALONG_LAYER_OVERLAY,
};

static void
apply_exclusive(struct wlr_box *usable_area, uint32_t anchor, int32_t exclusive,
		int32_t margin_top, int32_t margin_right, int32_t margin_bottom,
		int32_t margin_left)
{
	if (exclusive <= 0)
		return;

	struct {
		uint32_t anchors;
		int *positive_axis;
		int *negative_axis;
		int margin;
	} edges[] = {
		{
			.anchors =
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
			.positive_axis = &usable_area->y,
			.negative_axis = &usable_area->height,
			.margin = margin_top,
		},
		{
			.anchors =
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
			.positive_axis = NULL,
			.negative_axis = &usable_area->height,
			.margin = margin_bottom,
		},
		{
			.anchors =
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
			.positive_axis = &usable_area->x,
			.negative_axis = &usable_area->width,
			.margin = margin_left,
		},
		{
			.anchors =
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
			.positive_axis = NULL,
			.negative_axis = &usable_area->width,
			.margin = margin_right,
		},
	};

	for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); ++i)
	{
		if ((anchor & edges[i].anchors) == edges[i].anchors)
		{
			if (edges[i].positive_axis)
				*edges[i].positive_axis += exclusive + edges[i].margin;

			if (edges[i].negative_axis)
				*edges[i].negative_axis -= exclusive + edges[i].margin;
		}
	}
}

static void
arrange_layer(struct wlr_output *output, struct wl_list *list, struct wlr_box *usable_area, bool exclusive)
{
	struct hopalong_view *view;

	struct wlr_box full_area = { 0 };
	wlr_output_effective_resolution(output, &full_area.width, &full_area.height);

	wl_list_for_each_reverse(view, list, mapped_link)
	{
		struct wlr_layer_surface_v1 *layer = view->layer_surface;
    
    if (layer)
      {
		   struct wlr_layer_surface_v1_state *state = &layer->current;

		   if (state->exclusive_zone > 0 && !exclusive)
			   continue;

		   struct wlr_box bounds;

		   if (state->exclusive_zone == -1)
			   bounds = full_area;
		   else
			   bounds = *usable_area;

		   struct wlr_box box = {
			   .width = state->desired_width,
			   .height = state->desired_height
		   };

		   /* Horizontal axis */
		   const uint32_t both_horiz = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
			   | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;

		   if ((state->anchor & both_horiz) && box.width == 0)
		   {
			   box.x = bounds.x;
			   box.width = bounds.width;
		   }
		   else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT))
			   box.x = bounds.x;
		   else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT))
			   box.x = bounds.x + (bounds.width - box.width);
		   else
			   box.x = bounds.x + ((bounds.width / 2) - (box.width / 2));

		   /* Vertical axis */
		   const uint32_t both_vert = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
			   | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;

		   if ((state->anchor & both_vert) && box.height == 0)
		   {
			   box.y = bounds.y;
			   box.height = bounds.height;
		   }
		   else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP))
			   box.y = bounds.y;
		   else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM))
			   box.y = bounds.y + (bounds.height - box.height);
		   else
			   box.y = bounds.y + ((bounds.height / 2) - (box.height / 2));

		   /* Margin */
		   if ((state->anchor & both_horiz) == both_horiz)
		   {
			   int32_t total_margin = state->margin.left + state->margin.right;

			   box.x += state->margin.left;

			   if (total_margin < box.width)
				   box.width -= total_margin;
		   }
		   else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT))
			   box.x += state->margin.left;
		   else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT))
			   box.x -= state->margin.right;

		   if ((state->anchor & both_vert) == both_vert)
		   {
			   int32_t total_margin = state->margin.top + state->margin.bottom;

			   box.y += state->margin.top;

			   if (total_margin < box.height)
				   box.height -= total_margin;
		   }
		   else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP))
			   box.y += state->margin.top;
		   else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM))
			   box.y -= state->margin.bottom;

		   if (box.width < 0 || box.height < 0)
		   {
			   wlr_layer_surface_v1_destroy(layer);
			   continue;
		   }

		   apply_exclusive(usable_area, state->anchor,
			   state->exclusive_zone, state->margin.top,
			   state->margin.right, state->margin.bottom,
			   state->margin.left);

		   view->x = box.x;
		   view->y = box.y;

		   wlr_layer_surface_v1_configure(layer, box.width, box.height);
      }
	}
}

static void
hopalong_layer_shell_surface_commit(struct wl_listener *listener, void *data)
{
	struct hopalong_view *view = wl_container_of(listener, view, surface_commit);
	bool mapped = view->mapped;

	/* if the layer moved, prepare to put it on the right list. */
	if (mapped)
		hopalong_view_unmap(view);

	view->layer = layer_mapping[view->layer_surface->current.layer];
	hopalong_view_map(view);

	struct wlr_output *output = view->layer_surface->output;
	struct wlr_box usable_area = {};
	wlr_output_effective_resolution(output, &usable_area.width, &usable_area.height);

	struct hopalong_server *server = view->server;

	arrange_layer(output, &server->mapped_layers[HOPALONG_LAYER_OVERLAY], &usable_area, true);
	arrange_layer(output, &server->mapped_layers[HOPALONG_LAYER_TOP], &usable_area, true);
	arrange_layer(output, &server->mapped_layers[HOPALONG_LAYER_BOTTOM], &usable_area, true);
	arrange_layer(output, &server->mapped_layers[HOPALONG_LAYER_BACKGROUND], &usable_area, true);

	arrange_layer(output, &server->mapped_layers[HOPALONG_LAYER_OVERLAY], &usable_area, false);
	arrange_layer(output, &server->mapped_layers[HOPALONG_LAYER_TOP], &usable_area, false);
	arrange_layer(output, &server->mapped_layers[HOPALONG_LAYER_BOTTOM], &usable_area, false);
	arrange_layer(output, &server->mapped_layers[HOPALONG_LAYER_BACKGROUND], &usable_area, false);
}

static void
hopalong_layer_shell_nop(struct hopalong_view *view)
{
}

static const char *
hopalong_layer_shell_getprop(struct hopalong_view *view, enum hopalong_view_prop prop)
{
	return NULL;
}

static struct wlr_surface *
hopalong_layer_shell_get_surface(struct hopalong_view *view)
{
	return view->layer_surface->surface;
}

static bool
hopalong_layer_shell_get_geometry(struct hopalong_view *view, struct wlr_box *box)
{
	box->x = view->x;
	box->y = view->y;
	box->width = view->layer_surface->current.actual_width;
	box->height = view->layer_surface->current.actual_height;

	return true;
}

static struct wlr_surface *
hopalong_layer_shell_surface_at(struct hopalong_view *view, double x, double y, double *sx, double *sy)
{
	double view_sx = x - view->x;
	double view_sy = y - view->y;

	double _sx = 0.0, _sy = 0.0;
	struct wlr_surface *surface = wlr_layer_surface_v1_surface_at(view->layer_surface, view_sx, view_sy, &_sx, &_sy);

	if (surface != NULL)
	{
		*sx = _sx;
		*sy = _sy;
	}

	return surface;
}

static bool
hopalong_layer_shell_can_move_or_resize(struct hopalong_view *view)
{
	return false;
}

static const struct hopalong_view_ops hopalong_layer_shell_view_ops = {
	.minimize = hopalong_layer_shell_nop,
	.maximize = hopalong_layer_shell_nop,
	.close = hopalong_layer_shell_nop,
	.getprop = hopalong_layer_shell_getprop,
	.get_surface = hopalong_layer_shell_get_surface,
	.set_activated = (void *) hopalong_layer_shell_nop,
	.get_geometry = hopalong_layer_shell_get_geometry,
	.set_size = (void *) hopalong_layer_shell_nop,
	.surface_at = hopalong_layer_shell_surface_at,
	.can_move = hopalong_layer_shell_can_move_or_resize,
	.can_resize = hopalong_layer_shell_can_move_or_resize,
};

static void
hopalong_layer_shell_new_surface(struct wl_listener *listener, void *data)
{
	struct hopalong_server *server = wl_container_of(listener, server, new_layer_surface);
	struct wlr_layer_surface_v1 *layer_surface = data;

	struct hopalong_view *view = calloc(1, sizeof(*view));

	view->server = server;
	view->ops = &hopalong_layer_shell_view_ops;
	view->using_csd = true;

	view->layer_surface = layer_surface;
	layer_surface->data = view;  

	if (layer_surface->output == NULL)
	{
		struct wlr_output *output = wlr_output_layout_output_at(
			server->output_layout, server->cursor->x,
			server->cursor->y);

		layer_surface->output = output;
	}

	view->layer = layer_mapping[layer_surface->pending.layer];

	view->map.notify = hopalong_layer_shell_surface_map;
	wl_signal_add(&layer_surface->events.map, &view->map);

	view->unmap.notify = hopalong_layer_shell_surface_unmap;
	wl_signal_add(&layer_surface->events.unmap, &view->unmap);

	view->destroy.notify = hopalong_layer_shell_destroy;
	wl_signal_add(&layer_surface->events.destroy, &view->destroy);

	view->surface_commit.notify = hopalong_layer_shell_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit, &view->surface_commit);

	wl_list_insert(&server->views, &view->link);
}

void
hopalong_layer_shell_setup(struct hopalong_server *server)
{
	server->wlr_layer_shell = wlr_layer_shell_v1_create(server->display);

	server->new_layer_surface.notify = hopalong_layer_shell_new_surface;
	wl_signal_add(&server->wlr_layer_shell->events.new_surface, &server->new_layer_surface);
}

void
hopalong_layer_shell_teardown(struct hopalong_server *server)
{
}
