resources = gnome.compile_resources(
  'shopno-menu-resources',
  'resources/shopno-menu.gresource.xml',
  source_dir: 'resources',
  c_name: 'shopno_menu',
)

shopno_menu_deps = [
  gtk,
  gio_unix,
  libxfce4panel,
  libxfce4ui,
  libxfce4util,
  libxfconf,
  garcon,
]

# Everything except the panel glue, shared with the preview tool
shopno_menu_core = static_library(
  'shopno-menu-core',
  sources: [
    'shopno-app-actions.c',
    'shopno-app-model.c',
    'shopno-app-tile.c',
    'shopno-default-apps.c',
    'shopno-menu-settings.c',
    'shopno-menu-window.c',
    'shopno-session.c',
    resources,
  ],
  include_directories: config_inc,
  dependencies: shopno_menu_deps,
  gnu_symbol_visibility: 'hidden',
  pic: true,
)

shared_module(
  'shopno-menu',
  sources: [
    'shopno-menu-dialog.c',
    'shopno-menu-plugin.c',
  ],
  include_directories: config_inc,
  dependencies: shopno_menu_deps,
  link_whole: shopno_menu_core,
  gnu_symbol_visibility: 'hidden',
  install: true,
  install_dir: libdir / 'xfce4' / 'panel' / 'plugins',
)

shopno_menu_inc = include_directories('.')
