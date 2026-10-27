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

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4util/libxfce4util.h>

#include "shopno-menu-window.h"

#define DEFAULT_ICON_NAME "start-here"

typedef struct _ShopnoMenuPluginClass ShopnoMenuPluginClass;
typedef struct _ShopnoMenuPlugin      ShopnoMenuPlugin;

struct _ShopnoMenuPluginClass
{
  XfcePanelPluginClass __parent__;
};

struct _ShopnoMenuPlugin
{
  XfcePanelPlugin __parent__;

  GtkWidget *button;
  GtkWidget *icon;
  GtkWidget *window;
};

/* Resident: the module registers GResources and a screen-wide CSS
 * provider, neither of which survive being unloaded */
XFCE_PANEL_DEFINE_PLUGIN_RESIDENT (ShopnoMenuPlugin, shopno_menu_plugin,
                                   shopno_menu_window_register_type)



static void
shopno_menu_plugin_window_hidden (ShopnoMenuPlugin *self)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (self), FALSE);
}



static void
shopno_menu_plugin_popup (ShopnoMenuPlugin *self)
{
  gint x, y;

  if (self->window == NULL)
    {
      self->window = shopno_menu_window_new ();
      g_signal_connect_swapped (self->window, "hide",
                                G_CALLBACK (shopno_menu_plugin_window_hidden), self);
    }

  gtk_window_set_screen (GTK_WINDOW (self->window), gtk_widget_get_screen (self->button));

  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (self), TRUE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), TRUE);

  xfce_panel_plugin_position_widget (XFCE_PANEL_PLUGIN (self), self->window, self->button, &x, &y);
  shopno_menu_window_popup (SHOPNO_MENU_WINDOW (self->window), x, y);
}



static void
shopno_menu_plugin_toggle (ShopnoMenuPlugin *self)
{
  if (self->window != NULL && gtk_widget_get_visible (self->window))
    gtk_widget_hide (self->window);
  else
    shopno_menu_plugin_popup (self);
}



static gboolean
shopno_menu_plugin_button_press_event (GtkWidget        *button,
                                       GdkEventButton   *event,
                                       ShopnoMenuPlugin *self)
{
  /* Leave Ctrl+click and other buttons to the panel */
  if (event->button != 1
      || event->type != GDK_BUTTON_PRESS
      || (event->state & GDK_CONTROL_MASK) != 0)
    return GDK_EVENT_PROPAGATE;

  shopno_menu_plugin_toggle (self);

  return GDK_EVENT_STOP;
}



static void
shopno_menu_plugin_construct (XfcePanelPlugin *plugin)
{
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  xfce_panel_plugin_set_small (plugin, TRUE);
  xfce_panel_plugin_menu_show_about (plugin);
}



static void
shopno_menu_plugin_free_data (XfcePanelPlugin *plugin)
{
  ShopnoMenuPlugin *self = (ShopnoMenuPlugin *) plugin;

  if (self->window != NULL)
    gtk_widget_destroy (self->window);
}



static gboolean
shopno_menu_plugin_size_changed (XfcePanelPlugin *plugin,
                                 gint             size)
{
  ShopnoMenuPlugin *self = (ShopnoMenuPlugin *) plugin;

  size /= xfce_panel_plugin_get_nrows (plugin);
  gtk_widget_set_size_request (GTK_WIDGET (plugin), size, size);
  gtk_image_set_pixel_size (GTK_IMAGE (self->icon), xfce_panel_plugin_get_icon_size (plugin));

  return TRUE;
}



static void
shopno_menu_plugin_about (XfcePanelPlugin *plugin)
{
  const gchar *authors[] = { "FA Saikat", NULL };

  gtk_show_about_dialog (NULL,
                         "program-name", _("Shopno Menu"),
                         "version", PACKAGE_VERSION,
                         "comments", _("A Windows 11-style start menu for the Xfce panel"),
                         "website", "https://github.com/fa-saikat/xfce4-shopno-menu-plugin",
                         "license-type", GTK_LICENSE_GPL_2_0,
                         "authors", authors,
                         "logo-icon-name", DEFAULT_ICON_NAME,
                         NULL);
}



static gboolean
shopno_menu_plugin_remote_event (XfcePanelPlugin *plugin,
                                 const gchar     *name,
                                 const GValue    *value)
{
  /* Sent by xfce4-popup-shopno-menu, usually bound to the Super key */
  if (g_strcmp0 (name, "popup") != 0)
    return FALSE;

  shopno_menu_plugin_toggle ((ShopnoMenuPlugin *) plugin);

  return TRUE;
}



static void
shopno_menu_plugin_class_init (ShopnoMenuPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);

  plugin_class->construct = shopno_menu_plugin_construct;
  plugin_class->free_data = shopno_menu_plugin_free_data;
  plugin_class->size_changed = shopno_menu_plugin_size_changed;
  plugin_class->about = shopno_menu_plugin_about;
  plugin_class->remote_event = shopno_menu_plugin_remote_event;
}



static void
shopno_menu_plugin_init (ShopnoMenuPlugin *self)
{
  self->button = xfce_panel_create_toggle_button ();
  gtk_widget_set_tooltip_text (self->button, _("Start"));
  g_signal_connect (self->button, "button-press-event",
                    G_CALLBACK (shopno_menu_plugin_button_press_event), self);
  gtk_container_add (GTK_CONTAINER (self), self->button);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (self), self->button);

  self->icon = gtk_image_new_from_icon_name (DEFAULT_ICON_NAME, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (self->button), self->icon);

  gtk_widget_show_all (self->button);
}
