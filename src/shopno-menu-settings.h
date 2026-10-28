/*
 * Copyright (C) 2026 FA Saikat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __SHOPNO_MENU_SETTINGS_H__
#define __SHOPNO_MENU_SETTINGS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define SHOPNO_MENU_DEFAULT_ICON "start-here"
#define SHOPNO_MENU_MAX_RECENT   6

/* Values of the view-mode property, the layout of the All apps section */
#define SHOPNO_MENU_VIEW_CATEGORY "category"
#define SHOPNO_MENU_VIEW_GRID     "grid"
#define SHOPNO_MENU_VIEW_LIST     "list"

#define SHOPNO_TYPE_MENU_SETTINGS (shopno_menu_settings_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoMenuSettings, shopno_menu_settings, SHOPNO, MENU_SETTINGS, GObject)

ShopnoMenuSettings  *shopno_menu_settings_new           (void);

const gchar * const *shopno_menu_settings_get_pinned    (ShopnoMenuSettings  *settings);
void                 shopno_menu_settings_set_pinned    (ShopnoMenuSettings  *settings,
                                                         const gchar * const *pinned);
gboolean             shopno_menu_settings_is_pinned     (ShopnoMenuSettings  *settings,
                                                         const gchar         *desktop_id);
void                 shopno_menu_settings_pin           (ShopnoMenuSettings  *settings,
                                                         const gchar         *desktop_id);
void                 shopno_menu_settings_unpin         (ShopnoMenuSettings  *settings,
                                                         const gchar         *desktop_id);
void                 shopno_menu_settings_move_to_front (ShopnoMenuSettings  *settings,
                                                         const gchar         *desktop_id);

const gchar * const *shopno_menu_settings_get_recent    (ShopnoMenuSettings  *settings);
void                 shopno_menu_settings_set_recent    (ShopnoMenuSettings  *settings,
                                                         const gchar * const *recent);
void                 shopno_menu_settings_add_recent    (ShopnoMenuSettings  *settings,
                                                         const gchar         *desktop_id);
void                 shopno_menu_settings_remove_recent (ShopnoMenuSettings  *settings,
                                                         const gchar         *desktop_id);

gboolean             shopno_menu_settings_get_show_recent (ShopnoMenuSettings *settings);
gboolean             shopno_menu_settings_get_enable_blur (ShopnoMenuSettings *settings);
const gchar         *shopno_menu_settings_get_button_icon (ShopnoMenuSettings *settings);
const gchar         *shopno_menu_settings_get_view_mode   (ShopnoMenuSettings *settings);
gboolean             shopno_menu_settings_get_show_settings_dialogs (ShopnoMenuSettings *settings);

G_END_DECLS

#endif /* !__SHOPNO_MENU_SETTINGS_H__ */
