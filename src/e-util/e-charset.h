/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CHARSET_H
#define E_CHARSET_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

void		e_charset_add_to_g_menu		(GMenu *menu,
						 const gchar *action_name);

#define E_CHARSET_COLUMN_LABEL	0
#define E_CHARSET_COLUMN_VALUE 	1

GtkListStore *	e_charset_create_list_store	(void);

G_END_DECLS

#endif /* E_CHARSET_H */
