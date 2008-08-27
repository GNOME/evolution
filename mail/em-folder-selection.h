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

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include "em-folder-tree.h"

struct _GtkWindow;

void em_select_folder (struct _GtkWindow *parent_window, const char *title, const char *oklabel, const char *default_uri,
		       EMFTExcludeFunc exclude,
		       void (*done)(const char *uri, void *data),
		       void *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EM_FOLDER_SELECTION_H */
