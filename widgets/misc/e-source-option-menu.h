/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-source-option-menu.h
 *
 * Copyright (C) 2003  Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_SOURCE_OPTION_MENU_H_
#define _E_SOURCE_OPTION_MENU_H_

#include <libedataserver/e-source-list.h>

#include <gtk/gtkoptionmenu.h>

#define E_TYPE_SOURCE_OPTION_MENU			(e_source_option_menu_get_type ())
#define E_SOURCE_OPTION_MENU(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SOURCE_OPTION_MENU, ESourceOptionMenu))
#define E_SOURCE_OPTION_MENU_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SOURCE_OPTION_MENU, ESourceOptionMenuClass))
#define E_IS_SOURCE_OPTION_MENU(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SOURCE_OPTION_MENU))
#define E_IS_SOURCE_OPTION_MENU_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SOURCE_OPTION_MENU))


typedef struct _ESourceOptionMenu        ESourceOptionMenu;
typedef struct _ESourceOptionMenuPrivate ESourceOptionMenuPrivate;
typedef struct _ESourceOptionMenuClass   ESourceOptionMenuClass;

struct _ESourceOptionMenu {
	GtkOptionMenu parent;

	ESourceOptionMenuPrivate *priv;
};

struct _ESourceOptionMenuClass {
	GtkOptionMenuClass parent_class;

	void (* source_selected) (ESourceOptionMenu *menu,
				  ESource *selected_source);
};


GType  e_source_option_menu_get_type  (void);

GtkWidget *e_source_option_menu_new  (ESourceList *list);

ESource *e_source_option_menu_peek_selected  (ESourceOptionMenu *menu);
void     e_source_option_menu_select         (ESourceOptionMenu *menu,
					      ESource           *source);


#endif /* _E_SOURCE_OPTION_MENU_H_ */
