  /*      $Id$

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., Inc., 51 Franklin Street, Fifth Floor, Boston,
        MA 02110-1301, USA.


        oroborus      - (c) 2001 Ken Lynch
        xfwm4         - (c) 2002-2011 Olivier Fourdan
        xfwm4-wayland - (c) 2018-2021 adlo

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/X.h>
#include <X11/Xlib.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkwayland.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include <wayland-client.h>
#include <protocol/xfway-shell-client-protocol.h>
#include <protocol/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h>
#include "protocol/wlr-layer-shell-unstable-v1-client-protocol.h"
#include "../util/libgwater-wayland.h"

#include "display.h"
#include "screen.h"
#include "events.h"
#include "event_filter.h"
#include "frame.h"
#include "settings.h"
#include "client.h"
#include "menu.h"
#include "focus.h"
#include "keyboard.h"
#include "workspaces.h"
#include "mywindow.h"
#include "session.h"
#include "startup_notification.h"
#include "compositor.h"
#include "spinning_cursor.h"

#define BASE_EVENT_MASK \
    SubstructureNotifyMask|\
    StructureNotifyMask|\
    SubstructureRedirectMask|\
    ButtonPressMask|\
    ButtonReleaseMask|\
    KeyPressMask|\
    KeyReleaseMask|\
    FocusChangeMask|\
    PropertyChangeMask|\
    ColormapChangeMask

#ifdef HAVE_COMPOSITOR
#define MAIN_EVENT_MASK BASE_EVENT_MASK|ExposureMask
#else /* HAVE_COMPOSITOR */
#define MAIN_EVENT_MASK BASE_EVENT_MASK
#endif /* HAVE_COMPOSITOR */

#ifdef HAVE_COMPOSITOR
static gboolean compositor = TRUE;
static vblankMode vblank_mode = VBLANK_AUTO;
#define XFWM4_ERROR      (xfwm4_error_quark ())

static GQuark
xfwm4_error_quark (void)
{
  return g_quark_from_static_string ("xfwm4-error-quark");
}
#endif /* HAVE_COMPOSITOR */

struct wl_display *wayland_display = NULL;
static struct wl_registry *registry = NULL;

enum toplevel_state_field {
	TOPLEVEL_STATE_MAXIMIZED = (1 << 0),
	TOPLEVEL_STATE_MINIMIZED = (1 << 1),
	TOPLEVEL_STATE_ACTIVATED = (1 << 2),
	TOPLEVEL_STATE_FULLSCREEN = (1 << 3),
	TOPLEVEL_STATE_INVALID = (1 << 4),
};

#ifdef DEBUG
static gboolean
setupLog (gboolean debug)
{
    const gchar *str;
    gchar *logfile;
    int fd;

    if (debug)
    {
        str = g_getenv ("XFWM4_LOG_FILE");
        if (str)
        {
            logfile = g_strdup (str);
        }
        else
        {
            logfile = g_strdup_printf ("xfwm4-debug-%d.log", (int) getpid ());
        }
    }
    else
    {
        logfile = "/dev/null";
    }

    fd = dup(fileno(stderr));
    if (fd == -1)
    {
        g_warning ("Fail to open %s: %s", logfile, g_strerror (errno));
        g_free (logfile);
        return FALSE;
    }

    if (!freopen (logfile, "w", stderr))
    {
        g_warning ("Fail to redirect stderr: %s", g_strerror (errno));
        g_free (logfile);
        close (fd);
        return FALSE;
    }

    if (debug)
    {
        g_print ("Logging to %s\n", logfile);
        g_free (logfile);
    }

    return TRUE;
}
#endif /* DEBUG */

static void shell_window_handle_title(void *data,
		struct zxfwm_shell_window *shell_window,
		const char *title)
{
  Client *c = data;
  if (c->name)
    g_free (c->name);
  c->name = g_strdup (title);
  g_print (c->name);
}

static void shell_window_handle_app_id(void *data,
		struct zxfwm_shell_window *shell_window,
		const char *app_id)
{

}

static uint32_t xfwm_array_to_state(struct wl_array *array) {
	uint32_t state = 0;
	uint32_t *entry;
	wl_array_for_each(entry, array) {
		if (*entry == ZXFWM_SHELL_WINDOW_STATE_MAXIMIZED)
			state |= TOPLEVEL_STATE_MAXIMIZED;
		if (*entry == ZXFWM_SHELL_WINDOW_STATE_MINIMIZED)
			state |= TOPLEVEL_STATE_MINIMIZED;
		if (*entry == ZXFWM_SHELL_WINDOW_STATE_ACTIVATED)
			state |= TOPLEVEL_STATE_ACTIVATED;
		if (*entry == ZXFWM_SHELL_WINDOW_STATE_FULLSCREEN)
			state |= TOPLEVEL_STATE_FULLSCREEN;
	}

	return state;
}

static void shell_window_handle_state(void *data,
		struct zxfwm_shell_window *shell_window,
		struct wl_array *state) {
	Client *c = data;
	uint32_t s = xfwm_array_to_state(state);

  if (s & TOPLEVEL_STATE_ACTIVATED){
    clientUpdateFocus (c->screen_info, c, NO_FOCUS_FLAG);
    g_print ("activate\n");
  }
}

static void shell_window_handle_done(void *data,
		struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel)
{

}

static void shell_window_handle_closed(void *data,
		struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel)
{
  Client *c = data;

  //if (c->name)
    //g_free (c->name);

  clientUnframeWayland (c, FALSE);
}

static const struct zxfwm_shell_window_listener shell_window_impl = {
	.title = shell_window_handle_title,
	.app_id = shell_window_handle_app_id,
	.output_enter = NULL,
	.output_leave = NULL,
	.state = shell_window_handle_state,
	.done = shell_window_handle_done,
	.closed = shell_window_handle_closed,
};

static void shell_handle_toplevel (void *data, struct zxfwm_shell *shell, struct zxfwm_shell_window *toplevel)
{
  ScreenInfo *screen_info = data;
  Client *c;
  
  g_print ("window opened");

  c = clientFrameWayland (screen_info, toplevel, FALSE);

  zxfwm_shell_window_add_listener (toplevel, &shell_window_impl,
                                                c);
  
}

static void shell_handle_finished (void *data, struct zxfwm_shell *shell)
{
  
}

static void shell_handle_tabwin (void *data, struct zxfwm_shell *shell, uint32_t key, uint32_t modifiers, uint32_t key_press)
{
  ScreenInfo *screen_info = data;
  XfwmEvent *event;
  eventFilterStatus status;
  g_print ("\ntabwin");
  
  event = g_new0 (XfwmEvent, 1);
  
  event->meta.type = XFWM_EVENT_KEY;
  
  event->key.pressed = key_press;
  event->key.keycode = key + 8;
  event->key.state = modifiers;
  event->key.root =screen_info->xroot;
  
  status = eventFilterIterate (screen_info->display_info->xfilter, event);
  
  xfwm_device_free_event (event);

}

static void shell_handle_tabwin_next (void *data, struct zxfwm_shell *shell)
{

}

static void shell_handle_tabwin_destroy (void *data, struct zxfwm_shell *shell)
{

}

struct zxfwm_shell_listener shell_impl = {
  shell_handle_toplevel,
  shell_handle_finished,
  shell_handle_tabwin,
  shell_handle_tabwin_next,
  shell_handle_tabwin_destroy,
};

static void toplevel_handle_title(void *data,
		struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
		const char *title)
{
  Client *c = data;
  if (c->name)
    g_free (c->name);
  c->name = g_strdup (title);
  g_print (c->name);
}

static void toplevel_handle_app_id(void *data,
		struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
		const char *app_id)
{

}

static uint32_t array_to_state(struct wl_array *array) {
	uint32_t state = 0;
	uint32_t *entry;
	wl_array_for_each(entry, array) {
		if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
			state |= TOPLEVEL_STATE_MAXIMIZED;
		if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
			state |= TOPLEVEL_STATE_MINIMIZED;
		if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
			state |= TOPLEVEL_STATE_ACTIVATED;
		if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
			state |= TOPLEVEL_STATE_FULLSCREEN;
	}

	return state;
}

static void toplevel_handle_state(void *data,
		struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
		struct wl_array *state) {
	Client *c = data;
	uint32_t s = array_to_state(state);

  if (s & TOPLEVEL_STATE_ACTIVATED){
    clientUpdateFocus (c->screen_info, c, NO_FOCUS_FLAG);
    g_print ("activate\n");
  }
}

static void toplevel_handle_done(void *data,
		struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel)
{

}

static void toplevel_handle_closed(void *data,
		struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel)
{
  Client *c = data;

  //if (c->name)
    //g_free (c->name);

  clientUnframeWayland (c, FALSE);
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_impl = {
	.title = toplevel_handle_title,
	.app_id = toplevel_handle_app_id,
	.output_enter = NULL,
	.output_leave = NULL,
	.state = toplevel_handle_state,
	.done = toplevel_handle_done,
	.closed = toplevel_handle_closed,
};

static void toplevel_manager_handle_toplevel(void *data,
		struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager,
		struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel)
{
  ScreenInfo *screen_info = data;
  Client *c;
  
  g_print ("window opened");

  c = clientFrameWayland (screen_info, zwlr_toplevel, FALSE);

  zwlr_foreign_toplevel_handle_v1_add_listener (zwlr_toplevel, &toplevel_impl,
                                                c);
}

static void toplevel_manager_handle_finished(void *data,
		struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager) {
	zwlr_foreign_toplevel_manager_v1_destroy(toplevel_manager);
}

static const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_impl = {
	.toplevel = toplevel_manager_handle_toplevel,
	.finished = toplevel_manager_handle_finished,
};

static void
wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t format, int32_t fd, uint32_t size)
{
       DisplayInfo *client_state = data;
  g_print ("keymap");
       //assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);       

       char *map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
       if (map_shm == MAP_FAILED)
    {
      return;
    }
       g_print ("keymap2");

       struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
                       client_state->xkb_context, map_shm,
                       XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
       munmap(map_shm, size);
       close(fd);
  
  g_print ("keymap3");

       struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
       xkb_keymap_unref(client_state->xkb_keymap);
       xkb_state_unref(client_state->xkb_state);
       client_state->xkb_keymap = xkb_keymap;
       client_state->xkb_state = xkb_state;
}
static void wl_keyboard_enter (void              *data,
                               uint32_t           serial,
                               struct wl_surface *surface,
                               uint32_t          *keys)
{
  
}
static void wl_keyboard_leave (void              *data,
                               uint32_t           serial,
                               struct wl_surface *surface)
{
  
}
static void
wl_keyboard_key (void     *data,
                 uint32_t  serial,
                 uint32_t  time,
                 uint32_t  key,
                 uint32_t  state)
{
  
}
static void
wl_keyboard_modifiers (void     *data,
                       uint32_t  serial,
                       uint32_t  mods_depressed,
                       uint32_t  mods_latched,
                       uint32_t  mods_locked)
{
  
}
static void
wl_keyboard_repeat_info (void    *data,
                         int32_t  rate,
                         int32_t  delay)
{
  
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
       .keymap = wl_keyboard_keymap,
       .enter = wl_keyboard_enter,
       .leave = wl_keyboard_leave,
       .key = wl_keyboard_key,
       .modifiers = wl_keyboard_modifiers,
       .repeat_info = wl_keyboard_repeat_info,
};

static void
wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
  ScreenInfo *screen_info = data;
  DisplayInfo *display_info = screen_info->display_info;
  g_print ("\n***seat capabilities***\n");
  
  bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

       if (have_keyboard && display_info->wl_keyboard == NULL) {
               display_info->wl_keyboard = wl_seat_get_keyboard(display_info->wl_seat);
               wl_keyboard_add_listener(display_info->wl_keyboard,
                               &wl_keyboard_listener, display_info);
       } else if (!have_keyboard && display_info->wl_keyboard != NULL) {
               wl_keyboard_release(display_info->wl_keyboard);
               display_info->wl_keyboard = NULL;
       }
}

static void
wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
       fprintf(stderr, "seat name: %s\n", name);
}

static const struct wl_seat_listener wl_seat_listener = {
       .capabilities = wl_seat_capabilities,
       .name = wl_seat_name,
};

void global_add (void               *data,
                 struct wl_registry *registry,
                 uint32_t            name,
                 const char         *interface,
                 uint32_t            version)
{
  ScreenInfo *screen_info = data;
  DisplayInfo *display_info = screen_info->display_info;

  if (strcmp (interface, "zxfwm_shell") == 0)
    {
      struct zxfwm_shell *shell = NULL;
      g_print ("xfway-shell\n");
      shell = wl_registry_bind (registry, name, &zxfwm_shell_interface, 1);
      screen_info->xfwm_shell = shell;

      zxfwm_shell_add_listener (shell, &shell_impl, screen_info);
    }
  else if (strcmp(interface,
			"zwlr_foreign_toplevel_manager_v1") == 0) {
		screen_info->toplevel_manager = wl_registry_bind(registry, name,
				&zwlr_foreign_toplevel_manager_v1_interface,
				2);

    zwlr_foreign_toplevel_manager_v1_add_listener(screen_info->toplevel_manager,
				&toplevel_manager_impl, screen_info);
        g_print ("foreign-toplevel\n");
      }
  else if (strcmp (interface, "zwlr_layer_shell_v1") == 0)
    {
      screen_info->layer_shell = wl_registry_bind (registry, name, &zwlr_layer_shell_v1_interface, 1);
      g_print ("\nlayer shell\n");
    }
  else if (strcmp(interface,
			"wl_seat") == 0) {
      display_info->wl_seat = wl_registry_bind(registry, name,
				&wl_seat_interface,
				2);
        wl_seat_add_listener(display_info->wl_seat,
				&wl_seat_listener, screen_info);
        g_print ("\n***seat***\n");
      }
  else if (strcmp(interface,
			"wl_output") == 0) {
      screen_info->wl_output = wl_registry_bind(registry, name,
				&wl_output_interface,
				1);        
        g_print ("\n***output***\n");
      }
}
void global_remove (void               *data,
                    struct wl_registry *registry,
                    uint32_t            name)
{

}

struct wl_registry_listener registry_listener =
{
  .global = global_add,
  .global_remove = global_remove
};

static void
handleSignal (int sig)
{
    DisplayInfo *display_info;

    display_info = myDisplayGetDefault ();
    if (display_info)
    {
        switch (sig)
        {
            case SIGINT:
                /* Walk thru */
            case SIGTERM:
                gtk_main_quit ();
                display_info->quit = TRUE;
                break;
            case SIGHUP:
                /* Walk thru */
            case SIGUSR1:
                display_info->reload = TRUE;
                break;
            default:
                break;
        }
    }
}

static void
setupHandler (gboolean install)
{
    struct sigaction act;

    if (install)
        act.sa_handler = handleSignal;
    else
        act.sa_handler = SIG_DFL;

    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGINT,  &act, NULL);
    sigaction (SIGTERM, &act, NULL);
    sigaction (SIGHUP,  &act, NULL);
    sigaction (SIGUSR1, &act, NULL);
}

static void
cleanUp (void)
{
    GSList *screens;
    DisplayInfo *display_info;

    TRACE ("entering");

    setupHandler (FALSE);

    display_info = myDisplayGetDefault ();
    g_return_if_fail (display_info);

    eventFilterClose (display_info->xfilter);
    for (screens = display_info->screens; screens; screens = g_slist_next (screens))
    {
        ScreenInfo *screen_info_n = (ScreenInfo *) screens->data;
        myScreenClose (screen_info_n);
        g_free (screen_info_n);
    }
    sn_close_display ();
    sessionFreeWindowStates ();

    myDisplayClose (display_info);
    g_free (display_info);

    xfconf_shutdown();
}

static void
ensure_basedir_spec (void)
{
    char *new, *old, path[PATH_MAX];
    GError *error;
    GDir *gdir;
    const char *name;

    /* test if new directory is there */

    new = xfce_resource_save_location (XFCE_RESOURCE_CONFIG,
                                       "xfce4" G_DIR_SEPARATOR_S "xfwm4",
                                       FALSE);

    if (g_file_test (new, G_FILE_TEST_IS_DIR))
    {
        g_free (new);
        return;
    }

    error = NULL;
    if (!xfce_mkdirhier(new, 0700, &error))
    {
        g_warning("Unable to create config dir %s: %s", new, error->message);
        g_error_free (error);
        g_free (new);
        return;
    }

    g_free (new);

    /* copy xfwm4rc */

    old = xfce_get_userfile ("xfwm4rc", NULL);

    if (g_file_test (old, G_FILE_TEST_EXISTS))
    {
        FILE *r, *w;

        g_strlcpy (path, "xfce4/xfwm4/xfwm4rc", PATH_MAX);
        new = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, path, FALSE);

        r = fopen (old, "r");
        w = fopen (new, "w");

        g_free (new);

        if (w && r)
        {
            int c;

            while ((c = getc (r)) != EOF)
            {
                putc (c, w);
            }
        }

        if (r)
        {
            fclose (r);
        }

        if (w)
        {
            fclose (w);
        }
    }

    g_free (old);

    /* copy saved session data */

    new = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "sessions", FALSE);
    if (!xfce_mkdirhier(new, 0700, &error))
    {
        g_warning("Unable to create session dir %s: %s", new, error->message);
        g_error_free (error);
        g_free (new);
        return;
    }

    old = xfce_get_userfile ("sessions", NULL);
    gdir = g_dir_open (old, 0, NULL);

    if (gdir)
    {
        while ((name = g_dir_read_name (gdir)) != NULL)
        {
            FILE *r, *w;

            g_snprintf (path, PATH_MAX, "%s/%s", old, name);
            r = fopen (path, "r");

            g_snprintf (path, PATH_MAX, "%s/%s", new, name);
            w = fopen (path, "w");

            if (w && r)
            {
                int c;

                while ((c = getc (r)) != EOF)
                    putc (c, w);
            }

            if (r)
                fclose (r);
            if (w)
                fclose (w);
        }

        g_dir_close (gdir);
    }

    g_free (old);
    g_free (new);
}

static void
print_version (void)
{
    g_print ("\tThis is %s version %s (revision %s) for Xfce %s\n",
                    PACKAGE, VERSION, REVISION, xfce_version_string());
    g_print ("\tReleased under the terms of the GNU General Public License.\n");
    g_print ("\tCompiled against GTK+-%d.%d.%d, ",
                    GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
    g_print ("using GTK+-%d.%d.%d.\n",
                    gtk_major_version, gtk_minor_version, gtk_micro_version);
    g_print ("\n");
    g_print ("\tBuild configuration and supported features:\n");

    g_print ("\t- Startup notification support:                 ");
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif

    g_print ("\t- XSync support:                                ");
#ifdef HAVE_XSYNC
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif

    g_print ("\t- Render support:                               ");
#ifdef HAVE_RENDER
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif

    g_print ("\t- Xrandr support:                               ");
#ifdef HAVE_RANDR
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif

    g_print ("\t- Xpresent support:                             ");
#ifdef HAVE_PRESENT_EXTENSION
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif

    g_print ("\t- X Input 2 support:                            ");
#ifdef HAVE_XI2
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif

    g_print ("\t- Embedded compositor:                          ");
#ifdef HAVE_COMPOSITOR
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif

#ifdef HAVE_COMPOSITOR
    g_print ("\t- Epoxy support:                                ");
#ifdef HAVE_EPOXY
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif /* HAVE_EPOXY */
#endif /* HAVE_COMPOSITOR */

    g_print ("\t- KDE systray proxy (deprecated):               ");
#ifdef ENABLE_KDE_SYSTRAY_PROXY
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif
}

#ifdef HAVE_COMPOSITOR
static gboolean
compositor_callback (const gchar  *name,
                     const gchar  *value,
                     gpointer      user_data,
                     GError      **error)
{
    gboolean succeed = TRUE;

    g_return_val_if_fail (value != NULL, FALSE);

    if (strcmp (value, "off") == 0)
    {
        compositor = FALSE;
    }
    else if (strcmp (value, "on") == 0)
    {
        compositor = TRUE;
    }
    else
    {
        g_set_error (error, XFWM4_ERROR, 0, "Unrecognized compositor option \"%s\"", value);
        succeed = FALSE;
    }

    return succeed;
}

static gboolean
vblank_callback (const gchar  *name,
                 const gchar  *value,
                 gpointer      user_data,
                 GError      **error)
{
    gboolean succeed = TRUE;

    g_return_val_if_fail (value != NULL, FALSE);

#ifdef HAVE_PRESENT_EXTENSION
    if (strcmp (value, "xpresent") == 0)
    {
        vblank_mode = VBLANK_XPRESENT;
    }
    else
#endif /* HAVE_PRESENT_EXTENSION */
#ifdef HAVE_EPOXY
    if (strcmp (value, "glx") == 0)
    {
        vblank_mode = VBLANK_GLX;
    }
    else
#endif /* HAVE_EPOXY */
    if (strcmp (value, "off") == 0)
    {
        vblank_mode = VBLANK_OFF;
    }
    else
    {
        g_set_error (error, XFWM4_ERROR, 0, "Unrecognized compositor option \"%s\"", value);
        succeed = FALSE;
    }

    return succeed;
}

static void
init_compositor_screen (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;

    display_info = screen_info->display_info;
    if (vblank_mode != VBLANK_AUTO)
    {
        compositorSetVblankMode (screen_info, vblank_mode);
    }

    if (display_info->enable_compositor)
    {
        gboolean xfwm4_compositor;

        xfwm4_compositor = TRUE;
        if (screen_info->params->use_compositing)
        {
            /* Enable compositor if "use compositing" is enabled */
            xfwm4_compositor = compositorManageScreen (screen_info);
        }
        /*
           The user may want to use the manual compositing, but the installed
           system may not support it, so we need to double check, to see if
           initialization of the compositor was successful.
          */
        if (xfwm4_compositor)
        {
            /*
               Acquire selection on XFWM4_COMPOSITING_MANAGER to advertise our own
               compositing manager (used by WM tweaks to determine whether or not
               show the "compositor" tab.
             */
            setAtomIdManagerOwner (display_info, XFWM4_COMPOSITING_MANAGER,
                                   screen_info->xroot, screen_info->xfwm4_win);
        }
    }
}
#endif /* HAVE_COMPOSITOR */

static int
initialize (gboolean replace_wm)
{
    DisplayInfo *display_info;
    gint i, nscreens, default_screen;

    DBG ("xfwm4 starting, using GTK+-%d.%d.%d", gtk_major_version,
         gtk_minor_version, gtk_micro_version);
    g_print ("initialize\n");

    ensure_basedir_spec ();

    initMenuEventWin ();
    clientClearFocus (NULL);
    display_info = myDisplayInit (gdk_display_get_default ());
  //g_print ("\ndisplay\n");
    //wl_display_roundtrip (display_info->wayland_display);
    //wl_display_roundtrip (display_info->wayland_display);
  
  //if (gdk_wayland_display_query_registry (display_info->gdisplay, "zwlr_foreign_toplevel_manager_v1" == TRUE))
    //{
      //g_print ("\nforeign toplevel\n");
    //}
g_print ("\ndisplay\n");
#ifdef HAVE_COMPOSITOR
    display_info->enable_compositor = compositor;
#else
    display_info->enable_compositor = FALSE;
#endif /* HAVE_COMPOSITOR */

    //initModifiers (display_info->dpy);

    //setupHandler (TRUE);
    
  //wl_display_flush (display_info->wayland_display);
  //wl_display_roundtrip (display_info->wayland_display);
    registry = wl_display_get_registry (display_info->wayland_display);

    //nscreens = ScreenCount (display_info->dpy);
    nscreens = 1;
    //default_screen = DefaultScreen (display_info->dpy);
    for(i = 0; i < nscreens; i++)
    {
        ScreenInfo *screen_info;
        GdkScreen *gscr;
        Window temp_xwindow;
        GdkWindow *screen_window;

        //if (i == default_screen)
        //{
            gscr = gdk_display_get_default_screen (display_info->gdisplay);
        //}
 /*       else
        {
            /* create temp 1x1 child window on this screen */
/*            temp_xwindow = XCreateSimpleWindow (display_info->dpy,
                                                RootWindow (display_info->dpy, i),
                                                0, 0, 1, 1, 0, 0, 0);
            /* allocate new GdkWindow with GdkScreen for this window */
/*            screen_window =
                gdk_x11_window_foreign_new_for_display (display_info->gdisplay,
                                                        temp_xwindow);
            XDestroyWindow (display_info->dpy, temp_xwindow);

            if (screen_window == NULL)
            {
                g_warning ("Cannot create GdkScreen for screen %i", i);
                continue;
            }

            gscr = gdk_window_get_screen (screen_window);

            /* foreign windows have 2 references */
/*            g_object_unref (screen_window);
            g_object_unref (screen_window);
        }*/
        screen_info = myScreenInit (display_info, gscr, MAIN_EVENT_MASK, replace_wm);

         g_print ("mid initialize\n");
          if (!screen_info)
        {            
          continue;
        }

        if (!initSettings (screen_info))
        {
          g_print ("no s\n");  
          return -2;
        }
          
         wl_registry_add_listener (registry, &registry_listener, screen_info);
         wl_display_roundtrip (display_info->wayland_display);
         //wl_display_roundtrip (display_info->wayland_display);
          
          if (GDK_IS_X11_DISPLAY (display_info->gdisplay))
            {
#ifdef HAVE_COMPOSITOR
        if (display_info->enable_compositor)
        {
            init_compositor_screen (screen_info);
        }}
#endif /* HAVE_COMPOSITOR */
        //sn_init_display (screen_info);
        myDisplayAddScreen (display_info, screen_info);
        /*screen_info->current_ws = getNetCurrentDesktop (display_info, screen_info->xroot);
        setUTF8StringHint (display_info, screen_info->xfwm4_win, NET_WM_NAME, "Xfwm4");
        setNetSupportedHint (display_info, screen_info->xroot, screen_info->xfwm4_win);
        setNetDesktopInfo (display_info, screen_info->xroot, screen_info->current_ws,
                                   screen_info->width,
                                   screen_info->height);
        workspaceUpdateArea (screen_info);
        XSetInputFocus (display_info->dpy, screen_info->xfwm4_win, RevertToPointerRoot, CurrentTime);

        clientFrameAll (screen_info);

        initPerScreenCallbacks (screen_info);

        XDefineCursor (display_info->dpy, screen_info->xroot, myDisplayGetCursorRoot(display_info));*/
    
        }

    /* No screen to manage, give up */
    if (!display_info->nb_screens)
    {        
      return -1;
    }
    display_info->xfilter = eventFilterInit (display_info->devices, (gpointer) display_info);
    eventFilterPush (display_info->xfilter, xfwm4_event_filter, (gpointer) display_info);
    //initPerDisplayCallbacks (display_info);
      
    g_print ("exit initialize\n");

    return sessionStart (display_info);
}

static void
init_pango_cache (void)
{
    GtkWidget *tmp_win;
    PangoLayout *layout;

    /*
     * The first time the first Gtk application on a display uses pango,
     * pango grabs the XServer while it creates the font cache window.
     * Therefore, force the cache window to be created now instead of
     * trying to do it while we have another grab and deadlocking the server.
     */
    tmp_win = gtk_window_new (GTK_WINDOW_POPUP);
    layout = gtk_widget_create_pango_layout (tmp_win, "-");
    pango_layout_get_pixel_extents (layout, NULL, NULL);
    g_object_unref (G_OBJECT (layout));
    gtk_widget_destroy (GTK_WIDGET (tmp_win));
}

int
main (int argc, char **argv)
{
    gboolean version = FALSE;
    gboolean replace_wm = FALSE;
    int status;
    GOptionContext *context;
    GError *error = NULL;
    GWaterWaylandSource *source;
#ifdef DEBUG
    gboolean debug = FALSE;
#endif /* DEBUG */
    GOptionEntry option_entries[] =
    {
#ifdef HAVE_COMPOSITOR
        { "compositor", 'c', 0, G_OPTION_ARG_CALLBACK,
          compositor_callback, N_("Set the compositor mode"), "on|off" },
        { "vblank", 'b', 0, G_OPTION_ARG_CALLBACK,
          vblank_callback, N_("Set the vblank mode"), "off"
#ifdef HAVE_PRESENT_EXTENSION
          "|xpresent"
#endif /* HAVE_PRESENT_EXTENSION */
#ifdef HAVE_EPOXY
          "|glx"
#endif /* HAVE_EPOXY */
        },
#endif /* HAVE_COMPOSITOR */
        { "replace", 'r', 0, G_OPTION_ARG_NONE,
          &replace_wm, N_("Replace the existing window manager"), NULL },
        { "version", 'V', 0, G_OPTION_ARG_NONE,
          &version, N_("Print version information and exit"), NULL },
#ifdef DEBUG
        { "debug", 'd', 0, G_OPTION_ARG_NONE,
          &debug, N_("Enable debug logging"), NULL },
#endif /* DEBUG */
        { NULL }
    };

#ifdef HAVE_EPOXY
    /* NVIDIA proprietary/closed source driver queues up to 2 frames by
     * default before blocking in glXSwapBuffers(), whereas our compositor
     * expects `glXSwapBuffers()` to block until the next vblank.
     *
     * To avoid that, our compositor was issuing a `glXWaitGL()` immediately
     * after the call to `glXSwapBuffers()` but that translates as a busy
     * wait, hence dramatically increasing CPU usage of xfwm4 with the
     * NVIDIA proprietary/closed source driver.
     *
     * Instruct the NVIDIA proprietary/closed source driver to allow only
     * 1 frame using the environment variable “__GL_MaxFramesAllowed” so
     * that it matches our expectations.
     *
     * This must be set before libGL is loaded, hence before gtk_init().
     *
     * Taken from similar patch posted by NVIDIA developer for kwin:
     * https://phabricator.kde.org/D19867
     */
    g_setenv("__GL_MaxFramesAllowed", "1", TRUE);
#endif /* HAVE_EPOXY */

    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    /* xfwm4 is an X11 window manager, no point in trying to connect to
     * any other display server (like when running nested within a
     * Wayland compositor).
     */
    //gdk_set_allowed_backends ("x11");

#ifndef HAVE_XI2
    /* Disable XI2 in GDK */
    gdk_disable_multidevice ();
#endif

    context = g_option_context_new (_("[ARGUMENTS...]"));
    g_option_context_add_main_entries (context, option_entries, GETTEXT_PACKAGE);
    g_option_context_add_group (context, gtk_get_option_group (FALSE));
    g_option_context_add_group (context, xfce_sm_client_get_option_group (argc, argv));
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
          g_print ("%s: %s.\n", PACKAGE_NAME, error->message);
          g_print (_("Type \"%s --help\" for usage."), G_LOG_DOMAIN);
          g_print ("\n");
          g_error_free (error);

          return EXIT_FAILURE;
    }
    g_option_context_free (context);

#ifdef DEBUG
    setupLog (debug);
#endif /* DEBUG */
    DBG ("xfwm4 starting");
    g_print ("%s", "xfwm4 starting");    

    gtk_init (&argc, &argv);

    if (G_UNLIKELY (version))
    {
         print_version ();
         return EXIT_SUCCESS;
    }
    init_pango_cache ();

    status = initialize (replace_wm);
   
     /*
       status  < 0   =>   Error, cancel execution
       status == 0   =>   Run w/out session manager
       status == 1   =>   Connected to session manager
     */
    switch (status)
    {
        case -1:
            g_warning ("Could not find a screen to manage, exiting");
            exit (1);
            break;
        case -2:
            g_warning ("Missing data from default files");
            exit (1);
            break;
        case 0:
        case 1:
            /* enter GTK main loop */
            g_print ("gtk\n");

       //source = g_water_wayland_source_new_for_display (NULL, display_info->wayland_display);

      GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_widget_show (window);
            gtk_main ();
            break;
        default:
            g_warning ("Unknown error occurred");
            exit (1);
            break;
    }
    //cleanUp ();
    DBG ("xfwm4 terminated");
  g_print ("xfwm4 terminated\n");
    return 0;
}
