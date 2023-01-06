xfwm4_wayland_src = files(  
  'cursor.c',
  'decoration.c',
  'layer_shell.c',
  'main.c',
  'output.c',
  'seat.c',
  'server.c',
  'xdg_shell.c',
  'xwayland.c',
  )

xfwm4_wayland_dep = [  
  xfwm_server_protos,
  evdev,
  libinput,
  libxml2,
  wayland_server,
  wlroots,
  xkbcommon,
  gtk,
  ]

executable(
  'xfwm4-wayland',
  xfwm4_wayland_src,
  include_directories: [incdir],
  dependencies: [xfwm4_wayland_dep],
  install: true,
  )
