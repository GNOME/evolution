/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder-selection-dialog.h
 *
 * Copyright (C) 2000, 2001, 2002, 2003 Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifndef E_FOLDER_SELECTION_DIALOG_H
#define E_FOLDER_SELECTION_DIALOG_H

#include "e-storage-set.h"

#include <gtk/gtkdialog.h>

#ifdef cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define E_TYPE_FOLDER_SELECTION_DIALOG			(e_folder_selection_dialog_get_type ())
#define E_FOLDER_SELECTION_DIALOG(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_FOLDER_SELECTION_DIALOG, EFolderSelectionDialog))
#define E_FOLDER_SELECTION_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_FOLDER_SELECTION_DIALOG, EFolderSelectionDialogClass))
#define E_IS_FOLDER_SELECTION_DIALOG(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_FOLDER_SELECTION_DIALOG))
#define E_IS_FOLDER_SELECTION_DIALOG_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_FOLDER_SELECTION_DIALOG))


typedef struct _EFolderSelectionDialog        EFolderSelectionDialog;
typedef struct _EFolderSelectionDialogPrivate EFolderSelectionDialogPrivate;
typedef struct _EFolderSelectionDialogClass   EFolderSelectionDialogClass;

struct _EFolderSelectionDialog {
	GtkDialog parent;

	EFolderSelectionDialogPrivate *priv;
};

struct _EFolderSelectionDialogClass {
	GtkDialogClass parent_class;

	void (* folder_selected) (EFolderSelectionDialog *folder_selection_dialog,
				  const char *path);
	void (* cancelled)       (EFolderSelectionDialog *folder_selection_dialog);
};


GtkType    e_folder_selection_dialog_get_type   (void);
void       e_folder_selection_dialog_construct  (EFolderSelectionDialog *folder_selection_dialog,
						 EStorageSet            *storage_set,
						 const char             *title,
						 const char             *caption,
						 const char             *default_path,
						 const char             *allowed_types[],
						 gboolean                allow_creation);
GtkWidget *e_folder_selection_dialog_new        (EStorageSet            *storage_set,
						 const char             *title,
						 const char             *caption,
						 const char             *default_path,
						 const char             *allowed_types[],
						 gboolean                allow_creation);

const char *e_folder_selection_dialog_get_selected_path  (EFolderSelectionDialog *folder_selection_dialog);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* E_FOLDER_SELECTION_DIALOG_H */
