resources = gnome.compile_resources(
  'shopno-menu-resources',
  'resources/shopno-menu.gresource.xml',
  source_dir: 'resources',
  c_name: 'shopno_menu',
)

shared_module(
  'shopno-menu',
  sources: [
    'shopno-menu-plugin.c',
    'shopno-menu-window.c',
    resources,
  ],
  include_directories: config_inc,
  dependencies: [
    gtk,
    libxfce4panel,
    libxfce4ui,
    libxfce4util,
    garcon,
  ],
  gnu_symbol_visibility: 'hidden',
  install: true,
  install_dir: libdir / 'xfce4' / 'panel' / 'plugins',
)
