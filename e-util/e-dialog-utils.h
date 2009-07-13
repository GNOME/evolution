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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_DIALOG_UTILS_H
#define E_DIALOG_UTILS_H

#include <gtk/gtk.h>

void  e_notice                       (gpointer         parent,
				      GtkMessageType   type,
				      const gchar      *format,
				      ...);
void  e_notice_with_xid              (GdkNativeWindow  parent,
				      GtkMessageType   type,
				      const gchar      *format,
				      ...);

void  e_dialog_set_transient_for     (GtkWindow       *dialog,
				      GtkWidget       *parent_widget);
void  e_dialog_set_transient_for_xid (GtkWindow       *dialog,
				      GdkNativeWindow  xid);

gchar *e_file_dialog_save             (const gchar      *title, const gchar *fname);

gchar *e_file_dialog_save_folder	     (const gchar      *title);

GtkWidget * e_file_get_save_filesel (GtkWidget *parent, const gchar *title, const gchar *name, GtkFileChooserAction action);

gboolean e_file_can_save(GtkWindow *parent, const gchar *uri);
gboolean e_file_check_local(const gchar *name);

#endif
