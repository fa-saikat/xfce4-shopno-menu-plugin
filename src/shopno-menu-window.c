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

#include "shopno-menu-window.h"

/* Size of the menu frame, matching the Windows 11 Start menu at 100% scale */
#define MENU_WIDTH  640
#define MENU_HEIGHT 720

/* The pointer is often still grabbed by the panel button when the menu
 * maps, so retry the seat grab a few times before giving up */
#define GRAB_RETRY_INTERVAL 50
#define GRAB_MAX_RETRIES    10

struct _ShopnoMenuWindow
{
  GtkWindow  __parent__;

  GtkWidget *frame;
  GtkWidget *search_entry;

  GdkSeat   *grab_seat;
  guint      grab_retry_id;
  guint      grab_retries;
};

XFCE_PANEL_DEFINE_TYPE (ShopnoMenuWindow, shopno_menu_window, GTK_TYPE_WINDOW)



static void
shopno_menu_window_load_css (GdkScreen *screen)
{
  static gboolean  loaded = FALSE;
  GtkCssProvider  *provider;

  if (loaded)
    return;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/shopnoos/menu/shopno-menu.css");
  gtk_style_context_add_provider_for_screen (screen,
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  loaded = TRUE;
}



static void
shopno_menu_window_update_visual (ShopnoMenuWindow *self)
{
  GtkWidget       *widget = GTK_WIDGET (self);
  GdkScreen       *screen = gtk_widget_get_screen (widget);
  GdkVisual       *rgba_visual = gdk_screen_get_rgba_visual (screen);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  gboolean         composited = rgba_visual != NULL && gdk_screen_is_composited (screen);
  GdkVisual       *visual = composited ? rgba_visual : gdk_screen_get_system_visual (screen);

  /* The visual can only change while unrealized */
  if (visual != gtk_widget_get_visual (widget))
    {
      if (gtk_widget_get_realized (widget))
        gtk_widget_unrealize (widget);
      gtk_widget_set_visual (widget, visual);
    }

  gtk_widget_set_app_paintable (widget, composited);

  if (composited)
    gtk_style_context_add_class (context, "composited");
  else
    gtk_style_context_remove_class (context, "composited");
}



static gboolean
shopno_menu_window_try_grab (gpointer data)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (data);
  GdkWindow        *window = gtk_widget_get_window (GTK_WIDGET (self));
  GdkSeat          *seat;

  seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (self)));

  /* owner_events is TRUE so widgets of this process, including the panel
   * button, keep receiving their own events while the menu is open */
  if (gdk_seat_grab (seat, window, GDK_SEAT_CAPABILITY_ALL, TRUE,
                     NULL, NULL, NULL, NULL) == GDK_GRAB_SUCCESS)
    {
      self->grab_seat = seat;
      self->grab_retry_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (++self->grab_retries >= GRAB_MAX_RETRIES)
    {
      g_warning ("Failed to grab input, the menu will close on focus loss only");
      self->grab_retry_id = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}



static void
shopno_menu_window_ungrab (ShopnoMenuWindow *self)
{
  if (self->grab_retry_id != 0)
    {
      g_source_remove (self->grab_retry_id);
      self->grab_retry_id = 0;
    }

  if (self->grab_seat != NULL)
    {
      gdk_seat_ungrab (self->grab_seat);
      self->grab_seat = NULL;
    }
}



static gboolean
shopno_menu_window_map_event (GtkWidget   *widget,
                              GdkEventAny *event)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (widget);

  self->grab_retries = 0;
  if (shopno_menu_window_try_grab (self) == G_SOURCE_CONTINUE)
    self->grab_retry_id = g_timeout_add (GRAB_RETRY_INTERVAL, shopno_menu_window_try_grab, self);

  gtk_widget_grab_focus (self->search_entry);

  if (GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->map_event != NULL)
    return GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->map_event (widget, event);

  return GDK_EVENT_PROPAGATE;
}



static void
shopno_menu_window_hide (GtkWidget *widget)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (widget);

  shopno_menu_window_ungrab (self);
  gtk_entry_set_text (GTK_ENTRY (self->search_entry), "");

  GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->hide (widget);
}



static gboolean
shopno_menu_window_button_press_event (GtkWidget      *widget,
                                       GdkEventButton *event)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (widget);
  GtkAllocation     alloc;
  gint              x, y;

  /* With the seat grabbed, clicks anywhere on screen arrive here. Close the
   * menu when the click lands outside the visible frame. */
  gdk_window_get_origin (gtk_widget_get_window (widget), &x, &y);
  gtk_widget_get_allocation (self->frame, &alloc);
  x += alloc.x;
  y += alloc.y;

  if (event->x_root < x || event->x_root >= x + alloc.width
      || event->y_root < y || event->y_root >= y + alloc.height)
    {
      gtk_widget_hide (widget);
      return GDK_EVENT_STOP;
    }

  if (GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->button_press_event != NULL)
    return GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->button_press_event (widget, event);

  return GDK_EVENT_PROPAGATE;
}



static gboolean
shopno_menu_window_key_press_event (GtkWidget   *widget,
                                    GdkEventKey *event)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_widget_hide (widget);
      return GDK_EVENT_STOP;
    }

  return GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->key_press_event (widget, event);
}



static gboolean
shopno_menu_window_focus_out_event (GtkWidget     *widget,
                                    GdkEventFocus *event)
{
  /* Fallback for when the input grab failed */
  if (SHOPNO_MENU_WINDOW (widget)->grab_seat == NULL)
    gtk_widget_hide (widget);

  return GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->focus_out_event (widget, event);
}



static void
shopno_menu_window_class_init (ShopnoMenuWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->map_event = shopno_menu_window_map_event;
  widget_class->hide = shopno_menu_window_hide;
  widget_class->button_press_event = shopno_menu_window_button_press_event;
  widget_class->key_press_event = shopno_menu_window_key_press_event;
  widget_class->focus_out_event = shopno_menu_window_focus_out_event;
}



static void
shopno_menu_window_init (ShopnoMenuWindow *self)
{
  GtkWindow *window = GTK_WINDOW (self);

  shopno_menu_window_load_css (gtk_widget_get_screen (GTK_WIDGET (self)));

  gtk_window_set_title (window, _("Shopno Menu"));
  gtk_window_set_decorated (window, FALSE);
  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_skip_taskbar_hint (window, TRUE);
  gtk_window_set_skip_pager_hint (window, TRUE);
  gtk_window_set_keep_above (window, TRUE);
  gtk_window_stick (window);
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_POPUP_MENU);
  gtk_widget_add_events (GTK_WIDGET (self), GDK_BUTTON_PRESS_MASK);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "shopno-menu-window");

  shopno_menu_window_update_visual (self);

  self->frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_set_size_request (self->frame, MENU_WIDTH, MENU_HEIGHT);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->frame), "shopno-menu-frame");
  gtk_container_add (GTK_CONTAINER (self), self->frame);

  self->search_entry = gtk_search_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (self->search_entry), _("Search for apps"));
  gtk_style_context_add_class (gtk_widget_get_style_context (self->search_entry), "shopno-menu-search");
  gtk_box_pack_start (GTK_BOX (self->frame), self->search_entry, FALSE, FALSE, 0);

  gtk_widget_show_all (self->frame);
}



GtkWidget *
shopno_menu_window_new (void)
{
  return g_object_new (SHOPNO_TYPE_MENU_WINDOW, NULL);
}



void
shopno_menu_window_popup (ShopnoMenuWindow *window,
                          gint              x,
                          gint              y)
{
  g_return_if_fail (SHOPNO_IS_MENU_WINDOW (window));

  /* The compositor may have been toggled since the last popup */
  if (!gtk_widget_get_visible (GTK_WIDGET (window)))
    shopno_menu_window_update_visual (window);

  gtk_window_move (GTK_WINDOW (window), x, y);
  gtk_window_present_with_time (GTK_WINDOW (window), gtk_get_current_event_time ());
}
