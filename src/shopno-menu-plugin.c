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
#include <xfconf/xfconf.h>

#include "shopno-default-apps.h"
#include "shopno-menu-dialog.h"
#include "shopno-menu-settings.h"
#include "shopno-menu-window.h"

/* Desktop ids contain no ';', so lists are stored as one string. Xfconf
 * cannot store empty arrays, and an empty pinned list must persist. */
#define LIST_SEPARATOR ";"

/* Space between the menu and the panel or the edges of the screen */
#define POPUP_GAP 12

typedef struct _ShopnoMenuPluginClass ShopnoMenuPluginClass;
typedef struct _ShopnoMenuPlugin      ShopnoMenuPlugin;

struct _ShopnoMenuPluginClass
{
  XfcePanelPluginClass __parent__;
};

struct _ShopnoMenuPlugin
{
  XfcePanelPlugin     __parent__;

  ShopnoMenuSettings *settings;
  XfconfChannel      *channel;
  guint               create_window_id;

  GtkWidget          *button;
  GtkWidget          *icon;
  GtkWidget          *window;
};

/* Resident: the module registers GResources, static GTypes, and a
 * screen-wide CSS provider, none of which survive being unloaded */
XFCE_PANEL_DEFINE_PLUGIN_RESIDENT (ShopnoMenuPlugin, shopno_menu_plugin)



/*
 * Settings storage
 */

static gchar *
shopno_menu_plugin_property_path (ShopnoMenuPlugin *self,
                                  const gchar      *name)
{
  return g_strconcat (xfce_panel_plugin_get_property_base (XFCE_PANEL_PLUGIN (self)), "/", name, NULL);
}



static void
shopno_menu_plugin_load_list (ShopnoMenuPlugin *self,
                              const gchar      *name)
{
  gchar  *path = shopno_menu_plugin_property_path (self, name);
  gchar  *value;
  gchar **list;

  /* A missing property keeps the defaults from ShopnoMenuSettings */
  if (xfconf_channel_has_property (self->channel, path))
    {
      value = xfconf_channel_get_string (self->channel, path, "");
      list = g_strsplit (value, LIST_SEPARATOR, -1);
      g_object_set (self->settings, name, list, NULL);
      g_strfreev (list);
      g_free (value);
    }

  g_free (path);
}



/* On first run, pin the user's file manager, browser and terminal. Saved
 * right away, so the lookup never runs again. */
static void
shopno_menu_plugin_pin_defaults (ShopnoMenuPlugin *self)
{
  gchar  *path = shopno_menu_plugin_property_path (self, "pinned");
  gchar **pins;

  if (!xfconf_channel_has_property (self->channel, path))
    {
      pins = shopno_default_apps_get_pins ();
      shopno_menu_settings_set_pinned (self->settings, (const gchar * const *) pins);
      if (pins[0] == NULL)
        xfconf_channel_set_string (self->channel, path, "");
      g_strfreev (pins);
    }

  g_free (path);
}



static void
shopno_menu_plugin_save_list (ShopnoMenuSettings *settings,
                              GParamSpec         *pspec,
                              ShopnoMenuPlugin   *self)
{
  const gchar *name = g_param_spec_get_name (pspec);
  gchar       *path = shopno_menu_plugin_property_path (self, name);
  gchar      **list;
  gchar       *value;

  g_object_get (settings, name, &list, NULL);
  value = g_strjoinv (LIST_SEPARATOR, list);
  xfconf_channel_set_string (self->channel, path, value);

  g_free (value);
  g_strfreev (list);
  g_free (path);
}



static void
shopno_menu_plugin_bind (ShopnoMenuPlugin *self,
                         const gchar      *name,
                         GType             type)
{
  gchar *path = shopno_menu_plugin_property_path (self, name);

  xfconf_g_property_bind (self->channel, path, type, self->settings, name);
  g_free (path);
}



static void
shopno_menu_plugin_load_settings (ShopnoMenuPlugin *self)
{
  GError *error = NULL;

  if (!xfconf_init (&error))
    {
      g_warning ("Failed to connect to xfconf, settings will not be saved: %s", error->message);
      g_error_free (error);
      return;
    }

  self->channel = xfconf_channel_get ("xfce4-panel");

  shopno_menu_plugin_load_list (self, "pinned");
  shopno_menu_plugin_load_list (self, "recent");
  g_signal_connect (self->settings, "notify::pinned",
                    G_CALLBACK (shopno_menu_plugin_save_list), self);
  g_signal_connect (self->settings, "notify::recent",
                    G_CALLBACK (shopno_menu_plugin_save_list), self);
  shopno_menu_plugin_pin_defaults (self);

  shopno_menu_plugin_bind (self, "show-recent", G_TYPE_BOOLEAN);
  shopno_menu_plugin_bind (self, "enable-blur", G_TYPE_BOOLEAN);
  shopno_menu_plugin_bind (self, "button-icon", G_TYPE_STRING);
  shopno_menu_plugin_bind (self, "view-mode", G_TYPE_STRING);
  shopno_menu_plugin_bind (self, "show-settings-dialogs", G_TYPE_BOOLEAN);
}



/*
 * Popup
 */

static void
shopno_menu_plugin_update_icon (ShopnoMenuPlugin *self)
{
  const gchar *icon_name = shopno_menu_settings_get_button_icon (self->settings);

  if (g_path_is_absolute (icon_name))
    {
      GFile *file = g_file_new_for_path (icon_name);
      GIcon *icon = g_file_icon_new (file);

      gtk_image_set_from_gicon (GTK_IMAGE (self->icon), icon, GTK_ICON_SIZE_BUTTON);
      g_object_unref (icon);
      g_object_unref (file);
    }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (self->icon), icon_name, GTK_ICON_SIZE_BUTTON);
    }

  gtk_image_set_pixel_size (GTK_IMAGE (self->icon),
                            xfce_panel_plugin_get_icon_size (XFCE_PANEL_PLUGIN (self)));
}



static void
shopno_menu_plugin_window_hidden (ShopnoMenuPlugin *self)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (self), FALSE);
}



static void
shopno_menu_plugin_create_window (ShopnoMenuPlugin *self)
{
  if (self->create_window_id != 0)
    {
      g_source_remove (self->create_window_id);
      self->create_window_id = 0;
    }

  if (self->window != NULL)
    return;

  self->window = shopno_menu_window_new (self->settings);
  g_signal_connect_swapped (self->window, "hide",
                            G_CALLBACK (shopno_menu_plugin_window_hidden), self);
}



static gboolean
shopno_menu_plugin_create_window_idle (gpointer data)
{
  ShopnoMenuPlugin *self = data;

  self->create_window_id = 0;
  shopno_menu_plugin_create_window (self);

  return G_SOURCE_REMOVE;
}



static void
shopno_menu_plugin_add_gap (ShopnoMenuPlugin *self,
                            gint             *x,
                            gint             *y)
{
  GdkDisplay     *display = gtk_widget_get_display (self->button);
  GdkMonitor     *monitor;
  GdkRectangle    geometry;
  GtkRequisition  size;
  gboolean        horizontal = TRUE;

  /* Move the menu away from the panel, on the side it opens towards */
  switch (xfce_panel_plugin_arrow_type (XFCE_PANEL_PLUGIN (self)))
    {
    case GTK_ARROW_UP:
      *y -= POPUP_GAP;
      break;

    case GTK_ARROW_DOWN:
      *y += POPUP_GAP;
      break;

    case GTK_ARROW_LEFT:
      *x -= POPUP_GAP;
      horizontal = FALSE;
      break;

    case GTK_ARROW_RIGHT:
      *x += POPUP_GAP;
      horizontal = FALSE;
      break;

    default:
      break;
    }

  /* and keep it clear of the screen edges along the panel */
  monitor = gdk_display_get_monitor_at_window (display, gtk_widget_get_window (self->button));
  if (monitor == NULL)
    return;

  gdk_monitor_get_geometry (monitor, &geometry);
  gtk_widget_get_preferred_size (self->window, NULL, &size);

  if (horizontal)
    *x = MAX (geometry.x + POPUP_GAP, MIN (*x, geometry.x + geometry.width - size.width - POPUP_GAP));
  else
    *y = MAX (geometry.y + POPUP_GAP, MIN (*y, geometry.y + geometry.height - size.height - POPUP_GAP));
}



static void
shopno_menu_plugin_popup (ShopnoMenuPlugin *self)
{
  gint x, y;

  shopno_menu_plugin_create_window (self);

  gtk_window_set_screen (GTK_WINDOW (self->window), gtk_widget_get_screen (self->button));

  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (self), TRUE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), TRUE);

  xfce_panel_plugin_position_widget (XFCE_PANEL_PLUGIN (self), self->window, self->button, &x, &y);
  shopno_menu_plugin_add_gap (self, &x, &y);
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



/*
 * XfcePanelPlugin
 */

static void
shopno_menu_plugin_construct (XfcePanelPlugin *plugin)
{
  ShopnoMenuPlugin *self = (ShopnoMenuPlugin *) plugin;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  xfce_panel_plugin_set_small (plugin, TRUE);
  xfce_panel_plugin_menu_show_configure (plugin);
  xfce_panel_plugin_menu_show_about (plugin);

  shopno_menu_plugin_load_settings (self);
  g_signal_connect_swapped (self->settings, "notify::button-icon",
                            G_CALLBACK (shopno_menu_plugin_update_icon), self);
  shopno_menu_plugin_update_icon (self);

  /* Loading the applications menu takes a moment, so do it before the
   * first click rather than on it */
  self->create_window_id = g_idle_add (shopno_menu_plugin_create_window_idle, self);
}



static void
shopno_menu_plugin_free_data (XfcePanelPlugin *plugin)
{
  ShopnoMenuPlugin *self = (ShopnoMenuPlugin *) plugin;

  if (self->create_window_id != 0)
    g_source_remove (self->create_window_id);

  if (self->window != NULL)
    gtk_widget_destroy (self->window);

  g_signal_handlers_disconnect_by_data (self->settings, self);
  if (self->channel != NULL)
    {
      xfconf_g_property_unbind_all (self->settings);
      xfconf_shutdown ();
    }
  g_object_unref (self->settings);
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
shopno_menu_plugin_configure_plugin (XfcePanelPlugin *plugin)
{
  shopno_menu_dialog_show (plugin, ((ShopnoMenuPlugin *) plugin)->settings);
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
                         "logo-icon-name", SHOPNO_MENU_DEFAULT_ICON,
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
  plugin_class->configure_plugin = shopno_menu_plugin_configure_plugin;
  plugin_class->about = shopno_menu_plugin_about;
  plugin_class->remote_event = shopno_menu_plugin_remote_event;
}



static void
shopno_menu_plugin_init (ShopnoMenuPlugin *self)
{
  self->settings = shopno_menu_settings_new ();

  self->button = xfce_panel_create_toggle_button ();
  gtk_widget_set_tooltip_text (self->button, _("Start"));
  g_signal_connect (self->button, "button-press-event",
                    G_CALLBACK (shopno_menu_plugin_button_press_event), self);
  gtk_container_add (GTK_CONTAINER (self), self->button);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (self), self->button);

  self->icon = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (self->button), self->icon);

  gtk_widget_show_all (self->button);
}
