/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-dialog-utils.h
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
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef E_DIALOG_UTILS_H
#define E_DIALOG_UTILS_H

#include <gtk/gtk.h>

void  e_notice                       (gpointer         parent,
				      GtkMessageType   type,
				      const char      *format,
				      ...);
void  e_notice_with_xid              (GdkNativeWindow  parent,
				      GtkMessageType   type,
				      const char      *format,
				      ...);

void  e_dialog_set_transient_for     (GtkWindow       *dialog,
				      GtkWidget       *parent_widget);
void  e_dialog_set_transient_for_xid (GtkWindow       *dialog,
				      GdkNativeWindow  xid);

char *e_file_dialog_save             (const char      *title, const char *fname);

char *e_file_dialog_save_folder	     (const char      *title);

GtkWidget * e_file_get_save_filesel (GtkWidget *parent, const char *title, const char *name, GtkFileChooserAction action);

gboolean e_file_can_save(GtkWindow *parent, const char *uri);
gboolean e_file_check_local(const char *name);


#endif
