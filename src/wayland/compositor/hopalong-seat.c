/*
 * Hopalong - a friendly Wayland compositor
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 * 
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#include <stdlib.h>
#include "hopalong-seat.h"
#include "hopalong-server.h"
#include "hopalong-keybinding.h"
#include "protocol/xfway-shell-protocol.h"
#include <wlr/backend/wayland.h>
#include <wlr/backend/drm.h>
#include <wlr/backend/x11.h>
#include <linux/input.h>

#define XFWM_MOD_SHIFT     0x0001
#define XFWM_MOD_CONTROL   0x0002
#define XFWM_MOD_ALT       0x0004

bool cycling = FALSE;


static void
keyboard_handle_modifiers(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);
	return_if_fail(data != NULL);

	struct hopalong_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);

	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->device);
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat, &keyboard->device->keyboard->modifiers);
}

static void
keyboard_handle_key(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);
	return_if_fail(data != NULL);

  struct hopalong_keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct hopalong_server *server = keyboard->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *seat = server->seat;
  xkb_keysym_t tab_key;

        /* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
  
  if (server->is_windowed)
    tab_key = XKB_KEY_a;
  else
    tab_key = XKB_KEY_Tab;

	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(keyboard->device->keyboard->xkb_state, keycode, &syms);

	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
	bool handled = false;
    
  if (server->shell->child.desktop_shell)
  {
   for (int i = 0; i < nsyms; i++)
   {
   if (syms[i] == tab_key && modifiers == WLR_MODIFIER_ALT)
    {
        if ((enum wl_keyboard_key_state) event->state == WL_KEYBOARD_KEY_STATE_PRESSED)
	      {
          zxfwm_shell_send_tabwin (server->shell->child.desktop_shell,
                                  KEY_TAB,
                                  XFWM_MOD_ALT,
                                  (enum wl_keyboard_key_state) event->state == WL_KEYBOARD_KEY_STATE_PRESSED);
        
    
          cycling = TRUE;
        }
    }
    
      else if ((enum wl_keyboard_key_state) event->state == WL_KEYBOARD_KEY_STATE_RELEASED
               && syms[i] == XKB_KEY_Alt_L)
        {
          if (cycling)
            {
             zxfwm_shell_send_tabwin (server->shell->child.desktop_shell,
                                     KEY_TAB,
                                     XFWM_MOD_ALT,
                                     (enum wl_keyboard_key_state) event->state == WL_KEYBOARD_KEY_STATE_PRESSED);
              cycling = FALSE;
            }
      
       }   
   }
  }

	if ((enum wl_keyboard_key_state) event->state == WL_KEYBOARD_KEY_STATE_PRESSED)
	{
		for (int i = 0; i < nsyms; i++)
			handled = hopalong_keybinding_process(server, modifiers, syms[i]);
	}

	/* pass through */
	if (!handled)
	{
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
	}
}

static void
seat_new_keyboard(struct hopalong_server *server, struct wlr_input_device *device) 
{
	struct hopalong_keyboard *keyboard = calloc(1, sizeof(*keyboard));

	keyboard->server = server;
	keyboard->device = device;

	struct xkb_rule_names rules = { 0 };
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(device->keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);

	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&device->keyboard->events.key, &keyboard->key);

	wlr_seat_set_keyboard(server->seat, device);

	wl_list_insert(&server->keyboards, &keyboard->link);
}

static void
seat_new_pointer(struct hopalong_server *server, struct wlr_input_device *device)
{
	wlr_cursor_attach_input_device(server->cursor, device);
}

static void
hopalong_seat_new_input(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);
	return_if_fail(data != NULL);

	struct hopalong_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;

	switch (device->type)
	{
	case WLR_INPUT_DEVICE_KEYBOARD:
		seat_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		seat_new_pointer(server, device);
		break;
	default:
		break;
	}

	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;

	if (!wl_list_empty(&server->keyboards))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;

	wlr_seat_set_capabilities(server->seat, caps);
}

static void
hopalong_seat_request_cursor(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);
	return_if_fail(data != NULL);

	struct hopalong_server *server = wl_container_of(listener, server, request_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client = server->seat->pointer_state.focused_client;

	if (focused_client == event->seat_client)
		wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
}

static void
hopalong_seat_request_set_selection(struct wl_listener *listener, void *data)
{
	return_if_fail(listener != NULL);

	struct hopalong_server *server = wl_container_of(listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

void
hopalong_seat_setup(struct hopalong_server *server)
{
	wl_list_init(&server->keyboards);

	server->new_input.notify = hopalong_seat_new_input;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);

	/* XXX: this is definitely wrong, but systemd is also wrong */
	server->seat = wlr_seat_create(server->display, "seat0");

	server->request_cursor.notify = hopalong_seat_request_cursor;
	wl_signal_add(&server->seat->events.request_set_cursor, &server->request_cursor);

	server->request_set_selection.notify = hopalong_seat_request_set_selection;
	wl_signal_add(&server->seat->events.request_set_selection, &server->request_set_selection);
}

void
hopalong_seat_teardown(struct hopalong_server *server)
{
	if (server->seat)
		wlr_seat_destroy(server->seat);
}
