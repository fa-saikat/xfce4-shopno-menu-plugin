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

#include <glib/gi18n-lib.h>
#include <libxfce4ui/libxfce4ui.h>

#include "shopno-session.h"

#define ACCOUNTS_SERVICE "org.freedesktop.Accounts"
#define NOTIFY_SERVICE   "org.freedesktop.Notifications"
#define NOTIFY_PATH      "/org/freedesktop/Notifications"
#define DBUS_TIMEOUT     500

typedef struct
{
  const gchar *name;
  const gchar *label;
  /* Alternatives, the first one whose program is installed is used */
  const gchar *commands[3];
} ActionInfo;

static const ActionInfo actions[SHOPNO_SESSION_N_ACTIONS] =
{
  [SHOPNO_SESSION_ACCOUNT] =
    { "account", N_("Change account settings"), { "users-admin", "mugshot", NULL } },
  [SHOPNO_SESSION_SETTINGS] =
    { "settings", N_("Settings"), { "xfce4-settings-manager", NULL } },
  [SHOPNO_SESSION_LOCK] =
    { "lock", N_("Lock"), { "xflock4", NULL } },
  [SHOPNO_SESSION_SWITCH_USER] =
    { "switch-user", N_("Switch user"), { "dm-tool switch-to-greeter", "gdmflexiserver", NULL } },
  [SHOPNO_SESSION_LOG_OUT] =
    { "log-out", N_("Sign out"), { "xfce4-session-logout --logout --fast", NULL } },
  [SHOPNO_SESSION_SUSPEND] =
    { "suspend", N_("Sleep"), { "xfce4-session-logout --suspend", NULL } },
  [SHOPNO_SESSION_RESTART] =
    { "restart", N_("Restart"), { "xfce4-session-logout --reboot --fast", NULL } },
  [SHOPNO_SESSION_SHUT_DOWN] =
    { "shut-down", N_("Shut down"), { "xfce4-session-logout --halt --fast", NULL } },
  /* Copied to the clipboard, not saved */
  [SHOPNO_SESSION_SCREENSHOT_SCREEN] =
    { "screenshot-screen", N_("Entire screen"), { "xfce4-screenshooter --fullscreen --clipboard", NULL } },
  [SHOPNO_SESSION_SCREENSHOT_REGION] =
    { "screenshot-region", N_("Select a region"), { "xfce4-screenshooter --region --clipboard", NULL } },
};



static gboolean
shopno_session_command_available (const gchar *command)
{
  gchar *program = g_strndup (command, strcspn (command, " "));
  gchar *path = g_find_program_in_path (program);

  g_free (program);
  g_free (path);

  return path != NULL;
}



static const gchar *
shopno_session_action_get_command (ShopnoSessionAction action)
{
  for (guint i = 0; actions[action].commands[i] != NULL; i++)
    if (shopno_session_command_available (actions[action].commands[i]))
      return actions[action].commands[i];

  return NULL;
}



const gchar *
shopno_session_action_get_name (ShopnoSessionAction action)
{
  g_return_val_if_fail (action < SHOPNO_SESSION_N_ACTIONS, NULL);

  return actions[action].name;
}



const gchar *
shopno_session_action_get_label (ShopnoSessionAction action)
{
  g_return_val_if_fail (action < SHOPNO_SESSION_N_ACTIONS, NULL);

  return _(actions[action].label);
}



gboolean
shopno_session_action_available (ShopnoSessionAction action)
{
  g_return_val_if_fail (action < SHOPNO_SESSION_N_ACTIONS, FALSE);

  return shopno_session_action_get_command (action) != NULL;
}



void
shopno_session_action_run (ShopnoSessionAction  action,
                           GdkScreen           *screen)
{
  const gchar *command;
  GError      *error = NULL;

  g_return_if_fail (action < SHOPNO_SESSION_N_ACTIONS);

  command = shopno_session_action_get_command (action);
  if (command == NULL)
    return;

  if (!xfce_spawn_command_line (screen, command, FALSE, FALSE, TRUE, &error))
    {
      xfce_dialog_show_error (NULL, error, _("Failed to run \"%s\""), command);
      g_error_free (error);
    }
}



typedef struct
{
  GtkClipboard *clipboard;
  gulong        owner_change_id;
  gboolean      copied;
} Screenshot;



static void
shopno_session_screenshot_owner_change (Screenshot *shot)
{
  shot->copied = TRUE;
}



/* Sent straight over D-Bus, so there is no libnotify dependency */
static void
shopno_session_notify_copied (void)
{
  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  if (bus == NULL)
    return;

  g_dbus_connection_call (bus, NOTIFY_SERVICE, NOTIFY_PATH, NOTIFY_SERVICE, "Notify",
                          g_variant_new ("(susssasa{sv}i)", _("Shopno Menu"), 0, "camera-photo",
                                         _("Screenshot copied"), _("Paste it with Ctrl+V."),
                                         NULL, NULL, -1),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  g_object_unref (bus);
}



static void
shopno_session_screenshot_exited (GPid     pid,
                                  gint     status,
                                  gpointer data)
{
  Screenshot *shot = data;

  g_spawn_close_pid (pid);
  g_signal_handler_disconnect (shot->clipboard, shot->owner_change_id);

  /* Nothing is copied when the region selection is cancelled */
  if (shot->copied && g_spawn_check_wait_status (status, NULL))
    shopno_session_notify_copied ();

  g_free (shot);
}



/**
 * shopno_session_take_screenshot:
 *
 * Runs a screenshot action and shows a notification once the image is on
 * the clipboard.
 **/
void
shopno_session_take_screenshot (ShopnoSessionAction  action,
                                GdkScreen           *screen)
{
  const gchar  *command;
  gchar       **argv = NULL;
  GPid          pid;
  Screenshot   *shot;
  GError       *error = NULL;

  g_return_if_fail (action == SHOPNO_SESSION_SCREENSHOT_SCREEN
                    || action == SHOPNO_SESSION_SCREENSHOT_REGION);

  command = shopno_session_action_get_command (action);
  if (command == NULL)
    return;

  if (!g_shell_parse_argv (command, NULL, &argv, &error)
      || !g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                         NULL, NULL, &pid, &error))
    {
      xfce_dialog_show_error (NULL, error, _("Failed to run \"%s\""), command);
      g_error_free (error);
      g_strfreev (argv);
      return;
    }
  g_strfreev (argv);

  /* The clipboard changes owner when the screenshot tool copies the image */
  shot = g_new0 (Screenshot, 1);
  shot->clipboard = gtk_clipboard_get_for_display (gdk_screen_get_display (screen), GDK_SELECTION_CLIPBOARD);
  shot->owner_change_id = g_signal_connect_swapped (shot->clipboard, "owner-change",
                                                    G_CALLBACK (shopno_session_screenshot_owner_change), shot);
  g_child_watch_add (pid, shopno_session_screenshot_exited, shot);
}



gchar *
shopno_session_get_display_name (void)
{
  const gchar *name = g_get_real_name ();

  /* GLib returns "Unknown" when the GECOS field is empty */
  if (name == NULL || *name == '\0' || g_strcmp0 (name, "Unknown") == 0)
    name = g_get_user_name ();

  return g_strdup (name);
}



/* Ask AccountsService for the user's picture, which is what the login
 * screen and most account settings tools use */
static gchar *
shopno_session_get_accounts_icon (void)
{
  GDBusConnection *bus;
  GVariant        *result;
  GVariant        *value;
  gchar           *object_path = NULL;
  gchar           *icon = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
  if (bus == NULL)
    return NULL;

  result = g_dbus_connection_call_sync (bus, ACCOUNTS_SERVICE, "/org/freedesktop/Accounts",
                                        ACCOUNTS_SERVICE, "FindUserByName",
                                        g_variant_new ("(s)", g_get_user_name ()),
                                        G_VARIANT_TYPE ("(o)"), G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                        DBUS_TIMEOUT, NULL, NULL);
  if (result != NULL)
    {
      g_variant_get (result, "(o)", &object_path);
      g_variant_unref (result);
    }

  if (object_path != NULL)
    {
      result = g_dbus_connection_call_sync (bus, ACCOUNTS_SERVICE, object_path,
                                            "org.freedesktop.DBus.Properties", "Get",
                                            g_variant_new ("(ss)", ACCOUNTS_SERVICE ".User", "IconFile"),
                                            G_VARIANT_TYPE ("(v)"), G_DBUS_CALL_FLAGS_NONE,
                                            DBUS_TIMEOUT, NULL, NULL);
      if (result != NULL)
        {
          g_variant_get (result, "(v)", &value);
          if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
            icon = g_variant_dup_string (value, NULL);
          g_variant_unref (value);
          g_variant_unref (result);
        }
      g_free (object_path);
    }

  g_object_unref (bus);

  return icon;
}



gchar *
shopno_session_get_avatar_path (void)
{
  gchar *path = shopno_session_get_accounts_icon ();

  if (path != NULL && g_file_test (path, G_FILE_TEST_IS_REGULAR))
    return path;
  g_free (path);

  path = g_build_filename (g_get_home_dir (), ".face", NULL);
  if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
    return path;
  g_free (path);

  return NULL;
}
