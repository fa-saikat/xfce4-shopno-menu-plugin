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

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n-lib.h>

#include "shopno-app-model.h"

/* Wait for a burst of file changes (e.g. a package install) to settle */
#define RELOAD_DELAY 500

enum
{
  CHANGED,
  N_SIGNALS
};

struct _ShopnoAppModel
{
  GObject     __parent__;

  GarconMenu *menu;
  GPtrArray  *categories; /* ShopnoAppCategory */
  GHashTable *items;      /* desktop id -> GarconMenuItem */
  guint       reload_id;
};

typedef struct
{
  GarconMenuItem *item;
  gint            score;
} SearchMatch;

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (ShopnoAppModel, shopno_app_model, G_TYPE_OBJECT)



static void
shopno_app_category_free (gpointer data)
{
  ShopnoAppCategory *category = data;

  g_free (category->name);
  g_ptr_array_unref (category->items);
  g_free (category);
}



static gint
shopno_app_model_compare_items (gconstpointer a,
                                gconstpointer b)
{
  GarconMenuItem *item_a = *(GarconMenuItem **) a;
  GarconMenuItem *item_b = *(GarconMenuItem **) b;

  return g_utf8_collate (garcon_menu_item_get_name (item_a),
                         garcon_menu_item_get_name (item_b));
}



static void
shopno_app_model_collect (ShopnoAppModel    *self,
                          GarconMenu        *menu,
                          ShopnoAppCategory *category)
{
  GList *elements = garcon_menu_get_elements (menu);

  for (GList *li = elements; li != NULL; li = li->next)
    {
      if (!garcon_menu_element_get_visible (li->data))
        continue;

      if (GARCON_IS_MENU_ITEM (li->data))
        {
          GarconMenuItem *item = li->data;
          const gchar    *id = garcon_menu_item_get_desktop_id (item);

          if (id == NULL || garcon_menu_item_get_name (item) == NULL)
            continue;

          g_ptr_array_add (category->items, g_object_ref (item));
          if (!g_hash_table_contains (self->items, id))
            g_hash_table_insert (self->items, g_strdup (id), g_object_ref (item));
        }
      else if (GARCON_IS_MENU (li->data))
        {
          /* Nested menus are flattened into their top-level category */
          shopno_app_model_collect (self, li->data, category);
        }
    }

  g_list_free (elements);
}



static void shopno_app_model_reload_required (ShopnoAppModel *self);



static void
shopno_app_model_load (ShopnoAppModel *self)
{
  GError *error = NULL;
  GList  *menus;

  if (self->menu != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->menu, self);
      g_clear_object (&self->menu);
    }
  g_ptr_array_set_size (self->categories, 0);
  g_hash_table_remove_all (self->items);

  self->menu = garcon_menu_new_applications ();
  if (!garcon_menu_load (self->menu, NULL, &error))
    {
      g_warning ("Failed to load the applications menu: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect_swapped (self->menu, "reload-required",
                            G_CALLBACK (shopno_app_model_reload_required), self);

  menus = garcon_menu_get_menus (self->menu);
  for (GList *li = menus; li != NULL; li = li->next)
    {
      ShopnoAppCategory *category;

      if (!garcon_menu_element_get_visible (li->data))
        continue;

      category = g_new0 (ShopnoAppCategory, 1);
      category->name = g_strdup (garcon_menu_element_get_name (li->data));
      category->items = g_ptr_array_new_with_free_func (g_object_unref);
      shopno_app_model_collect (self, li->data, category);

      if (category->name == NULL || category->items->len == 0)
        {
          shopno_app_category_free (category);
          continue;
        }

      g_ptr_array_sort (category->items, shopno_app_model_compare_items);
      g_ptr_array_add (self->categories, category);
    }
  g_list_free (menus);
}



static gboolean
shopno_app_model_reload (gpointer data)
{
  ShopnoAppModel *self = SHOPNO_APP_MODEL (data);

  self->reload_id = 0;
  shopno_app_model_load (self);
  g_signal_emit (self, signals[CHANGED], 0);

  return G_SOURCE_REMOVE;
}



static void
shopno_app_model_reload_required (ShopnoAppModel *self)
{
  if (self->reload_id != 0)
    g_source_remove (self->reload_id);

  self->reload_id = g_timeout_add (RELOAD_DELAY, shopno_app_model_reload, self);
}



static void
shopno_app_model_finalize (GObject *object)
{
  ShopnoAppModel *self = SHOPNO_APP_MODEL (object);

  if (self->reload_id != 0)
    g_source_remove (self->reload_id);

  if (self->menu != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->menu, self);
      g_object_unref (self->menu);
    }

  g_ptr_array_unref (self->categories);
  g_hash_table_unref (self->items);

  G_OBJECT_CLASS (shopno_app_model_parent_class)->finalize (object);
}



static void
shopno_app_model_class_init (ShopnoAppModelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = shopno_app_model_finalize;

  /* Emitted after installed applications changed and the model reloaded */
  signals[CHANGED] =
    g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  garcon_set_environment_xdg (GARCON_ENVIRONMENT_XFCE);
}



static void
shopno_app_model_init (ShopnoAppModel *self)
{
  self->categories = g_ptr_array_new_with_free_func (shopno_app_category_free);
  self->items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  shopno_app_model_load (self);
}



ShopnoAppModel *
shopno_app_model_new (void)
{
  return g_object_new (SHOPNO_TYPE_APP_MODEL, NULL);
}



/**
 * shopno_app_model_get_categories:
 *
 * Returns: (transfer none) (element-type ShopnoAppCategory): the top-level
 * menu categories, valid until the next ::changed emission.
 **/
GPtrArray *
shopno_app_model_get_categories (ShopnoAppModel *model)
{
  g_return_val_if_fail (SHOPNO_IS_APP_MODEL (model), NULL);

  return model->categories;
}



/**
 * shopno_app_model_get_all:
 *
 * Returns: (transfer container) (element-type GarconMenuItem): every app,
 * sorted by name.
 **/
GPtrArray *
shopno_app_model_get_all (ShopnoAppModel *model)
{
  GPtrArray      *items;
  GHashTableIter  iter;
  gpointer        item;

  g_return_val_if_fail (SHOPNO_IS_APP_MODEL (model), NULL);

  items = g_ptr_array_new_full (g_hash_table_size (model->items), g_object_unref);

  g_hash_table_iter_init (&iter, model->items);
  while (g_hash_table_iter_next (&iter, NULL, &item))
    g_ptr_array_add (items, g_object_ref (item));

  g_ptr_array_sort (items, shopno_app_model_compare_items);

  return items;
}



GarconMenuItem *
shopno_app_model_lookup (ShopnoAppModel *model,
                         const gchar    *desktop_id)
{
  g_return_val_if_fail (SHOPNO_IS_APP_MODEL (model), NULL);

  if (desktop_id == NULL)
    return NULL;

  return g_hash_table_lookup (model->items, desktop_id);
}



static gchar *
shopno_app_model_fold (const gchar *text)
{
  gchar *normalized;
  gchar *folded;

  if (text == NULL)
    return NULL;

  normalized = g_utf8_normalize (text, -1, G_NORMALIZE_ALL);
  if (normalized == NULL)
    return NULL;

  folded = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  return folded;
}



static gboolean
shopno_app_model_contains (const gchar *haystack,
                           const gchar *needle)
{
  gchar    *folded = shopno_app_model_fold (haystack);
  gboolean  found = folded != NULL && strstr (folded, needle) != NULL;

  g_free (folded);

  return found;
}



/* Lower is better, -1 means no match */
static gint
shopno_app_model_score (GarconMenuItem *item,
                        const gchar    *query)
{
  const gchar *name = garcon_menu_item_get_name (item);
  const gchar *command = garcon_menu_item_get_command (item);
  gchar       *folded_name = shopno_app_model_fold (name);
  gint         score = -1;

  if (folded_name != NULL)
    {
      if (g_str_has_prefix (folded_name, query))
        score = 0;
      else if (g_str_match_string (query, name, TRUE))
        score = 1;
      else if (strstr (folded_name, query) != NULL)
        score = 2;
    }
  g_free (folded_name);

  if (score >= 0)
    return score;

  if (shopno_app_model_contains (garcon_menu_item_get_generic_name (item), query))
    return 3;

  for (GList *li = garcon_menu_item_get_keywords (item); li != NULL; li = li->next)
    if (shopno_app_model_contains (li->data, query))
      return 3;

  if (command != NULL)
    {
      gchar    *program = g_strndup (command, strcspn (command, " "));
      gchar    *base = g_path_get_basename (program);
      gboolean  found = shopno_app_model_contains (base, query);

      g_free (program);
      g_free (base);
      if (found)
        return 3;
    }

  if (shopno_app_model_contains (garcon_menu_item_get_comment (item), query))
    return 4;

  return -1;
}



static gint
shopno_app_model_compare_matches (gconstpointer a,
                                  gconstpointer b)
{
  const SearchMatch *match_a = a;
  const SearchMatch *match_b = b;

  if (match_a->score != match_b->score)
    return match_a->score - match_b->score;

  return g_utf8_collate (garcon_menu_item_get_name (match_a->item),
                         garcon_menu_item_get_name (match_b->item));
}



/**
 * shopno_app_model_search:
 *
 * Returns: (transfer container) (element-type GarconMenuItem): matching
 * items, best match first.
 **/
GPtrArray *
shopno_app_model_search (ShopnoAppModel *model,
                         const gchar    *query,
                         guint           max_results)
{
  GPtrArray      *results;
  GArray         *matches;
  GHashTableIter  iter;
  gpointer        item;
  gchar          *folded;
  gchar          *stripped;

  g_return_val_if_fail (SHOPNO_IS_APP_MODEL (model), NULL);

  results = g_ptr_array_new_with_free_func (g_object_unref);

  stripped = g_strstrip (g_strdup (query != NULL ? query : ""));
  folded = shopno_app_model_fold (stripped);
  g_free (stripped);

  if (folded == NULL || *folded == '\0')
    {
      g_free (folded);
      return results;
    }

  matches = g_array_new (FALSE, FALSE, sizeof (SearchMatch));

  g_hash_table_iter_init (&iter, model->items);
  while (g_hash_table_iter_next (&iter, NULL, &item))
    {
      SearchMatch match = { item, shopno_app_model_score (item, folded) };

      if (match.score >= 0)
        g_array_append_val (matches, match);
    }

  g_array_sort (matches, shopno_app_model_compare_matches);

  for (guint i = 0; i < matches->len && (max_results == 0 || i < max_results); i++)
    g_ptr_array_add (results, g_object_ref (g_array_index (matches, SearchMatch, i).item));

  g_array_free (matches, TRUE);
  g_free (folded);

  return results;
}



/**
 * shopno_app_get_info:
 *
 * Returns: (transfer full) (nullable): the desktop file of @item.
 **/
GDesktopAppInfo *
shopno_app_get_info (GarconMenuItem *item)
{
  GDesktopAppInfo *info = NULL;
  gchar           *uri;
  gchar           *path;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);

  uri = garcon_menu_item_get_uri (item);
  path = uri != NULL ? g_filename_from_uri (uri, NULL, NULL) : NULL;
  if (path != NULL)
    info = g_desktop_app_info_new_from_filename (path);
  g_free (uri);
  g_free (path);

  return info;
}



static GAppLaunchContext *
shopno_app_launch_context_new (GdkScreen *screen)
{
  GdkAppLaunchContext *context;

  context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
  gdk_app_launch_context_set_timestamp (context, gtk_get_current_event_time ());

  return G_APP_LAUNCH_CONTEXT (context);
}



static GDesktopAppInfo *
shopno_app_get_info_or_error (GarconMenuItem  *item,
                              GError         **error)
{
  GDesktopAppInfo *info = shopno_app_get_info (item);

  if (info == NULL)
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                 _("The application \"%s\" could not be found"),
                 garcon_menu_item_get_name (item));

  return info;
}



gboolean
shopno_app_launch (GarconMenuItem  *item,
                   GdkScreen       *screen,
                   GError         **error)
{
  GDesktopAppInfo   *info;
  GAppLaunchContext *context;
  gboolean           result;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  info = shopno_app_get_info_or_error (item, error);
  if (info == NULL)
    return FALSE;

  context = shopno_app_launch_context_new (screen);
  result = g_app_info_launch (G_APP_INFO (info), NULL, context, error);

  g_object_unref (context);
  g_object_unref (info);

  return result;
}



/* Launches one of the actions in the desktop file, such as "New Window" */
gboolean
shopno_app_launch_action (GarconMenuItem  *item,
                          const gchar     *action,
                          GdkScreen       *screen,
                          GError         **error)
{
  GDesktopAppInfo   *info;
  GAppLaunchContext *context;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  g_return_val_if_fail (action != NULL, FALSE);

  info = shopno_app_get_info_or_error (item, error);
  if (info == NULL)
    return FALSE;

  context = shopno_app_launch_context_new (screen);
  g_desktop_app_info_launch_action (info, action, context);

  g_object_unref (context);
  g_object_unref (info);

  return TRUE;
}
