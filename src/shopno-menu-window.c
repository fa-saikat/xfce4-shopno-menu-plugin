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

#include "shopno-app-model.h"
#include "shopno-app-tile.h"
#include "shopno-menu-window.h"
#include "shopno-session.h"

/* Size of the menu frame, matching the Windows 11 Start menu at 100% scale */
#define MENU_WIDTH  640
#define MENU_HEIGHT 720

#define GRID_COLUMNS       6
#define CATEGORY_COLUMNS   4
#define MAX_SEARCH_RESULTS 50
#define AVATAR_SIZE        32

/* Window roles, matched by the picom rules shipped in extras/picom */
#define ROLE_PLAIN "shopno-menu"
#define ROLE_BLUR  "shopno-menu-blur"

/* The pointer is often still grabbed by the panel button when the menu
 * maps, so retry the seat grab a few times before giving up */
#define GRAB_RETRY_INTERVAL 50
#define GRAB_MAX_RETRIES    10

#define CATEGORY_KEY "shopno-category"
#define RECENT_KEY   "shopno-recent"

enum
{
  PROP_0,
  PROP_SETTINGS,
  N_PROPERTIES
};

struct _ShopnoMenuWindow
{
  GtkWindow           __parent__;

  ShopnoMenuSettings *settings;
  ShopnoAppModel     *model;
  guint               rebuild_id;
  gboolean            home_dirty;

  GtkWidget          *frame;
  GtkWidget          *search_entry;
  GtkWidget          *search_placeholder;
  GtkWidget          *stack;

  GtkWidget          *home_scroll;
  GtkWidget          *pinned_grid;
  GtkWidget          *pinned_empty;
  GtkWidget          *recent_title;
  GtkWidget          *recent_grid;
  GtkWidget          *category_grid;

  GtkWidget          *search_scroll;
  GtkWidget          *search_list;
  GtkWidget          *search_empty;

  GtkWidget          *category_scroll;
  GtkWidget          *category_title;
  GtkWidget          *category_apps;

  GtkWidget          *app_menu;

  gboolean            grab_input;
  GdkSeat            *grab_seat;
  guint               grab_retry_id;
  guint               grab_retries;
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE (ShopnoMenuWindow, shopno_menu_window, GTK_TYPE_WINDOW)



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



static void
shopno_menu_window_update_blur (ShopnoMenuWindow *self)
{
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gboolean         blur = shopno_menu_settings_get_enable_blur (self->settings);

  /* With blur on, picom draws the blur, shadow and rounded corners, so the
   * frame fills the window. Otherwise the menu draws its own shadow. */
  gtk_window_set_role (GTK_WINDOW (self), blur ? ROLE_BLUR : ROLE_PLAIN);

  if (blur)
    gtk_style_context_add_class (context, "blur");
  else
    gtk_style_context_remove_class (context, "blur");
}



/*
 * Input grab
 */

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



/*
 * Launching
 */

static void
shopno_menu_window_launch (ShopnoMenuWindow *self,
                           GarconMenuItem   *item)
{
  GError *error = NULL;

  g_object_ref (item);
  gtk_widget_hide (GTK_WIDGET (self));

  if (shopno_app_launch (item, gtk_widget_get_screen (GTK_WIDGET (self)), &error))
    {
      shopno_menu_settings_add_recent (self->settings, garcon_menu_item_get_desktop_id (item));
    }
  else
    {
      xfce_dialog_show_error (NULL, error, _("Failed to launch \"%s\""),
                              garcon_menu_item_get_name (item));
      g_error_free (error);
    }

  g_object_unref (item);
}



static void
shopno_menu_window_grid_child_activated (GtkFlowBox       *grid,
                                         GtkFlowBoxChild  *child,
                                         ShopnoMenuWindow *self)
{
  GarconMenuItem *item = shopno_app_tile_get_item (gtk_bin_get_child (GTK_BIN (child)));

  if (item != NULL)
    shopno_menu_window_launch (self, item);
}



static void
shopno_menu_window_search_row_activated (GtkListBox       *list,
                                         GtkListBoxRow    *row,
                                         ShopnoMenuWindow *self)
{
  GarconMenuItem *item = shopno_app_tile_get_item (gtk_bin_get_child (GTK_BIN (row)));

  if (item != NULL)
    shopno_menu_window_launch (self, item);
}



/*
 * App context menu
 */

static void
shopno_menu_window_action_pin (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       data)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (data);

  shopno_menu_settings_pin (self->settings, g_variant_get_string (parameter, NULL));
}



static void
shopno_menu_window_action_unpin (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       data)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (data);

  shopno_menu_settings_unpin (self->settings, g_variant_get_string (parameter, NULL));
}



static void
shopno_menu_window_action_move_to_front (GSimpleAction *action,
                                         GVariant      *parameter,
                                         gpointer       data)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (data);

  shopno_menu_settings_move_to_front (self->settings, g_variant_get_string (parameter, NULL));
}



static void
shopno_menu_window_action_remove_recent (GSimpleAction *action,
                                         GVariant      *parameter,
                                         gpointer       data)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (data);

  shopno_menu_settings_remove_recent (self->settings, g_variant_get_string (parameter, NULL));
}



static const GActionEntry app_actions[] =
{
  { .name = "pin", .activate = shopno_menu_window_action_pin, .parameter_type = "s" },
  { .name = "unpin", .activate = shopno_menu_window_action_unpin, .parameter_type = "s" },
  { .name = "move-to-front", .activate = shopno_menu_window_action_move_to_front, .parameter_type = "s" },
  { .name = "remove-recent", .activate = shopno_menu_window_action_remove_recent, .parameter_type = "s" },
};



static void
shopno_menu_window_menu_append (GMenu       *menu,
                                const gchar *label,
                                const gchar *action,
                                const gchar *desktop_id)
{
  GMenuItem *menu_item = g_menu_item_new (label, NULL);

  g_menu_item_set_action_and_target_value (menu_item, action, g_variant_new_string (desktop_id));
  g_menu_append_item (menu, menu_item);
  g_object_unref (menu_item);
}



static void
shopno_menu_window_show_app_menu (ShopnoMenuWindow *self,
                                  GtkWidget        *child,
                                  gboolean          is_recent)
{
  GarconMenuItem      *item = shopno_app_tile_get_item (gtk_bin_get_child (GTK_BIN (child)));
  const gchar         *id;
  const gchar * const *pinned;
  GMenu               *menu;

  if (item == NULL)
    return;

  id = garcon_menu_item_get_desktop_id (item);
  pinned = shopno_menu_settings_get_pinned (self->settings);

  menu = g_menu_new ();
  if (shopno_menu_settings_is_pinned (self->settings, id))
    {
      shopno_menu_window_menu_append (menu, _("Unpin from Start"), "app.unpin", id);
      if (g_strcmp0 (pinned[0], id) != 0)
        shopno_menu_window_menu_append (menu, _("Move to front"), "app.move-to-front", id);
    }
  else
    {
      shopno_menu_window_menu_append (menu, _("Pin to Start"), "app.pin", id);
    }

  if (is_recent)
    shopno_menu_window_menu_append (menu, _("Remove from list"), "app.remove-recent", id);

  if (self->app_menu != NULL)
    gtk_widget_destroy (self->app_menu);

  self->app_menu = gtk_popover_new_from_model (child, G_MENU_MODEL (menu));
  g_object_add_weak_pointer (G_OBJECT (self->app_menu), (gpointer *) &self->app_menu);
  gtk_popover_popup (GTK_POPOVER (self->app_menu));

  g_object_unref (menu);
}



static gboolean
shopno_menu_window_grid_button_press (GtkWidget        *grid,
                                      GdkEventButton   *event,
                                      ShopnoMenuWindow *self)
{
  GtkFlowBoxChild *child;

  if (!gdk_event_triggers_context_menu ((GdkEvent *) event))
    return GDK_EVENT_PROPAGATE;

  child = gtk_flow_box_get_child_at_pos (GTK_FLOW_BOX (grid), event->x, event->y);
  if (child == NULL)
    return GDK_EVENT_PROPAGATE;

  shopno_menu_window_show_app_menu (self, GTK_WIDGET (child),
                                    g_object_get_data (G_OBJECT (grid), RECENT_KEY) != NULL);

  return GDK_EVENT_STOP;
}



static gboolean
shopno_menu_window_list_button_press (GtkWidget        *list,
                                      GdkEventButton   *event,
                                      ShopnoMenuWindow *self)
{
  GtkListBoxRow *row;

  if (!gdk_event_triggers_context_menu ((GdkEvent *) event))
    return GDK_EVENT_PROPAGATE;

  row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (list), event->y);
  if (row == NULL)
    return GDK_EVENT_PROPAGATE;

  shopno_menu_window_show_app_menu (self, GTK_WIDGET (row), FALSE);

  return GDK_EVENT_STOP;
}



/*
 * Pages
 */

static GtkWidget *
shopno_menu_window_title_new (const gchar *text)
{
  GtkWidget *label = gtk_label_new (text);

  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "shopno-menu-section-title");

  return label;
}



static GtkWidget *
shopno_menu_window_grid_new (ShopnoMenuWindow *self,
                             guint             columns,
                             const gchar      *style_class)
{
  GtkWidget *grid = gtk_flow_box_new ();

  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (grid), GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (grid), TRUE);
  /* Wrapping labels are at least as wide as their longest word, so with a
   * wide font the grid drops columns instead of widening the menu */
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (grid), 1);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (grid), columns);
  gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (grid), TRUE);
  gtk_widget_set_valign (grid, GTK_ALIGN_START);
  gtk_style_context_add_class (gtk_widget_get_style_context (grid), style_class);

  return grid;
}



static GtkWidget *
shopno_menu_window_app_grid_new (ShopnoMenuWindow *self)
{
  GtkWidget *grid = shopno_menu_window_grid_new (self, GRID_COLUMNS, "shopno-app-grid");

  g_signal_connect (grid, "child-activated",
                    G_CALLBACK (shopno_menu_window_grid_child_activated), self);
  g_signal_connect (grid, "button-press-event",
                    G_CALLBACK (shopno_menu_window_grid_button_press), self);

  return grid;
}



static GtkWidget *
shopno_menu_window_scroll_new (GtkWidget *child)
{
  GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (scroll), TRUE);
  gtk_container_add (GTK_CONTAINER (scroll), child);

  return scroll;
}



static void
shopno_menu_window_clear (GtkWidget *container)
{
  gtk_container_foreach (GTK_CONTAINER (container), (GtkCallback) (void (*) (void)) gtk_widget_destroy, NULL);
}



static guint
shopno_menu_window_fill_grid (ShopnoMenuWindow    *self,
                              GtkWidget           *grid,
                              const gchar * const *ids,
                              guint                max)
{
  guint n = 0;

  shopno_menu_window_clear (grid);

  for (guint i = 0; ids != NULL && ids[i] != NULL && (max == 0 || n < max); i++)
    {
      GarconMenuItem *item = shopno_app_model_lookup (self->model, ids[i]);

      if (item == NULL)
        continue;

      gtk_container_add (GTK_CONTAINER (grid), shopno_app_tile_new (item, SHOPNO_APP_TILE_GRID));
      n++;
    }

  return n;
}



static void
shopno_menu_window_rebuild_home (ShopnoMenuWindow *self)
{
  GPtrArray *categories;
  gboolean   show_recent;
  guint      n;

  if (self->rebuild_id != 0)
    {
      g_source_remove (self->rebuild_id);
      self->rebuild_id = 0;
    }

  if (self->app_menu != NULL)
    gtk_widget_destroy (self->app_menu);

  n = shopno_menu_window_fill_grid (self, self->pinned_grid,
                                    shopno_menu_settings_get_pinned (self->settings), 0);
  gtk_widget_set_visible (self->pinned_grid, n > 0);
  gtk_widget_set_visible (self->pinned_empty, n == 0);

  n = shopno_menu_window_fill_grid (self, self->recent_grid,
                                    shopno_menu_settings_get_recent (self->settings),
                                    SHOPNO_MENU_MAX_RECENT);
  show_recent = n > 0 && shopno_menu_settings_get_show_recent (self->settings);
  gtk_widget_set_visible (self->recent_title, show_recent);
  gtk_widget_set_visible (self->recent_grid, show_recent);

  shopno_menu_window_clear (self->category_grid);
  categories = shopno_app_model_get_categories (self->model);
  for (guint i = 0; i < categories->len; i++)
    {
      ShopnoAppCategory *category = g_ptr_array_index (categories, i);
      GtkWidget         *card = shopno_category_card_new (category);

      g_object_set_data (G_OBJECT (card), CATEGORY_KEY, category);
      gtk_container_add (GTK_CONTAINER (self->category_grid), card);
    }

  self->home_dirty = FALSE;
}



static gboolean
shopno_menu_window_rebuild_idle (gpointer data)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (data);

  self->rebuild_id = 0;
  shopno_menu_window_rebuild_home (self);

  return G_SOURCE_REMOVE;
}



static void
shopno_menu_window_queue_rebuild (ShopnoMenuWindow *self)
{
  self->home_dirty = TRUE;

  /* Rebuild later: this may run from a context menu action whose popover
   * is attached to a tile that the rebuild destroys */
  if (gtk_widget_get_visible (GTK_WIDGET (self)) && self->rebuild_id == 0)
    self->rebuild_id = g_idle_add (shopno_menu_window_rebuild_idle, self);
}



static void
shopno_menu_window_model_changed (ShopnoMenuWindow *self)
{
  /* Category pointers are invalid after a reload */
  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->stack)), "category") == 0)
    gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "home");

  shopno_menu_window_queue_rebuild (self);
}



static void
shopno_menu_window_show_category (ShopnoMenuWindow  *self,
                                  ShopnoAppCategory *category)
{
  GtkAdjustment *adjustment;

  gtk_label_set_text (GTK_LABEL (self->category_title), category->name);

  shopno_menu_window_clear (self->category_apps);
  for (guint i = 0; i < category->items->len; i++)
    gtk_container_add (GTK_CONTAINER (self->category_apps),
                       shopno_app_tile_new (g_ptr_array_index (category->items, i), SHOPNO_APP_TILE_GRID));

  adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->category_scroll));
  gtk_adjustment_set_value (adjustment, 0.0);

  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "category");
}



static void
shopno_menu_window_category_activated (GtkFlowBox       *grid,
                                       GtkFlowBoxChild  *child,
                                       ShopnoMenuWindow *self)
{
  ShopnoAppCategory *category;

  category = g_object_get_data (G_OBJECT (gtk_bin_get_child (GTK_BIN (child))), CATEGORY_KEY);
  if (category != NULL)
    shopno_menu_window_show_category (self, category);
}



static void
shopno_menu_window_show_home (ShopnoMenuWindow *self)
{
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "home");
}



/*
 * Search
 */

static void
shopno_menu_window_search_changed (ShopnoMenuWindow *self)
{
  const gchar *text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
  GPtrArray   *results;
  GtkWidget   *row;

  if (*text == '\0')
    {
      shopno_menu_window_show_home (self);
      return;
    }

  shopno_menu_window_clear (self->search_list);

  results = shopno_app_model_search (self->model, text, MAX_SEARCH_RESULTS);
  for (guint i = 0; i < results->len; i++)
    gtk_container_add (GTK_CONTAINER (self->search_list),
                       shopno_app_tile_new (g_ptr_array_index (results, i), SHOPNO_APP_TILE_LIST));

  gtk_widget_set_visible (self->search_list, results->len > 0);
  gtk_widget_set_visible (self->search_empty, results->len == 0);

  /* The best match is highlighted and launched by Enter */
  row = GTK_WIDGET (gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->search_list), 0));
  if (row != NULL)
    gtk_list_box_select_row (GTK_LIST_BOX (self->search_list), GTK_LIST_BOX_ROW (row));

  g_ptr_array_unref (results);

  gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->search_scroll)), 0.0);
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "search");
}



static void
shopno_menu_window_search_text_changed (ShopnoMenuWindow *self)
{
  const gchar *text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));

  /* Not done in ::search-changed, which is delayed while typing */
  gtk_widget_set_visible (self->search_placeholder, *text == '\0');
}



static void
shopno_menu_window_search_activate (ShopnoMenuWindow *self)
{
  GtkListBoxRow *row;

  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->stack)), "search") != 0)
    return;

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->search_list));
  if (row != NULL)
    shopno_menu_window_search_row_activated (GTK_LIST_BOX (self->search_list), row, self);
}



static GtkWidget *
shopno_menu_window_first_child (GtkWidget *container)
{
  GList     *children = gtk_container_get_children (GTK_CONTAINER (container));
  GtkWidget *first = children != NULL ? children->data : NULL;

  g_list_free (children);

  return first;
}



static gboolean
shopno_menu_window_search_key_press (GtkWidget        *entry,
                                     GdkEventKey      *event,
                                     ShopnoMenuWindow *self)
{
  const gchar *page;
  GtkWidget   *target = NULL;

  if (event->keyval != GDK_KEY_Down && event->keyval != GDK_KEY_KP_Down)
    return GDK_EVENT_PROPAGATE;

  /* Move from the search box into the content below it */
  page = gtk_stack_get_visible_child_name (GTK_STACK (self->stack));
  if (g_strcmp0 (page, "search") == 0)
    {
      target = GTK_WIDGET (gtk_list_box_get_selected_row (GTK_LIST_BOX (self->search_list)));
    }
  else if (g_strcmp0 (page, "category") == 0)
    {
      target = shopno_menu_window_first_child (self->category_apps);
    }
  else
    {
      if (gtk_widget_get_visible (self->pinned_grid))
        target = shopno_menu_window_first_child (self->pinned_grid);
      if (target == NULL)
        target = shopno_menu_window_first_child (self->category_grid);
    }

  if (target == NULL)
    return GDK_EVENT_PROPAGATE;

  gtk_widget_grab_focus (target);

  return GDK_EVENT_STOP;
}



/*
 * Footer
 */

static gboolean
shopno_menu_window_avatar_draw (GtkWidget *widget,
                                cairo_t   *cr,
                                GdkPixbuf *pixbuf)
{
  gdouble width = gtk_widget_get_allocated_width (widget);
  gdouble height = gtk_widget_get_allocated_height (widget);

  cairo_arc (cr, width / 2.0, height / 2.0, MIN (width, height) / 2.0, 0.0, 2.0 * G_PI);
  cairo_clip (cr);

  cairo_scale (cr, width / gdk_pixbuf_get_width (pixbuf), height / gdk_pixbuf_get_height (pixbuf));
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0.0, 0.0);
  cairo_paint (cr);

  return GDK_EVENT_STOP;
}



static GtkWidget *
shopno_menu_window_avatar_new (void)
{
  gchar     *path = shopno_session_get_avatar_path ();
  GdkPixbuf *pixbuf = NULL;
  GtkWidget *avatar;

  /* Loaded at twice the size so it stays sharp on HiDPI screens */
  if (path != NULL)
    pixbuf = gdk_pixbuf_new_from_file_at_scale (path, AVATAR_SIZE * 2, AVATAR_SIZE * 2, FALSE, NULL);
  g_free (path);

  if (pixbuf == NULL)
    {
      avatar = gtk_image_new_from_icon_name ("avatar-default-symbolic", GTK_ICON_SIZE_DND);
      gtk_image_set_pixel_size (GTK_IMAGE (avatar), AVATAR_SIZE);
      return avatar;
    }

  avatar = gtk_drawing_area_new ();
  gtk_widget_set_size_request (avatar, AVATAR_SIZE, AVATAR_SIZE);
  gtk_widget_set_valign (avatar, GTK_ALIGN_CENTER);
  g_signal_connect_data (avatar, "draw", G_CALLBACK (shopno_menu_window_avatar_draw),
                         pixbuf, (GClosureNotify) (void (*) (void)) g_object_unref, 0);

  return avatar;
}



static void
shopno_menu_window_session_activate (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       data)
{
  ShopnoMenuWindow    *self = SHOPNO_MENU_WINDOW (data);
  ShopnoSessionAction  session_action;

  session_action = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (action), "shopno-session-action"));

  /* Hide first: the screen locker cannot grab input while the menu does */
  gtk_widget_hide (GTK_WIDGET (self));
  shopno_session_action_run (session_action, gtk_widget_get_screen (GTK_WIDGET (self)));
}



static void
shopno_menu_window_session_append (GMenu               *menu,
                                   ShopnoSessionAction  action)
{
  gchar *detailed;

  if (!shopno_session_action_available (action))
    return;

  detailed = g_strconcat ("session.", shopno_session_action_get_name (action), NULL);
  g_menu_append (menu, shopno_session_action_get_label (action), detailed);
  g_free (detailed);
}



static GtkWidget *
shopno_menu_window_footer_button_new (GtkWidget   *button,
                                      const gchar *icon_name,
                                      const gchar *tooltip)
{
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text (button, tooltip);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

  if (icon_name != NULL)
    gtk_button_set_image (GTK_BUTTON (button),
                          gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR));

  return button;
}



static GtkWidget *
shopno_menu_window_footer_new (ShopnoMenuWindow *self)
{
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GtkWidget          *footer;
  GtkWidget          *button;
  GtkWidget          *box;
  GtkWidget          *label;
  GMenu              *menu;
  GMenu              *section;
  gchar              *name;

  for (guint i = 0; i < SHOPNO_SESSION_N_ACTIONS; i++)
    {
      GSimpleAction *action = g_simple_action_new (shopno_session_action_get_name (i), NULL);

      g_object_set_data (G_OBJECT (action), "shopno-session-action", GUINT_TO_POINTER (i));
      g_signal_connect (action, "activate", G_CALLBACK (shopno_menu_window_session_activate), self);
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
      g_object_unref (action);
    }
  gtk_widget_insert_action_group (GTK_WIDGET (self), "session", G_ACTION_GROUP (group));
  g_object_unref (group);

  footer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_style_context_add_class (gtk_widget_get_style_context (footer), "shopno-menu-footer");

  /* User button: avatar and name, with account and session actions */
  button = shopno_menu_window_footer_button_new (gtk_menu_button_new (), NULL, NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "shopno-menu-user");
  gtk_menu_button_set_direction (GTK_MENU_BUTTON (button), GTK_ARROW_UP);
  gtk_box_pack_start (GTK_BOX (footer), button, FALSE, FALSE, 0);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_add (GTK_CONTAINER (button), box);
  gtk_box_pack_start (GTK_BOX (box), shopno_menu_window_avatar_new (), FALSE, FALSE, 0);

  name = shopno_session_get_display_name ();
  label = gtk_label_new (name);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  g_free (name);

  menu = g_menu_new ();
  section = g_menu_new ();
  shopno_menu_window_session_append (section, SHOPNO_SESSION_ACCOUNT);
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);
  section = g_menu_new ();
  shopno_menu_window_session_append (section, SHOPNO_SESSION_LOCK);
  shopno_menu_window_session_append (section, SHOPNO_SESSION_SWITCH_USER);
  shopno_menu_window_session_append (section, SHOPNO_SESSION_LOG_OUT);
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), G_MENU_MODEL (menu));
  g_object_unref (menu);

  /* Power button, with the actions in Windows 11 order */
  button = shopno_menu_window_footer_button_new (gtk_menu_button_new (), "system-shutdown-symbolic", _("Power"));
  gtk_menu_button_set_direction (GTK_MENU_BUTTON (button), GTK_ARROW_UP);
  gtk_box_pack_end (GTK_BOX (footer), button, FALSE, FALSE, 0);

  menu = g_menu_new ();
  shopno_menu_window_session_append (menu, SHOPNO_SESSION_SUSPEND);
  shopno_menu_window_session_append (menu, SHOPNO_SESSION_SHUT_DOWN);
  shopno_menu_window_session_append (menu, SHOPNO_SESSION_RESTART);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), G_MENU_MODEL (menu));
  gtk_widget_set_sensitive (button, g_menu_model_get_n_items (G_MENU_MODEL (menu)) > 0);
  g_object_unref (menu);

  if (shopno_session_action_available (SHOPNO_SESSION_SETTINGS))
    {
      button = shopno_menu_window_footer_button_new (gtk_button_new (), "preferences-system-symbolic",
                                                     shopno_session_action_get_label (SHOPNO_SESSION_SETTINGS));
      gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "session.settings");
      gtk_box_pack_end (GTK_BOX (footer), button, FALSE, FALSE, 0);
    }

  return footer;
}



/*
 * GtkWidget
 */

static gboolean
shopno_menu_window_map_event (GtkWidget   *widget,
                              GdkEventAny *event)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (widget);

  if (self->grab_input)
    {
      self->grab_retries = 0;
      if (shopno_menu_window_try_grab (self) == G_SOURCE_CONTINUE)
        self->grab_retry_id = g_timeout_add (GRAB_RETRY_INTERVAL, shopno_menu_window_try_grab, self);
    }

  gtk_widget_grab_focus (self->search_entry);

  if (GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->map_event != NULL)
    return GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->map_event (widget, event);

  return GDK_EVENT_PROPAGATE;
}



static void
shopno_menu_window_hide (GtkWidget *widget)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (widget);
  GtkAdjustment    *adjustment;

  shopno_menu_window_ungrab (self);

  if (self->app_menu != NULL)
    gtk_widget_destroy (self->app_menu);

  GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->hide (widget);

  /* Open on the home page next time, like Windows does */
  gtk_entry_set_text (GTK_ENTRY (self->search_entry), "");
  shopno_menu_window_show_home (self);
  adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->home_scroll));
  gtk_adjustment_set_value (adjustment, 0.0);
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
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (widget);
  GtkWidget        *focus = gtk_window_get_focus (GTK_WINDOW (widget));
  gunichar          c = gdk_keyval_to_unicode (event->keyval);
  const gchar      *page;

  if (event->keyval == GDK_KEY_Escape)
    {
      /* Step back one level per press: search, category, then close */
      page = gtk_stack_get_visible_child_name (GTK_STACK (self->stack));
      if (*gtk_entry_get_text (GTK_ENTRY (self->search_entry)) != '\0')
        gtk_entry_set_text (GTK_ENTRY (self->search_entry), "");
      else if (g_strcmp0 (page, "category") == 0)
        shopno_menu_window_show_home (self);
      else
        gtk_widget_hide (widget);

      return GDK_EVENT_STOP;
    }

  if ((event->keyval == GDK_KEY_Menu
       || (event->keyval == GDK_KEY_F10 && (event->state & GDK_SHIFT_MASK) != 0))
      && (GTK_IS_FLOW_BOX_CHILD (focus) || GTK_IS_LIST_BOX_ROW (focus))
      && shopno_app_tile_get_item (gtk_bin_get_child (GTK_BIN (focus))) != NULL)
    {
      shopno_menu_window_show_app_menu (self, focus,
                                        g_object_get_data (G_OBJECT (gtk_widget_get_parent (focus)),
                                                           RECENT_KEY) != NULL);
      return GDK_EVENT_STOP;
    }

  /* Typing anywhere goes to the search box. Space is left alone so it can
   * still activate the focused item. */
  if (focus != self->search_entry
      && c != 0 && c != ' ' && g_unichar_isprint (c)
      && (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) == 0)
    {
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self->search_entry));
      return gtk_widget_event (self->search_entry, (GdkEvent *) event);
    }

  return GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->key_press_event (widget, event);
}



static gboolean
shopno_menu_window_focus_out_event (GtkWidget     *widget,
                                    GdkEventFocus *event)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (widget);

  /* Fallback for when the input grab failed */
  if (self->grab_input && self->grab_seat == NULL)
    gtk_widget_hide (widget);

  return GTK_WIDGET_CLASS (shopno_menu_window_parent_class)->focus_out_event (widget, event);
}



/*
 * GObject
 */

static void
shopno_menu_window_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      self->settings = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
shopno_menu_window_constructed (GObject *object)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (object);

  G_OBJECT_CLASS (shopno_menu_window_parent_class)->constructed (object);

  if (self->settings == NULL)
    self->settings = shopno_menu_settings_new ();

  g_signal_connect_swapped (self->settings, "notify::pinned",
                            G_CALLBACK (shopno_menu_window_queue_rebuild), self);
  g_signal_connect_swapped (self->settings, "notify::recent",
                            G_CALLBACK (shopno_menu_window_queue_rebuild), self);
  g_signal_connect_swapped (self->settings, "notify::show-recent",
                            G_CALLBACK (shopno_menu_window_queue_rebuild), self);
  g_signal_connect_swapped (self->settings, "notify::enable-blur",
                            G_CALLBACK (shopno_menu_window_update_blur), self);

  shopno_menu_window_update_blur (self);
  shopno_menu_window_rebuild_home (self);
}



static void
shopno_menu_window_dispose (GObject *object)
{
  ShopnoMenuWindow *self = SHOPNO_MENU_WINDOW (object);

  shopno_menu_window_ungrab (self);

  if (self->rebuild_id != 0)
    {
      g_source_remove (self->rebuild_id);
      self->rebuild_id = 0;
    }

  if (self->settings != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->settings, self);
      g_clear_object (&self->settings);
    }

  if (self->model != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->model, self);
      g_clear_object (&self->model);
    }

  G_OBJECT_CLASS (shopno_menu_window_parent_class)->dispose (object);
}



static void
shopno_menu_window_class_init (ShopnoMenuWindowClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->set_property = shopno_menu_window_set_property;
  gobject_class->constructed = shopno_menu_window_constructed;
  gobject_class->dispose = shopno_menu_window_dispose;

  widget_class->map_event = shopno_menu_window_map_event;
  widget_class->hide = shopno_menu_window_hide;
  widget_class->button_press_event = shopno_menu_window_button_press_event;
  widget_class->key_press_event = shopno_menu_window_key_press_event;
  widget_class->focus_out_event = shopno_menu_window_focus_out_event;

  properties[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL, SHOPNO_TYPE_MENU_SETTINGS,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);
}



static void
shopno_menu_window_init (ShopnoMenuWindow *self)
{
  GtkWindow          *window = GTK_WINDOW (self);
  GSimpleActionGroup *group;
  GtkWidget          *overlay;
  GtkWidget          *content;
  GtkWidget          *page;
  GtkWidget          *box;
  GtkWidget          *button;

  self->grab_input = TRUE;
  self->model = shopno_app_model_new ();
  g_signal_connect_swapped (self->model, "changed",
                            G_CALLBACK (shopno_menu_window_model_changed), self);

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

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), app_actions, G_N_ELEMENTS (app_actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "app", G_ACTION_GROUP (group));
  g_object_unref (group);

  self->frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request (self->frame, MENU_WIDTH, MENU_HEIGHT);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->frame), "shopno-menu-frame");
  gtk_container_add (GTK_CONTAINER (self), self->frame);

  content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (content), "shopno-menu-content");
  gtk_box_pack_start (GTK_BOX (self->frame), content, TRUE, TRUE, 0);

  /* Search box. GTK 3 hides an entry's placeholder while it has focus,
   * and the menu always focuses it, so draw our own placeholder. */
  overlay = gtk_overlay_new ();
  gtk_box_pack_start (GTK_BOX (content), overlay, FALSE, FALSE, 0);

  self->search_entry = gtk_search_entry_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (self->search_entry), "shopno-menu-search");
  gtk_container_add (GTK_CONTAINER (overlay), self->search_entry);
  g_signal_connect_swapped (self->search_entry, "search-changed",
                            G_CALLBACK (shopno_menu_window_search_changed), self);
  g_signal_connect_swapped (self->search_entry, "changed",
                            G_CALLBACK (shopno_menu_window_search_text_changed), self);
  g_signal_connect_swapped (self->search_entry, "activate",
                            G_CALLBACK (shopno_menu_window_search_activate), self);
  g_signal_connect (self->search_entry, "key-press-event",
                    G_CALLBACK (shopno_menu_window_search_key_press), self);

  self->search_placeholder = gtk_label_new (_("Search for apps"));
  gtk_widget_set_halign (self->search_placeholder, GTK_ALIGN_START);
  gtk_widget_set_valign (self->search_placeholder, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->search_placeholder),
                               "shopno-menu-search-placeholder");
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), self->search_placeholder);
  gtk_overlay_set_overlay_pass_through (GTK_OVERLAY (overlay), self->search_placeholder, TRUE);

  self->stack = gtk_stack_new ();
  gtk_stack_set_transition_type (GTK_STACK (self->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_set_transition_duration (GTK_STACK (self->stack), 120);
  gtk_box_pack_start (GTK_BOX (content), self->stack, TRUE, TRUE, 0);

  /* Home page: pinned, recent, and all apps by category */
  page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (page), "shopno-menu-page");
  self->home_scroll = shopno_menu_window_scroll_new (page);
  gtk_stack_add_named (GTK_STACK (self->stack), self->home_scroll, "home");

  gtk_box_pack_start (GTK_BOX (page), shopno_menu_window_title_new (_("Pinned")), FALSE, FALSE, 0);
  self->pinned_grid = shopno_menu_window_app_grid_new (self);
  gtk_box_pack_start (GTK_BOX (page), self->pinned_grid, FALSE, FALSE, 0);
  self->pinned_empty = gtk_label_new (_("Right-click an app to pin it here."));
  gtk_style_context_add_class (gtk_widget_get_style_context (self->pinned_empty), "shopno-menu-empty");
  gtk_box_pack_start (GTK_BOX (page), self->pinned_empty, FALSE, FALSE, 0);

  self->recent_title = shopno_menu_window_title_new (_("Recent"));
  gtk_box_pack_start (GTK_BOX (page), self->recent_title, FALSE, FALSE, 0);
  self->recent_grid = shopno_menu_window_app_grid_new (self);
  g_object_set_data (G_OBJECT (self->recent_grid), RECENT_KEY, GINT_TO_POINTER (TRUE));
  gtk_box_pack_start (GTK_BOX (page), self->recent_grid, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (page), shopno_menu_window_title_new (_("All")), FALSE, FALSE, 0);
  self->category_grid = shopno_menu_window_grid_new (self, CATEGORY_COLUMNS, "shopno-category-grid");
  g_signal_connect (self->category_grid, "child-activated",
                    G_CALLBACK (shopno_menu_window_category_activated), self);
  gtk_box_pack_start (GTK_BOX (page), self->category_grid, FALSE, FALSE, 0);

  /* Search results page */
  page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (page), "shopno-menu-page");
  self->search_scroll = shopno_menu_window_scroll_new (page);
  gtk_stack_add_named (GTK_STACK (self->stack), self->search_scroll, "search");

  gtk_box_pack_start (GTK_BOX (page), shopno_menu_window_title_new (_("Apps")), FALSE, FALSE, 0);
  self->search_list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->search_list), GTK_SELECTION_BROWSE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (self->search_list), TRUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->search_list), "shopno-search-list");
  g_signal_connect (self->search_list, "row-activated",
                    G_CALLBACK (shopno_menu_window_search_row_activated), self);
  g_signal_connect (self->search_list, "button-press-event",
                    G_CALLBACK (shopno_menu_window_list_button_press), self);
  gtk_box_pack_start (GTK_BOX (page), self->search_list, FALSE, FALSE, 0);
  self->search_empty = gtk_label_new (_("No apps found"));
  gtk_style_context_add_class (gtk_widget_get_style_context (self->search_empty), "shopno-menu-empty");
  gtk_box_pack_start (GTK_BOX (page), self->search_empty, FALSE, FALSE, 0);

  /* Category page */
  page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (page), "shopno-menu-page");
  self->category_scroll = shopno_menu_window_scroll_new (page);
  gtk_stack_add_named (GTK_STACK (self->stack), self->category_scroll, "category");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_style_context_add_class (gtk_widget_get_style_context (box), "shopno-menu-section-header");
  gtk_box_pack_start (GTK_BOX (page), box, FALSE, FALSE, 0);
  button = gtk_button_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text (button, _("Back"));
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (shopno_menu_window_show_home), self);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  self->category_title = shopno_menu_window_title_new (NULL);
  gtk_box_pack_start (GTK_BOX (box), self->category_title, TRUE, TRUE, 0);

  self->category_apps = shopno_menu_window_app_grid_new (self);
  gtk_box_pack_start (GTK_BOX (page), self->category_apps, FALSE, FALSE, 0);

  gtk_box_pack_end (GTK_BOX (self->frame), shopno_menu_window_footer_new (self), FALSE, FALSE, 0);

  gtk_widget_show_all (self->frame);
  gtk_widget_hide (self->search_empty);
}



GtkWidget *
shopno_menu_window_new (ShopnoMenuSettings *settings)
{
  return g_object_new (SHOPNO_TYPE_MENU_WINDOW, "settings", settings, NULL);
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

  if (window->home_dirty)
    shopno_menu_window_rebuild_home (window);

  gtk_window_move (GTK_WINDOW (window), x, y);
  gtk_window_present_with_time (GTK_WINDOW (window), gtk_get_current_event_time ());
}



/* Disabling the grab is only meant for the preview tool, so that a menu
 * opened on a developer's desktop does not capture all input */
void
shopno_menu_window_set_grab_input (ShopnoMenuWindow *window,
                                   gboolean          grab_input)
{
  g_return_if_fail (SHOPNO_IS_MENU_WINDOW (window));

  window->grab_input = grab_input;
}
