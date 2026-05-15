/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Rodrigo Moya <rodrigo@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __E_CATEGORIES_CONFIG_H__
#define __E_CATEGORIES_CONFIG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean e_categories_config_get_icon_for (const gchar *category, GdkPixbuf **pixbuf);
void     e_categories_config_open_dialog_for_entry (GtkEntry *entry);

G_END_DECLS

#endif
