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

#ifndef __SHOPNO_APP_MODEL_H__
#define __SHOPNO_APP_MODEL_H__

#include <garcon/garcon.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct
{
  gchar     *name;
  GPtrArray *items; /* GarconMenuItem, sorted by name */
} ShopnoAppCategory;

#define SHOPNO_TYPE_APP_MODEL (shopno_app_model_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoAppModel, shopno_app_model, SHOPNO, APP_MODEL, GObject)

ShopnoAppModel *shopno_app_model_new            (void);

GPtrArray      *shopno_app_model_get_categories (ShopnoAppModel  *model);

GPtrArray      *shopno_app_model_get_all        (ShopnoAppModel  *model);

GarconMenuItem *shopno_app_model_lookup         (ShopnoAppModel  *model,
                                                 const gchar     *desktop_id);

GPtrArray      *shopno_app_model_search         (ShopnoAppModel  *model,
                                                 const gchar     *query,
                                                 guint            max_results);

gboolean        shopno_app_launch               (GarconMenuItem  *item,
                                                 GdkScreen       *screen,
                                                 GError         **error);

G_END_DECLS

#endif /* !__SHOPNO_APP_MODEL_H__ */
