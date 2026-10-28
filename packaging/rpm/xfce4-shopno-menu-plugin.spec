Name:           xfce4-shopno-menu-plugin
Version:        0.1.0
Release:        1%{?dist}
Summary:        Windows 11 style Start menu for the Xfce panel

License:        GPL-2.0-or-later
URL:            https://github.com/fa-saikat/xfce4-shopno-menu-plugin
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  gettext
BuildRequires:  meson
BuildRequires:  pkgconfig(garcon-1)
BuildRequires:  pkgconfig(gio-unix-2.0)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gtk+-3.0)
BuildRequires:  pkgconfig(libxfce4panel-2.0)
BuildRequires:  pkgconfig(libxfce4ui-2)
BuildRequires:  pkgconfig(libxfce4util-1.0)
BuildRequires:  pkgconfig(libxfconf-0)

Requires:       xfce4-panel
Recommends:     exo
Suggests:       picom

%description
Shopno Menu is an application menu plugin for the Xfce panel, modelled on
the Windows 11 Start menu, without ads or a Recommended section.

It offers search, pinned and recently used apps, all apps by category or in
a grid or list, user and power controls, and optional background blur with
picom. It follows the GTK and icon theme.

%prep
%autosetup

%build
%meson -Dpreview=false
%meson_build

%install
%meson_install
# No translations yet; keep the build working once they are added
%find_lang %{name} || touch %{name}.lang

%files -f %{name}.lang
%license COPYING
%doc README.md extras/picom/shopno-menu.conf
%{_bindir}/xfce4-popup-shopno-menu
%{_libdir}/xfce4/panel/plugins/libshopno-menu.so
%{_datadir}/xfce4/panel/plugins/shopno-menu.desktop

%changelog
* Wed Oct 28 2026 FA Saikat <fahimabrar.saikat@gmail.com> - 0.1.0-1
- Initial release
