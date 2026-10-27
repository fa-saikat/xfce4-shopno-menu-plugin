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

#include "config.h"

#include <string.h>

#include "shopno-app-tile.h"

#define TILE_ICON_SIZE          32
#define CATEGORY_ICON_SIZE      32
#define CATEGORY_PREVIEW_ICONS  4
#define FALLBACK_ICON           "application-x-executable"
#define ITEM_KEY                "shopno-item"



GtkWidget *
shopno_app_icon_new (const gchar *icon_name,
                     gint         size)
{
  GIcon     *icon;
  GtkWidget *image;

  if (icon_name != NULL && g_path_is_absolute (icon_name))
    {
      GFile *file = g_file_new_for_path (icon_name);

      icon = g_file_icon_new (file);
      g_object_unref (file);
    }
  else if (icon_name != NULL && *icon_name != '\0')
    {
      gchar *name = g_strdup (icon_name);

      /* Some desktop files wrongly include the file extension */
      if (g_str_has_suffix (name, ".png") || g_str_has_suffix (name, ".svg")
          || g_str_has_suffix (name, ".xpm"))
        name[strlen (name) - 4] = '\0';

      /* Not a GThemedIcon with fallback names: GTK tries every name in a
       * theme before moving on to its parent, so a theme that ships the
       * fallback icon (Adwaita) would hide app icons installed in hicolor */
      if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), name))
        icon = g_themed_icon_new (name);
      else
        icon = g_themed_icon_new (FALLBACK_ICON);

      g_free (name);
    }
  else
    {
      icon = g_themed_icon_new (FALLBACK_ICON);
    }

  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  gtk_image_set_pixel_size (GTK_IMAGE (image), size);
  g_object_unref (icon);

  return image;
}



static GtkWidget *
shopno_app_tile_label_new (const gchar *text,
                           const gchar *style_class)
{
  GtkWidget *label = gtk_label_new (text);

  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), style_class);

  return label;
}



GtkWidget *
shopno_app_tile_new (GarconMenuItem      *item,
                     ShopnoAppTileLayout  layout)
{
  const gchar *name = garcon_menu_item_get_name (item);
  const gchar *comment = garcon_menu_item_get_comment (item);
  GtkWidget   *tile;
  GtkWidget   *label;
  GtkWidget   *text_box;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);

  if (layout == SHOPNO_APP_TILE_GRID)
    {
      tile = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
      gtk_style_context_add_class (gtk_widget_get_style_context (tile), "grid");
      gtk_box_pack_start (GTK_BOX (tile),
                          shopno_app_icon_new (garcon_menu_item_get_icon_name (item), TILE_ICON_SIZE),
                          FALSE, FALSE, 0);

      label = shopno_app_tile_label_new (name, "shopno-app-name");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD);
      gtk_label_set_lines (GTK_LABEL (label), 2);
      /* No minimum width, so long names and wide fonts cannot push the
       * grid wider than the menu */
      gtk_label_set_max_width_chars (GTK_LABEL (label), 10);
      gtk_widget_set_valign (label, GTK_ALIGN_START);
      gtk_box_pack_start (GTK_BOX (tile), label, FALSE, FALSE, 0);

      if (comment != NULL && *comment != '\0')
        {
          gchar *tooltip = g_strdup_printf ("%s\n%s", name, comment);

          gtk_widget_set_tooltip_text (tile, tooltip);
          g_free (tooltip);
        }
      else
        {
          gtk_widget_set_tooltip_text (tile, name);
        }
    }
  else
    {
      tile = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      gtk_style_context_add_class (gtk_widget_get_style_context (tile), "list");
      gtk_box_pack_start (GTK_BOX (tile),
                          shopno_app_icon_new (garcon_menu_item_get_icon_name (item), TILE_ICON_SIZE),
                          FALSE, FALSE, 0);

      text_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
      gtk_widget_set_valign (text_box, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (tile), text_box, TRUE, TRUE, 0);

      label = shopno_app_tile_label_new (name, "shopno-app-name");
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_box_pack_start (GTK_BOX (text_box), label, FALSE, FALSE, 0);

      if (comment != NULL && *comment != '\0')
        {
          label = shopno_app_tile_label_new (comment, "shopno-app-comment");
          gtk_label_set_xalign (GTK_LABEL (label), 0.0);
          gtk_box_pack_start (GTK_BOX (text_box), label, FALSE, FALSE, 0);
        }
    }

  gtk_style_context_add_class (gtk_widget_get_style_context (tile), "shopno-app-tile");
  g_object_set_data_full (G_OBJECT (tile), ITEM_KEY, g_object_ref (item), g_object_unref);
  gtk_widget_show_all (tile);

  return tile;
}



GarconMenuItem *
shopno_app_tile_get_item (GtkWidget *tile)
{
  g_return_val_if_fail (GTK_IS_WIDGET (tile), NULL);

  return g_object_get_data (G_OBJECT (tile), ITEM_KEY);
}



GtkWidget *
shopno_category_card_new (ShopnoAppCategory *category)
{
  GtkWidget *card;
  GtkWidget *preview;
  GtkWidget *label;

  g_return_val_if_fail (category != NULL, NULL);

  card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_style_context_add_class (gtk_widget_get_style_context (card), "shopno-category-card");

  /* A 2x2 preview of the first apps, like the Windows 11 category view */
  preview = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (preview), 16);
  gtk_grid_set_column_spacing (GTK_GRID (preview), 16);
  gtk_widget_set_halign (preview, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (preview), "shopno-category-preview");
  gtk_box_pack_start (GTK_BOX (card), preview, FALSE, FALSE, 0);

  for (guint i = 0; i < CATEGORY_PREVIEW_ICONS; i++)
    {
      GtkWidget *icon;

      if (i < category->items->len)
        {
          GarconMenuItem *item = g_ptr_array_index (category->items, i);

          icon = shopno_app_icon_new (garcon_menu_item_get_icon_name (item), CATEGORY_ICON_SIZE);
        }
      else
        {
          /* Keep the card size constant for categories with few apps */
          icon = gtk_image_new ();
          gtk_widget_set_size_request (icon, CATEGORY_ICON_SIZE, CATEGORY_ICON_SIZE);
        }

      gtk_grid_attach (GTK_GRID (preview), icon, i % 2, i / 2, 1, 1);
    }

  label = shopno_app_tile_label_new (category->name, "shopno-category-name");
  gtk_box_pack_start (GTK_BOX (card), label, FALSE, FALSE, 0);

  gtk_widget_set_tooltip_text (card, category->name);
  gtk_widget_show_all (card);

  return card;
}
