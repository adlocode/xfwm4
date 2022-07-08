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

#ifndef HOPALONG_COMPOSITOR_SHELL_H
#define HOPALONG_COMPOSITOR_SHELL_H

#include "hopalong-server.h"

extern struct hopalong_view *hopalong_shell_desktop_view_at(struct hopalong_server *server, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy);
extern bool hopalong_shell_view_at(struct hopalong_view *view,
        double lx, double ly, struct wlr_surface **surface, double *sx, double *sy);

#endif