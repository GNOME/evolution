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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *      Rodney Dawes <dobey@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_FOLDER_UTILS_H
#define EM_FOLDER_UTILS_H

#include <gtk/gtk.h>
#include <camel/camel.h>
#include <mail/em-folder-tree.h>

G_BEGIN_DECLS

gint		em_folder_utils_copy_folders	(CamelStore *fromstore,
						 const gchar *frombase,
						 CamelStore *tostore,
						 const gchar *tobase,
						 gint delete);

/* FIXME These API's are really busted.  There is no consistency and
 *       most rely on the wrong data. */

void		em_folder_utils_copy_folder	(GtkWindow *parent,
						 CamelFolderInfo *folderinfo,
						 gboolean delete);
void		em_folder_utils_delete_folder	(CamelFolder *folder);
void		em_folder_utils_create_folder	(CamelFolderInfo *folderinfo,
						 EMFolderTree *emft,
						 GtkWindow *parent);
void		em_folder_utils_unsubscribe_folder
						(const gchar *folder_uri);

const gchar *	em_folder_utils_get_icon_name	(guint32 flags);

G_END_DECLS

#endif /* EM_FOLDER_UTILS_H */
