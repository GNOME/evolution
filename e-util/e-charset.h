/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CHARSET_H
#define E_CHARSET_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

GSList *	e_charset_add_radio_actions	(GtkActionGroup *action_group,
						 const gchar *action_prefix,
						 const gchar *default_charset,
						 GCallback callback,
						 gpointer user_data);

G_END_DECLS

#endif /* E_CHARSET_H */
