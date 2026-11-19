project('libmali-cix', 'c',
  version : '0.1.0',
  license : 'MIT',
  meson_version : '>= 0.62',
  default_options : ['c_std=gnu11', 'warning_level=2', 'buildtype=release'])

app_config = get_option('app_config')
mali_library = get_option('mali_library')

dl_dep = dependency('dl')

app_config_lib = static_library('app_config', 'src/app_config.c',
  gnu_symbol_visibility : 'hidden',
  pic : true)

# glvnd EGL vendor library ("gate"): routes each process to libmali or to Mesa per app.json.
shared_library('EGL_mali', 'src/egl_mali.c',
  c_args : [
    '-DMALI_APP_CONFIG="@0@"'.format(app_config),
    '-DMALI_LIBRARY="@0@"'.format(mali_library),
  ],
  link_with : app_config_lib,
  dependencies : dl_dep,
  gnu_symbol_visibility : 'hidden',
  version : '0.0.0',
  soversion : '0',
  install : true,
  install_dir : get_option('mali_libdir') != '' ? get_option('mali_libdir') : get_option('libdir') / 'mali')

install_data('data/10_mali.json', install_dir : get_option('datadir') / 'glvnd' / 'egl_vendor.d')
install_data('data/app.json', install_dir : app_config.split('/app.json')[0])
install_data('scripts/mali-switch', install_dir : get_option('sbindir'), install_mode : 'rwxr-xr-x')

test('app_config', executable('test_app_config', 'tests/test_app_config.c',
  link_with : app_config_lib, include_directories : include_directories('src')))
