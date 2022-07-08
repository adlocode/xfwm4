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

#ifndef HOPALONG_COMPOSITOR_PANGO_UTIL_H
#define HOPALONG_COMPOSITOR_PANGO_UTIL_H

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "hopalong-server.h"
#include "hopalong-view.h"
#include "hopalong-output.h"

extern size_t hopalong_pango_util_escape_markup_text(const char *src, char *dest, size_t dest_size);
extern PangoLayout *hopalong_pango_util_get_pango_layout(cairo_t *cairo, const char *font, const char *text, double scale, bool markup);
extern void hopalong_pango_util_get_text_size(cairo_t *cairo, const char *font, int *width, int *height,
	int *baseline, double scale, bool markup, const char *fmt, ...);
extern void hopalong_pango_util_printf(cairo_t *cairo, const char *font, double scale, bool markup, const char *fmt, ...);

#endif
