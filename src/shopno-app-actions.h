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

#ifndef __SHOPNO_APP_ACTIONS_H__
#define __SHOPNO_APP_ACTIONS_H__

#include <garcon/garcon.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Actions on an app's desktop file, offered in its right-click menu */

gboolean shopno_app_can_add_to_desktop (void);
gboolean shopno_app_add_to_desktop     (GarconMenuItem  *item,
                                        GError         **error);

gboolean shopno_app_can_add_to_panel   (void);
gboolean shopno_app_add_to_panel       (GarconMenuItem  *item,
                                        GError         **error);

gboolean shopno_app_can_edit           (void);
gboolean shopno_app_edit               (GarconMenuItem  *item,
                                        GError         **error);

G_END_DECLS

#endif /* !__SHOPNO_APP_ACTIONS_H__ */
