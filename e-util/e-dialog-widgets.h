/*
 * Evolution internal utilities - Glade dialog widget utilities
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_DIALOG_WIDGETS_H
#define E_DIALOG_WIDGETS_H

void e_dialog_editable_set (GtkWidget *widget, const gchar *value);
gchar *e_dialog_editable_get (GtkWidget *widget);

void e_dialog_combo_box_set (GtkWidget *widget, gint value, const gint *value_map);
gint e_dialog_combo_box_get (GtkWidget *widget, const gint *value_map);

#endif
