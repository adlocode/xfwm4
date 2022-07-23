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

#ifndef HOPALONG_COMPOSITOR_VIEW_H
#define HOPALONG_COMPOSITOR_VIEW_H

#include <wayland-server-core.h>
#include <wlr/backend.h>
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
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "hopalong-style.h"
#include "xfwm-shell.h"

struct hopalong_output;
struct hopalong_server;
struct hopalong_view;

/*
 * Hopalong has five layers, mediated by the wlr-layer-shell, xdg-shell and
 * xwayland-shell protocols.  xdg-shell and xwayland-shell surfaces are always
 * placed in the HOPALONG_LAYER_MIDDLE layer.
 */
enum hopalong_layer {
	HOPALONG_LAYER_BACKGROUND,
	HOPALONG_LAYER_BOTTOM,
	HOPALONG_LAYER_MIDDLE,
	HOPALONG_LAYER_TOP,
	HOPALONG_LAYER_OVERLAY,
	HOPALONG_LAYER_COUNT
};

enum hopalong_view_frame_area {
	HOPALONG_VIEW_FRAME_AREA_TOP,
	HOPALONG_VIEW_FRAME_AREA_BOTTOM,
	HOPALONG_VIEW_FRAME_AREA_LEFT,
	HOPALONG_VIEW_FRAME_AREA_RIGHT,
	HOPALONG_VIEW_FRAME_AREA_TITLEBAR,
	HOPALONG_VIEW_FRAME_AREA_MINIMIZE,
	HOPALONG_VIEW_FRAME_AREA_MAXIMIZE,
	HOPALONG_VIEW_FRAME_AREA_CLOSE,
	HOPALONG_VIEW_FRAME_AREA_COUNT,
};

enum hopalong_view_prop {
	HOPALONG_VIEW_TITLE,
	HOPALONG_VIEW_APP_ID,
};

struct hopalong_view_ops {
	void (*minimize)(struct hopalong_view *view);
	void (*maximize)(struct hopalong_view *view);
	void (*close)(struct hopalong_view *view);
	const char *(*getprop)(struct hopalong_view *view, enum hopalong_view_prop prop);
	struct wlr_surface *(*get_surface)(struct hopalong_view *view);
	void (*set_activated)(struct hopalong_view *view, bool activated);
	bool (*get_geometry)(struct hopalong_view *view, struct wlr_box *box);
	void (*set_size)(struct hopalong_view *view, int width, int height);
	struct wlr_surface *(*surface_at)(struct hopalong_view *view, double x, double y, double *sx, double *sy);
	bool (*can_move)(struct hopalong_view *view);
	bool (*can_resize)(struct hopalong_view *view);
};

struct hopalong_view {
	struct wl_list link;
	struct wl_list mapped_link;

	enum hopalong_layer layer;

	struct hopalong_server *server;
	const struct hopalong_view_ops *ops;

	struct wlr_xdg_surface *xdg_surface;
	struct wlr_xwayland_surface *xwayland_surface;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_configure;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener set_title;
	struct wl_listener surface_commit;
	bool mapped;
	int x, y;

	struct wlr_box frame_areas[HOPALONG_VIEW_FRAME_AREA_COUNT];

	/* the area of the frame the pointer is hovering over if any */
	int frame_area;
	int frame_area_edges;

	/* textures owned by this view */
	struct wlr_texture *title;
	struct wlr_texture *title_inactive;
	struct wlr_box title_box;
	bool title_dirty;

	/* client-side decorations */
	bool using_csd;
	bool activated;
	bool hide_title_bar;
  
  struct xfwm_shell_window *shell_window;
};

struct hopalong_generated_textures {
	struct wlr_texture *minimize;
	struct wlr_texture *minimize_inactive;

	struct wlr_texture *maximize;
	struct wlr_texture *maximize_inactive;

	struct wlr_texture *close;
	struct wlr_texture *close_inactive;
};

extern struct hopalong_generated_textures *hopalong_generate_builtin_textures_for_output(struct hopalong_output *output, const struct hopalong_style *style);
extern bool hopalong_view_generate_textures(struct hopalong_output *output, struct hopalong_view *view);

extern void hopalong_view_minimize(struct hopalong_view *view);
extern void hopalong_view_maximize(struct hopalong_view *view);
extern void hopalong_view_close(struct hopalong_view *view);
extern const char *hopalong_view_getprop(struct hopalong_view *view, enum hopalong_view_prop prop);
extern void hopalong_view_focus(struct hopalong_view *view, struct wlr_surface *surface);
extern void hopalong_view_destroy(struct hopalong_view *view);
extern struct wlr_surface *hopalong_view_get_surface(struct hopalong_view *view);
extern void hopalong_view_set_activated(struct hopalong_view *view, bool activated);
extern bool hopalong_view_get_geometry(struct hopalong_view *view, struct wlr_box *box);
extern void hopalong_view_set_size(struct hopalong_view *view, int new_width, int new_height);
extern void hopalong_view_map(struct hopalong_view *view);
extern void hopalong_view_unmap(struct hopalong_view *view);
extern void hopalong_view_reparent(struct hopalong_view *view);
extern struct wlr_surface *hopalong_view_surface_at(struct hopalong_view *view, double x, double y, double *sx, double *sy);
extern bool hopalong_view_can_move(struct hopalong_view *view);
extern bool hopalong_view_can_resize(struct hopalong_view *view);

#endif
