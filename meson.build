project('xfwm4', 'c',
  version: '4.18.0',
  meson_version: '>=0.50',
  default_options: 'c_std=c11',)
  
xfwm4_version = meson.project_version()

ver_arr = xfwm4_version.split('.')

xfwm4_major_version = ver_arr[0]
xfwm4_minor_version = ver_arr[1]
xfwm4_micro_version = ver_arr[2]

glib_minimum_version = '2.66.0'
gtk_minimum_version = '3.24.0'
xfce_minimum_version = '4.8.0'
libxfce4ui_minimum_version = '4.12.0'
libxfce4kbd_private_minimum_version = '4.12.0'
xfconf_minimum_version = '4.13.0'
xfconf_legacy_version = '4.12.0'
xcomposite_minimum_version = '0.2'
wnck_minimum_version = '3.14'
startup_notification_minimum_version = '0.5'
intltool_minimum_version = '0.35'
libepoxy_minimum_version = '1.0'
xpresent_minimum_version = '1.0'
presentproto_minimum_version = '1.1'

prefix = get_option('prefix')

bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
libdir = join_paths(prefix, get_option('libdir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
includedir = join_paths(prefix, get_option('includedir'))
sysconfdir = get_option('sysconfdir')

pkgname = meson.project_name()

pkgdatadir = join_paths(datadir, pkgname)
pkglibdir = join_paths(libdir, pkgname)
pkgincludedir = join_paths(includedir, pkgname)

pcdir = join_paths(libdir, 'pkgconfig')

gettext_package = meson.project_name()
localedir = join_paths(datadir, 'locale')

top_srcdir = meson.current_source_dir()
builddir = meson.current_build_dir()

#localedir = '"' + get_option('localedir') + '"'


add_project_arguments('-DWLR_USE_UNSTABLE', language: 'c')

cc = meson.get_compiler('c')

wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_egl    = dependency('wayland-egl')
wayland_protos = dependency('wayland-protocols', version: '>=1.14')
pixman         = dependency('pixman-1')
cairo          = dependency('cairo')
pango          = dependency('pango')
pangocairo     = dependency('pangocairo')
glesv2	       = dependency('glesv2')
wlroots        = dependency('wlroots', version: '>=0.16.0')
xkbcommon      = dependency('xkbcommon')
glib           = dependency('glib-2.0', version: '>=' + glib_minimum_version)
x11            = dependency('x11')
compositor     = [
                  dependency('xcomposite'),
                  dependency('xfixes'),
                  dependency('xdamage'),
                 ]
xrender        = dependency('xrender')
xext           = dependency('xext')
xrandr         = dependency('xrandr')
libinput       = dependency('libinput', version: '>=1.10.0')
egl            = dependency('egl', version: '>=1.0.0')
evdev          = dependency('libevdev', version: '>=1.5.8')
libxml2        = dependency('libxml-2.0')
gtk            = dependency('gtk+-3.0', version: '>=' + gtk_minimum_version)
libxfce4util   = dependency('libxfce4util-1.0',
                            version: '>=' + xfce_minimum_version)
libxfce4ui     = dependency('libxfce4ui-2',
                            version: '>=' + libxfce4ui_minimum_version)
libxfce4kbd_private = dependency('libxfce4kbd-private-3',
                                 version: '>='
                                 + libxfce4kbd_private_minimum_version)
libxfconf      = dependency('libxfconf-0', version: '>=' + xfconf_minimum_version)
libwnck        = dependency('libwnck-3.0',
                            version: '>=' + wnck_minimum_version)
xinerama       = dependency('xinerama')

m_dep          = cc.find_library('m')

# glibc lacks strl*. if we can't detect them, assume we need libbsd
lacking_libc = false
if cc.has_function('strlcpy') == false or cc.has_function('strlcat') == false
  lacking_libc = true
  bsd_overlay = dependency('libbsd-overlay')
endif

if get_option('xwayland')
	xcb    = dependency('xcb', required: true)

	if xcb.found()
		add_project_arguments('-DUSE_XWAYLAND', language: 'c')
	endif
endif



conf_data = configuration_data()
conf_data.set('PACKAGE', '"' + pkgname + '"')
conf_data.set('VERSION', '"' + xfwm4_version + '"')
conf_data.set('REVISION', '"bdb2b4f88"')
conf_data.set('GETTEXT_PACKAGE', '"' + gettext_package + '"')
conf_data.set('PACKAGE_NAME', '"' + pkgname + '"')
conf_data.set('PACKAGE_LOCALE_DIR', '"' + localedir + '"')
conf_data.set('HAVE_LIBX11', 1)
conf_data.set('HAVE_COMPOSITOR', 1)

exec_prefix = get_option ('prefix')
helper_path_prefix = exec_prefix + '/lib'

add_global_arguments('-DHAVE_CONFIG_H', language: 'c')
add_global_arguments('-DDATADIR=' + '"' + datadir + '"', language: 'c')
add_global_arguments('-DPACKAGE_DATADIR=' + '"' + pkgdatadir + '"', language: 'c')
add_global_arguments('-DHELPERDIR=' + '"' + helper_path_prefix + '"', language: 'c')
add_global_arguments('-DBINDIR=' + '"' + bindir + '"', language: 'c')

add_project_arguments(
  '-Wno-unused-parameter',
  '-Wno-unused-variable',
  '-D_DEFAULT_SOURCE',
  '-D_POSIX_C_SOURCE=200112L',
  '-DWLR_USE_UNSTABLE',
  '-DPACKAGE_NAME="' + meson.project_name() + '"',
  '-DPACKAGE_VERSION="' + meson.project_version() + '"',
  language: 'c')

configure_file(output: 'config.h',
  configuration: conf_data
)

incdir = include_directories('.')

subdir('protocol')
subdir('common')
subdir('src')
