compositor_inc = include_directories('.')


xfwm4_wayland_sources = [
  'hopalong-server.c',
  'hopalong-output.c',
  'hopalong-xdg.c',
  'hopalong-cursor.c',
  'hopalong-seat.c',
  'hopalong-view.c',
  'hopalong-pango-util.c',
  'hopalong-decoration.c',
  'hopalong-environment.c',
  'hopalong-xwayland.c',
  'hopalong-style.c',
  'hopalong-shell.c',
  'hopalong-layer-shell.c',
  'hopalong-keybinding.c',
  'os-compatibility.c',
  'hopalong-main.c',
  top_srcdir + '/util/signal.c',
  'xfwm-shell.c'
]

xfwm4_wayland_dependencies = [
  server_protos,
  xfwm_server_protos,
  wayland_server,
  cairo,
  pangocairo,
  pixman,
  glesv2,
  wlroots,
  xkbcommon,
  glib
]

if lacking_libc
  xfwm4_wayland_dependencies += bsd_overlay
endif

xfwm4_wayland_exe = executable('xfwm4-wayland',
  xfwm4_wayland_sources,
  dependencies: xfwm4_wayland_dependencies,
  include_directories: incdir,
  install: true
)
