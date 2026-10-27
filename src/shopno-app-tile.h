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

#ifndef __SHOPNO_APP_TILE_H__
#define __SHOPNO_APP_TILE_H__

#include <gtk/gtk.h>

#include "shopno-app-model.h"

G_BEGIN_DECLS

typedef enum
{
  SHOPNO_APP_TILE_GRID, /* icon above a one-line name */
  SHOPNO_APP_TILE_LIST, /* icon beside the name and description */
  SHOPNO_APP_TILE_ROW,  /* small icon beside the name */
} ShopnoAppTileLayout;

GtkWidget      *shopno_app_icon_new      (const gchar         *icon_name,
                                          gint                 size);

GtkWidget      *shopno_app_tile_new      (GarconMenuItem      *item,
                                          ShopnoAppTileLayout  layout);
GarconMenuItem *shopno_app_tile_get_item (GtkWidget           *tile);

GtkWidget      *shopno_category_card_new (ShopnoAppCategory   *category);

G_END_DECLS

#endif /* !__SHOPNO_APP_TILE_H__ */
