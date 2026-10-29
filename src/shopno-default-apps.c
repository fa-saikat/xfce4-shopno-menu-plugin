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

/*
 * Finds the user's file manager, web browser and terminal, to pin on first
 * run. Only called when no pins are saved yet, so the app scans below cost
 * nothing on later startups.
 */

#include "config.h"

#include <gio/gdesktopappinfo.h>
#include <libxfce4util/libxfce4util.h>

#include "shopno-default-apps.h"

#define PREFERRED_TERMINAL "xfce4-terminal.desktop"

typedef struct
{
  const gchar *helper;    /* Key in xfce4/helpers.rc */
  const gchar *mime_type; /* XDG fallback, NULL if there is none */
  const gchar *category;  /* Last resort, NULL to skip */
} Role;

/* Pinned in this order */
static const Role roles[] =
{
  { "FileManager", "inode/directory", NULL },
  { "WebBrowser", "x-scheme-handler/https", NULL },
  { "TerminalEmulator", NULL, "TerminalEmulator" },
};



static gchar *
shopno_default_apps_shown_id (GAppInfo *info)
{
  if (info == NULL || !g_app_info_should_show (info))
    return NULL;

  return g_strdup (g_app_info_get_id (info));
}



/* The desktop id of the app that runs one of @binaries */
static gchar *
shopno_default_apps_find_by_binary (GList        *apps,
                                    gchar * const *binaries)
{
  for (GList *li = apps; li != NULL; li = li->next)
    {
      const gchar *executable = g_app_info_get_executable (li->data);
      gchar       *base;
      gboolean     found;

      if (executable == NULL || !g_app_info_should_show (li->data))
        continue;

      base = g_path_get_basename (executable);
      found = g_strv_contains ((const gchar * const *) binaries, base);
      g_free (base);

      if (found)
        return g_strdup (g_app_info_get_id (li->data));
    }

  return NULL;
}



/* Maps an Xfce helper name such as "google-chrome" to an app's desktop id */
static gchar *
shopno_default_apps_from_helper (const gchar  *helper,
                                 GList       **apps)
{
  GDesktopAppInfo *info;
  gchar           *desktop_id;
  gchar           *resource;
  gchar           *path;
  GKeyFile        *key_file;
  gchar          **binaries;
  gchar           *id = NULL;

  /* Usually the helper and the app share a name */
  desktop_id = g_strconcat (helper, ".desktop", NULL);
  info = g_desktop_app_info_new (desktop_id);
  g_free (desktop_id);
  if (info != NULL)
    {
      id = shopno_default_apps_shown_id (G_APP_INFO (info));
      g_object_unref (info);
      if (id != NULL)
        return id;
    }

  /* Otherwise match the binaries the helper file lists */
  resource = g_strconcat ("xfce4/helpers/", helper, ".desktop", NULL);
  path = xfce_resource_lookup (XFCE_RESOURCE_DATA, resource);
  g_free (resource);
  if (path == NULL)
    return NULL;

  key_file = g_key_file_new ();
  if (g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL))
    {
      binaries = g_key_file_get_string_list (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                             "X-XFCE-Binaries", NULL, NULL);
      if (binaries != NULL)
        {
          if (*apps == NULL)
            *apps = g_app_info_get_all ();
          id = shopno_default_apps_find_by_binary (*apps, binaries);
          g_strfreev (binaries);
        }
    }
  g_key_file_free (key_file);
  g_free (path);

  return id;
}



/* Prefers xfce4-terminal, the Xfce default, over other terminals */
static gchar *
shopno_default_apps_from_category (const gchar  *category,
                                   GList       **apps)
{
  GDesktopAppInfo *info = g_desktop_app_info_new (PREFERRED_TERMINAL);
  gchar           *id = shopno_default_apps_shown_id (G_APP_INFO (info));

  g_clear_object (&info);
  if (id != NULL)
    return id;

  if (*apps == NULL)
    *apps = g_app_info_get_all ();

  for (GList *li = *apps; li != NULL; li = li->next)
    {
      const gchar *categories;
      gchar      **list;
      gboolean     found;

      if (!G_IS_DESKTOP_APP_INFO (li->data) || !g_app_info_should_show (li->data))
        continue;

      categories = g_desktop_app_info_get_categories (li->data);
      if (categories == NULL)
        continue;

      list = g_strsplit (categories, ";", -1);
      found = g_strv_contains ((const gchar * const *) list, category);
      g_strfreev (list);

      if (found)
        return g_strdup (g_app_info_get_id (li->data));
    }

  return NULL;
}



/**
 * shopno_default_apps_get_pins:
 *
 * Returns: (transfer full): desktop ids of the installed file manager, web
 * browser and terminal, possibly empty.
 **/
gchar **
shopno_default_apps_get_pins (void)
{
  GStrvBuilder *builder = g_strv_builder_new ();
  GPtrArray    *seen = g_ptr_array_new_with_free_func (g_free);
  GList        *apps = NULL;
  XfceRc       *helpers;
  gchar       **pins;

  /* Merges the user's choices over the system defaults */
  helpers = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "xfce4/helpers.rc", TRUE);

  for (guint i = 0; i < G_N_ELEMENTS (roles); i++)
    {
      const gchar *helper = helpers != NULL ? xfce_rc_read_entry (helpers, roles[i].helper, NULL) : NULL;
      gchar       *id = NULL;

      if (helper != NULL && *helper != '\0')
        id = shopno_default_apps_from_helper (helper, &apps);

      if (id == NULL && roles[i].mime_type != NULL)
        {
          GAppInfo *info = g_app_info_get_default_for_type (roles[i].mime_type, FALSE);

          id = shopno_default_apps_shown_id (info);
          g_clear_object (&info);
        }

      if (id == NULL && roles[i].category != NULL)
        id = shopno_default_apps_from_category (roles[i].category, &apps);

      if (id != NULL && !g_ptr_array_find_with_equal_func (seen, id, g_str_equal, NULL))
        {
          g_strv_builder_add (builder, id);
          g_ptr_array_add (seen, id);
        }
      else
        {
          g_free (id);
        }
    }

  if (helpers != NULL)
    xfce_rc_close (helpers);
  g_list_free_full (apps, g_object_unref);
  g_ptr_array_unref (seen);

  pins = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);

  return pins;
}
