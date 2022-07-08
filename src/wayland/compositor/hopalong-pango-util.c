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

#include "hopalong-pango-util.h"

size_t
hopalong_pango_util_escape_markup_text(const char *src, char *dest, size_t dest_size)
{
	size_t length = 0;

	return_val_if_fail(dest != NULL, 0);

	while (src[0])
	{
		switch (src[0])
		{
		case '&':
			length += 5;
			strlcat(dest, "&amp;", dest_size);
			break;

		case '<':
			length += 4;
			strlcat(dest, "&lt;", dest_size);
			break;

		case '>':
			length += 4;
			strlcat(dest, "&gt;", dest_size);
			break;

		case '\'':
			length += 6;
			strlcat(dest, "&apos;", dest_size);
			break;

		case '"':
			length += 6;
			strlcat(dest, "&quot;", dest_size);
			break;

		default:
			dest[length] = *src;
			dest[length + 1] = '\0';
			length += 1;
			break;
        	}

		src++;
	}

	return length;
}

PangoLayout *
hopalong_pango_util_get_pango_layout(cairo_t *cairo, const char *font, const char *text, double scale, bool markup)
{
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	PangoAttrList *attrs;

	if (markup)
	{
		char *buf;
		GError *error = NULL;

		if (pango_parse_markup(text, -1, 0, &attrs, &buf, NULL, &error))
		{
			pango_layout_set_text(layout, buf, -1);
			free(buf);
		}
		else
		{
			wlr_log(WLR_ERROR, "pango_parse_markup '%s' -> error %s", text, error->message);

			g_error_free(error);
			markup = false; // fallback to plain text
		}
	}

	if (!markup)
	{
		attrs = pango_attr_list_new();
		pango_layout_set_text(layout, text, -1);
	}

	pango_attr_list_insert(attrs, pango_attr_scale_new(scale));
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_single_paragraph_mode(layout, 1);
	pango_layout_set_attributes(layout, attrs);
	pango_attr_list_unref(attrs);
	pango_font_description_free(desc);

	return layout;
}

void
hopalong_pango_util_get_text_size(cairo_t *cairo, const char *font, int *width, int *height,
	int *baseline, double scale, bool markup, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	// Add one since vsnprintf excludes null terminator.
	int length = vsnprintf(NULL, 0, fmt, args) + 1;
	va_end(args);

	char *buf = malloc(length);
	return_if_fail(buf != NULL);

	va_start(args, fmt);
	vsnprintf(buf, length, fmt, args);
	va_end(args);

	PangoLayout *layout = hopalong_pango_util_get_pango_layout(cairo, font, buf, scale, markup);
	pango_cairo_update_layout(cairo, layout);
	pango_layout_get_pixel_size(layout, width, height);

	if (baseline != NULL)
		*baseline = pango_layout_get_baseline(layout) / PANGO_SCALE;

	g_object_unref(layout);
	free(buf);
}

void
hopalong_pango_util_printf(cairo_t *cairo, const char *font, double scale, bool markup, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	// Add one since vsnprintf excludes null terminator.
	int length = vsnprintf(NULL, 0, fmt, args) + 1;

	va_end(args);

	char *buf = malloc(length);
	return_if_fail(buf != NULL);

	va_start(args, fmt);
	vsnprintf(buf, length, fmt, args);
	va_end(args);

	PangoLayout *layout = hopalong_pango_util_get_pango_layout(cairo, font, buf, scale, markup);
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_get_font_options(cairo, fo);
	pango_cairo_context_set_font_options(pango_layout_get_context(layout), fo);
	cairo_font_options_destroy(fo);
	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);

	g_object_unref(layout);
	free(buf);
}
