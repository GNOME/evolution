/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
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

#ifndef E_POPUP_MENU_H
#define E_POPUP_MENU_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_POPUP_SEPARATOR  { (gchar *) "", NULL, (NULL), NULL, 0 }
#define E_POPUP_TERMINATOR { NULL, NULL, (NULL), NULL, 0 }

#define E_POPUP_ITEM(name,fn,disable_mask) { (gchar *) (name), NULL, (fn), NULL, (disable_mask), NULL, NULL, 0, 0, 0, 0 }

typedef struct _EPopupMenu EPopupMenu;

struct _EPopupMenu {
	gchar *name;
	gchar *pixname;
	GCallback fn;

	EPopupMenu *submenu;
	guint32 disable_mask;

	/* Added post 0.19 */
	GtkWidget *pixmap_widget;
	gpointer closure;

	guint is_toggle : 1;
	guint is_radio : 1;
	guint is_active : 1;

	guint use_custom_closure : 1;
};

GtkMenu    *e_popup_menu_create              (EPopupMenu       *menu_list,
					      guint32           disable_mask,
					      guint32           hide_mask,
					      void             *default_closure);
GtkMenu    *e_popup_menu_create_with_domain  (EPopupMenu       *menu_list,
					      guint32           disable_mask,
					      guint32           hide_mask,
					      void             *default_closure,
					      const gchar       *domain);

G_END_DECLS

#endif /* E_POPUP_MENU_H */
