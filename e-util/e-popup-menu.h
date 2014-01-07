/*
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
 * Authors:
 *		Miguel de Icaza <miguel@ximian.com>
 *		Jody Goldberg (jgoldberg@home.com)
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_POPUP_MENU_H
#define E_POPUP_MENU_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_POPUP_SEPARATOR  { (gchar *) "", NULL, (NULL), 0 }
#define E_POPUP_TERMINATOR { NULL, NULL, (NULL), 0 }

#define E_POPUP_ITEM(name,fn,disable_mask) \
	{ (gchar *) (name), NULL, (fn), (disable_mask) }

typedef struct _EPopupMenu EPopupMenu;

struct _EPopupMenu {
	gchar *name;
	gchar *pixname;
	GCallback fn;
	guint32 disable_mask;
};

GtkMenu *	e_popup_menu_create_with_domain	(EPopupMenu *menu_list,
						 guint32 disable_mask,
						 guint32 hide_mask,
						 gpointer default_closure,
						 const gchar *domain);

G_END_DECLS

#endif /* E_POPUP_MENU_H */
