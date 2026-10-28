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
#include <glib/gstdio.h>
#include <libxfce4util/libxfce4util.h>

#include "shopno-app-actions.h"

#define PANEL_COMMAND  "xfce4-panel"
#define EDITOR_COMMAND "exo-desktop-item-edit"



static gboolean
shopno_app_has_program (const gchar *program)
{
  gchar    *path = g_find_program_in_path (program);
  gboolean  found = path != NULL;

  g_free (path);

  return found;
}



static GFile *
shopno_app_get_file (GarconMenuItem  *item,
                     GError         **error)
{
  GFile *file = garcon_menu_item_get_file (item);

  if (file == NULL || !g_file_is_native (file))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   _("The desktop file of \"%s\" could not be found"),
                   garcon_menu_item_get_name (item));
      g_clear_object (&file);
    }

  return file;
}



static gboolean
shopno_app_spawn (const gchar  **argv,
                  GError       **error)
{
  return g_spawn_async (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH,
                        NULL, NULL, NULL, error);
}



gboolean
shopno_app_can_add_to_desktop (void)
{
  return g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP) != NULL;
}



gboolean
shopno_app_add_to_desktop (GarconMenuItem  *item,
                           GError         **error)
{
  const gchar *desktop_dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  GFile       *source;
  GFile       *target;
  gchar       *path;
  GError      *trust_error = NULL;
  gboolean     result;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  source = shopno_app_get_file (item, error);
  if (source == NULL)
    return FALSE;

  path = g_build_filename (desktop_dir, garcon_menu_item_get_desktop_id (item), NULL);
  target = g_file_new_for_path (path);
  g_free (path);

  result = g_file_copy (source, target, G_FILE_COPY_OVERWRITE | G_FILE_COPY_TARGET_DEFAULT_PERMS,
                        NULL, NULL, NULL, error)
           && g_file_set_attribute_uint32 (target, G_FILE_ATTRIBUTE_UNIX_MODE, 0755,
                                           G_FILE_QUERY_INFO_NONE, NULL, error);

  /* Mark it trusted like Thunar does, so xfdesktop launches it without
   * asking first. Not fatal: without gvfs metadata it only asks once. */
  if (result && !xfce_g_file_set_trusted (target, TRUE, NULL, &trust_error))
    {
      g_debug ("Failed to mark the desktop launcher as trusted: %s", trust_error->message);
      g_error_free (trust_error);
    }

  g_object_unref (source);
  g_object_unref (target);

  return result;
}



gboolean
shopno_app_can_add_to_panel (void)
{
  return shopno_app_has_program (PANEL_COMMAND);
}



gboolean
shopno_app_add_to_panel (GarconMenuItem  *item,
                         GError         **error)
{
  GFile       *file;
  gchar       *path;
  const gchar *argv[4] = { PANEL_COMMAND, "--add=launcher", NULL, NULL };
  gboolean     result;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  file = shopno_app_get_file (item, error);
  if (file == NULL)
    return FALSE;

  /* The panel asks which panel to add it to when there are several */
  path = g_file_get_path (file);
  argv[2] = path;
  result = shopno_app_spawn (argv, error);

  g_free (path);
  g_object_unref (file);

  return result;
}



gboolean
shopno_app_can_edit (void)
{
  return shopno_app_has_program (EDITOR_COMMAND);
}



gboolean
shopno_app_edit (GarconMenuItem  *item,
                 GError         **error)
{
  GFile    *source;
  GFile    *target;
  gchar    *user_dir;
  gchar    *path;
  gboolean  result = TRUE;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  source = shopno_app_get_file (item, error);
  if (source == NULL)
    return FALSE;

  /* System files are not writable, so edit a copy in the user's
   * applications folder. Named after the desktop id, it replaces the
   * original in menus instead of adding a second entry. */
  user_dir = g_build_filename (g_get_user_data_dir (), "applications", NULL);
  path = g_build_filename (user_dir, garcon_menu_item_get_desktop_id (item), NULL);
  target = g_file_new_for_path (path);

  if (!g_file_equal (source, target) && !g_file_query_exists (target, NULL))
    {
      g_mkdir_with_parents (user_dir, 0700);
      result = g_file_copy (source, target, G_FILE_COPY_TARGET_DEFAULT_PERMS, NULL, NULL, NULL, error);
    }

  if (result)
    {
      const gchar *argv[] = { EDITOR_COMMAND, path, NULL };

      result = shopno_app_spawn (argv, error);
    }

  g_free (path);
  g_free (user_dir);
  g_object_unref (target);
  g_object_unref (source);

  return result;
}
