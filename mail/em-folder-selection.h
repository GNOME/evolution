/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef EM_FOLDER_SELECTION_H
#define EM_FOLDER_SELECTION_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gtk/gtkwindow.h>
#include <camel/camel-folder.h>

CamelFolder *em_folder_selection_run_dialog (GtkWindow *parent_window,
					     const char *title,
					     const char *caption,
					     CamelFolder *default_folder);

char *em_folder_selection_run_dialog_uri (GtkWindow *parent_window,
					  const char *title,
					  const char *caption,
					  const char *default_folder_uri);

void em_select_folder (GtkWindow *parent_window, const char *title, const char *text,
		       const char *default_folder_uri,
		       void (*done)(const char *uri, void *data),
		       void *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EM_FOLDER_SELECTION_H */
