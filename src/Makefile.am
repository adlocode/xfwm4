bin_PROGRAMS = xfwm4 xfwm4-wayland xfwm4-wayland-shell

xfwm4_wayland_CPPFLAGS = 					\
	-DBINDIR='"$(bindir)"'			\
	-DLIBEXECDIR='"$(libexecdir)"'	

xfwm4_SOURCES =								\
	client.c							\
	client.h							\
	compositor.c							\
	compositor.h							\
	cycle.c								\
	cycle.h								\
	device.c							\
	device.h							\
	display.c							\
	display.h							\
	event_filter.c							\
	event_filter.h							\
	events.c							\
	events.h							\
	focus.c								\
	focus.h								\
	frame.c								\
	frame.h								\
	hints.c								\
	hints.h								\
	icons.c								\
	icons.h								\
	keyboard.c							\
	keyboard.h							\
	main.c								\
	menu.c								\
	menu.h								\
	misc.c								\
	misc.h								\
	moveresize.c							\
	moveresize.h							\
	mypixmap.c							\
	mypixmap.h							\
	mywindow.c							\
	mywindow.h							\
	netwm.c								\
	netwm.h								\
	parserc.c							\
	parserc.h							\
	placement.c							\
	placement.h							\
	poswin.c							\
	poswin.h							\
	screen.c							\
	screen.h							\
	session.c							\
	session.h							\
	settings.c							\
	settings.h							\
	spinning_cursor.c						\
	spinning_cursor.h						\
	stacking.c							\
	stacking.h							\
	startup_notification.c						\
	startup_notification.h						\
	tabwin.c							\
	tabwin.h							\
	terminate.c							\
	terminate.h							\
	transients.c							\
	transients.h							\
	ui_style.c							\
	ui_style.h							\
	wireframe.c							\
	wireframe.h							\
	workspaces.c							\
	workspaces.h							\
	xsync.c								\
	xsync.h								\
		$(top_srcdir)/util/libgwater-wayland.c 				\
	$(top_srcdir)/util/libgwater-wayland.h
	xpm-color-table.h

xfwm4_CFLAGS =								\
	$(GLIB_CFLAGS) 							\
	$(GTK_CFLAGS) 							\
	$(COMPOSITOR_CFLAGS)						\
	$(EPOXY_CFLAGS)							\
	$(LIBSTARTUP_NOTIFICATION_CFLAGS)				\
	$(LIBX11_CFLAGS)						\
	$(LIBXFCE4KBD_PRIVATE_CFLAGS)					\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCONF_CFLAGS)						\
	$(PRESENT_EXTENSION_CFLAGS)					\
	$(RANDR_CFLAGS)							\
	$(RENDER_CFLAGS)						\
	$(XINERAMA_CFLAGS)						\
	$(WAYLAND_CLIENT_CFLAGS) \
	$(XKBCOMMON_CFLAGS) \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DDATADIR=\"$(datadir)\"					\
	-DHELPERDIR=\"$(HELPER_PATH_PREFIX)\"				\
	-DPACKAGE_DATADIR=\"$(pkgdatadir)\"				\
	-DG_LOG_DOMAIN=\"xfwm4\"

xfwm4_LDADD =								\
	$(top_builddir)/common/libxfwm-common.la 			\
	$(GLIB_LIBS) 							\
	$(GTK_LIBS) 							\
	$(COMPOSITOR_LIBS)						\
	$(EPOXY_LIBS)							\
	$(LIBSTARTUP_NOTIFICATION_LIBS)					\
	$(LIBX11_LDFLAGS)						\
	$(LIBX11_LIBS)							\
	$(LIBXFCE4KBD_PRIVATE_LIBS)					\
	$(LIBXFCE4UI_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCONF_LIBS)						\
	$(PRESENT_EXTENSION_LIBS)					\
	$(RANDR_LIBS) 							\
	$(RENDER_LIBS)							\
	$(XINERAMA_LIBS)						\
	$(WAYLAND_CLIENT_LIBS) \
	$(XKBCOMMON_LIBS) \
	$(MATH_LIBS)
	
xfwm4_wayland_SOURCES = \
$(top_builddir)/protocol/xfway-shell-client-protocol.c \
$(top_builddir)/protocol/xfway-shell-client-protocol.h \
$(top_builddir)/protocol/xfway-shell-server-protocol.h \
$(top_builddir)/protocol/window-switcher-unstable-v1-server-protocol.c \
$(top_builddir)/protocol/window-switcher-unstable-v1-server-protocol.h \
$(top_builddir)/util/signal.c \
$(top_builddir)/util/signal.h \
$(top_builddir)/protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.c \
$(top_builddir)/wlr-foreign-toplevel-management-unstable-v1-protocol.h \
wlr_foreign_toplevel_management_v1.c \
wlr_foreign_toplevel_management_v1.h \
$(top_builddir)/protocol/wlr-layer-shell-unstable-v1-protocol.c \
$(top_builddir)/protocol/wlr-layer-shell-unstable-v1-protocol.h \
wlr_layer_shell_v1.c \
wlr_layer_shell_v1.h \
$(top_builddir)/protocol/xdg-shell.c \
$(top_builddir)/protocol/xdg-shell.h \
os-compatibility.c \
os-compatibility.h \
$(top_builddir)/util/helpers.h \
xfway.h \
window-switcher.c \
shell.c \
shell.h \
server.h \
main-wayland.c

xfwm4_wayland_CFLAGS = \
$(WAYLAND_SERVER_CFLAGS) \
$(LIBWESTON_CFLAGS) \
$(LIBWESTON_DESKTOP_CFLAGS) \
$(LIBINPUT_CFLAGS) \
$(XKBCOMMON_CFLAGS) \
$(EGL_CFLAGS) \
$(EVDEV_CFLAGS) \
$(GTK_CFLAGS) \
$(LIBXFCONF_CFLAGS)

xfwm4_wayland_LDADD = \
$(WAYLAND_SERVER_LIBS) \
$(LIBWESTON_LIBS) \
$(LIBWESTON_DESKTOP_LIBS) \
$(LIBINPUT_LIBS) \
$(XKBCOMMON_LIBS) \
$(EGL_LIBS) \
$(EVDEV_LIBS) \
$(PIXMAN_LIBS) \
$(GTK_LIBS) \
$(LIBXFCONF_LIBS)
	
xfwm4_wayland_shell_SOURCES =								\
	client.c							\
	client.h							\
	compositor.c							\
	compositor.h							\
	cycle.c								\
	cycle.h								\
	device.c							\
	device.h							\
	display.c							\
	display.h							\
	event_filter.c							\
	event_filter.h							\
	events.c							\
	events.h							\
	focus.c								\
	focus.h								\
	frame.c								\
	frame.h								\
	hints.c								\
	hints.h								\
	icons.c								\
	icons.h								\
	keyboard.c							\
	keyboard.h							\
	main-shell-client.c								\
	menu.c								\
	menu.h								\
	misc.c								\
	misc.h								\
	moveresize.c							\
	moveresize.h							\
	mypixmap.c							\
	mypixmap.h							\
	mywindow.c							\
	mywindow.h							\
	netwm.c								\
	netwm.h								\
	parserc.c							\
	parserc.h							\
	placement.c							\
	placement.h							\
	poswin.c							\
	poswin.h							\
	screen.c							\
	screen.h							\
	session.c							\
	session.h							\
	settings.c							\
	settings.h							\
	spinning_cursor.c						\
	spinning_cursor.h						\
	stacking.c							\
	stacking.h							\
	startup_notification.c						\
	startup_notification.h						\
	tabwin.c							\
	tabwin.h							\
	terminate.c							\
	terminate.h							\
	transients.c							\
	transients.h							\
	ui_style.c							\
	ui_style.h							\
	wireframe.c							\
	wireframe.h							\
	workspaces.c							\
	workspaces.h							\
	xsync.c								\
	xsync.h								\
	xpm-color-table.h						\
	$(top_srcdir)/protocol/xfway-shell-client-protocol.c \
	$(top_srcdir)/protocol/xfway-shell-client-protocol.h \
	$(top_srcdir)/protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.c \
	$(top_srcdir)/protocol/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h \
	$(top_srcdir)/util/libgwater-wayland.c 				\
	$(top_srcdir)/util/libgwater-wayland.h

xfwm4_wayland_shell_CFLAGS =								\
	$(WAYLAND_SERVER_CFLAGS) \
	$(WAYLAND_CLIENT_CFLAGS) \
	$(LIBWESTON_CFLAGS) \
	$(LIBWESTON_DESKTOP_CFLAGS) \
	$(LIBINPUT_CFLAGS) \
	$(XKBCOMMON_CFLAGS) \
	$(EGL_CFLAGS) \
	$(EVDEV_CFLAGS) \
	$(GLIB_CFLAGS) 							\
	$(GTK_CFLAGS) 							\
	$(COMPOSITOR_CFLAGS)						\
	$(EPOXY_CFLAGS)							\
	$(LIBSTARTUP_NOTIFICATION_CFLAGS)				\
	$(LIBX11_CFLAGS)						\
	$(LIBXFCE4KBD_PRIVATE_CFLAGS)					\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCONF_CFLAGS)						\
	$(PRESENT_EXTENSION_CFLAGS)					\
	$(RANDR_CFLAGS)							\
	$(RENDER_CFLAGS)						\
	$(XINERAMA_CFLAGS)						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DDATADIR=\"$(datadir)\"					\
	-DHELPERDIR=\"$(HELPER_PATH_PREFIX)\"				\
	-DPACKAGE_DATADIR=\"$(pkgdatadir)\"				\
	-DG_LOG_DOMAIN=\"xfwm4\"

xfwm4_wayland_shell_LDADD =								\
	$(WAYLAND_SERVER_LIBS) \
	$(WAYLAND_CLIENT_LIBS) \
	$(LIBWESTON_LIBS) \
	$(LIBWESTON_DESKTOP_LIBS) \
	$(LIBINPUT_LIBS) \
	$(XKBCOMMON_LIBS) \
	$(EGL_LIBS) \
	$(EVDEV_LIBS) \
	$(PIXMAN_LIBS) \
	$(top_builddir)/common/libxfwm-common.la 			\
	$(GLIB_LIBS) 							\
	$(GTK_LIBS) 							\
	$(COMPOSITOR_LIBS)						\
	$(EPOXY_LIBS)							\
	$(LIBSTARTUP_NOTIFICATION_LIBS)					\
	$(LIBX11_LDFLAGS)						\
	$(LIBX11_LIBS)							\
	$(LIBXFCE4KBD_PRIVATE_LIBS)					\
	$(LIBXFCE4UI_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCONF_LIBS)						\
	$(PRESENT_EXTENSION_LIBS)					\
	$(RANDR_LIBS) 							\
	$(RENDER_LIBS)							\
	$(XINERAMA_LIBS)						\
	$(MATH_LIBS)

AM_CPPFLAGS = 								\
	-I${top_builddir} 						\
	$(PLATFORM_CPPFLAGS)

EXTRA_DIST = 								\
	gen-color-table.pl
