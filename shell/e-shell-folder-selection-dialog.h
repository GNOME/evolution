/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-folder-selection-dialog.h
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifndef E_SHELL_FOLDER_SELECTION_DIALOG_H
#define E_SHELL_FOLDER_SELECTION_DIALOG_H

#include <libgnomeui/gnome-dialog.h>

#include "e-shell.h"

#ifdef cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define E_TYPE_SHELL_FOLDER_SELECTION_DIALOG			(e_shell_folder_selection_dialog_get_type ())
#define E_SHELL_FOLDER_SELECTION_DIALOG(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL_FOLDER_SELECTION_DIALOG, EShellFolderSelectionDialog))
#define E_SHELL_FOLDER_SELECTION_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_FOLDER_SELECTION_DIALOG, EShellFolderSelectionDialogClass))
#define E_IS_SHELL_FOLDER_SELECTION_DIALOG(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL_FOLDER_SELECTION_DIALOG))
#define E_IS_SHELL_FOLDER_SELECTION_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_FOLDER_SELECTION_DIALOG))


typedef struct _EShellFolderSelectionDialog        EShellFolderSelectionDialog;
typedef struct _EShellFolderSelectionDialogPrivate EShellFolderSelectionDialogPrivate;
typedef struct _EShellFolderSelectionDialogClass   EShellFolderSelectionDialogClass;

struct _EShellFolderSelectionDialog {
	GnomeDialog parent;

	EShellFolderSelectionDialogPrivate *priv;
};

struct _EShellFolderSelectionDialogClass {
	GnomeDialogClass parent_class;
};


GtkType     e_shell_folder_selection_dialog_get_type           (void);
void        e_shell_folder_selection_dialog_construct          (EShellFolderSelectionDialog *folder_selection_dialog,
								EShell                      *shell,
								const char                  *title,
								const char                  *default_path);
GtkWidget  *e_shell_folder_selection_dialog_new                (EShell                      *shell,
								const char                  *title,
								const char                  *default_path);

const char *e_shell_folder_selection_dialog_get_selected_path  (EShellFolderSelectionDialog *folder_selection_dialog);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* E_SHELL_FOLDER_SELECTION_DIALOG_H */
