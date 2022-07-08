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

#ifndef HOPALONG_COMPOSITOR_PALETTE_H
#define HOPALONG_COMPOSITOR_PALETTE_H

#include <glib-2.0/glib.h>

struct hopalong_style {
	float base_bg[4];

	float title_bar_bg[4];
	float title_bar_bg_inactive[4];

	float title_bar_fg[4];
	float title_bar_fg_inactive[4];

	float minimize_btn_fg[4];
	float minimize_btn_fg_inactive[4];

	float maximize_btn_fg[4];
	float maximize_btn_fg_inactive[4];

	float close_btn_fg[4];
	float close_btn_fg_inactive[4];

	float border[4];
	float border_inactive[4];

	size_t border_thickness;
	size_t title_bar_height;
	size_t title_bar_padding;

	const char *title_bar_font;
};

extern const struct hopalong_style *hopalong_style_get_default(void);
extern const struct hopalong_style *hopalong_style_load(const char *style_name);

#endif