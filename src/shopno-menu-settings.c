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

#include "shopno-menu-settings.h"

enum
{
  PROP_0,
  PROP_PINNED,
  PROP_RECENT,
  PROP_SHOW_RECENT,
  PROP_ENABLE_BLUR,
  PROP_BUTTON_ICON,
  PROP_VIEW_MODE,
  PROP_SHOW_SETTINGS_DIALOGS,
  N_PROPERTIES
};

struct _ShopnoMenuSettings
{
  GObject   __parent__;

  gchar   **pinned;
  gchar   **recent;
  gboolean  show_recent;
  gboolean  enable_blur;
  gchar    *button_icon;
  gchar    *view_mode;
  gboolean  show_settings_dialogs;
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE (ShopnoMenuSettings, shopno_menu_settings, G_TYPE_OBJECT)



/* Returns @strv without @id, with @prepend (if any) as the first element,
 * and at most @max elements (0 for no limit) */
static gchar **
strv_rebuild (const gchar * const *strv,
              const gchar         *id,
              const gchar         *prepend,
              guint                max)
{
  GStrvBuilder *builder = g_strv_builder_new ();
  gchar       **result;
  guint         n = 0;

  if (prepend != NULL)
    {
      g_strv_builder_add (builder, prepend);
      n++;
    }

  for (guint i = 0; strv != NULL && strv[i] != NULL; i++)
    {
      if (max > 0 && n >= max)
        break;
      if (g_strcmp0 (strv[i], id) == 0)
        continue;

      g_strv_builder_add (builder, strv[i]);
      n++;
    }

  result = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);

  return result;
}



static gboolean
strv_replace (gchar               ***field,
              const gchar * const   *value)
{
  const gchar * const empty[] = { NULL };

  if (value == NULL)
    value = empty;

  if (*field != NULL && g_strv_equal ((const gchar * const *) *field, value))
    return FALSE;

  g_strfreev (*field);
  *field = g_strdupv ((gchar **) value);

  return TRUE;
}



static void
shopno_menu_settings_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ShopnoMenuSettings *self = SHOPNO_MENU_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_PINNED:
      g_value_set_boxed (value, self->pinned);
      break;

    case PROP_RECENT:
      g_value_set_boxed (value, self->recent);
      break;

    case PROP_SHOW_RECENT:
      g_value_set_boolean (value, self->show_recent);
      break;

    case PROP_ENABLE_BLUR:
      g_value_set_boolean (value, self->enable_blur);
      break;

    case PROP_BUTTON_ICON:
      g_value_set_string (value, self->button_icon);
      break;

    case PROP_VIEW_MODE:
      g_value_set_string (value, self->view_mode);
      break;

    case PROP_SHOW_SETTINGS_DIALOGS:
      g_value_set_boolean (value, self->show_settings_dialogs);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
shopno_menu_settings_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ShopnoMenuSettings *self = SHOPNO_MENU_SETTINGS (object);
  gboolean            bool_value;
  const gchar        *icon;
  const gchar        *mode;

  switch (prop_id)
    {
    case PROP_PINNED:
      shopno_menu_settings_set_pinned (self, g_value_get_boxed (value));
      break;

    case PROP_RECENT:
      shopno_menu_settings_set_recent (self, g_value_get_boxed (value));
      break;

    case PROP_SHOW_RECENT:
      bool_value = g_value_get_boolean (value);
      if (self->show_recent != bool_value)
        {
          self->show_recent = bool_value;
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_ENABLE_BLUR:
      bool_value = g_value_get_boolean (value);
      if (self->enable_blur != bool_value)
        {
          self->enable_blur = bool_value;
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_BUTTON_ICON:
      icon = g_value_get_string (value);
      if (g_strcmp0 (self->button_icon, icon) != 0)
        {
          g_free (self->button_icon);
          self->button_icon = g_strdup (icon);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_VIEW_MODE:
      mode = g_value_get_string (value);
      /* Unknown values, from a newer version for example, fall back to the default */
      if (g_strcmp0 (mode, SHOPNO_MENU_VIEW_GRID) != 0 && g_strcmp0 (mode, SHOPNO_MENU_VIEW_LIST) != 0)
        mode = SHOPNO_MENU_VIEW_CATEGORY;
      if (g_strcmp0 (self->view_mode, mode) != 0)
        {
          g_free (self->view_mode);
          self->view_mode = g_strdup (mode);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_SHOW_SETTINGS_DIALOGS:
      bool_value = g_value_get_boolean (value);
      if (self->show_settings_dialogs != bool_value)
        {
          self->show_settings_dialogs = bool_value;
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
shopno_menu_settings_finalize (GObject *object)
{
  ShopnoMenuSettings *self = SHOPNO_MENU_SETTINGS (object);

  g_strfreev (self->pinned);
  g_strfreev (self->recent);
  g_free (self->button_icon);
  g_free (self->view_mode);

  G_OBJECT_CLASS (shopno_menu_settings_parent_class)->finalize (object);
}



static void
shopno_menu_settings_class_init (ShopnoMenuSettingsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shopno_menu_settings_get_property;
  gobject_class->set_property = shopno_menu_settings_set_property;
  gobject_class->finalize = shopno_menu_settings_finalize;

  properties[PROP_PINNED] =
    g_param_spec_boxed ("pinned", NULL, NULL, G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_RECENT] =
    g_param_spec_boxed ("recent", NULL, NULL, G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SHOW_RECENT] =
    g_param_spec_boolean ("show-recent", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_ENABLE_BLUR] =
    g_param_spec_boolean ("enable-blur", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_BUTTON_ICON] =
    g_param_spec_string ("button-icon", NULL, NULL, SHOPNO_MENU_DEFAULT_ICON,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_VIEW_MODE] =
    g_param_spec_string ("view-mode", NULL, NULL, SHOPNO_MENU_VIEW_CATEGORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SHOW_SETTINGS_DIALOGS] =
    g_param_spec_boolean ("show-settings-dialogs", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);
}



static void
shopno_menu_settings_init (ShopnoMenuSettings *self)
{
  /* Filled from shopno_default_apps_get_pins() on first run */
  self->pinned = g_new0 (gchar *, 1);
  self->recent = g_new0 (gchar *, 1);
  self->show_recent = TRUE;
  self->enable_blur = FALSE;
  self->button_icon = g_strdup (SHOPNO_MENU_DEFAULT_ICON);
  self->view_mode = g_strdup (SHOPNO_MENU_VIEW_CATEGORY);
  self->show_settings_dialogs = FALSE;
}



ShopnoMenuSettings *
shopno_menu_settings_new (void)
{
  return g_object_new (SHOPNO_TYPE_MENU_SETTINGS, NULL);
}



const gchar * const *
shopno_menu_settings_get_pinned (ShopnoMenuSettings *settings)
{
  g_return_val_if_fail (SHOPNO_IS_MENU_SETTINGS (settings), NULL);

  return (const gchar * const *) settings->pinned;
}



void
shopno_menu_settings_set_pinned (ShopnoMenuSettings  *settings,
                                 const gchar * const *pinned)
{
  g_return_if_fail (SHOPNO_IS_MENU_SETTINGS (settings));

  if (strv_replace (&settings->pinned, pinned))
    g_object_notify_by_pspec (G_OBJECT (settings), properties[PROP_PINNED]);
}



gboolean
shopno_menu_settings_is_pinned (ShopnoMenuSettings *settings,
                                const gchar        *desktop_id)
{
  g_return_val_if_fail (SHOPNO_IS_MENU_SETTINGS (settings), FALSE);

  return desktop_id != NULL
         && g_strv_contains ((const gchar * const *) settings->pinned, desktop_id);
}



void
shopno_menu_settings_pin (ShopnoMenuSettings *settings,
                          const gchar        *desktop_id)
{
  GStrvBuilder *builder;
  gchar       **pinned;

  g_return_if_fail (SHOPNO_IS_MENU_SETTINGS (settings));
  g_return_if_fail (desktop_id != NULL);

  if (shopno_menu_settings_is_pinned (settings, desktop_id))
    return;

  builder = g_strv_builder_new ();
  g_strv_builder_addv (builder, (const gchar **) settings->pinned);
  g_strv_builder_add (builder, desktop_id);
  pinned = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);

  shopno_menu_settings_set_pinned (settings, (const gchar * const *) pinned);
  g_strfreev (pinned);
}



void
shopno_menu_settings_unpin (ShopnoMenuSettings *settings,
                            const gchar        *desktop_id)
{
  gchar **pinned;

  g_return_if_fail (SHOPNO_IS_MENU_SETTINGS (settings));

  pinned = strv_rebuild ((const gchar * const *) settings->pinned, desktop_id, NULL, 0);
  shopno_menu_settings_set_pinned (settings, (const gchar * const *) pinned);
  g_strfreev (pinned);
}



void
shopno_menu_settings_move_to_front (ShopnoMenuSettings *settings,
                                    const gchar        *desktop_id)
{
  gchar **pinned;

  g_return_if_fail (SHOPNO_IS_MENU_SETTINGS (settings));

  if (!shopno_menu_settings_is_pinned (settings, desktop_id))
    return;

  pinned = strv_rebuild ((const gchar * const *) settings->pinned, desktop_id, desktop_id, 0);
  shopno_menu_settings_set_pinned (settings, (const gchar * const *) pinned);
  g_strfreev (pinned);
}



const gchar * const *
shopno_menu_settings_get_recent (ShopnoMenuSettings *settings)
{
  g_return_val_if_fail (SHOPNO_IS_MENU_SETTINGS (settings), NULL);

  return (const gchar * const *) settings->recent;
}



void
shopno_menu_settings_set_recent (ShopnoMenuSettings  *settings,
                                 const gchar * const *recent)
{
  g_return_if_fail (SHOPNO_IS_MENU_SETTINGS (settings));

  if (strv_replace (&settings->recent, recent))
    g_object_notify_by_pspec (G_OBJECT (settings), properties[PROP_RECENT]);
}



void
shopno_menu_settings_add_recent (ShopnoMenuSettings *settings,
                                 const gchar        *desktop_id)
{
  gchar **recent;

  g_return_if_fail (SHOPNO_IS_MENU_SETTINGS (settings));
  g_return_if_fail (desktop_id != NULL);

  recent = strv_rebuild ((const gchar * const *) settings->recent,
                         desktop_id, desktop_id, SHOPNO_MENU_MAX_RECENT);
  shopno_menu_settings_set_recent (settings, (const gchar * const *) recent);
  g_strfreev (recent);
}



void
shopno_menu_settings_remove_recent (ShopnoMenuSettings *settings,
                                    const gchar        *desktop_id)
{
  gchar **recent;

  g_return_if_fail (SHOPNO_IS_MENU_SETTINGS (settings));

  recent = strv_rebuild ((const gchar * const *) settings->recent, desktop_id, NULL, 0);
  shopno_menu_settings_set_recent (settings, (const gchar * const *) recent);
  g_strfreev (recent);
}



gboolean
shopno_menu_settings_get_show_recent (ShopnoMenuSettings *settings)
{
  g_return_val_if_fail (SHOPNO_IS_MENU_SETTINGS (settings), TRUE);

  return settings->show_recent;
}



gboolean
shopno_menu_settings_get_enable_blur (ShopnoMenuSettings *settings)
{
  g_return_val_if_fail (SHOPNO_IS_MENU_SETTINGS (settings), FALSE);

  return settings->enable_blur;
}



const gchar *
shopno_menu_settings_get_button_icon (ShopnoMenuSettings *settings)
{
  g_return_val_if_fail (SHOPNO_IS_MENU_SETTINGS (settings), SHOPNO_MENU_DEFAULT_ICON);

  if (settings->button_icon == NULL || *settings->button_icon == '\0')
    return SHOPNO_MENU_DEFAULT_ICON;

  return settings->button_icon;
}



const gchar *
shopno_menu_settings_get_view_mode (ShopnoMenuSettings *settings)
{
  g_return_val_if_fail (SHOPNO_IS_MENU_SETTINGS (settings), SHOPNO_MENU_VIEW_CATEGORY);

  return settings->view_mode;
}



gboolean
shopno_menu_settings_get_show_settings_dialogs (ShopnoMenuSettings *settings)
{
  g_return_val_if_fail (SHOPNO_IS_MENU_SETTINGS (settings), FALSE);

  return settings->show_settings_dialogs;
}
