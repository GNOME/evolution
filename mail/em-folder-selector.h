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

#ifndef EM_FOLDER_SELECTOR_H
#define EM_FOLDER_SELECTOR_H

#include <gtk/gtk.h>
#include <mail/em-folder-tree.h>

/* Standard GObject macros */
#define EM_TYPE_FOLDER_SELECTOR \
	(em_folder_selector_get_type ())
#define EM_FOLDER_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FOLDER_SELECTOR, EMFolderSelector))
#define EM_FOLDER_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FOLDER_SELECTOR, EMFolderSelectorClass))
#define EM_IS_FOLDER_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FOLDER_SELECTOR))
#define EM_IS_FOLDER_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FOLDER_SELECTOR))
#define EM_FOLDER_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FOLDER_SELECTOR, EMFolderSelectorClass))

G_BEGIN_DECLS

typedef struct _EMFolderSelector EMFolderSelector;
typedef struct _EMFolderSelectorClass EMFolderSelectorClass;
typedef struct _EMFolderSelectorPrivate EMFolderSelectorPrivate;

struct _EMFolderSelector {
	GtkDialog parent;

	guint32 flags;
	EMFolderTree *emft;

	GtkEntry *name_entry;
	gchar *selected_path;
	gchar *selected_uri;

	gchar *created_uri;
	guint created_id;
};

struct _EMFolderSelectorClass {
	GtkDialogClass parent_class;

};

enum {
	EM_FOLDER_SELECTOR_CAN_CREATE = 1
};

enum {
	EM_FOLDER_SELECTOR_RESPONSE_NEW = 1
};

GType		em_folder_selector_get_type	(void);
void		em_folder_selector_construct	(EMFolderSelector *emfs,
						 EMFolderTree *emft,
						 guint32 flags,
						 const gchar *title,
						 const gchar *text,
						 const gchar *oklabel);
GtkWidget *	em_folder_selector_new		(GtkWindow *parent,
						 EMFolderTree *emft,
						 guint32 flags,
						 const gchar *title,
						 const gchar *text,
						 const gchar *oklabel);
GtkWidget *	em_folder_selector_create_new	(GtkWindow *parent,
						 EMFolderTree *emft,
						 guint32 flags,
						 const gchar *title,
						 const gchar *text);
void		em_folder_selector_set_selected	(EMFolderSelector *emfs,
						 const gchar *uri);
void		em_folder_selector_set_selected_list
						(EMFolderSelector *emfs,
						 GList *list);
const gchar *	em_folder_selector_get_selected_uri
						(EMFolderSelector *emfs);
const gchar *	em_folder_selector_get_selected_path
						(EMFolderSelector *emfs);
GList *		em_folder_selector_get_selected_uris
						(EMFolderSelector *emfs);
GList *		em_folder_selector_get_selected_paths
						(EMFolderSelector *emfs);

G_END_DECLS

#endif /* EM_FOLDER_SELECTOR_H */
