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

#include <glib/gi18n-lib.h>
#include <libxfce4ui/libxfce4ui.h>

#include "shopno-menu-dialog.h"

#define BINDING_FLAGS (G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE)

#define BLUR_GUIDE_URL "https://github.com/fa-saikat/xfce4-shopno-menu-plugin#background-blur"



static GtkWidget *
shopno_menu_dialog_check_new (ShopnoMenuSettings *settings,
                              const gchar        *property,
                              const gchar        *label)
{
  GtkWidget *check = gtk_check_button_new_with_mnemonic (label);

  g_object_bind_property (settings, property, check, "active", BINDING_FLAGS);

  return check;
}



void
shopno_menu_dialog_show (XfcePanelPlugin    *plugin,
                         ShopnoMenuSettings *settings)
{
  GtkWidget *dialog;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *check;
  gchar     *markup;

  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (SHOPNO_IS_MENU_SETTINGS (settings));

  dialog = xfce_titled_dialog_new_with_mixed_buttons (_("Shopno Menu"),
                                                      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "window-close-symbolic", _("_Close"), GTK_RESPONSE_CLOSE,
                                                      NULL);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "preferences-desktop");
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

  xfce_panel_plugin_block_menu (plugin);
  g_signal_connect_swapped (dialog, "destroy", G_CALLBACK (xfce_panel_plugin_unblock_menu), plugin);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), grid, TRUE, TRUE, 0);

  label = gtk_label_new_with_mnemonic (_("Panel button _icon:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  entry = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), SHOPNO_MENU_DEFAULT_ICON);
  gtk_widget_set_tooltip_text (entry, _("An icon name from your icon theme, or the path to an image"));
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  g_object_bind_property (settings, "button-icon", entry, "text", BINDING_FLAGS);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);

  check = shopno_menu_dialog_check_new (settings, "show-recent", _("Show _recently used apps"));
  gtk_grid_attach (GTK_GRID (grid), check, 0, 1, 2, 1);

  check = shopno_menu_dialog_check_new (settings, "show-settings-dialogs", _("Show _settings dialogs in All apps"));
  gtk_widget_set_tooltip_text (check, _("Settings dialogs can always be found by searching"));
  gtk_grid_attach (GTK_GRID (grid), check, 0, 2, 2, 1);

  check = shopno_menu_dialog_check_new (settings, "enable-blur", _("_Blur the background behind the menu"));
  gtk_grid_attach (GTK_GRID (grid), check, 0, 3, 2, 1);

  markup = g_strdup_printf ("<small>%s <a href=\"%s\">%s</a></small>",
                            _("Blur needs the picom compositor with the Shopno Menu rules."),
                            BLUR_GUIDE_URL, _("How to set it up"));
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 50);
  gtk_widget_set_margin_start (label, 24);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 4, 2, 1);
  g_free (markup);

  gtk_widget_show_all (dialog);
}
