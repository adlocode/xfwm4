#ifndef _XFWM_XDG_SHELL_H
#define _XFWM_XDG_SHELL_H

#include "server.h"

typedef struct _Client Client;

void init_xdg_shell(xfwmWaylandCompositor *server);
void focus_view(Client *view, struct wlr_surface *surface);
struct wlr_output *get_active_output(Client *view);
Client *get_view_at(
		xfwmWaylandCompositor *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);
#endif
