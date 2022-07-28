/*
 * Copyright (C) 2018 - 2022 adlo
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "xfwm-shell.h"
#include <stdio.h>
#include <getopt.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include "os-compatibility.h"
#include "util/helpers.h"
//#include <wlr/types/wlr_seat.h>
//#include <wlr/util/log.h>
#include "util/signal.h"
#include <protocol/xfway-shell-protocol.h>

#define FOREIGN_TOPLEVEL_MANAGEMENT_V1_VERSION 1

static const struct zxfwm_shell_window_interface toplevel_handle_impl;

static struct wl_list child_process_list;

struct weston_process;
typedef void (*weston_process_cleanup_func_t)(struct weston_process *process,
					    int status);

struct weston_process {
	pid_t pid;
	weston_process_cleanup_func_t cleanup;
	struct wl_list link;
};

struct process_info {
	struct weston_process proc;
	char *path;
};

struct xfwm_tabwin
{
  struct hopalong_view *view;
  struct wlr_output *output;
  
  struct wl_listener surface_commit;
  struct wl_listener destroy;
};

static inline void *
zalloc(size_t size)
{
	return calloc(1, size);
}

static struct xfwm_shell_window *toplevel_handle_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
			&zxfwm_shell_window_interface,
			&toplevel_handle_impl));
	return wl_resource_get_user_data(resource);
}

static void toplevel_handle_send_maximized_event(struct wl_resource *resource,
		bool state) {
	struct xfwm_shell_window *toplevel =
		toplevel_handle_from_resource(resource);
	if (!toplevel) {
		return;
	}
	struct xfwm_shell_window_maximized_event event = {
		.toplevel = toplevel,
		.maximized = state,
	};
	wlr_signal_emit_safe(&toplevel->events.request_maximize, &event);
}

void foreign_toplevel_handle_set_maximized(struct wl_client *client,
		struct wl_resource *resource) {
	toplevel_handle_send_maximized_event(resource, true);
}

void foreign_toplevel_handle_unset_maximized(struct wl_client *client,
		struct wl_resource *resource) {
	toplevel_handle_send_maximized_event(resource, false);
}

static void toplevel_send_minimized_event(struct wl_resource *resource,
		bool state) {
	struct xfwm_shell_window *toplevel =
		toplevel_handle_from_resource(resource);
	if (!toplevel) {
		return;
	}

	struct xfwm_shell_window_minimized_event event = {
		.toplevel = toplevel,
		.minimized = state,
	};
	wlr_signal_emit_safe(&toplevel->events.request_minimize, &event);
}

static void foreign_toplevel_handle_set_minimized(struct wl_client *client,
		struct wl_resource *resource) {
	toplevel_send_minimized_event(resource, true);
}

static void foreign_toplevel_handle_unset_minimized(struct wl_client *client,
		struct wl_resource *resource) {
	toplevel_send_minimized_event(resource, false);
}

static void toplevel_send_fullscreen_event(struct wl_resource *resource,
		bool state, struct wl_resource *output_resource) {
	/*struct xfwm_shell_window *toplevel =
		toplevel_handle_from_resource(resource);
	if (!toplevel) {
		return;
	}

	struct wlr_output *output = NULL;
	if (output_resource) {
		output = wlr_output_from_resource(output_resource);
	}
	struct xfwm_shell_window_fullscreen_event event = {
		.toplevel = toplevel,
		.fullscreen = state,
		.output = output,
	};
	wlr_signal_emit_safe(&toplevel->events.request_fullscreen, &event);*/
}

static void foreign_toplevel_handle_set_fullscreen(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *output) {
	toplevel_send_fullscreen_event(resource, true, output);
}

static void foreign_toplevel_handle_unset_fullscreen(struct wl_client *client,
		struct wl_resource *resource) {
	toplevel_send_fullscreen_event(resource, false, NULL);
}

static void xfwm_shell_send_focus_signal (struct wl_client   *client,                                            
                                            struct wl_resource *handle_resource,
                                            struct wl_resource *seat_resource)
{
  struct xfwm_shell_window *toplevel;
  struct wlr_seat *seat;
  
  Shell *shell;
  
  toplevel = wl_resource_get_user_data (handle_resource);
  seat = wl_resource_get_user_data (seat_resource);
  //shell = wl_resource_get_user_data (shell_resource);
  //return;
  struct xfwm_shell_window_focus_event event = {
		.toplevel = toplevel,
		.seat = seat,
	};
	wlr_signal_emit_safe(&toplevel->events.shell_request_focus, &event);
  
}
  
static void xfwm_shell_send_raise_signal (struct wl_client   *client,                                            
                                            struct wl_resource *handle_resource,
                                            struct wl_resource *seat_resource)
{
  struct xfwm_shell_window *toplevel;
  struct wlr_seat *seat;  
  Shell *shell;
  
  toplevel = wl_resource_get_user_data (handle_resource);
  seat = wl_resource_get_user_data (seat_resource);
  //shell = wl_resource_get_user_data (shell_resource);
  
  struct xfwm_shell_window_raise_event event = {
		.toplevel = toplevel,
		.seat = seat,
	};
	wlr_signal_emit_safe(&toplevel->events.shell_request_raise, &event);
  
}

static void foreign_toplevel_handle_activate(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *seat_resource) {
	struct xfwm_shell_window *toplevel =
		toplevel_handle_from_resource(resource);
	if (!toplevel) {
		return;
	}

	struct weston_seat *seat = wl_resource_get_user_data (seat_resource);
	struct xfwm_shell_window_activated_event event = {
		.toplevel = toplevel,
		.seat = seat,
	};
	wlr_signal_emit_safe(&toplevel->events.request_activate, &event);
}

static void foreign_toplevel_handle_close(struct wl_client *client,
		struct wl_resource *resource) {
	struct xfwm_shell_window *toplevel =
		toplevel_handle_from_resource(resource);
	if (!toplevel) {
		return;
	}
	wlr_signal_emit_safe(&toplevel->events.request_close, toplevel);
}

static void foreign_toplevel_handle_set_rectangle(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *surface,
		int32_t x, int32_t y, int32_t width, int32_t height) {
	struct xfwm_shell_window *toplevel =
		toplevel_handle_from_resource(resource);
	if (!toplevel) {
		return;
	}

	if (width < 0 || height < 0) {
		wl_resource_post_error(resource,
			ZXFWM_SHELL_WINDOW_ERROR_INVALID_RECTANGLE,
			"invalid rectangle passed to set_rectangle: width/height < 0");
		return;
	}

	/*struct xfwm_shell_window_set_rectangle_event event = {
		.toplevel = toplevel,
		.surface = wlr_surface_from_resource(surface),
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};
	wlr_signal_emit_safe(&toplevel->events.set_rectangle, &event);*/
}

static void foreign_toplevel_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct zxfwm_shell_window_interface toplevel_handle_impl = {
	.set_maximized = foreign_toplevel_handle_set_maximized,
	.unset_maximized = foreign_toplevel_handle_unset_maximized,
	.set_minimized = foreign_toplevel_handle_set_minimized,
	.unset_minimized = foreign_toplevel_handle_unset_minimized,
  .focus = xfwm_shell_send_focus_signal,
  .raise = xfwm_shell_send_raise_signal,
	.activate = foreign_toplevel_handle_activate,
	.close = foreign_toplevel_handle_close,
	.set_rectangle = foreign_toplevel_handle_set_rectangle,
	.destroy = foreign_toplevel_handle_destroy,
	.set_fullscreen = foreign_toplevel_handle_set_fullscreen,
	.unset_fullscreen = foreign_toplevel_handle_unset_fullscreen,
};

static void toplevel_idle_send_done(void *data) {
	struct xfwm_shell_window *toplevel = data;
	struct wl_resource *resource;
	wl_resource_for_each(resource, &toplevel->resources) {
		zxfwm_shell_window_send_done(resource);
	}

	toplevel->idle_source = NULL;
}

static void toplevel_update_idle_source(
	struct xfwm_shell_window *toplevel) {
	if (toplevel->idle_source) {
		return;
	}

	toplevel->idle_source = wl_event_loop_add_idle(toplevel->manager->event_loop,
		toplevel_idle_send_done, toplevel);
}

void xfwm_shell_window_set_title(
		struct xfwm_shell_window *toplevel, const char *title) {
  if (toplevel->title)
	   free(toplevel->title);
	toplevel->title = strdup(title);

	struct wl_resource *resource;
	wl_resource_for_each(resource, &toplevel->resources) {
		zxfwm_shell_window_send_title(resource, title);
	}

	toplevel_update_idle_source(toplevel);
}

void xfwm_shell_window_set_app_id(
		struct xfwm_shell_window *toplevel, const char *app_id) {
	free(toplevel->app_id);
	toplevel->app_id = strdup(app_id);

	struct wl_resource *resource;
	wl_resource_for_each(resource, &toplevel->resources) {
		zxfwm_shell_window_send_app_id(resource, app_id);
	}

	toplevel_update_idle_source(toplevel);
}

/*static void send_output_to_resource(struct wl_resource *resource,
		struct wlr_output *output, bool enter) {
	struct wl_client *client = wl_resource_get_client(resource);
	struct wl_resource *output_resource;

	wl_resource_for_each(output_resource, &output->resources) {
		if (wl_resource_get_client(output_resource) == client) {
			if (enter) {
				zxfwm_shell_window_send_output_enter(resource,
					output_resource);
			} else {
				zxfwm_shell_window_send_output_leave(resource,
					output_resource);
			}
		}
	}
}*/

/*static void toplevel_send_output(struct xfwm_shell_window *toplevel,
		struct wlr_output *output, bool enter) {
	struct wl_resource *resource;
	wl_resource_for_each(resource, &toplevel->resources) {
		send_output_to_resource(resource, output, enter);
	}

	toplevel_update_idle_source(toplevel);
}

static void toplevel_handle_output_destroy(struct wl_listener *listener,
		void *data) {
	struct xfwm_shell_window_output *toplevel_output =
		wl_container_of(listener, toplevel_output, output_destroy);
	xfwm_shell_window_output_leave(toplevel_output->toplevel,
		toplevel_output->output);
}

void xfwm_shell_window_output_enter(
		struct xfwm_shell_window *toplevel,
		struct wlr_output *output) {
	struct xfwm_shell_window_output *toplevel_output;
	wl_list_for_each(toplevel_output, &toplevel->outputs, link) {
		if (toplevel_output->output == output) {
			return; // we have already sent output_enter event
		}
	}

	toplevel_output =
		calloc(1, sizeof(struct xfwm_shell_window_output));
	if (!toplevel_output) {
		wlr_log(WLR_ERROR, "failed to allocate memory for toplevel output");
		return;
	}

	toplevel_output->output = output;
	toplevel_output->toplevel = toplevel;
	wl_list_insert(&toplevel->outputs, &toplevel_output->link);

	toplevel_output->output_destroy.notify = toplevel_handle_output_destroy;
	wl_signal_add(&output->events.destroy, &toplevel_output->output_destroy);

	toplevel_send_output(toplevel, output, true);
}

static void toplevel_output_destroy(
		struct xfwm_shell_window_output *toplevel_output) {
	wl_list_remove(&toplevel_output->link);
	wl_list_remove(&toplevel_output->output_destroy.link);
	free(toplevel_output);
}

void xfwm_shell_window_output_leave(
		struct xfwm_shell_window *toplevel,
		struct wlr_output *output) {
	struct xfwm_shell_window_output *toplevel_output_iterator;
	struct xfwm_shell_window_output *toplevel_output = NULL;

	wl_list_for_each(toplevel_output_iterator, &toplevel->outputs, link) {
		if (toplevel_output_iterator->output == output) {
			toplevel_output = toplevel_output_iterator;
			break;
		}
	}

	if (toplevel_output) {
		toplevel_send_output(toplevel, output, false);
		toplevel_output_destroy(toplevel_output);
	} else {
		// XXX: log an error? crash?
	}
}*/

static bool fill_array_from_toplevel_state(struct wl_array *array,
		uint32_t state) {
	if (state & XFWM_SHELL_WINDOW_STATE_MAXIMIZED) {
		uint32_t *index = wl_array_add(array, sizeof(uint32_t));
		if (index == NULL) {
			return false;
		}
		*index = ZXFWM_SHELL_WINDOW_STATE_MAXIMIZED;
	}
	if (state & XFWM_SHELL_WINDOW_STATE_MINIMIZED) {
		uint32_t *index = wl_array_add(array, sizeof(uint32_t));
		if (index == NULL) {
			return false;
		}
		*index = ZXFWM_SHELL_WINDOW_STATE_MINIMIZED;
	}
	if (state & XFWM_SHELL_WINDOW_STATE_ACTIVATED) {
		uint32_t *index = wl_array_add(array, sizeof(uint32_t));
		if (index == NULL) {
			return false;
		}
		*index = ZXFWM_SHELL_WINDOW_STATE_ACTIVATED;
	}
	if (state & XFWM_SHELL_WINDOW_STATE_FULLSCREEN) {
		uint32_t *index = wl_array_add(array, sizeof(uint32_t));
		if (index == NULL) {
			return false;
		}
		*index = ZXFWM_SHELL_WINDOW_STATE_FULLSCREEN;
	}

	return true;
}

static void toplevel_send_state(struct xfwm_shell_window *toplevel) {
	struct wl_array states;
	wl_array_init(&states);
	bool r = fill_array_from_toplevel_state(&states, toplevel->state);
	if (!r) {
		struct wl_resource *resource;
		wl_resource_for_each(resource, &toplevel->resources) {
			wl_resource_post_no_memory(resource);
		}

		wl_array_release(&states);
		return;
	}

	struct wl_resource *resource;
	wl_resource_for_each(resource, &toplevel->resources) {
		zxfwm_shell_window_send_state(resource, &states);
	}

	wl_array_release(&states);
	toplevel_update_idle_source(toplevel);
}

void xfwm_shell_window_set_maximized(
		struct xfwm_shell_window *toplevel, bool maximized) {
	if (maximized) {
		toplevel->state |= XFWM_SHELL_WINDOW_STATE_MAXIMIZED;
	} else {
		toplevel->state &= ~XFWM_SHELL_WINDOW_STATE_MAXIMIZED;
	}
	toplevel_send_state(toplevel);
}

void xfwm_shell_window_set_minimized(
		struct xfwm_shell_window *toplevel, bool minimized) {
	if (minimized) {
		toplevel->state |= XFWM_SHELL_WINDOW_STATE_MINIMIZED;
	} else {
		toplevel->state &= ~XFWM_SHELL_WINDOW_STATE_MINIMIZED;
	}
	toplevel_send_state(toplevel);
}

void xfwm_shell_window_set_activated(
		struct xfwm_shell_window *toplevel, bool activated) {
	if (activated) {
		toplevel->state |= XFWM_SHELL_WINDOW_STATE_ACTIVATED;
	} else {
		toplevel->state &= ~XFWM_SHELL_WINDOW_STATE_ACTIVATED;
	}
	toplevel_send_state(toplevel);
}

void xfwm_shell_window_set_fullscreen(
		struct xfwm_shell_window * toplevel, bool fullscreen) {
	if (fullscreen) {
		toplevel->state |= XFWM_SHELL_WINDOW_STATE_FULLSCREEN;
	} else {
		toplevel->state &= ~XFWM_SHELL_WINDOW_STATE_FULLSCREEN;
	}
	toplevel_send_state(toplevel);
}

void xfwm_shell_window_destroy(
		struct xfwm_shell_window *toplevel) {
	if (!toplevel) {
		return;
	}

	wlr_signal_emit_safe(&toplevel->events.destroy, toplevel);

	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &toplevel->resources) {
		zxfwm_shell_window_send_closed(resource);
		wl_resource_set_user_data(resource, NULL);
		wl_list_remove(wl_resource_get_link(resource));
		wl_list_init(wl_resource_get_link(resource));
	}

	/*struct xfwm_shell_window_output *toplevel_output, *tmp2;
	wl_list_for_each_safe(toplevel_output, tmp2, &toplevel->outputs, link) {
		toplevel_output_destroy(toplevel_output);
	}*/

	if (toplevel->idle_source) {
		wl_event_source_remove(toplevel->idle_source);
	}

	wl_list_remove(&toplevel->link);

	free(toplevel->title);
	free(toplevel->app_id);
	free(toplevel);
}

static void foreign_toplevel_resource_destroy(struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

static struct wl_resource *create_toplevel_resource_for_resource(
		struct xfwm_shell_window *toplevel,
		struct wl_resource *manager_resource) {
	struct wl_client *client = wl_resource_get_client(manager_resource);
	struct wl_resource *resource = wl_resource_create(client,
			&zxfwm_shell_window_interface,
			wl_resource_get_version(manager_resource), 0);
	if (!resource) {
		wl_client_post_no_memory(client);
		return NULL;
	}

	wl_resource_set_implementation(resource, &toplevel_handle_impl, toplevel,
		foreign_toplevel_resource_destroy);

	wl_list_insert(&toplevel->resources, wl_resource_get_link(resource));
	zxfwm_shell_send_toplevel(manager_resource, resource);
	return resource;
}

struct xfwm_shell_window *
xfwm_shell_window_create(
		Shell *manager) {
	struct xfwm_shell_window *toplevel = calloc(1,
			sizeof(struct xfwm_shell_window));
	if (!toplevel) {
		return NULL;
	}

	wl_list_insert(&manager->toplevels, &toplevel->link);
	toplevel->manager = manager;

	wl_list_init(&toplevel->resources);
	wl_list_init(&toplevel->outputs);

	wl_signal_init(&toplevel->events.request_maximize);
	wl_signal_init(&toplevel->events.request_minimize);
	wl_signal_init(&toplevel->events.request_activate);      
	wl_signal_init(&toplevel->events.request_fullscreen);
	wl_signal_init(&toplevel->events.request_close);
	wl_signal_init(&toplevel->events.set_rectangle);
  wl_signal_init(&toplevel->events.shell_request_focus);
  wl_signal_init(&toplevel->events.shell_request_raise);
	wl_signal_init(&toplevel->events.destroy);
      
  toplevel->title = NULL;

	struct wl_resource *manager_resource, *tmp;
	wl_resource_for_each_safe(manager_resource, tmp, &manager->resources) {
		create_toplevel_resource_for_resource(toplevel, manager_resource);
	}

	return toplevel;
}

static const struct zxfwm_shell_interface
	foreign_toplevel_manager_impl;

static void
xfwm_shell_tabwin_nop (struct hopalong_view *view)
{
  
}

static struct wlr_surface *
xfwm_shell_tabwin_get_surface (struct hopalong_view *view)
{  
  return view->tabwin_surface;
}

static struct wlr_surface *
xfwm_shell_tabwin_surface_at (struct hopalong_view *view, double x, double y, double *sx, double *sy)
{
  return NULL;
}

static void
xfwm_shell_tabwin_destroy (struct wl_listener *listener,
                           void               *data)
{
  struct xfwm_tabwin *tabwin = wl_container_of (listener, tabwin, destroy);
  struct hopalong_view *view = tabwin->view;
  
  wl_list_remove (&tabwin->surface_commit.link);
  wl_list_remove (&tabwin->destroy.link);
  
  view->tabwin_surface = NULL;  
  
  hopalong_view_destroy (view);
  free (tabwin);
}

static const struct hopalong_view_ops hopalong_layer_shell_view_ops = {
	.minimize = xfwm_shell_tabwin_nop,
	.maximize = xfwm_shell_tabwin_nop,
	.close = xfwm_shell_tabwin_nop,
	.getprop = xfwm_shell_tabwin_nop,
	.get_surface = xfwm_shell_tabwin_get_surface,
	.set_activated = (void *) xfwm_shell_tabwin_nop,
	.get_geometry = xfwm_shell_tabwin_nop,
	.set_size = (void *) xfwm_shell_tabwin_nop,
	.surface_at = xfwm_shell_tabwin_surface_at,
	.can_move = xfwm_shell_tabwin_nop,
	.can_resize = xfwm_shell_tabwin_nop,
};

static void
xfwm_shell_tabwin_map (struct wl_listener *listener,
                       void               *data)
{
  
}

static void
xfwm_shell_tabwin_commit (struct wl_listener *listener,
                          void               *data)
{
  struct xfwm_tabwin *tabwin = wl_container_of (listener, tabwin, surface_commit);
  struct hopalong_view *view = tabwin->view;
  struct wlr_output *output = tabwin->output;
  
  view->using_csd = true;
  
  view->x = (output->width / 2) - (view->tabwin_surface->current.width / 2);
  view->y = (output->height / 2) - (view->tabwin_surface->current.height / 2);  
}

static void xfwm_shell_handle_set_tabwin (struct wl_client   *client,
                                          struct wl_resource *resource,
                                          struct wl_resource *surface_resource,
                                          struct wl_resource *output_resource)
{
  Shell *shell = wl_resource_get_user_data (resource);
  struct wlr_surface *surface = wl_resource_get_user_data (surface_resource);
  
  struct hopalong_server *server = shell->server;
  
  struct wlr_output *output = wl_resource_get_user_data (output_resource);
  
  if (output == NULL)
    {
      output = wlr_output_layout_output_at(
			server->output_layout, server->cursor->x,
			server->cursor->y);
    }
  
  struct hopalong_view *view = calloc(1, sizeof(*view));
  
  view->tabwin_surface = surface;
  
  view->server = server;
	view->ops = &hopalong_layer_shell_view_ops;
	view->using_csd = true;
  
  view->layer = HOPALONG_LAYER_OVERLAY;
  
  view->x = view->y = 64;
  
  struct xfwm_tabwin *tabwin = calloc(1, sizeof(*tabwin));
  
  tabwin->view = view;
  tabwin->output = output;  
  
  wlr_log (WLR_INFO, "\nwidth: %d\n", surface->current.width);
  
  view->map.notify = xfwm_shell_tabwin_map;
  
  tabwin->surface_commit.notify = xfwm_shell_tabwin_commit;
  wl_signal_add (&surface->events.commit, &tabwin->surface_commit);
  
  tabwin->destroy.notify = xfwm_shell_tabwin_destroy;
  wl_signal_add (&surface->events.destroy, &tabwin->destroy);
    
  wl_list_insert(&server->views, &view->link);
  hopalong_view_map (view);  
}

static void foreign_toplevel_manager_handle_stop(struct wl_client *client,
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&zxfwm_shell_interface,
		&foreign_toplevel_manager_impl));

	zxfwm_shell_send_finished(resource);
	wl_resource_destroy(resource);
}

static const struct zxfwm_shell_interface
		xfwm_desktop_shell_implementation = {
  .set_tabwin = xfwm_shell_handle_set_tabwin,
	.stop = foreign_toplevel_manager_handle_stop
};

static void foreign_toplevel_manager_resource_destroy(
		struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

static void toplevel_send_details_to_toplevel_resource(
		struct xfwm_shell_window *toplevel,
		struct wl_resource *resource) {
	if (toplevel->title) {
		zxfwm_shell_window_send_title(resource, toplevel->title);
	}
	if (toplevel->app_id) {
		zxfwm_shell_window_send_app_id(resource, toplevel->app_id);
	}

	/*struct xfwm_shell_window_output *output;
	wl_list_for_each(output, &toplevel->outputs, link) {
		send_output_to_resource(resource, output->output, true);
	}*/

	struct wl_array states;
	wl_array_init(&states);
	bool r = fill_array_from_toplevel_state(&states, toplevel->state);
	if (!r) {
		wl_resource_post_no_memory(resource);
		wl_array_release(&states);
		return;
	}

	zxfwm_shell_window_send_state(resource, &states);
	wl_array_release(&states);

	zxfwm_shell_window_send_done(resource);
}

static void foreign_toplevel_manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	Shell *manager = data;
	struct wl_resource *resource = wl_resource_create(client,
			&zxfwm_shell_interface, version, id);
	

	wl_resource_set_implementation(resource, &foreign_toplevel_manager_impl,
			manager, foreign_toplevel_manager_resource_destroy);	

}

static void
unbind_desktop_shell(struct wl_resource *resource)
{
	Shell *shell = wl_resource_get_user_data(resource);

	shell->child.desktop_shell = NULL;
}

static void
bind_desktop_shell(struct wl_client *client,
		   void *data, uint32_t version, uint32_t id)
{
	Shell *shell = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &zxfwm_shell_interface,
				      1, id);
  
  if (!resource) {
		wl_client_post_no_memory(client);
		return;
  }

  wlr_log (WLR_INFO, "\nbind desktop shell\n");

	if (client == shell->child.client) {
		wl_resource_set_implementation(resource,
					       &xfwm_desktop_shell_implementation,
					       shell, unbind_desktop_shell);
		shell->child.desktop_shell = resource;		
    
    wl_list_insert(&shell->resources, wl_resource_get_link(resource));

	  struct xfwm_shell_window *toplevel, *tmp;
	  wl_list_for_each_safe(toplevel, tmp, &shell->toplevels, link) {
		  struct wl_resource *toplevel_resource =
			  create_toplevel_resource_for_resource(toplevel, resource);
		  toplevel_send_details_to_toplevel_resource(toplevel,
			  toplevel_resource);
	  }
  return;
  }

	wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
			       "permission to bind desktop_shell denied");
}

WL_EXPORT size_t
weston_module_path_from_env(const char *name, char *path, size_t path_len)
{
	const char *mapping = getenv("WESTON_MODULE_MAP");
	const char *end;
	const int name_len = strlen(name);

	if (!mapping)
		return 0;

	end = mapping + strlen(mapping);
	while (mapping < end && *mapping) {
		const char *filename, *next;

		/* early out: impossibly short string */
		if (end - mapping < name_len + 1)
			return 0;

		filename = &mapping[name_len + 1];
		next = strchrnul(mapping, ';');

		if (strncmp(mapping, name, name_len) == 0 &&
		    mapping[name_len] == '=') {
			size_t file_len = next - filename; /* no trailing NUL */
			if (file_len >= path_len)
				return 0;
			strncpy(path, filename, file_len);
			path[file_len] = '\0';
			return file_len;
		}

		mapping = next + 1;
	}

	return 0;
}

WL_EXPORT char *
wet_get_binary_path(const char *name)
{
	char path[PATH_MAX];
	size_t len;
  
  wlr_log (WLR_INFO, "get binary path");
  wlr_log (WLR_INFO, BINDIR);

	len = weston_module_path_from_env(name, path, sizeof path);
	if (len > 0)
		return strdup(path);

	len = snprintf(path, sizeof path, "%s/%s", BINDIR, name);
	if (len >= sizeof path)
		return NULL;
  
  wlr_log (WLR_INFO, "exit get binary path");

	return strdup(path);
}

static void
child_client_exec(int sockfd, const char *path)
{
	int clientfd;
	char s[32];
	sigset_t allsigs;

	/* do not give our signal mask to the new process */
	sigfillset(&allsigs);
	sigprocmask(SIG_UNBLOCK, &allsigs, NULL);

	/* Launch clients as the user. Do not lauch clients with wrong euid.*/
	if (seteuid(getuid()) == -1) {
		wlr_log(WLR_ERROR, "compositor: failed seteuid\n");
		return;
	}

	/* SOCK_CLOEXEC closes both ends, so we dup the fd to get a
	 * non-CLOEXEC fd to pass through exec. */
	clientfd = dup(sockfd);
	if (clientfd == -1) {
		wlr_log(WLR_ERROR, "compositor: dup failed: %m\n");
		return;
	}

	snprintf(s, sizeof s, "%d", clientfd);
	setenv("WAYLAND_SOCKET", s, 1);

	if (execl(path, path, NULL) < 0)
		wlr_log(WLR_ERROR, "compositor: executing '%s' failed: %m\n",
			path);
}

static void
process_handle_sigchld(struct weston_process *process, int status)
{
	struct process_info *pinfo =
		container_of(process, struct process_info, proc);

	/*
	 * There are no guarantees whether this runs before or after
	 * the wl_client destructor.
	 */

	if (WIFEXITED(status)) {
		wlr_log (WLR_INFO, "%s exited with status %d\n", pinfo->path,
			   WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		wlr_log (WLR_INFO, "%s died on signal %d\n", pinfo->path,
			   WTERMSIG(status));
	} else {
		wlr_log (WLR_INFO, "%s disappeared\n", pinfo->path);
	}

	free(pinfo->path);
	free(pinfo);
}

WL_EXPORT void
weston_watch_process(struct weston_process *process)
{
	wl_list_insert(&child_process_list, &process->link);
}

WL_EXPORT struct wl_client *
weston_client_launch(Shell *server,
		     struct weston_process *proc,
		     const char *path,
		     weston_process_cleanup_func_t cleanup)
{
	int sv[2];
	pid_t pid;
	struct wl_client *client;

	wlr_log (WLR_INFO, "launching '%s'\n", path);

	if (os_socketpair_cloexec(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
		wlr_log(WLR_ERROR, "weston_client_launch: "
			"socketpair failed while launching '%s': %m\n",
			path);
		return NULL;
	}

	pid = fork();
	if (pid == -1) {
		close(sv[0]);
		close(sv[1]);
		wlr_log(WLR_ERROR, "weston_client_launch: "
			"fork failed while launching '%s': %m\n", path);
		return NULL;
	}

	if (pid == 0) {
		child_client_exec(sv[1], path);
		_exit(-1);
	}

	close(sv[1]);

	client = wl_client_create(server->display, sv[0]);
	if (!client) {
		close(sv[0]);
		wlr_log(WLR_ERROR, "weston_client_launch: "
			"wl_client_create failed while launching '%s'.\n",
			path);
		return NULL;
	}

	proc->pid = pid;
	proc->cleanup = cleanup;
	weston_watch_process(proc);

	return client;
}

WL_EXPORT struct wl_client *
weston_client_start(Shell *server, const char *path)
{
	struct process_info *pinfo;
	struct wl_client *client;

	pinfo = zalloc(sizeof *pinfo);
	if (!pinfo)
		return NULL;

	pinfo->path = strdup(path);
	if (!pinfo->path)
		goto out_free;

	client = weston_client_launch(server, &pinfo->proc, path,
				      process_handle_sigchld);
	if (!client)
		goto out_str;
  
  wlr_log (WLR_INFO, "\nclient started");

	return client;

out_str:
	free(pinfo->path);

out_free:
	free(pinfo);

	return NULL;
}

static void
desktop_shell_client_destroy(struct wl_listener *listener, void *data)
{
	Shell *shell;

	shell = container_of(listener, Shell,
			     child.client_destroy_listener);

	wl_list_remove(&shell->child.client_destroy_listener.link);
	shell->child.client = NULL;
	/*
	 * unbind_desktop_shell() will reset shell->child.desktop_shell
	 * before the respawned process has a chance to create a new
	 * desktop_shell object, because we are being called from the
	 * wl_client destructor which destroys all wl_resources before
	 * returning.
	 */

	//if (!check_desktop_shell_crash_too_early(shell))
		//respawn_desktop_shell_process(shell);

	//shell_fade_startup(shell);
}

static void
launch_desktop_shell_process(void *data)
{
	Shell *server = data;
  char *client;  
  
  wlr_log (WLR_INFO, "\nlaunch desktop shell\n");

  client = wet_get_binary_path ("xfwm4-wayland-shell");

  server->child.client = weston_client_start (server, client);

	if (!server->child.client) {
		wlr_log(WLR_ERROR, "not able to start client");
		return;
	}

	server->child.client_destroy_listener.notify =
		desktop_shell_client_destroy;
	wl_client_add_destroy_listener(server->child.client,
				       &server->child.client_destroy_listener);
}

void xfwm_shell_destroy(
		Shell *manager) {
	if (!manager) {
		return;
	}

	struct xfwm_shell_window *toplevel, *tmp_toplevel;
	wl_list_for_each_safe(toplevel, tmp_toplevel, &manager->toplevels, link) {
		xfwm_shell_window_destroy(toplevel);
	}

	struct wl_resource *resource, *tmp_resource;
	wl_resource_for_each_safe(resource, tmp_resource, &manager->resources) {
		wl_resource_destroy(resource);
	}

	wlr_signal_emit_safe(&manager->events.destroy, manager);
	wl_list_remove(&manager->display_destroy.link);

	wl_global_destroy(manager->global);
	free(manager);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	Shell *manager =
		wl_container_of(listener, manager, display_destroy);
	xfwm_shell_destroy(manager);
}

Shell *xfwm_shell_create(struct hopalong_server *server,
		                                 struct wl_display *display)
{
	Shell *manager = calloc(1,
			sizeof(Shell));
	if (!manager) {
		return NULL;
	}
                                     
  manager->display = display;
	manager->event_loop = wl_display_get_event_loop(display);
  manager->server = server;
      
  wl_list_init(&child_process_list);
  
	manager->global =  wl_global_create(display,
			&zxfwm_shell_interface,
			FOREIGN_TOPLEVEL_MANAGEMENT_V1_VERSION, manager,
			bind_desktop_shell);
                                     
 if (!manager->global) {
		free(manager);
		return NULL;
	} 
                                  
  wlr_log (WLR_INFO, "\ncreate shell\n");
  
	wl_event_loop_add_idle(manager->event_loop, launch_desktop_shell_process, manager);

	wl_signal_init(&manager->events.destroy);
	wl_list_init(&manager->resources);
	wl_list_init(&manager->toplevels);

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}
