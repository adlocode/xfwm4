#ifndef _XFWM_DECORATION_H
#define _XFWM_DECORATION_H

#include <wlr/types/wlr_xdg_decoration_v1.h>

#include "server.h"

struct _xfwmDecoration {
	xfwmWaylandCompositor *server;

	struct wl_listener toplevel_decoration_destroy;
	struct wl_listener request_mode;
	struct wl_listener mode_destroy;
};

typedef struct _xfwmDecoration xfwmDecoration;

void init_xdg_decoration(xfwmWaylandCompositor *server);
#endif
