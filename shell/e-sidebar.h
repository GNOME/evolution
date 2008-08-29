/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-sidebar.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_SIDEBAR_H
#define E_SIDEBAR_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_SIDEBAR \
	(e_sidebar_get_type ())
#define E_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SIDEBAR, ESidebar))
#define E_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SIDEBAR, ESidebarClass))
#define E_IS_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SIDEBAR))
#define E_IS_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_SIDEBAR))
#define E_SIDEBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SIDEBAR, ESidebarClass))

#define E_SIDEBAR_DEFAULT_TOOLBAR_STYLE		GTK_TOOLBAR_BOTH_HORIZ

G_BEGIN_DECLS

typedef struct _ESidebar ESidebar;
typedef struct _ESidebarClass ESidebarClass;
typedef struct _ESidebarPrivate ESidebarPrivate;

struct _ESidebar {
	GtkBin parent;
	ESidebarPrivate *priv;
};

struct _ESidebarClass {
	GtkBinClass parent_class;

	void		(*style_changed)	(ESidebar *sidebar,
						 GtkToolbarStyle style);
};

GType		e_sidebar_get_type		(void);
GtkWidget *	e_sidebar_new			(void);
void		e_sidebar_add_action		(ESidebar *sidebar,
						 GtkAction *action);
gboolean	e_sidebar_get_actions_visible	(ESidebar *sidebar);
void		e_sidebar_set_actions_visible	(ESidebar *sidebar,
						 gboolean visible);
const gchar *	e_sidebar_get_icon_name		(ESidebar *sidebar);
void		e_sidebar_set_icon_name		(ESidebar *sidebar,
						 const gchar *icon_name);
const gchar *	e_sidebar_get_primary_text	(ESidebar *sidebar);
void		e_sidebar_set_primary_text	(ESidebar *sidebar,
						 const gchar *primary_text);
const gchar *	e_sidebar_get_secondary_text	(ESidebar *sidebar);
void		e_sidebar_set_secondary_text	(ESidebar *sidebar,
						 const gchar *secondary_text);
GtkToolbarStyle	e_sidebar_get_style		(ESidebar *sidebar);
void		e_sidebar_set_style		(ESidebar *sidebar,
						 GtkToolbarStyle style);
void		e_sidebar_unset_style		(ESidebar *sidebar);

G_END_DECLS

#endif /* E_SIDEBAR_H */
