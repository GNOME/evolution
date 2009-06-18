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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_FOLDER_SELECTION_H
#define EM_FOLDER_SELECTION_H

#include <gtk/gtk.h>
#include "em-folder-tree.h"

G_BEGIN_DECLS

void em_select_folder (GtkWindow *parent_window, const gchar *title, const gchar *oklabel, const gchar *default_uri,
		       EMFTExcludeFunc exclude,
		       void (*done)(const gchar *uri, gpointer data),
		       gpointer data);

G_END_DECLS

#endif /* EM_FOLDER_SELECTION_H */
