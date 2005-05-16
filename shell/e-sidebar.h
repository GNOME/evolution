/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-sidebar.h
 *
 * Copyright (C) 2003  Ettore Perazzoli
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

#ifndef _E_SIDEBAR_H_
#define _E_SIDEBAR_H_

#include <gtk/gtkcontainer.h>

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

	void (* button_selected) (ESidebar *sidebar, int id);
};


GType      e_sidebar_get_type  (void);
GtkWidget *e_sidebar_new       (void);

void  e_sidebar_set_selection_widget  (ESidebar   *sidebar,
				       GtkWidget  *widget);

void  e_sidebar_add_button  (ESidebar   *sidebar,
			     const char *label,
			     const char *tooltips,
			     GdkPixbuf  *icon,
			     int         id);

void  e_sidebar_select_button  (ESidebar *sidebar,
				int       id);

ESidebarMode e_sidebar_get_mode (ESidebar *sidebar);
void e_sidebar_set_mode (ESidebar *sidebar, ESidebarMode mode);

void  e_sidebar_set_show_buttons  (ESidebar *sidebar, gboolean show);
gboolean  e_sidebar_get_show_buttons  (ESidebar *sidebar);


#endif /* _E_SIDEBAR_H_ */
