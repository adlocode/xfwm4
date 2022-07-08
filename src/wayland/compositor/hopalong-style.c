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

#include "hopalong-server.h"
#include "hopalong-style.h"

static const struct hopalong_style default_style = {
	.base_bg = {0.3, 0.2, 0.5, 1.0},

	.title_bar_bg = {0.1, 0.1, 0.9, 1.0},
	.title_bar_bg_inactive = {0.9, 0.9, 0.9, 1.0},

	.title_bar_fg = {1.0, 1.0, 1.0, 1.0},
	.title_bar_fg_inactive = {0.2, 0.2, 0.2, 1.0},

	.minimize_btn_fg = {1.0, 1.0, 1.0, 1.0},
	.minimize_btn_fg_inactive = {0.2, 0.2, 0.2, 1.0},

	.maximize_btn_fg = {1.0, 1.0, 1.0, 1.0},
	.maximize_btn_fg_inactive = {0.2, 0.2, 0.2, 1.0},

	.close_btn_fg = {1.0, 1.0, 1.0, 1.0},
	.close_btn_fg_inactive = {0.2, 0.2, 0.2, 1.0},

	.border = {0.5, 0.5, 0.5, 1.0},
	.border_inactive = {0.5, 0.5, 0.5, 1.0},

	.border_thickness = 1,
	.title_bar_height = 32,
	.title_bar_padding = 8,

	.title_bar_font = "Sans 10",
};

const struct hopalong_style *
hopalong_style_get_default(void)
{
	return &default_style;
}

static struct hopalong_style custom_style = {};

/*
 * Loads a color in the format of #rrggbbaa and converts it to floating point.
 */
static void
hopalong_style_load_color_value(GKeyFile *kf, const char *setting, float (*value)[4])
{
	gchar *val = g_key_file_get_string(kf, "Hopalong Style", setting, NULL);
	if (val == NULL)
		return;

	if (*val != '#' && strlen(val) != 9)
	{
		g_free(val);
		return;
	}

	char scratch[3] = {};

	/* copy red component */
	strncpy(scratch, val + 1, 2);
	unsigned long r = strtoul(scratch, NULL, 16);

	/* copy green component */
	strncpy(scratch, val + 3, 2);
	unsigned long g = strtoul(scratch, NULL, 16);

	/* copy blue component */
	strncpy(scratch, val + 5, 2);
	unsigned long b = strtoul(scratch, NULL, 16);

	/* copy alpha component */
	strncpy(scratch, val + 7, 2);
	unsigned long a = strtoul(scratch, NULL, 16);

	(*value)[0] = r / 255.;
	(*value)[1] = g / 255.;
	(*value)[2] = b / 255.;
	(*value)[3] = a / 255.;

	g_free(val);
}

static void
hopalong_style_load_size_value(GKeyFile *kf, const char *setting, size_t *value)
{
	int val = g_key_file_get_integer(kf, "Hopalong Style", setting, NULL);
	if (!val)
		*value = val;
}

static void
hopalong_style_load_string_value(GKeyFile *kf, const char *setting, const char **value)
{
	gchar *val = g_key_file_get_string(kf, "Hopalong Style", setting, NULL);
	if (val != NULL)
		*value = val;
}

const struct hopalong_style *
hopalong_style_load(const char *style_name)
{
	memcpy(&custom_style, &default_style, sizeof custom_style);

	GKeyFile *kf = g_key_file_new();
	GError *err = NULL;

	if (!g_key_file_load_from_file(kf, style_name, G_KEY_FILE_NONE, &err))
	{
		wlr_log(WLR_ERROR, "Could not load style %s: %s", style_name, err->message);
		return &custom_style;
	}

	hopalong_style_load_color_value(kf, "BaseBG", &custom_style.base_bg);
	hopalong_style_load_color_value(kf, "TitleBarBG", &custom_style.title_bar_bg);
	hopalong_style_load_color_value(kf, "TitleBarBGInactive", &custom_style.title_bar_bg_inactive);
	hopalong_style_load_color_value(kf, "TitleBarFG", &custom_style.title_bar_fg);
	hopalong_style_load_color_value(kf, "TitleBarFGInactive", &custom_style.title_bar_fg_inactive);
	hopalong_style_load_color_value(kf, "MinimizeButtonFG", &custom_style.minimize_btn_fg);
	hopalong_style_load_color_value(kf, "MinimizeButtonFGInactive", &custom_style.minimize_btn_fg_inactive);
	hopalong_style_load_color_value(kf, "MaximizeButtonFG", &custom_style.maximize_btn_fg);
	hopalong_style_load_color_value(kf, "MaximizeButtonFGInactive", &custom_style.maximize_btn_fg_inactive);
	hopalong_style_load_color_value(kf, "CloseButtonFG", &custom_style.close_btn_fg);
	hopalong_style_load_color_value(kf, "CloseButtonFGInactive", &custom_style.close_btn_fg_inactive);
	hopalong_style_load_color_value(kf, "Border", &custom_style.border);
	hopalong_style_load_color_value(kf, "BorderInactive", &custom_style.border_inactive);

	hopalong_style_load_size_value(kf, "BorderThickness", &custom_style.border_thickness);
	hopalong_style_load_size_value(kf, "TitleBarHeight", &custom_style.title_bar_height);
	hopalong_style_load_size_value(kf, "TitleBarPadding", &custom_style.title_bar_padding);

	hopalong_style_load_string_value(kf, "TitleBarFont", &custom_style.title_bar_font);

	g_key_file_free(kf);

	return &custom_style;
}