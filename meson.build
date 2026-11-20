project('libmali-cix', 'c',
  version : '0.1.0',
  license : 'MIT',
  meson_version : '>= 0.62',
  default_options : ['c_std=gnu11', 'warning_level=2', 'buildtype=release'])

app_config = get_option('app_config')
mali_library = get_option('mali_library')
mali_libdir = get_option('mali_libdir') != '' ? get_option('mali_libdir') : get_option('libdir') / 'mali'
cc = meson.get_compiler('c')

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
  install_dir : mali_libdir)

install_data('data/10_mali.json', install_dir : get_option('datadir') / 'glvnd' / 'egl_vendor.d')
install_data('data/app.json', install_dir : app_config.split('/app.json')[0])
install_data('scripts/mali-switch', install_dir : get_option('sbindir'), install_mode : 'rwxr-xr-x')

if get_option('blobs')
  # libmali (GLES, EGL, Vulkan, OpenCL, GBM), in <libdir>/mali only: every manifest below names a bare
  # soname, so all of it resolves only while mali-switch puts that directory on the linker path.
  install_data('blobs/aarch64-linux-gnu/libmali.so.0.54.1', install_dir : mali_libdir, install_mode : 'rwxr-xr-x')
  install_symlink('libmali.so.0', pointing_to : 'libmali.so.0.54.1', install_dir : mali_libdir)
  install_symlink('libmali.so', pointing_to : 'libmali.so.0', install_dir : mali_libdir)
  libmali_dep = cc.find_library('mali', dirs : meson.current_source_dir() / 'blobs' / 'aarch64-linux-gnu')

  # libgbm.so.1: libmali's own GBM (see src/gbm_wrapper.c)
  shared_library('gbm', 'src/gbm_wrapper.c',
    dependencies : libmali_dep,
    link_args : ['-Wl,--no-as-needed'],
    version : '1.0.0',
    soversion : '1',
    install : true,
    install_dir : mali_libdir)

  install_data('data/mali.json', install_dir : get_option('datadir') / 'vulkan' / 'icd.d')
  install_data('data/mali.icd', install_dir : get_option('sysconfdir') / 'OpenCL' / 'vendors')
  install_data('blobs/firmware/mali_csffw.bin', install_dir : get_option('prefix') / 'lib' / 'firmware')
endif

if get_option('wsi_layer')
  install_data('wsi/libVkLayer_window_system_integration.so', install_dir : mali_libdir)
  install_data('wsi/VkLayer_window_system_integration.json',
    install_dir : get_option('datadir') / 'vulkan' / 'implicit_layer.d')
endif

install_data('data/60-mali-dma-heap.rules', install_dir : get_option('prefix') / 'lib' / 'udev' / 'rules.d')
install_data('data/linlondp-render.conf', install_dir : get_option('prefix') / 'lib' / 'modprobe.d')
# linlondp -> zink loader rule; mali-switch links it as /etc/drirc in vendor mode
install_data('data/drirc-linlondp-zink.conf', install_dir : get_option('datadir') / 'libmali-cix')

test('app_config', executable('test_app_config', 'tests/test_app_config.c',
  link_with : app_config_lib, include_directories : include_directories('src')))
