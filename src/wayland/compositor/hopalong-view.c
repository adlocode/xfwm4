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

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "hopalong-server.h"
#include "hopalong-view.h"
#include "hopalong-output.h"
#include "hopalong-pango-util.h"

static struct wlr_texture *
generate_minimize_texture(struct hopalong_output *output, const float color[4])
{
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 32, 32);
	return_val_if_fail(surface != NULL, false);

	cairo_t *cr = cairo_create(surface);
	return_val_if_fail(cr != NULL, false);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

	cairo_set_source_rgba(cr, color[0], color[1], color[2], color[3]);
	cairo_set_line_width(cr, 3.0);

	cairo_move_to(cr, 8, 16);
	cairo_line_to(cr, 16, 24);
	cairo_line_to(cr, 24, 16);

	cairo_stroke(cr);

	cairo_surface_flush(surface);

	unsigned char *data = cairo_image_surface_get_data(surface);
	struct wlr_renderer *renderer = output->wlr_output->renderer;
	struct wlr_texture *texture = wlr_texture_from_pixels(renderer,
		WL_SHM_FORMAT_ARGB8888,
		cairo_image_surface_get_stride(surface),
		32, 32, data);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	return texture;
}

static struct wlr_texture *
generate_maximize_texture(struct hopalong_output *output, const float color[4])
{
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 32, 32);
	return_val_if_fail(surface != NULL, false);

	cairo_t *cr = cairo_create(surface);
	return_val_if_fail(cr != NULL, false);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

	cairo_set_source_rgba(cr, color[0], color[1], color[2], color[3]);
	cairo_set_line_width(cr, 3.0);

	cairo_move_to(cr, 8, 16);
	cairo_line_to(cr, 16, 8);
	cairo_line_to(cr, 24, 16);
	cairo_line_to(cr, 16, 24);
	cairo_line_to(cr, 8, 16);

	cairo_stroke(cr);

	cairo_surface_flush(surface);

	unsigned char *data = cairo_image_surface_get_data(surface);
	struct wlr_renderer *renderer = output->wlr_output->renderer;
	struct wlr_texture *texture = wlr_texture_from_pixels(renderer,
		WL_SHM_FORMAT_ARGB8888,
		cairo_image_surface_get_stride(surface),
		32, 32, data);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	return texture;
}

static struct wlr_texture *
generate_close_texture(struct hopalong_output *output, const float color[4])
{
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 32, 32);
	return_val_if_fail(surface != NULL, false);

	cairo_t *cr = cairo_create(surface);
	return_val_if_fail(cr != NULL, false);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

	cairo_set_source_rgba(cr, color[0], color[1], color[2], color[3]);
	cairo_set_line_width(cr, 3.0);

	cairo_move_to(cr, 8, 24);
	cairo_line_to(cr, 24, 8);

	cairo_move_to(cr, 24, 24);
	cairo_line_to(cr, 8, 8);

	cairo_stroke(cr);

	cairo_surface_flush(surface);

	unsigned char *data = cairo_image_surface_get_data(surface);
	struct wlr_renderer *renderer = output->wlr_output->renderer;
	struct wlr_texture *texture = wlr_texture_from_pixels(renderer,
		WL_SHM_FORMAT_ARGB8888,
		cairo_image_surface_get_stride(surface),
		32, 32, data);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	return texture;
}

struct hopalong_generated_textures *
hopalong_generate_builtin_textures_for_output(struct hopalong_output *output, const struct hopalong_style *style)
{
	struct hopalong_generated_textures *gentex = calloc(1, sizeof(*gentex));

	gentex->minimize = generate_minimize_texture(output, style->minimize_btn_fg);
	gentex->minimize_inactive = generate_minimize_texture(output, style->minimize_btn_fg_inactive);

	gentex->maximize = generate_maximize_texture(output, style->maximize_btn_fg);
	gentex->maximize_inactive = generate_maximize_texture(output, style->maximize_btn_fg_inactive);

	gentex->close = generate_close_texture(output, style->close_btn_fg);
	gentex->close_inactive = generate_close_texture(output, style->close_btn_fg_inactive);

	return gentex;
}

static bool
hopalong_view_generate_title_texture(struct hopalong_output *output, struct hopalong_view *view)
{
	view->title_dirty = false;
  return true;
  
  //This code has been removed to fix bug
  
  struct wlr_renderer *renderer = output->wlr_output->renderer;

	if (view->title)
	{
		wlr_texture_destroy(view->title);
		wlr_texture_destroy(view->title_inactive);
	}

	const struct hopalong_style *style = view->server->style;
	const char *font = style->title_bar_font;

	char title[4096] = {};

	const char *title_data = hopalong_view_getprop(view, HOPALONG_VIEW_TITLE);
	if (title_data == NULL)
		title_data = hopalong_view_getprop(view, HOPALONG_VIEW_APP_ID);
	if (title_data != NULL)
		strlcpy(title, title_data, sizeof title);

	float scale = output->wlr_output->scale;
	int w = 400;
	int h = 32;

	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	return_val_if_fail(surface != NULL, false);

	cairo_t *cr = cairo_create(surface);
	return_val_if_fail(cr != NULL, false);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
	cairo_font_options_t *fo = cairo_font_options_create();
	return_val_if_fail(fo != NULL, false);

	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);

	cairo_set_font_options(cr, fo);
	hopalong_pango_util_get_text_size(cr, font, &w, NULL, NULL, scale, true, "%s", title);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	return_val_if_fail(surface != NULL, false);

	cr = cairo_create(surface);
	return_val_if_fail(cr != NULL, false);

	PangoContext *pango = pango_cairo_create_context(cr);
	cairo_move_to(cr, 0, 0);

	/* active color first */
	cairo_set_source_rgba(cr, style->title_bar_fg[0], style->title_bar_fg[1],
			      style->title_bar_fg[2], style->title_bar_fg[3]);
	hopalong_pango_util_printf(cr, font, scale, true, "%s", title);
	cairo_surface_flush(surface);

	unsigned char *data = cairo_image_surface_get_data(surface);

	view->title = wlr_texture_from_pixels(renderer,
		WL_SHM_FORMAT_ARGB8888,
		cairo_image_surface_get_stride(surface),
		w, h, data);

	/* inactive color */
	cairo_set_source_rgba(cr, style->title_bar_fg_inactive[0], style->title_bar_fg_inactive[1],
			      style->title_bar_fg_inactive[2], style->title_bar_fg_inactive[3]);
	hopalong_pango_util_printf(cr, font, scale, true, "%s", title);
	cairo_surface_flush(surface);

	data = cairo_image_surface_get_data(surface);

	view->title_inactive = wlr_texture_from_pixels(renderer,
		WL_SHM_FORMAT_ARGB8888,
		cairo_image_surface_get_stride(surface),
		w, h, data);

	view->title_box.width = w;
	view->title_box.height = h;
	view->title_dirty = false;

	g_object_unref(pango);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	return true;
}

bool
hopalong_view_generate_textures(struct hopalong_output *output, struct hopalong_view *view)
{
	return_val_if_fail(output != NULL, false);
	return_val_if_fail(view != NULL, false);

	if (view->title_dirty && !hopalong_view_generate_title_texture(output, view))
		return false;

	return true;
}

void
hopalong_view_minimize(struct hopalong_view *view)
{
	return_if_fail(view != NULL);
	return_if_fail(view->ops != NULL);

	view->ops->minimize(view);
}

void
hopalong_view_maximize(struct hopalong_view *view)
{
	return_if_fail(view != NULL);
	return_if_fail(view->ops != NULL);

	view->ops->maximize(view);
}

void
hopalong_view_close(struct hopalong_view *view)
{
	return_if_fail(view != NULL);
	return_if_fail(view->ops != NULL);

	view->ops->close(view);
}

const char *
hopalong_view_getprop(struct hopalong_view *view, enum hopalong_view_prop prop)
{
	return_val_if_fail(view != NULL, NULL);
	return_val_if_fail(view->ops != NULL, NULL);

	return view->ops->getprop(view, prop);
}

void
hopalong_view_destroy(struct hopalong_view *view)
{
	return_if_fail(view != NULL);

	wl_list_remove(&view->link);

	if (view->mapped)
		wl_list_remove(&view->mapped_link);

	if (view->title != NULL)
	{
		wlr_texture_destroy(view->title);
		wlr_texture_destroy(view->title_inactive);
	}

	free(view);
}

struct wlr_surface *
hopalong_view_get_surface(struct hopalong_view *view)
{
	return_val_if_fail(view != NULL, NULL);
	return_val_if_fail(view->ops != NULL, NULL);

	return view->ops->get_surface(view);
}

static struct hopalong_view *
hopalong_view_from_wlr_surface(struct hopalong_server *server, struct wlr_surface *surface)
{
	struct hopalong_view *view;

	wl_list_for_each(view, &server->views, link)
	{
		if (surface == hopalong_view_get_surface(view))
			return view;
	}

	return NULL;
}

void
hopalong_view_set_activated(struct hopalong_view *view, bool activated)
{
	return_if_fail(view != NULL);
	return_if_fail(view->ops != NULL);

	view->ops->set_activated(view, activated);
	view->activated = activated;
}

void
hopalong_view_focus(struct hopalong_view *view, struct wlr_surface *surface)
{
	return_if_fail(view != NULL);

	struct hopalong_server *server = view->server;
	return_if_fail(server != NULL);

	struct wlr_seat *seat = server->seat;
	return_if_fail(seat != NULL);

	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface)
		return;

	if (prev_surface != NULL)
	{
		struct hopalong_view *prev_view = hopalong_view_from_wlr_surface(server, prev_surface);

		if (prev_view != NULL)
			hopalong_view_set_activated(prev_view, false);
	}

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

	surface = hopalong_view_get_surface(view);
	if (surface != NULL)
		wlr_seat_keyboard_notify_enter(seat, surface,
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);

	hopalong_view_reparent(view);

	view->frame_area = -1;
	view->frame_area_edges = WLR_EDGE_NONE;
}

bool
hopalong_view_get_geometry(struct hopalong_view *view, struct wlr_box *box)
{
	return_val_if_fail(view != NULL, false);
	return_val_if_fail(view->ops != NULL, false);
	return_val_if_fail(box != NULL, false);

	return view->ops->get_geometry(view, box);
}

void
hopalong_view_set_size(struct hopalong_view *view, int new_width, int new_height)
{
	return_if_fail(view != NULL);
	return_if_fail(view->ops != NULL);

	return view->ops->set_size(view, new_width, new_height);
}

void
hopalong_view_map(struct hopalong_view *view)
{
	return_if_fail(view != NULL);

	struct hopalong_server *server = view->server;
	return_if_fail(server != NULL);

	view->mapped = true;

	wl_list_insert(&server->mapped_layers[view->layer], &view->mapped_link);
	hopalong_view_set_activated(view, true);
}

void
hopalong_view_unmap(struct hopalong_view *view)
{
	return_if_fail(view != NULL);

	view->mapped = false;

	wl_list_remove(&view->mapped_link);
}

void
hopalong_view_reparent(struct hopalong_view *view)
{
	return_if_fail(view != NULL);

	if (view->mapped)
		hopalong_view_unmap(view);

	hopalong_view_map(view);
}

struct wlr_surface *
hopalong_view_surface_at(struct hopalong_view *view, double x, double y, double *sx, double *sy)
{
	return_val_if_fail(view != NULL, NULL);
	return_val_if_fail(view->ops != NULL, NULL);

	return view->ops->surface_at(view, x, y, sx, sy);
}

bool
hopalong_view_can_move(struct hopalong_view *view)
{
	return_val_if_fail(view != NULL, false);
	return_val_if_fail(view->ops != NULL, false);

	return view->ops->can_move(view);
}

bool
hopalong_view_can_resize(struct hopalong_view *view)
{
	return_val_if_fail(view != NULL, false);
	return_val_if_fail(view->ops != NULL, false);

	return view->ops->can_resize(view);
}
