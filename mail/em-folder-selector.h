/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder-selection-dialog.h
 *
 * Copyright (C) 2000, 2001, 2002, 2003 Ximian, Inc.
 *
 * Authors: Ettore Perazzoli
 *	    Michael Zucchi
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

struct _EStorageSet;
struct _EStorageSetView;

struct _EMFolderSelector {
	GtkDialog parent;

	guint32 flags;
	struct _EStorageSet *ess;
	struct _EStorageSetView *essv;

	struct _GtkEntry *name_entry;
	char *selected;
	char *selected_uri;
};

struct _EMFolderSelectorClass {
	GtkDialogClass parent_class;

#if 0
	void (* folder_selected) (EMFolderSelector *folder_selection_dialog,
				  const char *path);
	void (* cancelled)       (EMFolderSelector *folder_selection_dialog);
#endif
};

enum {
	EM_FOLDER_SELECTOR_CAN_CREATE = 1,
};

enum {
	EM_FOLDER_SELECTOR_RESPONSE_NEW = 1,
};

GtkType    em_folder_selector_get_type (void);
void       em_folder_selector_construct(EMFolderSelector *, struct _EStorageSet *, guint32, const char *, const char *);
/* for selecting folders */
GtkWidget *em_folder_selector_new      (struct _EStorageSet *, guint32, const char *, const char *);

/* for creating folders */
GtkWidget *em_folder_selector_create_new(struct _EStorageSet *ess, guint32 flags, const char *title, const char *text);

void em_folder_selector_set_selected    (EMFolderSelector *emfs, const char *path);
void em_folder_selector_set_selected_uri(EMFolderSelector *emfs, const char *uri);

const char *em_folder_selector_get_selected    (EMFolderSelector *emfs);
const char *em_folder_selector_get_selected_uri(EMFolderSelector *emfs);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* EM_FOLDER_SELECTOR_H */
