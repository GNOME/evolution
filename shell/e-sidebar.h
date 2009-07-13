/*
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
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_SIDEBAR_H_
#define _E_SIDEBAR_H_

#include <gtk/gtk.h>

#define E_TYPE_SIDEBAR			(e_sidebar_get_type ())
#define E_SIDEBAR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SIDEBAR, ESidebar))
#define E_SIDEBAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SIDEBAR, ESidebarClass))
#define E_IS_SIDEBAR(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SIDEBAR))
#define E_IS_SIDEBAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SIDEBAR))

typedef struct _ESidebar        ESidebar;
typedef struct _ESidebarPrivate ESidebarPrivate;
typedef struct _ESidebarClass   ESidebarClass;

typedef enum {
	E_SIDEBAR_MODE_TEXT,
	E_SIDEBAR_MODE_ICON,
	E_SIDEBAR_MODE_BOTH,
	E_SIDEBAR_MODE_TOOLBAR
} ESidebarMode;

struct _ESidebar {
	GtkContainer parent;

	ESidebarPrivate *priv;
};

struct _ESidebarClass {
	GtkContainerClass parent_class;

	/* signals */
	void (* button_selected) (ESidebar *sidebar, gint id);
	void (* button_pressed)  (ESidebar *sidebar, GdkEventButton *event, gint id);
};

GType      e_sidebar_get_type  (void);
GtkWidget *e_sidebar_new       (void);

void  e_sidebar_set_selection_widget  (ESidebar   *sidebar,
				       GtkWidget  *widget);

void  e_sidebar_add_button  (ESidebar   *sidebar,
			     const gchar *label,
			     const gchar *tooltips,
			     const gchar *icon_name,
			     gint         id);

void  e_sidebar_select_button  (ESidebar *sidebar,
				gint       id);

void e_sidebar_change_button_icon (ESidebar *sidebar,
				   const gchar *icon_name,
				   gint button_id);

ESidebarMode e_sidebar_get_mode (ESidebar *sidebar);
void e_sidebar_set_mode (ESidebar *sidebar, ESidebarMode mode);

void  e_sidebar_set_show_buttons  (ESidebar *sidebar, gboolean show);
gboolean  e_sidebar_get_show_buttons  (ESidebar *sidebar);

#endif /* _E_SIDEBAR_H_ */
