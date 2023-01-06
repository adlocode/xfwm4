#ifndef _XFWM_XDG_SHELL_H
#define _XFWM_XDG_SHELL_H

#include "server.h"

void init_xdg_shell(xfwmServer *server);
void focus_view(View *view, struct wlr_surface *surface);
struct wlr_output *get_active_output(View *view);
View *get_view_at(
		xfwmServer *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);
#endif
