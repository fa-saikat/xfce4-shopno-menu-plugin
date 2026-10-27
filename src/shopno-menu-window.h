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

#ifndef __SHOPNO_MENU_WINDOW_H__
#define __SHOPNO_MENU_WINDOW_H__

#include <gtk/gtk.h>

#include "shopno-menu-settings.h"

G_BEGIN_DECLS

#define SHOPNO_TYPE_MENU_WINDOW (shopno_menu_window_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoMenuWindow, shopno_menu_window, SHOPNO, MENU_WINDOW, GtkWindow)

GtkWidget *shopno_menu_window_new            (ShopnoMenuSettings *settings);

void       shopno_menu_window_popup          (ShopnoMenuWindow   *window,
                                              gint                x,
                                              gint                y);

void       shopno_menu_window_set_grab_input (ShopnoMenuWindow   *window,
                                              gboolean            grab_input);

G_END_DECLS

#endif /* !__SHOPNO_MENU_WINDOW_H__ */
