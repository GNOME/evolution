/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* em-folder-selection.h - UI for selecting folders.
 *
 * Copyright (C) 2002 Ximian, Inc.
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

#ifndef EM_FOLDER_SELECTION_H
#define EM_FOLDER_SELECTION_H

#include <camel/camel-folder.h>

#include <gtk/gtkwindow.h>

CamelFolder *em_folder_selection_run_dialog (GtkWindow *parent_window,
					     const char *title,
					     const char *caption,
					     CamelFolder *default_folder);
char *em_folder_selection_run_dialog_uri(GtkWindow *parent_window,
					 const char *title,
					 const char *caption,
					 const char *default_folder_uri);

void em_select_folder(GtkWindow *parent_window, const char *title, const char *text, const char *default_folder_uri, void (*done)(const char *uri, void *data), void *data);

#endif /* EM_FOLDER_SELECTION_H */
