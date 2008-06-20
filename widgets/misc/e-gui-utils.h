/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-gui-utils.h
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
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
