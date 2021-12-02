/* Copyright (C) 2019 adlo
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server.h>
#include <libweston/libweston.h>
#include "wlr_layer_shell_v1.h"
//#include <wlr/types/wlr_output.h>
//#include <wlr/types/wlr_surface.h>
//#include <wlr/types/wlr_xdg_shell.h>
//#include <wlr/util/log.h>
#include "util/signal.h"
#include <protocol/wlr-layer-shell-unstable-v1-protocol.h>

const enum zwlr_layer_shell_v1_layer t = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
const enum zwlr_layer_shell_v1_layer r = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
const enum zwlr_layer_shell_v1_layer b = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
const enum zwlr_layer_shell_v1_layer l = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;

typedef struct
{
  int32_t width;
  int32_t height;
} coords;

struct lsh_margin {
	int32_t top, right, bottom, left;
};

void position(struct wlr_layer_surface_v1 *surface,
              coords surface_size,
              coords output_size,
              int *out_x, int *out_y) {
		int32_t w, h, ow, oh, x = 0, y = 0;
    uint32_t anchor = surface->current.anchor;
		w = surface_size.width;
    h = surface_size.height;
    ow = output_size.width;
    oh = output_size.height;

		if ((anchor & (t | b)) == (t | b) ||
		    !(((anchor & t) != 0) || ((anchor & b) != 0))) {  // both or neither
			y = oh / 2 - h / 2 + surface->current.margin.top / 2 - surface->current.margin.bottom / 2;
		} else if ((anchor & b) != 0) {
			y = oh - h - surface->current.margin.bottom;
		} else if ((anchor & t) != 0) {
			y = surface->current.margin.top;
		}
		if ((anchor & (l | r)) == (l | r) ||
		    !(((anchor & l) != 0) || ((anchor & r) != 0))) {  // both or neither
			x = ow / 2 - w / 2 + surface->current.margin.left / 2 - surface->current.margin.right / 2;
		} else if ((anchor & r) != 0) {
			x = ow - w - surface->current.margin.right;
		} else if ((anchor & l) != 0) {
			x = surface->current.margin.left;
		}

    *out_x = x;
    *out_y = y;
	}

void next_size(struct wlr_layer_surface_v1 *surface,
               coords old_size, coords output_size,
               int32_t *out_w, int32_t *out_h) {
		int32_t w, h, ow, oh, rw, rh;
    uint32_t anchor = surface->current.anchor;
		w = old_size.width;
    h = old_size.height;
    ow = output_size.width;
    oh = output_size.height;
		//std::tie(rw, rh) = req_size;
		if (rw > 0) {
			w = rw;
		}
		if (rh > 0) {
			h = rh;
		}
		if ((anchor & (l | r)) == (l | r)) {
			w = ow - surface->current.margin.left - surface->current.margin.right;
		}
		if ((anchor & (t | b)) == (t | b)) {
			h = oh - surface->current.margin.top - surface->current.margin.bottom;
		}
		*out_w = w;
    *out_h = h;
	}

static void resource_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct zwlr_layer_shell_v1_interface layer_shell_implementation;
static const struct zwlr_layer_surface_v1_interface layer_surface_implementation;

static struct wlr_layer_shell_v1 *layer_shell_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &zwlr_layer_shell_v1_interface,
		&layer_shell_implementation));
	return wl_resource_get_user_data(resource);
}

static struct wlr_layer_surface_v1 *layer_surface_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &zwlr_layer_surface_v1_interface,
		&layer_surface_implementation));
	return wl_resource_get_user_data(resource);
}

//static const struct wlr_surface_role layer_surface_role;

bool wlr_surface_is_layer_surface(struct wlr_surface *surface) {
	//return surface->role == &layer_surface_role;
}

struct wlr_layer_surface_v1 *wlr_layer_surface_v1_from_wlr_surface(
		struct wlr_surface *surface) {
	assert(wlr_surface_is_layer_surface(surface));
	//return (struct wlr_layer_surface_v1 *)surface->role_data;
}

static void layer_surface_configure_destroy(
		struct wlr_layer_surface_v1_configure *configure) {
	if (configure == NULL) {
		return;
	}
	wl_list_remove(&configure->link);
	free(configure);
}

static void layer_surface_handle_ack_configure(struct wl_client *client,
		struct wl_resource *resource, uint32_t serial) {
	struct wlr_layer_surface_v1 *surface = layer_surface_from_resource(resource);

	bool found = false;
	struct wlr_layer_surface_v1_configure *configure, *tmp;
	wl_list_for_each_safe(configure, tmp, &surface->configure_list, link) {
		if (configure->serial < serial) {
			layer_surface_configure_destroy(configure);
		} else if (configure->serial == serial) {
			found = true;
			break;
		} else {
			break;
		}
	}
	if (!found) {
		wl_resource_post_error(resource,
			ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SURFACE_STATE,
			"wrong configure serial: %u", serial);
		return;
	}

	if (surface->acked_configure) {
		layer_surface_configure_destroy(surface->acked_configure);
	}
	surface->acked_configure = configure;
	wl_list_remove(&configure->link);
	wl_list_init(&configure->link);
}

static void layer_surface_handle_set_size(struct wl_client *client,
		struct wl_resource *resource, uint32_t width, uint32_t height) {
	struct wlr_layer_surface_v1 *surface = layer_surface_from_resource(resource);
	surface->client_pending.desired_width = width;
	surface->client_pending.desired_height = height;
}

static void layer_surface_handle_set_anchor(struct wl_client *client,
		struct wl_resource *resource, uint32_t anchor) {
	const uint32_t max_anchor =
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	if (anchor > max_anchor) {
		wl_resource_post_error(resource,
			ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_ANCHOR,
			"invalid anchor %d", anchor);
	}
	struct wlr_layer_surface_v1 *surface = layer_surface_from_resource(resource);
	surface->client_pending.anchor = anchor;
}

static void layer_surface_handle_set_exclusive_zone(struct wl_client *client,
		struct wl_resource *resource, int32_t zone) {
	struct wlr_layer_surface_v1 *surface = layer_surface_from_resource(resource);
	surface->client_pending.exclusive_zone = zone;
}

static void layer_surface_handle_set_margin(
		struct wl_client *client, struct wl_resource *resource,
		int32_t top, int32_t right, int32_t bottom, int32_t left) {
	struct wlr_layer_surface_v1 *surface = layer_surface_from_resource(resource);
	surface->client_pending.margin.top = top;
	surface->client_pending.margin.right = right;
	surface->client_pending.margin.bottom = bottom;
	surface->client_pending.margin.left = left;
}

static void layer_surface_handle_set_keyboard_interactivity(
		struct wl_client *client, struct wl_resource *resource,
		uint32_t interactive) {
	struct wlr_layer_surface_v1 *surface = layer_surface_from_resource(resource);
	surface->client_pending.keyboard_interactive = !!interactive;
}

static void layer_surface_handle_get_popup(struct wl_client *client,
		struct wl_resource *layer_resource,
		struct wl_resource *popup_resource) {
	/*struct wlr_layer_surface_v1 *parent =
		layer_surface_from_resource(layer_resource);
	struct wlr_xdg_surface *popup_surface =
		wlr_xdg_surface_from_popup_resource(popup_resource);

	assert(popup_surface->role == WLR_XDG_SURFACE_ROLE_POPUP);
	struct wlr_xdg_popup *popup = popup_surface->popup;
	popup->parent = parent->surface;
	wl_list_insert(&parent->popups, &popup->link);
	wlr_signal_emit_safe(&parent->events.new_popup, popup);*/
}

static const struct zwlr_layer_surface_v1_interface layer_surface_implementation = {
	.destroy = resource_handle_destroy,
	.ack_configure = layer_surface_handle_ack_configure,
	.set_size = layer_surface_handle_set_size,
	.set_anchor = layer_surface_handle_set_anchor,
	.set_exclusive_zone = layer_surface_handle_set_exclusive_zone,
	.set_margin = layer_surface_handle_set_margin,
	.set_keyboard_interactivity = layer_surface_handle_set_keyboard_interactivity,
	.get_popup = layer_surface_handle_get_popup,
};

static void layer_surface_unmap(struct wlr_layer_surface_v1 *surface) {
	// TODO: probably need to ungrab before this event
	wlr_signal_emit_safe(&surface->events.unmap, surface);

	struct wlr_layer_surface_v1_configure *configure, *tmp;
	wl_list_for_each_safe(configure, tmp, &surface->configure_list, link) {
		layer_surface_configure_destroy(configure);
	}

	surface->configured = surface->mapped = false;
	surface->configure_serial = 0;
	if (surface->configure_idle) {
		wl_event_source_remove(surface->configure_idle);
		surface->configure_idle = NULL;
	}
	surface->configure_next_serial = 0;
}

static void layer_surface_destroy(struct wlr_layer_surface_v1 *surface) {
	if (surface->configured && surface->mapped) {
		layer_surface_unmap(surface);
	}
	wlr_signal_emit_safe(&surface->events.destroy, surface);
	wl_resource_set_user_data(surface->resource, NULL);
	surface->surface->role_name = NULL;
	wl_list_remove(&surface->surface_destroy.link);
	wl_list_remove(&surface->link);
  weston_view_damage_below (surface->view);
  weston_view_destroy (surface->view);
  weston_surface_unmap (surface->surface);
  weston_compositor_schedule_repaint (surface->surface->compositor);
  surface->surface->committed = NULL;
  surface->surface->committed_private = NULL;
	free(surface->namespace);
	free(surface);
}

static void layer_surface_resource_destroy(struct wl_resource *resource) {
	struct wlr_layer_surface_v1 *surface =
		layer_surface_from_resource(resource);
	if (surface != NULL) {
		layer_surface_destroy(surface);
	}
}

static bool layer_surface_state_changed(struct wlr_layer_surface_v1 *surface) {
	struct wlr_layer_surface_v1_state *state;
	if (wl_list_empty(&surface->configure_list)) {
		if (surface->acked_configure) {
			state = &surface->acked_configure->state;
		} else if (!surface->configured) {
			return true;
		} else {
			state = &surface->current;
		}
	} else {
		struct wlr_layer_surface_v1_configure *configure =
			wl_container_of(surface->configure_list.prev, configure, link);
		state = &configure->state;
	}

	bool changed = state->actual_width != surface->server_pending.actual_width
		|| state->actual_height != surface->server_pending.actual_height;
	return changed;
}

void wlr_layer_surface_v1_configure(struct wlr_layer_surface_v1 *surface,
		uint32_t width, uint32_t height) {
	surface->server_pending.actual_width = width;
	surface->server_pending.actual_height = height;
	if (layer_surface_state_changed(surface)) {
		struct wl_display *display =
			wl_client_get_display(wl_resource_get_client(surface->resource));
		struct wlr_layer_surface_v1_configure *configure =
			calloc(1, sizeof(struct wlr_layer_surface_v1_configure));
		if (configure == NULL) {
			wl_client_post_no_memory(wl_resource_get_client(surface->resource));
			return;
		}
		surface->configure_next_serial = wl_display_next_serial(display);
		wl_list_insert(surface->configure_list.prev, &configure->link);
		configure->state.actual_width = width;
		configure->state.actual_height = height;
		configure->serial = surface->configure_next_serial;
		zwlr_layer_surface_v1_send_configure(surface->resource,
				configure->serial, configure->state.actual_width,
				configure->state.actual_height);
	}
}

void wlr_layer_surface_v1_close(struct wlr_layer_surface_v1 *surface) {
	if (surface->closed) {
		return;
	}
	surface->closed = true;
	layer_surface_unmap(surface);
	zwlr_layer_surface_v1_send_closed(surface->resource);
}

static void layer_surface_role_commit(struct weston_surface *weston_surface,
                                      int32_t                sx,
                                      int32_t                sy) {
	struct wlr_layer_surface_v1 *surface =
		weston_surface->committed_private;
	if (surface == NULL) {
		return;
	}

  if (!weston_view_is_mapped (surface->view))
      {
        switch (surface->layer)
          {
            break;
          case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
            weston_layer_entry_insert (&surface->shell->xfwm_display->background_layer.view_list,
                                       &surface->view->layer_link);
            break;
          case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
            weston_layer_entry_insert (&surface->shell->xfwm_display->bottom_layer.view_list,
                                       &surface->view->layer_link);
            break;
          case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
            weston_layer_entry_insert (&surface->shell->xfwm_display->top_layer.view_list,
                                       &surface->view->layer_link);
            break;
          case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
            weston_layer_entry_insert (&surface->shell->xfwm_display->overlay_layer.view_list,
                                       &surface->view->layer_link);
          }
        surface->view->is_mapped = true;
      }

  if (surface->view->output)
    surface->view->output = surface->head;
  if (surface->view->output == NULL)
    weston_view_update_transform (surface->view);
  if (surface->view->output == NULL)
    return;

	if (surface->closed) {
		// Ignore commits after the compositor has closed it
		return;
	}

	if (surface->acked_configure) {
		struct wlr_layer_surface_v1_configure *configure =
			surface->acked_configure;
		surface->configured = true;
		surface->configure_serial = configure->serial;
		surface->current.actual_width = configure->state.actual_width;
		surface->current.actual_height = configure->state.actual_height;
		layer_surface_configure_destroy(configure);
		surface->acked_configure = NULL;
	}

	/*if (weston_view_is_mapped (surface->view) && !surface->configured) {
			ZWLR_LAYER_SHELL_V1_ERROR_ALREADY_CONSTRUCTED,
			"layer_surface has never been configured");
		return;
	}*/

	surface->current.anchor = surface->client_pending.anchor;
	surface->current.exclusive_zone = surface->client_pending.exclusive_zone;
	surface->current.margin = surface->client_pending.margin;
	surface->current.keyboard_interactive =
		surface->client_pending.keyboard_interactive;
	surface->current.desired_width = surface->client_pending.desired_width;
	surface->current.desired_height = surface->client_pending.desired_height;

  coords surface_size, output_size;
  surface_size.width = surface->current.desired_width;
  surface_size.height = surface->current.desired_height;
  output_size.width = surface->view->output->width;
  output_size.height = surface->view->output->height;
  int32_t x, y, nw, nh;
  position (surface, surface_size, output_size, &x, &y);
  weston_view_set_position (surface->view, x + surface->view->output->x, y + surface->view->output->y);
  next_size (surface, surface_size, output_size, &nw, &nh);
  if (nw != weston_surface->width || nh != weston_surface->height)
    zwlr_layer_surface_v1_send_configure (surface->resource, 0, nw, nh);

  weston_view_update_transform (surface->view);
  weston_surface_damage (weston_surface);
  weston_compositor_schedule_repaint (weston_surface->compositor);

	if (!surface->added) {
		surface->added = true;
		wlr_signal_emit_safe(&surface->shell->events.new_surface,
				surface);
		// either the compositor found a suitable output or it must
		// have closed the surface
		assert(surface->output || surface->closed);
	}
	if (surface->configured && //weston_view_is_mapped (surface->view) &&
			!surface->mapped) {
		surface->mapped = true;
		wlr_signal_emit_safe(&surface->events.map, surface);
        weston_log ("map\n");
	}
	if (surface->configured && !weston_view_is_mapped (surface->view) &&
			surface->mapped) {
		layer_surface_unmap(surface);
	}
}

/*static const struct wlr_surface_role layer_surface_role = {
	.name = "zwlr_layer_surface_v1",
	.commit = layer_surface_role_commit,
};*/

static void handle_surface_destroyed(struct wl_listener *listener,
		void *data) {
	struct wlr_layer_surface_v1 *layer_surface =
		wl_container_of(listener, layer_surface, surface_destroy);
	layer_surface_destroy(layer_surface);
}

static void layer_shell_handle_get_layer_surface(struct wl_client *wl_client,
		struct wl_resource *client_resource, uint32_t id,
		struct wl_resource *surface_resource,
		struct wl_resource *output_resource,
		uint32_t layer, const char *namespace) {
	struct wlr_layer_shell_v1 *shell =
		layer_shell_from_resource(client_resource);
  struct weston_surface *weston_surface = wl_resource_get_user_data (surface_resource);

	struct wlr_layer_surface_v1 *surface =
		calloc(1, sizeof(struct wlr_layer_surface_v1));
	if (surface == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}

	if (weston_surface_set_role (weston_surface, "zwlr_layer_surface_v1", surface_resource,
                                ZWLR_LAYER_SHELL_V1_ERROR_ROLE) < 0) {
		free(surface);
		return;
	}

  surface->shell = shell;
	surface->surface = weston_surface;
  surface->resource = surface_resource;

  surface->view = weston_view_create (weston_surface);

  if (output_resource != NULL)
  surface->head = wl_resource_get_user_data (output_resource);
  else
    {
      surface->head = NULL;
    }

  weston_surface->committed_private = surface;
  weston_surface->committed = layer_surface_role_commit;

	if (output_resource) {
		surface->output = wl_resource_get_user_data (surface_resource);
	}
	surface->layer = layer;
	if (layer > ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
		free(surface);
		wl_resource_post_error(client_resource,
				ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER,
				"Invalid layer %d", layer);
		return;
	}
	surface->namespace = strdup(namespace);
	if (surface->namespace == NULL) {
		free(surface);
		wl_client_post_no_memory(wl_client);
		return;
	}
	surface->resource = wl_resource_create(wl_client,
		&zwlr_layer_surface_v1_interface,
		wl_resource_get_version(client_resource),
		id);
	if (surface->resource == NULL) {
		free(surface->namespace);
		free(surface);
		wl_client_post_no_memory(wl_client);
		return;
	}

	wl_list_init(&surface->configure_list);
	wl_list_init(&surface->popups);

	wl_signal_init(&surface->events.destroy);
	wl_signal_init(&surface->events.map);
	wl_signal_init(&surface->events.unmap);
	wl_signal_init(&surface->events.new_popup);

	wl_signal_add(&surface->surface->destroy_signal,
		&surface->surface_destroy);
	surface->surface_destroy.notify = handle_surface_destroyed;

	weston_log ("new layer_surface %p (res %p)",
			surface, surface->resource);
	wl_resource_set_implementation(surface->resource,
		&layer_surface_implementation, surface, layer_surface_resource_destroy);
	wl_list_insert(&shell->surfaces, &surface->link);
}

static const struct zwlr_layer_shell_v1_interface layer_shell_implementation = {
	.get_layer_surface = layer_shell_handle_get_layer_surface,
};

static void client_handle_destroy(struct wl_resource *resource) {
	struct wl_client *client = wl_resource_get_client(resource);
	struct wlr_layer_shell_v1 *shell = layer_shell_from_resource(resource);
	struct wlr_layer_surface_v1 *surface, *tmp = NULL;
	wl_list_for_each_safe(surface, tmp, &shell->surfaces, link) {
		if (wl_resource_get_client(surface->resource) == client) {
			layer_surface_destroy(surface);
		}
	}
	wl_list_remove(wl_resource_get_link(resource));
}

static void layer_shell_bind(struct wl_client *wl_client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_layer_shell_v1 *layer_shell = data;
	assert(wl_client && layer_shell);

	struct wl_resource *resource = wl_resource_create(
			wl_client, &zwlr_layer_shell_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	wl_resource_set_implementation(resource,
			&layer_shell_implementation, layer_shell, client_handle_destroy);
	wl_list_insert(&layer_shell->resources, wl_resource_get_link(resource));
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_layer_shell_v1 *layer_shell =
		wl_container_of(listener, layer_shell, display_destroy);
	wlr_layer_shell_v1_destroy(layer_shell);
}

struct wlr_layer_shell_v1 *wlr_layer_shell_v1_create(struct wl_display *display, xfwmDisplay *xfwm_display) {
	struct wlr_layer_shell_v1 *layer_shell =
		calloc(1, sizeof(struct wlr_layer_shell_v1));
	if (!layer_shell) {
		return NULL;
	}

	wl_list_init(&layer_shell->resources);
	wl_list_init(&layer_shell->surfaces);

	struct wl_global *global = wl_global_create(display,
		&zwlr_layer_shell_v1_interface, 1, layer_shell, layer_shell_bind);
	if (!global) {
		free(layer_shell);
		return NULL;
	}
	layer_shell->global = global;

  layer_shell->xfwm_display = xfwm_display;

	wl_signal_init(&layer_shell->events.new_surface);
	wl_signal_init(&layer_shell->events.destroy);

	layer_shell->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &layer_shell->display_destroy);

	return layer_shell;
}

void wlr_layer_shell_v1_destroy(struct wlr_layer_shell_v1 *layer_shell) {
	if (!layer_shell) {
		return;
	}
	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &layer_shell->resources) {
		wl_resource_destroy(resource);
	}
	wlr_signal_emit_safe(&layer_shell->events.destroy, layer_shell);
	wl_list_remove(&layer_shell->display_destroy.link);
	wl_global_destroy(layer_shell->global);
	free(layer_shell);
}

/*struct layer_surface_iterator_data {
	wlr_surface_iterator_func_t user_iterator;
	void *user_data;
	int x, y;
};

/*static void layer_surface_iterator(struct wlr_surface *surface,
		int sx, int sy, void *data) {
	struct layer_surface_iterator_data *iter_data = data;
	iter_data->user_iterator(surface, iter_data->x + sx, iter_data->y + sy,
		iter_data->user_data);
}

/*static void xdg_surface_for_each_surface(struct wlr_xdg_surface *surface,
		int x, int y, wlr_surface_iterator_func_t iterator, void *user_data) {
	struct layer_surface_iterator_data data = {
		.user_iterator = iterator,
		.user_data = user_data,
		.x = x, .y = y,
	};
	wlr_surface_for_each_surface(
			surface->surface, layer_surface_iterator, &data);

	struct wlr_xdg_popup *popup_state;
	wl_list_for_each(popup_state, &surface->popups, link) {
		struct wlr_xdg_surface *popup = popup_state->base;
		if (!popup->configured) {
			continue;
		}

		double popup_sx = popup_state->geometry.x - popup_state->base->geometry.x;
		double popup_sy = popup_state->geometry.y - popup_state->base->geometry.y;

		xdg_surface_for_each_surface(popup,
			x + popup_sx,
			y + popup_sy,
			iterator, user_data);
	}
}

/*static void layer_surface_for_each_surface(struct wlr_layer_surface_v1 *surface,
		int x, int y, wlr_surface_iterator_func_t iterator, void *user_data) {
	struct layer_surface_iterator_data data = {
		.user_iterator = iterator,
		.user_data = user_data,
		.x = x, .y = y,
	};
	wlr_surface_for_each_surface(surface->surface,
			layer_surface_iterator, &data);

	struct wlr_xdg_popup *popup_state;
	wl_list_for_each(popup_state, &surface->popups, link) {
		struct wlr_xdg_surface *popup = popup_state->base;
		if (!popup->configured) {
			continue;
		}

		double popup_sx, popup_sy;
		popup_sx = popup->popup->geometry.x - popup->geometry.x;
		popup_sy = popup->popup->geometry.y - popup->geometry.y;

		xdg_surface_for_each_surface(popup,
			popup_sx, popup_sy, iterator, user_data);
	}
}

/*void wlr_layer_surface_v1_for_each_surface(struct wlr_layer_surface_v1 *surface,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	layer_surface_for_each_surface(surface, 0, 0, iterator, user_data);
}

struct wlr_surface *wlr_layer_surface_v1_surface_at(
		struct wlr_layer_surface_v1 *surface, double sx, double sy,
		double *sub_x, double *sub_y) {
	struct wlr_xdg_popup *popup_state;
	wl_list_for_each(popup_state, &surface->popups, link) {
		struct wlr_xdg_surface *popup = popup_state->base;

		double popup_sx = popup_state->geometry.x - popup->geometry.x;
		double popup_sy = popup_state->geometry.y - popup->geometry.y;

		struct wlr_surface *sub = wlr_xdg_surface_surface_at(popup,
			sx - popup_sx,
			sy - popup_sy,
			sub_x, sub_y);
		if (sub != NULL) {
			return sub;
		}
	}

	return wlr_surface_surface_at(surface->surface, sx, sy, sub_x, sub_y);
}*/
