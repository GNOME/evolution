/*
 * Evolution internal utilities - Glade dialog widget utilities
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_DIALOG_WIDGETS_H
#define E_DIALOG_WIDGETS_H

#include <gtk/gtk.h>
#include <camel/camel.h>

void e_dialog_combo_box_set (GtkWidget *widget, gint value, const gint *value_map);
gint e_dialog_combo_box_get (GtkWidget *widget, const gint *value_map);

GtkWidget *	e_dialog_button_new_with_icon	(const gchar *icon_name,
						 const gchar *label);

GtkWidget *	e_dialog_offline_settings_new_limit_box
						(CamelOfflineSettings *offline_settings);

GtkWidget *	e_dialog_new_mark_seen_box	(gpointer object);

#endif
