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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef GAL_GUI_UTILS_H
#define GAL_GUI_UTILS_H

#include <gtk/gtk.h>
#include <glade/glade-xml.h>

G_BEGIN_DECLS

void  e_popup_menu                              (GtkMenu  *menu,
						 GdkEvent *event);
void  e_auto_kill_popup_menu_on_selection_done  (GtkMenu  *menu);

void  e_container_foreach_leaf      (GtkContainer *container,
				     GtkCallback   callback,
				     gpointer      closure);
void  e_container_focus_nth_entry   (GtkContainer *container,
				     int           n);
gint  e_container_change_tab_order  (GtkContainer *container,
				     GList        *widgets);

/* Returns TRUE on success. */
gboolean  e_glade_xml_connect_widget  (GladeXML      *gui,
				       char          *name,
				       char          *signal,
				       GCallback      cb,
				       gpointer       closure);
gboolean  e_glade_xml_set_sensitive   (GladeXML      *gui,
				       char          *name,
				       gboolean       sensitive);

G_END_DECLS

#endif /* GAL_GUI_UTILS_H */
