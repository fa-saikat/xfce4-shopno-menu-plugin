i18n.merge_file(
  input: 'shopno-menu.desktop.in',
  output: 'shopno-menu.desktop',
  type: 'desktop',
  po_dir: meson.project_source_root() / 'po',
  install: true,
  install_dir: datadir / 'xfce4' / 'panel' / 'plugins',
)

install_data(
  'xfce4-popup-shopno-menu',
  install_dir: get_option('bindir'),
  install_mode: 'rwxr-xr-x',
)
