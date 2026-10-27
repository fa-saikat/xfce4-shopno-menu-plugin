# Contributing to Shopno Menu

## Reporting bugs

Open an issue with the form that fits: **Bug report** for things that do not
work, **Visual or layout issue** for things that look wrong, **Feature
request**, or **Packaging request or problem**. For questions, use
[Discussions](https://github.com/fa-saikat/xfce4-shopno-menu-plugin/discussions)
instead.

To get debug output, run the panel from a terminal:

```sh
xfce4-panel -q
PANEL_DEBUG=1 xfce4-panel
```

## Development setup

Follow the build steps in the [README](README.md). After changing the code,
reinstall and restart the panel:

```sh
meson compile -C build && sudo meson install -C build && xfce4-panel -r
```

### Preview tool

Reinstalling the plugin for every change is slow. For layout and style work,
build the preview tool, which opens the menu in a normal window:

```sh
meson setup build -Dpreview=true
meson compile -C build
./build/tools/shopno-menu-preview --no-grab
```

`--no-grab` stops the menu from capturing all keyboard and mouse input.
Other options: `--blur`, `--no-recent`, and `--pin ID` (repeatable) to pin
desktop ids such as `firefox-esr.desktop`. Settings are not saved.

## Code style

The code follows the Xfce C style:

- Two-space indentation, no tabs.
- GNU brace placement, and a space before the opening parenthesis of a call.
- Function return types on their own line.
- Prefix public symbols with `shopno_` and types with `Shopno`.
- Build with `-Dwerror=true` before sending a pull request.

When in doubt, match the code around your change.

## Pull requests

- Keep each pull request focused on one change.
- Write commit messages in the imperative mood ("Add recent apps row"), with a
  short summary line and an optional body explaining why.
- Include a screenshot for visual changes.

## Translations

Translations use gettext. To add a language:

1. Add the language code to `po/LINGUAS`.
2. Run `meson compile -C build xfce4-shopno-menu-plugin-update-po`.
3. Translate `po/<code>.po` and open a pull request.

## License

By contributing, you agree that your contributions are licensed under the
GNU General Public License v2.0 or later.
