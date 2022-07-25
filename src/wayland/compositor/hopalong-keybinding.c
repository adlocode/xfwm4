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

#include "hopalong-server.h"
#include "hopalong-keybinding.h"
#include <linux/input.h>
#include "protocol/xfway-shell-protocol.h"

#define XFWM_MOD_SHIFT     0x0001
#define XFWM_MOD_CONTROL   0x0002
#define XFWM_MOD_ALT       0x0004

#if 0

struct hopalong_keybinding {
	uint32_t modifiers;
	xkb_keysym_t sym;
	void (*action)(struct hopalong_server *server, uint32_t modifiers, xkb_keysym_t sym);
};

#endif

bool
hopalong_keybinding_process(struct hopalong_server *server, uint32_t modifiers, xkb_keysym_t sym)
{
	struct hopalong_keybinding *binding;

	wl_list_for_each_reverse(binding, &server->keybindings, link)
	{
		if (modifiers == binding->modifiers && sym == binding->sym)
		{
			binding->action(server, modifiers, sym);
			return true;
		}
	}

	return false;
}

static void
switch_vt(struct hopalong_server *server, uint32_t modifiers, xkb_keysym_t sym)
{
	struct wlr_session *session = wlr_backend_get_session(server->backend);

	if (session != NULL)
	{
		unsigned vt = sym - XKB_KEY_XF86Switch_VT_1 + 1;
		wlr_session_change_vt(session, vt);
	}
}

static void
terminate(struct hopalong_server *server, uint32_t modifiers, xkb_keysym_t sym)
{
	wl_display_terminate(server->display);
}

static void
switch_activity(struct hopalong_server *server, uint32_t modifiers, xkb_keysym_t sym)
{
	/* backwards or forwards? */
	bool backwards = (modifiers & WLR_MODIFIER_SHIFT) != 0;

	if (wl_list_length(&server->mapped_layers[HOPALONG_LAYER_MIDDLE]) < 2)
		return;

	struct hopalong_view *current_view = wl_container_of(server->mapped_layers[HOPALONG_LAYER_MIDDLE].next, current_view, mapped_link);
	struct hopalong_view *last_view = wl_container_of(server->mapped_layers[HOPALONG_LAYER_MIDDLE].prev, last_view, mapped_link);
	struct hopalong_view *next_view = backwards ? last_view : wl_container_of(current_view->mapped_link.next, next_view, mapped_link);

	return_if_fail(next_view != NULL);
  
  wlr_log (WLR_INFO, "\nswitch activity\n");
  
  //if (server->shell->child.desktop_shell)
    //zxfwm_shell_send_tabwin (server->shell->child.desktop_shell, KEY_TAB, XFWM_MOD_ALT, 1);

	hopalong_view_focus(next_view, hopalong_view_get_surface(next_view));

	if (backwards)
		return;

	/* Move the previous view to the end of the list */
	wl_list_remove(&current_view->mapped_link);
	wl_list_insert(server->mapped_layers[HOPALONG_LAYER_MIDDLE].prev, &current_view->mapped_link);
}

static void
toggle_title_bar(struct hopalong_server *server, uint32_t modifiers, xkb_keysym_t sym)
{
	struct hopalong_view *current_view = wl_container_of(server->mapped_layers[HOPALONG_LAYER_MIDDLE].next, current_view, mapped_link);

	if (current_view != NULL)
		current_view->hide_title_bar ^= true;
}

static struct hopalong_keybinding *
hopalong_keybinding_add(struct hopalong_server *server, uint32_t modifiers, xkb_keysym_t sym,
	void (*action)(struct hopalong_server *server, uint32_t modifiers, xkb_keysym_t sym))
{
	struct hopalong_keybinding *binding = calloc(1, sizeof(*binding));

	binding->modifiers = modifiers;
	binding->sym = sym;
	binding->action = action;
	wl_list_insert(&server->keybindings, &binding->link);

	return binding;
}

void
hopalong_keybinding_setup(struct hopalong_server *server)
{
	wl_list_init(&server->keybindings);

	/* XXX: make keybindings actually configurable */
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_1, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_2, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_3, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_4, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_5, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_6, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_7, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_8, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_9, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_10, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_11, switch_vt);
	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_12, switch_vt);

	hopalong_keybinding_add(server, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_BackSpace, terminate);

  //This code commented out as this is now implemented client-side
	/*
  hopalong_keybinding_add(server, WLR_MODIFIER_SHIFT | WLR_MODIFIER_ALT, XKB_KEY_Tab, switch_activity);
	hopalong_keybinding_add(server, WLR_MODIFIER_ALT, XKB_KEY_Tab, switch_activity);
  
  hopalong_keybinding_add(server, WLR_MODIFIER_ALT, XKB_KEY_a, switch_activity);

	hopalong_keybinding_add(server, WLR_MODIFIER_SHIFT | WLR_MODIFIER_ALT, XKB_KEY_ISO_Left_Tab, switch_activity);
	hopalong_keybinding_add(server, WLR_MODIFIER_ALT, XKB_KEY_ISO_Left_Tab, switch_activity);
  */

	hopalong_keybinding_add(server, WLR_MODIFIER_SHIFT | WLR_MODIFIER_ALT, XKB_KEY_D, toggle_title_bar);
}

void
hopalong_keybinding_teardown(struct hopalong_server *server)
{
	struct hopalong_keybinding *binding, *next;

	wl_list_for_each_safe(binding, next, &server->keybindings, link)
	{
		wl_list_remove(&binding->link);
		free(binding);
	}
}
