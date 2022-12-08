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
#include "hopalong-server.h"
#include "hopalong-output.h"

#include <wlr/render/gles2.h>
#include <GLES2/gl2.h>

struct render_data {
	struct wlr_output *output;
	struct hopalong_view *view;
	struct wlr_renderer *renderer;
	struct timespec *when;
	struct hopalong_generated_textures *textures;
};

static void
scale_box_coords(struct wlr_box *box, float scale)
{
	box->x *= scale;
	box->y *= scale;
}

static void
scale_box(struct wlr_box *box, float scale)
{
	scale_box_coords(box, scale);

	box->width *= scale;
	box->height *= scale;
}

static void
render_surface(struct wlr_surface *surface, int sx, int sy, void *data)
{
	return_if_fail(surface != NULL);

	struct render_data *rdata = data;
	return_if_fail(rdata != NULL);

	struct hopalong_view *view = rdata->view;
	return_if_fail(view != NULL);

	struct wlr_output *output = rdata->output;
	return_if_fail(output != NULL);

	/* get a GPU texture */
	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (texture == NULL)
		return;

	/* translate to output-local coordinates */
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(view->server->output_layout, output, &ox, &oy);
	ox += view->x + sx;
	oy += view->y + sy;

	/* set up our box */
	struct wlr_box box = {
		.x = ox,
		.y = oy,
		.width = surface->current.width,
		.height = surface->current.height,
	};
	scale_box(&box, output->scale);

	/* convert box to matrix */
	float matrix[9];
	enum wl_output_transform transform = wlr_output_transform_invert(surface->current.transform);
	wlr_matrix_project_box(matrix, &box, transform, 0, output->transform_matrix);

	/* render the matrix + texture to the screen */
	wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);

	/* tell the client rendering is done */
	wlr_surface_send_frame_done(surface, rdata->when);
}

static void
render_texture(struct wlr_output *output, struct wlr_box *box, struct wlr_texture *texture, float texture_scale)
{
	return_if_fail(texture != NULL);

	struct wlr_renderer *renderer = output->renderer;
	struct wlr_box scalebox = {
		.x = box->x,
		.y = box->y,
		.width = box->width * texture_scale,
		.height = box->height * texture_scale,
	};
	scale_box_coords(&scalebox, output->scale);

	if (wlr_renderer_is_gles2 (renderer))
    {
      struct wlr_gles2_texture_attribs attribs;
	    wlr_gles2_texture_get_attribs(texture, &attribs);
	    glBindTexture(attribs.target, attribs.tex);
	    glTexParameteri(attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

	float matrix[9];
	wlr_matrix_project_box(matrix, &scalebox,
		WL_OUTPUT_TRANSFORM_NORMAL,
		0.0, output->transform_matrix);

	wlr_render_texture_with_matrix(renderer, texture, matrix, 1.0);
}

static void
render_rect(struct wlr_output *output, struct wlr_box *box, const float color[4])
{
	struct wlr_renderer *renderer = output->renderer;
	struct wlr_box scalebox = {
		.x = box->x,
		.y = box->y,
		.width = box->width,
		.height = box->height
	};
	scale_box(&scalebox, output->scale);

	wlr_render_rect(renderer, &scalebox, color, output->transform_matrix);
}

#define BORDER_HITBOX_THICKNESS		(4)

static void
render_view_surface(struct hopalong_view *view, struct render_data *data)
{
	if (view->xdg_surface != NULL)
	{
		wlr_xdg_surface_for_each_surface(view->xdg_surface, render_surface, data);
		return;
	}
	else if (view->layer_surface != NULL)
	{
		wlr_layer_surface_v1_for_each_surface(view->layer_surface, render_surface, data);
		return;
	}
	else if (view->xwayland_surface != NULL)
	{
		struct wlr_surface *surface = hopalong_view_get_surface(view);

		render_surface(surface, 0, 0, data);
		return;
	}
  else if (view->tabwin_surface)
    {
      struct wlr_surface *surface = hopalong_view_get_surface (view);
      
      render_surface (surface, 0, 0, data);
    }

	wlr_log(WLR_ERROR, "render_view_surface: don't know how to render view %p", view);
}

static void
render_container(struct hopalong_view *view, struct render_data *data)
{
	const struct hopalong_style *style = view->server->style;
	return_if_fail(style != NULL);

	struct render_data *rdata = data;
	return_if_fail(rdata != NULL);

	struct wlr_output *output = rdata->output;
	return_if_fail(output != NULL);

	if (view->using_csd)
	{
		render_view_surface(view, data);
		return;
	}

	/* translate to output-local coordinates */
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(view->server->output_layout, output, &ox, &oy);
	ox += view->x;
	oy += view->y + 1;

	/* scratch geometry */
	struct wlr_box box;
	if (!hopalong_view_get_geometry(view, &box))
		return;

	box.x = (ox - style->border_thickness);
	box.y = (oy - style->border_thickness);
	box.width = (box.width + (style->border_thickness * 2));
	box.height = (box.height + (style->border_thickness * 2)) - 1;

	/* copy scratch to base_box */
	struct wlr_box base_box = {
		.x = box.x,
		.y = box.y,
		.width = box.width,
		.height = box.height,
	};

	int title_bar_offset = (view->hide_title_bar ? 0 : style->title_bar_height) + style->border_thickness;

	/* render borders, starting with top */
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_TOP] = (struct wlr_box){
		.x = base_box.x,
		.y = base_box.y - title_bar_offset,
		.width = base_box.width,
		.height = title_bar_offset,
	};
	render_rect(output, &view->frame_areas[HOPALONG_VIEW_FRAME_AREA_TOP], style->border);
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_TOP].y -= BORDER_HITBOX_THICKNESS;
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_TOP].height += BORDER_HITBOX_THICKNESS;

	/* bottom border */
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_BOTTOM] = (struct wlr_box){
		.x = base_box.x,
		.y = base_box.y + base_box.height - style->border_thickness,
		.width = base_box.width,
		.height = style->border_thickness,
	};
	render_rect(output, &view->frame_areas[HOPALONG_VIEW_FRAME_AREA_BOTTOM], style->border);
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_BOTTOM].height += BORDER_HITBOX_THICKNESS;

	/* left border */
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_LEFT] = (struct wlr_box){
		.x = base_box.x,
		.y = base_box.y,
		.width = style->border_thickness,
		.height = base_box.height,
	};
	render_rect(output, &view->frame_areas[HOPALONG_VIEW_FRAME_AREA_LEFT], style->border);
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_LEFT].x -= BORDER_HITBOX_THICKNESS;
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_LEFT].width += BORDER_HITBOX_THICKNESS;

	/* right border */
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_RIGHT] = (struct wlr_box){
		.x = base_box.x + base_box.width - style->border_thickness,
		.y = base_box.y,
		.width = style->border_thickness,
		.height = base_box.height,
	};
	render_rect(output, &view->frame_areas[HOPALONG_VIEW_FRAME_AREA_RIGHT], style->border);
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_RIGHT].width += BORDER_HITBOX_THICKNESS;

	/* title bar */
	if (view->hide_title_bar)
		goto skip_title_bar;

	bool activated = view->activated;

	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_TITLEBAR] = (struct wlr_box){
		.x = base_box.x + style->border_thickness,
		.y = base_box.y - style->title_bar_height,
		.width = base_box.width - (style->border_thickness * 2),
		.height = style->title_bar_height + 1,
	};
	render_rect(output, &view->frame_areas[HOPALONG_VIEW_FRAME_AREA_TITLEBAR],
		activated ? style->title_bar_bg : style->title_bar_bg_inactive);

	/* title bar text */
	if (view->title != NULL)
	{
		box = (struct wlr_box){
			.x = view->frame_areas[HOPALONG_VIEW_FRAME_AREA_TITLEBAR].x + style->title_bar_padding,
			.y = view->frame_areas[HOPALONG_VIEW_FRAME_AREA_TITLEBAR].y + style->title_bar_padding,
			.width = view->title_box.width,
			.height = view->title_box.height,
		};

		render_texture(output, &box, activated ? view->title : view->title_inactive, 1.0f);
	}

	/* close button */
	return_if_fail(rdata->textures->close != NULL);
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_CLOSE] = (struct wlr_box){
		.x = view->frame_areas[HOPALONG_VIEW_FRAME_AREA_TITLEBAR].x + (base_box.width - style->border_thickness - (style->title_bar_padding * 3)),
		.y = view->frame_areas[HOPALONG_VIEW_FRAME_AREA_TITLEBAR].y + style->title_bar_padding,
		.width = 16,
		.height = 16,
	};
	render_texture(output, &view->frame_areas[HOPALONG_VIEW_FRAME_AREA_CLOSE],
		activated ? rdata->textures->close : rdata->textures->close_inactive, output->scale);

	return_if_fail(rdata->textures->maximize != NULL);
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_MAXIMIZE] = (struct wlr_box){
		.x = view->frame_areas[HOPALONG_VIEW_FRAME_AREA_CLOSE].x - (16 + style->title_bar_padding),
		.y = view->frame_areas[HOPALONG_VIEW_FRAME_AREA_CLOSE].y,
		.width = 16,
		.height = 16,
	};
	render_texture(output, &view->frame_areas[HOPALONG_VIEW_FRAME_AREA_MAXIMIZE],
		activated ? rdata->textures->maximize : rdata->textures->maximize_inactive, output->scale);

	return_if_fail(rdata->textures->minimize != NULL);
	view->frame_areas[HOPALONG_VIEW_FRAME_AREA_MINIMIZE] = (struct wlr_box){
		.x = view->frame_areas[HOPALONG_VIEW_FRAME_AREA_MAXIMIZE].x - (16 + style->title_bar_padding),
		.y = view->frame_areas[HOPALONG_VIEW_FRAME_AREA_MAXIMIZE].y,
		.width = 16,
		.height = 16,
	};
	render_texture(output, &view->frame_areas[HOPALONG_VIEW_FRAME_AREA_MINIMIZE],
		activated ? rdata->textures->minimize : rdata->textures->minimize_inactive, output->scale);

skip_title_bar:
	/* render the surface itself */
	render_view_surface(view, data);
}

static void
regenerate_textures(struct hopalong_output *output)
{
	struct hopalong_view *view;

	wl_list_for_each_reverse(view, &output->server->views, link)
	{
		if (!view->mapped)
			continue;

		hopalong_view_generate_textures(output, view);
	}
}

static void
hopalong_output_frame_notify(struct wl_listener *listener, void *data)
{
	struct hopalong_output *output = wl_container_of(listener, output, frame);
	return_if_fail(output != NULL);

	struct wlr_renderer *renderer = output->server->renderer;
	return_if_fail(renderer != NULL);

	const struct hopalong_style *style = output->server->style;
	return_if_fail(style != NULL);

	/* regenerate textures */
	regenerate_textures(output);

	/* get our render TS */
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (!wlr_output_attach_render(output->wlr_output, NULL))
		return;

	/* start rendering */
	wlr_renderer_begin(renderer, output->wlr_output->width, output->wlr_output->height);

	/* clear to something slightly off-gray in order to show the renderer is alive */
	wlr_renderer_clear(renderer, style->base_bg);

	/* render the views */
	for (size_t i = 0; i < HOPALONG_LAYER_COUNT; i++)
	{
		struct hopalong_view *view;

		wl_list_for_each_reverse(view, &output->server->mapped_layers[i], mapped_link)
		{
			struct render_data rdata = {
				.output = output->wlr_output,
				.view = view,
				.renderer = renderer,
				.when = &now,
				.textures = output->generated_textures,
			};

			render_container(view, &rdata);
		}
	}

	/* renderer our cursor if we need to */
	wlr_output_render_software_cursors(output->wlr_output, NULL);

	/* finish rendering */
	wlr_renderer_end(renderer);
	wlr_output_commit(output->wlr_output);
}

#define HIDPI_DPI_LIMIT (2 * 96)
#define HIDPI_MIN_HEIGHT 1200
#define MM_PER_INCH 25.4

static int
compute_default_scale(struct wlr_output *output)
{
	struct wlr_box box = {
		.width = output->width,
		.height = output->height
	};

	if (output->pending.committed & WLR_OUTPUT_STATE_MODE)
	{
		switch (output->pending.mode_type)
		{
		case WLR_OUTPUT_STATE_MODE_FIXED:
			box.width = output->pending.mode->width;
			box.height = output->pending.mode->height;
			break;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			box.width = output->pending.custom_mode.width;
			box.height = output->pending.custom_mode.height;
			break;
		}
	}

	enum wl_output_transform transform = output->transform;
	if (output->pending.committed & WLR_OUTPUT_STATE_TRANSFORM)
		transform = output->pending.transform;

	wlr_box_transform(&box, &box, transform, box.width, box.height);

	int width = box.width;
	int height = box.height;

	if (height < HIDPI_MIN_HEIGHT)
		return 1;

	if (output->phys_width == 0 || output->phys_height == 0)
		return 1;

	double dpi_x = (double) width / (output->phys_width / MM_PER_INCH);
	double dpi_y = (double) height / (output->phys_height / MM_PER_INCH);
	wlr_log(WLR_INFO, "Output DPI: %fx%f", dpi_x, dpi_y);
	if (dpi_x <= HIDPI_DPI_LIMIT || dpi_y <= HIDPI_DPI_LIMIT)
		return 1;

	return 2;
}

static void
output_configure(struct hopalong_output *output)
{
	wlr_output_set_scale(output->wlr_output, compute_default_scale(output->wlr_output));
	wlr_output_commit(output->wlr_output);
}

/*
 * Creates a new Hopalong output from a wlroots output.
 */
struct hopalong_output *
hopalong_output_new_from_wlr_output(struct hopalong_server *server, struct wlr_output *wlr_output)
{
	return_val_if_fail(server != NULL, NULL);
	return_val_if_fail(wlr_output != NULL, NULL);

	struct hopalong_output *output = calloc(1, sizeof(*output));
	return_val_if_fail(output != NULL, NULL);

	output->wlr_output = wlr_output;
	output->server = server;

	output->frame.notify = hopalong_output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	wlr_xcursor_manager_load(server->cursor_mgr, wlr_output->scale);

	output_configure(output);

	output->generated_textures = hopalong_generate_builtin_textures_for_output(output, server->style);

	wl_list_insert(&server->outputs, &output->link);

	return output;
}

/*
 * Destroys a Hopalong output object.
 */
void
hopalong_output_destroy(struct hopalong_output *output)
{
	return_if_fail(output != NULL);

	/* XXX: should we destroy the underlying wlr_output? */

	wl_list_remove(&output->link);
	free(output);
}
