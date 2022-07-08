compositor_inc = include_directories('.')


hopalong_sources = [
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
  'hopalong-main.c',
]

hopalong_dependencies = [
  server_protos,
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
  hopalong_dependencies += bsd_overlay
endif

hopalong_exe = executable('hopalong',
  hopalong_sources,
  dependencies: hopalong_dependencies,
  install: true
)
