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

#include <wayland-server.h>
#include <libweston/libweston.h>
#include <libweston-desktop/libweston-desktop.h>
#include <protocol/window-switcher-unstable-v1-server-protocol.h>
#include <gtk/gtk.h>

struct weston_window_switcher
{
  struct weston_compositor *compositor;
  struct wl_client *client;
  struct wl_resource *binding;
  struct wl_list windows;
};

struct weston_window_switcher_window
{
  struct wl_list link;
  struct weston_window_switcher *switcher;
  struct wl_resource *resource;
  struct weston_desktop_surface *surface;
  struct weston_view *view;
  struct wl_listener surface_destroy_listener;
  struct wl_listener view_destroy_listener;
};

static void _weston_window_switcher_request_destroy (struct wl_client   *client,
                                                     struct wl_resource *resource)
{

}

static void
_weston_window_switcher_window_surface_destroyed (struct wl_listener *listener,
                                                  void               *data)
{
  struct weston_window_switcher_window *self = wl_container_of (listener, self, surface_destroy_listener);

  self->surface = NULL;
}

static void
_weston_window_switcher_window_destroy (struct wl_resource *resource)
{

}

static void
_weston_window_switcher_window_request_switch_to (struct wl_client   *client,
                                                  struct wl_resource *resource,
                                                  struct wl_resource *seat_resource,
                                                  uint32_t            serial)
{
  struct weston_window_switcher_window *self = wl_resource_get_user_data (resource);
  struct weston_surface *surface = weston_desktop_surface_get_surface (self->surface);
  struct weston_seat *seat = wl_resource_get_user_data (seat_resource);
  struct weston_keyboard *keyboard = weston_seat_get_keyboard (seat);
  struct weston_pointer *pointer = weston_seat_get_pointer (seat);
  struct weston_touch *touch = weston_seat_get_touch (seat);

  if (keyboard == NULL)
    return;

  if ((keyboard != NULL) && (keyboard->grab_serial == serial))
    weston_keyboard_set_focus (keyboard, surface);
  else if ((pointer != NULL) && (pointer->grab_serial == serial))
    weston_keyboard_set_focus (keyboard, surface);
  else if ((touch != NULL) && (touch->grab_serial == serial))
    weston_keyboard_set_focus (keyboard, surface);

}

static void
_weston_window_switcher_window_request_close (struct wl_client   *client,
                                              struct wl_resource *resource,
                                              struct wl_resource *seat_resource,
                                              uint32_t            serial)
{
  struct weston_window_switcher_window *self = wl_resource_get_user_data (resource);
  struct weston_seat *seat = wl_resource_get_user_data (seat_resource);
  struct weston_keyboard *keyboard = weston_seat_get_keyboard (seat);
  struct weston_pointer *pointer = weston_seat_get_pointer (seat);
  struct weston_touch *touch = weston_seat_get_touch (seat);

  if (keyboard == NULL)
    return;

  if ((keyboard != NULL) && (keyboard->grab_serial == serial))
    weston_desktop_surface_close (self->surface);
  else if ((pointer != NULL) && (pointer->grab_serial == serial))
    weston_desktop_surface_close (self->surface);
  else if ((touch != NULL) && (touch->grab_serial == serial))
    weston_desktop_surface_close (self->surface);
}

static void
_weston_window_switcher_window_request_show (struct wl_client   *client,
                                             struct wl_resource *resource,
                                             struct wl_resource *surface,
                                             int32_t             x,
                                             int32_t             y,
                                             int32_t             width,
                                             int32_t             height)
{

}

static const struct zww_window_switcher_window_v1_interface weston_window_switcher_window_implementation = {
  .destroy = _weston_window_switcher_request_destroy,
  .switch_to = _weston_window_switcher_window_request_switch_to,
  .close = _weston_window_switcher_window_request_close,
  .show = _weston_window_switcher_window_request_show,
};

void
_weston_window_switcher_window_create (struct weston_window_switcher *switcher,
                                       struct weston_surface         *surface)
{
  struct weston_window_switcher_window *self;
  struct weston_desktop_surface *dsurface = weston_surface_get_desktop_surface (surface);

  if (dsurface == NULL)
    return;

  if (switcher->client == NULL)
    return;

  weston_log ("\nserver: window create\n");

  wl_list_for_each (self,&switcher->windows, link)
    {
      if (self->surface == dsurface)
        return;
    }

  self = zalloc (sizeof (struct weston_window_switcher_window));
  if (self == NULL)
    {
      wl_client_post_no_memory (switcher->client);
      return;
    }

  self->switcher = switcher;
  self->surface = dsurface;

  self->resource = wl_resource_create (switcher->client, &zww_window_switcher_window_v1_interface,
                                       wl_resource_get_version (switcher->binding), 0);
  if (self->resource == NULL)
    {
      wl_client_post_no_memory (switcher->client);
      return;
    }

  self->surface_destroy_listener.notify = _weston_window_switcher_window_surface_destroyed;
  wl_signal_add (&surface->destroy_signal, &self->surface_destroy_listener);
  wl_resource_set_implementation(self->resource, &weston_window_switcher_window_implementation,
                                 self, _weston_window_switcher_window_destroy);
  zww_window_switcher_v1_send_window (switcher->binding, self->resource);

  const char *title = weston_desktop_surface_get_title (self->surface);
  if (title != NULL)
      zww_window_switcher_window_v1_send_title (self->resource, title);
const char *app_id = weston_desktop_surface_get_app_id (self->surface);
if (app_id != NULL)
      zww_window_switcher_window_v1_send_app_id (self->resource, app_id);
zww_window_switcher_window_v1_send_done (self->resource);

}

static const struct zww_window_switcher_v1_interface weston_window_switcher_implementation =
{
  .destroy = _weston_window_switcher_request_destroy,
};

static void
_weston_window_switcher_bind (struct wl_client *client,
                              void             *data,
                              uint32_t          version,
                              uint32_t          id)
{
  struct weston_window_switcher *self = data;
  struct wl_resource *resource;

  weston_log ("\nserver: switcher bind\n");

  resource = wl_resource_create (client, &zww_window_switcher_v1_interface, version, id);
  wl_resource_set_implementation (resource, &weston_window_switcher_implementation,
                                  self, NULL);

  if (self->binding != NULL)
    {
      wl_resource_post_error (resource, ZWW_WINDOW_SWITCHER_V1_ERROR_BOUND,
                              "interface object already bound");
      wl_resource_destroy (resource);
      return;
    }

  self->client = client;
  self->binding = resource;

  struct weston_view *view;
  wl_list_for_each (view, &self->compositor->view_list, link)
    _weston_window_switcher_window_create (self, view->surface);
}

WL_EXPORT int
weston_window_switcher_module_init (struct weston_compositor *compositor,
                                    struct weston_window_switcher **out_switcher,
                                    int argc, char *argv[])
{
  struct weston_window_switcher *window_switcher;
  window_switcher = zalloc (sizeof (struct weston_window_switcher));

  window_switcher->compositor = compositor;
  window_switcher->client = NULL;

  wl_list_init (&window_switcher->windows);

  if (wl_global_create (window_switcher->compositor->wl_display,
                        &zww_window_switcher_v1_interface, 1,
                        window_switcher, _weston_window_switcher_bind) == NULL)
    return -1;

  *out_switcher = window_switcher;

  return 0;
}
