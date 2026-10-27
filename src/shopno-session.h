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

#ifndef __SHOPNO_SESSION_H__
#define __SHOPNO_SESSION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  SHOPNO_SESSION_ACCOUNT,
  SHOPNO_SESSION_SETTINGS,
  SHOPNO_SESSION_LOCK,
  SHOPNO_SESSION_SWITCH_USER,
  SHOPNO_SESSION_LOG_OUT,
  SHOPNO_SESSION_SUSPEND,
  SHOPNO_SESSION_RESTART,
  SHOPNO_SESSION_SHUT_DOWN,
  SHOPNO_SESSION_N_ACTIONS
} ShopnoSessionAction;

const gchar *shopno_session_action_get_name    (ShopnoSessionAction action);
const gchar *shopno_session_action_get_label   (ShopnoSessionAction action);
gboolean     shopno_session_action_available   (ShopnoSessionAction action);
void         shopno_session_action_run         (ShopnoSessionAction action,
                                                GdkScreen          *screen);

gchar       *shopno_session_get_display_name   (void);
gchar       *shopno_session_get_avatar_path    (void);

G_END_DECLS

#endif /* !__SHOPNO_SESSION_H__ */
