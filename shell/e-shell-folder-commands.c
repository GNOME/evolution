/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-folder-commands.h
 *
 * Copyright (C) 2001  Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-folder-commands.h"

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>

#include <gtk/gtksignal.h>

#include "e-shell-constants.h"
#include "e-shell-folder-creation-dialog.h"
#include "e-shell-folder-selection-dialog.h"


/* Utility functions.  */

static const char *
get_folder_name (EShell *shell,
		 const char *path)
{
	EStorageSet *storage_set;
	EFolder *folder;

	storage_set = e_shell_get_storage_set (shell);
	folder = e_storage_set_get_folder (storage_set, path);

	return e_folder_get_name (folder);
}


/* The data passed to the signals handled during the execution of the folder
   commands.  */

enum _FolderCommand {
	FOLDER_COMMAND_COPY,
	FOLDER_COMMAND_MOVE
};
typedef enum _FolderCommand FolderCommand;

struct _FolderCommandData {
	EShell *shell;
	EShellView *shell_view;
	FolderCommand command;
	char *source_path;
	char *destination_path;
};
typedef struct _FolderCommandData FolderCommandData;

static FolderCommandData *
folder_command_data_new (EShell *shell,
			 EShellView *shell_view,
			 FolderCommand command,
			 const char *source_path,
			 const char *destination_path)
{
	FolderCommandData *new;

	new = g_new (FolderCommandData, 1);
	new->shell            = shell;
	new->shell_view       = shell_view;
	new->command     = command;
	new->source_path      = g_strdup (source_path);
	new->destination_path = g_strdup (destination_path);

	return new;
}

static void
folder_command_data_free (FolderCommandData *folder_command_data)
{
	g_free (folder_command_data->source_path);
	g_free (folder_command_data->destination_path);

	g_free (folder_command_data);
}


/* Callback for the storage result.  */

static void
xfer_result_callback (EStorageSet *storage_set,
		      EStorageResult result,
		      void *data)
{
	FolderCommandData *folder_command_data;

	folder_command_data = (FolderCommandData *) data;

	/* FIXME do something.  */

	folder_command_data_free (folder_command_data);
}


/* The signals for the folder selection dialog.  This used for the copy and
   move commands.  */

static void
folder_selection_dialog_folder_selected_callback (EShellFolderSelectionDialog *folder_selection_dialog,
						  const char *path,
						  void *data)
{
	FolderCommandData *folder_command_data;
	EStorageSet *storage_set;
	gboolean remove_source;

	folder_command_data = (FolderCommandData *) data;

	folder_command_data->destination_path = g_concat_dir_and_file (path,
								       g_basename (folder_command_data->source_path));

	switch (folder_command_data->command) {
	case FOLDER_COMMAND_COPY:
		remove_source = FALSE;
		break;
	case FOLDER_COMMAND_MOVE:
		remove_source = TRUE;
		break;
	default:
		g_assert_not_reached ();
		return;
	}

	storage_set = e_shell_get_storage_set (folder_command_data->shell);

	e_storage_set_async_xfer_folder (storage_set,
					 folder_command_data->source_path,
					 folder_command_data->destination_path,
					 remove_source,
					 xfer_result_callback,
					 folder_command_data);

	gtk_widget_destroy (GTK_WIDGET (folder_selection_dialog));
}

static void
folder_selection_dialog_cancelled_callback (EShellFolderSelectionDialog *folder_selection_dialog,
					    void *data)
{
	folder_command_data_free ((FolderCommandData *) data);
}

static void
connect_folder_selection_dialog_signals (EShellFolderSelectionDialog *folder_selection_dialog,
					 FolderCommandData *folder_command_data)
{
	g_assert (folder_command_data != NULL);

	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "folder_selected",
			    GTK_SIGNAL_FUNC (folder_selection_dialog_folder_selected_callback),
			    folder_command_data);

	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "cancelled",
			    GTK_SIGNAL_FUNC (folder_selection_dialog_cancelled_callback),
			    folder_command_data);
}


/* Create new folder.  */

void
e_shell_command_create_new_folder (EShell *shell,
				   EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	/* FIXME: Should handle the result stuff.  */
	e_shell_show_folder_creation_dialog (shell, GTK_WINDOW (shell_view),
					     e_shell_view_get_current_path (shell_view),
					     NULL /* result_callback */,
					     NULL /* result_callback_data */);
}


/* Open folder in other window.   */

void
e_shell_command_open_folder_in_other_window (EShell *shell,
					     EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	e_shell_new_view (shell, e_shell_view_get_current_uri (shell_view));
}


/* Copy folder.  */

void
e_shell_command_copy_folder (EShell *shell,
			     EShellView *shell_view)
{
	GtkWidget *folder_selection_dialog;
	FolderCommandData *data;
	const char *current_path;
	const char *current_uri;
	char *caption;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	current_path = e_shell_view_get_current_path (shell_view);

	if (current_path == NULL) {
		g_warning ("Called `e_shell_command_copy_folder()' without a valid displayed folder");
		return;
	}

	caption = g_strdup_printf (_("Specify a folder to copy folder \"%s\" into:"),
				   get_folder_name (shell, current_path));

	current_uri = e_shell_view_get_current_uri (shell_view);
	folder_selection_dialog = e_shell_folder_selection_dialog_new (shell,
								       _("Copy folder"),
								       caption,
								       current_uri,
								       NULL);

	g_free (caption);

	data = folder_command_data_new (shell, shell_view, FOLDER_COMMAND_COPY, current_path, NULL);
	connect_folder_selection_dialog_signals (E_SHELL_FOLDER_SELECTION_DIALOG (folder_selection_dialog),
						 data);

	gtk_widget_show (folder_selection_dialog);
}


/* Move folder.  */

void
e_shell_command_move_folder (EShell *shell,
			     EShellView *shell_view)
{
	GtkWidget *folder_selection_dialog;
	FolderCommandData *data;
	const char *current_path;
	const char *current_uri;
	char *caption;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	current_path = e_shell_view_get_current_path (shell_view);
	if (current_path == NULL) {
		g_warning ("Called `e_shell_command_move_folder()' without a valid displayed folder");
		return;
	}

	caption = g_strdup_printf (_("Specify a folder to move folder \"%s\" into:"),
				   get_folder_name (shell, current_path));

	current_uri = e_shell_view_get_current_uri (shell_view);
	folder_selection_dialog = e_shell_folder_selection_dialog_new (shell,
								       _("Move folder"),
								       caption,
								       current_uri,
								       NULL);

	g_free (caption);

	data = folder_command_data_new (shell, shell_view, FOLDER_COMMAND_MOVE, current_path, NULL);
	connect_folder_selection_dialog_signals (E_SHELL_FOLDER_SELECTION_DIALOG (folder_selection_dialog),
						 data);

	gtk_widget_show (folder_selection_dialog);
}


void
e_shell_command_rename_folder (EShell *shell,
			       EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	g_warning ("To be implemented");
}


void
e_shell_command_add_to_shortcut_bar (EShell *shell,
				     EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	g_warning ("To be implemented");
}


void
e_shell_command_folder_properties (EShell *shell,
				   EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	g_warning ("To be implemented");
}
