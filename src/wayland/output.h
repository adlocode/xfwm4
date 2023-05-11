/* Copyright (c) 2018 - 2023 adlo
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _XFWM_OUTPUT_H
#define _XFWM_OUTPUT_H

#include <stdlib.h>
#include <time.h>

#include "server.h"

struct _Output {
	struct wlr_output *wlr_output;
	xfwmWaylandCompositor *server;

	struct {
		struct wlr_scene_tree *shell_background;
		struct wlr_scene_tree *shell_bottom;
		struct wlr_scene_tree *shell_fullscreen;
		struct wlr_scene_tree *shell_overlay;
		struct wlr_scene_tree *shell_top;
	} layers;

#if ! WLR_CHECK_VERSION(0, 17, 0)
	/* DEPRECATED: Use a tool like swaybg instead */
	struct wlr_scene_rect *background;
#endif
	struct wlr_box geometry;

	struct wl_listener destroy;
	struct wl_listener frame;
	struct wl_listener request_state;

	struct wl_list link;
};

typedef struct _Output Output;

void output_frame_notify(struct wl_listener* listener, void *data);
void output_destroy_notify(struct wl_listener* listener, void *data);
void new_output_notify(struct wl_listener* listener, void *data);

#endif /* output.h */
