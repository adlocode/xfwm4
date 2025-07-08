#ifndef _XFWM_CURSOR_H
#define _XFWM_CURSOR_H
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

struct _xfwmWaylandCompositor;
typedef struct _xfwmWaylandCompositor xfwmWaylandCompositor;

enum CursorMode {
	XFWM_CURSOR_PASSTHROUGH,
	XFWM_CURSOR_MOVE,
	XFWM_CURSOR_RESIZE,
};

struct _xfwmCursor {
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;

	xfwmWaylandCompositor *server;

	enum CursorMode cursor_mode;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;

	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wl_listener request_cursor;
};

typedef struct _xfwmCursor xfwmCursor;

xfwmCursor *cursor_create(xfwmWaylandCompositor *server);
void cursor_destroy(xfwmCursor *cursor);
void reset_cursor_mode(xfwmWaylandCompositor *server);

#endif /* cursor.h */
