/*
 * e-menu-tool-action.h
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

/* This is a trivial GtkAction subclass that sets the toolbar
 * item type to GtkMenuToolButton instead of GtkToolButton. */

#ifndef E_MENU_TOOL_ACTION_H
#define E_MENU_TOOL_ACTION_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MENU_TOOL_ACTION \
	(e_menu_tool_action_get_type ())
#define E_MENU_TOOL_ACTION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MENU_TOOL_ACTION, EMenuToolAction))
#define E_MENU_TOOL_ACTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MENU_TOOL_ACTION, EMenuToolActionClass))
#define E_IS_MENU_TOOL_ACTION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MENU_TOOL_ACTION))
#define E_IS_MENU_TOOL_ACTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MENU_TOOL_ACTION))
#define E_MENU_TOOL_ACTION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MENU_TOOL_ACTION, EMenuToolActionClass))

G_BEGIN_DECLS

typedef struct _EMenuToolAction EMenuToolAction;
typedef struct _EMenuToolActionClass EMenuToolActionClass;

struct _EMenuToolAction {
	GtkAction parent;
};

struct _EMenuToolActionClass {
	GtkActionClass parent_class;
};

GType		e_menu_tool_action_get_type	(void);
EMenuToolAction *
		e_menu_tool_action_new		(const gchar *name,
						 const gchar *label,
						 const gchar *tooltip,
						 const gchar *stock_id);

G_END_DECLS

#endif /* E_MENU_TOOL_ACTION_H */
