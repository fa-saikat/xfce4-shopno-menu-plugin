# Shopno Menu

A start menu for the Xfce panel, modeled on the Windows 11 Start menu, without
the ads and "Recommended" suggestions.

Shopno Menu is developed for ShopnoOS but works
on any distribution that ships Xfce 4.18 or newer.

> [!WARNING]
> **Early development.** The panel button and the popup window work, but the
> menu has no content yet. It is not ready for daily use.

## Planned features

- [ ] Search for apps as you type
- [ ] Pinned apps grid (add, remove, and reorder)
- [ ] Recently used apps row
- [ ] All apps, grouped by category
- [ ] User avatar and name, with a link to account settings
- [ ] Session controls: lock, switch user, log out, suspend, restart, shut down
- [ ] Optional background blur (with [picom](https://github.com/yshui/picom))
- [ ] Settings dialog
- [x] Follows the active GTK theme and icon theme
- [x] Open with the Super key (`xfce4-popup-shopno-menu`)

## Building

Dependencies:

| Library          | Minimum version |
|------------------|-----------------|
| GTK              | 3.24            |
| libxfce4panel    | 4.18            |
| libxfce4ui       | 4.18            |
| libxfce4util     | 4.18            |
| garcon           | 4.18            |

You also need `meson`, `ninja`, a C compiler, and `gettext`.

**Debian / Ubuntu**

```sh
sudo apt install build-essential meson ninja-build gettext pkg-config \
  libgtk-3-dev libxfce4panel-2.0-dev libxfce4ui-2-dev libxfce4util-dev libgarcon-1-dev
```

**Fedora**

```sh
sudo dnf install gcc meson ninja-build gettext gtk3-devel \
  xfce4-panel-devel libxfce4ui-devel libxfce4util-devel garcon-devel
```

**Arch Linux**

```sh
sudo pacman -S --needed base-devel meson gettext gtk3 xfce4-panel libxfce4ui libxfce4util garcon
```

Build and install:

```sh
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
```

The prefix must match the one Xfce is installed to (`/usr` on almost every
distribution), otherwise the panel will not find the plugin.

Then restart the panel with `xfce4-panel -r`, right-click the panel, choose
**Panel > Add New Items…**, and add **Shopno Menu**.

## Opening with the Super key

Open **Settings > Keyboard > Application Shortcuts**, add a shortcut with the
command `xfce4-popup-shopno-menu`, and press the Super key when asked.

## Theming

Colors come from your GTK theme, and icons from your icon theme. To change
the layout or colors, override the `.shopno-menu-*` CSS classes in
`~/.config/gtk-3.0/gtk.css`. The default rules are in
[`src/resources/shopno-menu.css`](src/resources/shopno-menu.css).

## Contributing

Bug reports, translations, and patches are welcome. See
[CONTRIBUTING.md](CONTRIBUTING.md).

## License

Shopno Menu is released under the
[GNU General Public License v2.0 or later](COPYING).

Windows is a trademark of Microsoft Corporation. This project is not
affiliated with or endorsed by Microsoft.
