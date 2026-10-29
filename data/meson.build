i18n.merge_file(
  input: 'shopno-menu.desktop.in',
  output: 'shopno-menu.desktop',
  type: 'desktop',
  po_dir: meson.project_source_root() / 'po',
  install: true,
  install_dir: datadir / 'xfce4' / 'panel' / 'plugins',
)

# The ShopnoOS logo for now, until Shopno Menu has its own (#22)
install_data(
  'icons/shopno-menu.svg',
  install_dir: datadir / 'icons' / 'hicolor' / 'scalable' / 'apps',
)

install_data(
  'xfce4-popup-shopno-menu',
  install_dir: get_option('bindir'),
  install_mode: 'rwxr-xr-x',
)
