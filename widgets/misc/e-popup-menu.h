/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-popup-menu.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
 *   Jody Goldberg (jgoldberg@home.com)
 *   Jeffrey Stedfast <fejj@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef E_POPUP_MENU_H
#define E_POPUP_MENU_H

#include <gtk/gtkmenu.h>
#include <gtk/gtkwidget.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

#define E_POPUP_SEPARATOR  { "", NULL, (NULL), NULL, NULL, 0 }
#define E_POPUP_TERMINATOR { NULL, NULL, (NULL), NULL, NULL, 0 }

typedef struct _EPopupMenu EPopupMenu;

struct _EPopupMenu {
	char *name;
	GtkWidget *pixmap;
	void (*fn) (GtkWidget *widget, void *closure);
	void *closure;
	EPopupMenu *submenu;
	guint32 disable_mask;
};

GtkMenu *e_popup_menu_create  (EPopupMenu     *menu_list,
			       guint32         disable_mask,
			       guint32         hide_mask,
			       void           *default_closure);

void     e_popup_menu_run     (EPopupMenu     *menu_list,
			       GdkEvent       *event,
			       guint32         disable_mask,
			       guint32         hide_mask,
			       void           *default_closure);

END_GNOME_DECLS

#endif /* E_POPUP_MENU_H */
