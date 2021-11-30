@SET_MAKE@

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CPPFLAGS = 					\
	-DBINDIR='"$(bindir)"'			\
	-DLIBEXECDIR='"$(libexecdir)"'	

EXTRA_DIST =								\
	example.gtkrc-2.0						\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	COMPOSITOR							\
	README.md

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update

SUBDIRS = 								\
	defaults 							\
	helper-dialog 							\
	icons 								\
	protocol				\
	po 								\
	common 								\
	settings-dialogs						\
	src 								\
	themes
	
BUILT_SOURCES =								\
	protocol/xfway-shell-client-protocol.c				\
	protocol/xfway-shell-client-protocol.h				\
	protocol/xfway-shell-server-protocol.h				\
	protocol/window-switcher-unstable-v1-server-protocol.h		\
	protocol/window-switcher-unstable-v1-client-protocol.h		\
	protocol/window-switcher-unstable-v1-server-protocol.c		\
	protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.h	\
	protocol/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h	\
	protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.c		\
	protocol/wlr-layer-shell-unstable-v1-protocol.h	\
	protocol/wlr-layer-shell-unstable-v1-client-protocol.h	\
	protocol/wlr-layer-shell-unstable-v1-protocol.c \
	protocol/xdg-shell.c \
	protocol/xdg-shell.h

distclean-local:
	rm -rf *.cache

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

.SECONDEXPANSION:

define protostability
$(if $(findstring unstable,$1),unstable,stable)
endef

define protoname
$(shell echo $1 | $(SED) 's/\([a-z\-]\+\)-[a-z]\+-v[0-9]\+/\1/')
endef

protocol/xfway-shell-client-protocol.h : $(top_srcdir)/protocol/xfway-shell.xml
	$(AM_V_GEN) $(wayland_scanner) client-header $(top_srcdir)/protocol/xfway-shell.xml $(top_srcdir)/protocol/xfway-shell-client-protocol.h

protocol/xfway-shell-client-protocol.c : $(top_srcdir)/protocol/xfway-shell.xml
	$(AM_V_GEN) $(wayland_scanner) private-code $(top_srcdir)/protocol/xfway-shell.xml $(top_srcdir)/protocol/xfway-shell-client-protocol.c

protocol/xfway-shell-server-protocol.h : $(top_srcdir)/protocol/xfway-shell.xml
	$(AM_V_GEN) $(wayland_scanner) server-header $(top_srcdir)/protocol/xfway-shell.xml $(top_srcdir)/protocol/xfway-shell-server-protocol.h


protocol/window-switcher-unstable-v1-server-protocol.h : $(top_srcdir)/protocol/window-switcher-unstable-v1.xml
	$(AM_V_GEN) $(wayland_scanner) server-header $(top_srcdir)/protocol/window-switcher-unstable-v1.xml $(top_srcdir)/protocol/window-switcher-unstable-v1-server-protocol.h

protocol/window-switcher-unstable-v1-client-protocol.h : $(top_srcdir)/protocol/window-switcher-unstable-v1.xml
	$(AM_V_GEN) $(wayland_scanner) client-header $(top_srcdir)/protocol/window-switcher-unstable-v1.xml $(top_srcdir)/protocol/window-switcher-unstable-v1-client-protocol.h

protocol/window-switcher-unstable-v1-server-protocol.c : $(top_srcdir)/protocol/window-switcher-unstable-v1.xml
	$(AM_V_GEN) $(wayland_scanner) private-code $(top_srcdir)/protocol/window-switcher-unstable-v1.xml $(top_srcdir)/protocol/window-switcher-unstable-v1-server-protocol.c


protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.h : $(top_srcdir)/protocol/wlr-foreign-toplevel-management-unstable-v1.xml
	$(AM_V_GEN) $(wayland_scanner) server-header $(top_srcdir)/protocol/wlr-foreign-toplevel-management-unstable-v1.xml $(top_srcdir)/protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.h

protocol/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h : $(top_srcdir)/protocol/window-switcher-unstable-v1.xml
	$(AM_V_GEN) $(wayland_scanner) client-header $(top_srcdir)/protocol/wlr-foreign-toplevel-management-unstable-v1.xml $(top_srcdir)/protocol/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h

protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.c : $(top_srcdir)/protocol/wlr-foreign-toplevel-management-unstable-v1.xml
	$(AM_V_GEN) $(wayland_scanner) private-code $(top_srcdir)/protocol/wlr-foreign-toplevel-management-unstable-v1.xml $(top_srcdir)/protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.c


protocol/wlr-layer-shell-unstable-v1-protocol.h : $(top_srcdir)/protocol/wlr-layer-shell-unstable-v1.xml
	$(AM_V_GEN) $(wayland_scanner) server-header $(top_srcdir)/protocol/wlr-layer-shell-unstable-v1.xml $(top_srcdir)/protocol/wlr-layer-shell-unstable-v1-protocol.h

protocol/wlr-layer-shell-unstable-v1-client-protocol.h : $(top_srcdir)/protocol/wlr-layer-shell-unstable-v1.xml
	$(AM_V_GEN) $(wayland_scanner) client-header $(top_srcdir)/protocol/wlr-layer-shell-unstable-v1.xml $(top_srcdir)/protocol/wlr-layer-shell-unstable-v1-client-protocol.h

protocol/wlr-layer-shell-unstable-v1-protocol.c : $(top_srcdir)/protocol/wlr-layer-shell-unstable-v1.xml
	$(AM_V_GEN) $(wayland_scanner) private-code $(top_srcdir)/protocol/wlr-layer-shell-unstable-v1.xml $(top_srcdir)/protocol/wlr-layer-shell-unstable-v1-protocol.c
	
protocol/xdg-shell.h : $(top_srcdir)/protocol/xdg-shell-unstable-v6.xml
	$(AM_V_GEN) $(wayland_scanner) server-header $(top_srcdir)/protocol/xdg-shell-unstable-v6.xml $(top_srcdir)/protocol/xdg-shell.h
	
protocol/xdg-shell.c : $(top_srcdir)/protocol/xdg-shell-unstable-v6.xml
	$(AM_V_GEN) $(wayland_scanner) private-code $(top_srcdir)/protocol/xdg-shell-unstable-v6.xml $(top_srcdir)/protocol/xdg-shell.c
