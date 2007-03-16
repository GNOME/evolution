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

#ifndef EM_FOLDER_SELECTOR_H
#define EM_FOLDER_SELECTOR_H

#include <gtk/gtkdialog.h>

#ifdef cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define EM_TYPE_FOLDER_SELECTOR			(em_folder_selector_get_type ())
#define EM_FOLDER_SELECTOR(obj)			(GTK_CHECK_CAST ((obj), E_TYPEM_FOLDER_SELECTOR, EMFolderSelector))
#define EM_FOLDER_SELECTOR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPEM_FOLDER_SELECTOR, EMFolderSelectorClass))
#define EM_IS_FOLDER_SELECTOR(obj)		(GTK_CHECK_TYPE ((obj), E_TYPEM_FOLDER_SELECTOR))
#define EM_IS_FOLDER_SELECTOR_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPEM_FOLDER_SELECTOR))

typedef struct _EMFolderSelector        EMFolderSelector;
typedef struct _EMFolderSelectorPrivate EMFolderSelectorPrivate;
typedef struct _EMFolderSelectorClass   EMFolderSelectorClass;

struct _EMFolderSelector {
	GtkDialog parent;
	
	guint32 flags;
	struct _EMFolderTree *emft;
	
	struct _GtkEntry *name_entry;
	char *selected_path;
	char *selected_uri;
	
	char *created_uri;
	guint created_id;
};

struct _EMFolderSelectorClass {
	GtkDialogClass parent_class;
	
};

enum {
	EM_FOLDER_SELECTOR_CAN_CREATE = 1,
};

enum {
	EM_FOLDER_SELECTOR_RESPONSE_NEW = 1,
};

GType em_folder_selector_get_type (void);

void em_folder_selector_construct (EMFolderSelector *emfs, struct _EMFolderTree *emft, guint32 flags, const char *title, const char *text, const char *oklabel);

/* for selecting folders */
GtkWidget *em_folder_selector_new (struct _EMFolderTree *emft, guint32 flags, const char *title, const char *text, const char *oklabel);

/* for creating folders */
GtkWidget *em_folder_selector_create_new (struct _EMFolderTree *emft, guint32 flags, const char *title, const char *text);

void em_folder_selector_set_selected (EMFolderSelector *emfs, const char *uri);
void em_folder_selector_set_selected_list (EMFolderSelector *emfs, GList *list);

const char *em_folder_selector_get_selected_uri (EMFolderSelector *emfs);
const char *em_folder_selector_get_selected_path (EMFolderSelector *emfs);

GList *em_folder_selector_get_selected_uris (EMFolderSelector *emfs);
GList *em_folder_selector_get_selected_paths (EMFolderSelector *emfs);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* EM_FOLDER_SELECTOR_H */
