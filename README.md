# Shopno Menu

A start menu for the Xfce panel, modeled on the Windows 11 Start menu, without
the ads and "Recommended" suggestions.

Shopno Menu is developed for ShopnoOS but works
on any distribution that ships Xfce 4.18 or newer.

> [!NOTE]
> Shopno Menu is young. It works, but expect rough edges, and please
> [report them](https://github.com/fa-saikat/xfce4-shopno-menu-plugin/issues).

## Features

- Search apps as you type, by name, keywords, or description. Enter opens the
  best match.
- Pinned apps grid. Right-click any app to pin, unpin, or move it to the front.
- Recently used apps row (can be turned off).
- All apps, grouped by category, with the Windows 11 icon-preview cards.
- User avatar and name, with account settings, lock, switch user, and sign out.
- Power button with sleep, shut down, and restart.
- Follows your GTK theme (light or dark) and icon theme.
- Optional background blur with [picom](https://github.com/yshui/picom).
- Opens with the Super key.
- Full keyboard navigation: type to search, arrow keys to move, Escape to go
  back.

Session actions use the standard Xfce tools (`xflock4`,
`xfce4-session-logout`, `xfce4-settings-manager`). An action whose tool is not
installed is hidden. Switch user needs `dm-tool` (LightDM), and account
settings need `mugshot` or `users-admin`.

## Building

Dependencies:

| Library          | Minimum version |
|------------------|-----------------|
| GTK              | 3.24            |
| libxfce4panel    | 4.18            |
| libxfce4ui       | 4.18            |
| libxfce4util     | 4.18            |
| garcon           | 4.18            |
| xfconf           | 4.18            |

You also need `meson`, `ninja`, a C compiler, and `gettext`.

**Debian / Ubuntu**

```sh
sudo apt install build-essential meson ninja-build gettext pkg-config \
  libgtk-3-dev libxfce4panel-2.0-dev libxfce4ui-2-dev libxfce4util-dev libxfconf-0-dev libgarcon-1-dev
```

**Fedora**

```sh
sudo dnf install gcc meson ninja-build gettext gtk3-devel \
  xfce4-panel-devel libxfce4ui-devel libxfce4util-devel xfconf-devel garcon-devel
```

**Arch Linux**

```sh
sudo pacman -S --needed base-devel meson gettext gtk3 xfce4-panel libxfce4ui libxfce4util xfconf garcon
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

## Background blur

Blur is drawn by the compositor, so it needs [picom](https://github.com/yshui/picom)
instead of the built-in Xfce compositor:

1. Turn off the Xfce compositor in **Settings > Window Manager Tweaks >
   Compositor**, and run picom with a blur method such as
   `blur-method = "dual_kawase";`.
2. Add the rules from [`extras/picom/shopno-menu.conf`](extras/picom/shopno-menu.conf)
   to your picom configuration.
3. Right-click the menu button, choose **Properties**, and turn on **Blur the
   background behind the menu**.

With blur off, the menu draws its own rounded corners and shadow and works
with any compositor, or none.

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
