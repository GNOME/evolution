/*
 * e-menu-button.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* This is like a GtkMenuToolButton, expect not a GtkToolItem. */

#ifndef E_MENU_BUTTON_H
#define E_MENU_BUTTON_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MENU_BUTTON \
	(e_menu_button_get_type ())
#define E_MENU_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MENU_BUTTON, EMenuButton))
#define E_MENU_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MENU_BUTTON, EMenuButtonClass))
#define E_IS_MENU_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MENU_BUTTON))
#define E_IS_MENU_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MENU_BUTTON))
#define E_MENU_BUTTON_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MENU_BUTTON, EMenuButtonClass))

G_BEGIN_DECLS

typedef struct _EMenuButton EMenuButton;
typedef struct _EMenuButtonClass EMenuButtonClass;
typedef struct _EMenuButtonPrivate EMenuButtonPrivate;

struct _EMenuButton {
	GtkButton parent;
	EMenuButtonPrivate *priv;
};

struct _EMenuButtonClass {
	GtkButtonClass parent_class;

	/* Signals */
	void		(*show_menu)		(EMenuButton *menu_button);
};

GType		e_menu_button_get_type		(void);
GtkWidget *	e_menu_button_new		(void);
GtkWidget *	e_menu_button_get_menu		(EMenuButton *menu_button);
void		e_menu_button_set_menu		(EMenuButton *menu_button,
						 GtkWidget *menu);

G_END_DECLS

#endif /* E_MENU_BUTTON_H */
