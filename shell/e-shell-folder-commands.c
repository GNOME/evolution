/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-folder-commands.h
 *
 * Copyright (C) 2001  Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-folder-commands.h"

#include "e-util/e-dialog-utils.h"
#include "e-util/e-request.h"

#include "e-shell-constants.h"
#include "e-shell-folder-creation-dialog.h"
#include "e-shell-folder-selection-dialog.h"
#include "e-shell-utils.h"

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>

#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>

#include <string.h>


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

static int
get_folder_unread (EShell *shell,
		   const char *path)
{
	EStorageSet *storage_set;
	EFolder *folder;

	storage_set = e_shell_get_storage_set (shell);
	folder = e_storage_set_get_folder (storage_set, path);

	return e_folder_get_unread_count (folder);
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
	new->command          = command;
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

	if (result != E_STORAGE_OK) {
		const char *fmt;

		if (folder_command_data->command == FOLDER_COMMAND_COPY)
			fmt = _("Cannot copy folder: %s");
		else
			fmt = _("Cannot move folder: %s");

		e_notice (folder_command_data->shell_view, GTK_MESSAGE_ERROR,
			  fmt, e_storage_result_to_string (result));
	}

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
	char *base_name;
	gboolean remove_source;

	folder_command_data = (FolderCommandData *) data;

	base_name = g_path_get_basename (folder_command_data->source_path);
	folder_command_data->destination_path = g_build_filename (path, base_name, NULL);
	g_free (base_name);

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

	if (strcmp (folder_command_data->destination_path,
		    folder_command_data->source_path) == 0) {
		const char *msg;

		if (remove_source)
			msg = _("Cannot move a folder over itself.");
		else
			msg = _("Cannot copy a folder over itself.");

		e_notice (folder_selection_dialog, GTK_MESSAGE_ERROR, msg);
		return;
	}

	if (remove_source) {
		int source_len;

		source_len = strlen (folder_command_data->source_path);
		if (strncmp (folder_command_data->destination_path,
			     folder_command_data->source_path,
			     source_len) == 0) {
			e_notice (folder_selection_dialog, GTK_MESSAGE_ERROR,
				  _("Cannot move a folder into one of its descendants."));
			return;
		}
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

	g_signal_connect (folder_selection_dialog, "folder_selected",
			  G_CALLBACK (folder_selection_dialog_folder_selected_callback),
			  folder_command_data);

	g_signal_connect (folder_selection_dialog, "cancelled",
			  G_CALLBACK (folder_selection_dialog_cancelled_callback),
			  folder_command_data);
}


/* Create new folder.  */

void
e_shell_command_create_new_folder (EShell *shell,
				   EShellView *shell_view,
				   const char *parent_folder_path)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL || parent_folder_path != NULL);
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (parent_folder_path != NULL || g_path_is_absolute (parent_folder_path));

	if (parent_folder_path == NULL)
		parent_folder_path = e_shell_view_get_current_path (shell_view);

	/* FIXME: Should handle the result stuff.  */
	e_shell_show_folder_creation_dialog (shell, GTK_WINDOW (shell_view),
					     e_shell_view_get_current_path (shell_view),
					     NULL /* Default type. Take it from parent */,
					     NULL /* result_callback */,
					     NULL /* result_callback_data */);
}


/* Open folder in other window.   */

void
e_shell_command_open_folder_in_other_window (EShell *shell,
					     EShellView *shell_view,
					     const char *folder_path)
{
	EShellView *view;
	char *uri;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	if (folder_path == NULL)
		folder_path = e_shell_view_get_current_path (shell_view);

	uri = g_strconcat (E_SHELL_URI_PREFIX, folder_path, NULL);
	view = e_shell_create_view (shell, uri, shell_view);
	g_free (uri);

	gtk_widget_show (GTK_WIDGET (view));
}


/* Copy folder.  */

void
e_shell_command_copy_folder (EShell *shell,
			     EShellView *shell_view,
			     const char *folder_path)
{
	GtkWidget *folder_selection_dialog;
	FolderCommandData *data;
	char *uri;
	char *caption;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (folder_path == NULL || g_path_is_absolute (folder_path));

	if (folder_path == NULL)
		folder_path = e_shell_view_get_current_path (shell_view);

	if (folder_path == NULL) {
		g_warning ("Called `e_shell_command_copy_folder()' without a valid displayed folder");
		return;
	}

	caption = g_strdup_printf (_("Specify a folder to copy folder \"%s\" into:"),
				   get_folder_name (shell, folder_path));

	uri = g_strconcat (E_SHELL_URI_PREFIX, folder_path, NULL);
	folder_selection_dialog = e_shell_folder_selection_dialog_new (shell, _("Copy Folder"),
								       caption, uri, NULL, TRUE);

	g_free (caption);
	g_free (uri);

	data = folder_command_data_new (shell, shell_view, FOLDER_COMMAND_COPY, folder_path, NULL);
	connect_folder_selection_dialog_signals (E_SHELL_FOLDER_SELECTION_DIALOG (folder_selection_dialog),
						 data);

	gtk_widget_show (folder_selection_dialog);
}


/* Move folder.  */

void
e_shell_command_move_folder (EShell *shell,
			     EShellView *shell_view,
			     const char *folder_path)
{
	GtkWidget *folder_selection_dialog;
	FolderCommandData *data;
	char *uri;
	char *caption;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (folder_path == NULL || g_path_is_absolute (folder_path));

	if (folder_path == NULL)
		folder_path = e_shell_view_get_current_path (shell_view);

	if (folder_path == NULL) {
		g_warning ("Called `e_shell_command_move_folder()' without a valid displayed folder");
		return;
	}

	caption = g_strdup_printf (_("Specify a folder to move folder \"%s\" into:"),
				   get_folder_name (shell, folder_path));

	uri = g_strconcat (E_SHELL_URI_PREFIX, folder_path, NULL);
	folder_selection_dialog = e_shell_folder_selection_dialog_new (shell, _("Move Folder"),
								       caption, uri, NULL, TRUE);

	g_free (caption);
	g_free (uri);

	data = folder_command_data_new (shell, shell_view, FOLDER_COMMAND_MOVE, folder_path, NULL);
	connect_folder_selection_dialog_signals (E_SHELL_FOLDER_SELECTION_DIALOG (folder_selection_dialog),
						 data);

	gtk_widget_show (folder_selection_dialog);
}

static void
delete_cb (EStorageSet *storage_set,
	   EStorageResult result,
	   void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	if (result != E_STORAGE_OK)
		e_notice (shell_view, GTK_MESSAGE_ERROR,
			  _("Cannot delete folder:\n%s"), e_storage_result_to_string (result));
}

static GtkResponseType
delete_dialog (EShellView *shell_view, const char *folder_name)
{
	GtkWidget *dialog;
	GtkResponseType response;
	char *title;

	dialog = gtk_message_dialog_new (GTK_WINDOW (shell_view),
					 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 _("Really delete folder \"%s\"?"), folder_name);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_DELETE, GTK_RESPONSE_OK);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6); 
	
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 6);

	title = g_strdup_printf (_("Delete \"%s\""), folder_name);
	gtk_window_set_title (GTK_WINDOW (dialog), title);
	g_free (title);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return response;
}

void
e_shell_command_delete_folder (EShell *shell,
			       EShellView *shell_view,
			       const char *folder_path)
{
	EStorageSet *storage_set;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL || folder_path != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (folder_path != NULL || g_path_is_absolute (folder_path));

	storage_set = e_shell_get_storage_set (shell);

	if (folder_path == NULL)
		folder_path = e_shell_view_get_current_path (shell_view);

	if (delete_dialog (shell_view, get_folder_name (shell, folder_path)) == GTK_RESPONSE_OK)
		e_storage_set_async_remove_folder (storage_set, folder_path, delete_cb, shell_view);
}


struct _RenameCallbackData {
	EShellView *shell_view;
	char *new_path;
};
typedef struct _RenameCallbackData RenameCallbackData;

static RenameCallbackData *
rename_callback_data_new (EShellView *shell_view,
			  const char *new_path)
{
	RenameCallbackData *callback_data;

	callback_data = g_new (RenameCallbackData, 1);

	g_object_ref (shell_view);
	callback_data->shell_view = shell_view;

	callback_data->new_path = g_strdup (new_path);

	return callback_data;
}

static void
rename_callback_data_free (RenameCallbackData *callback_data)
{
	g_object_unref (callback_data->shell_view);
	g_free (callback_data->new_path);

	g_free (callback_data);
}

static void
rename_cb (EStorageSet *storage_set, EStorageResult result, void *data)
{
	RenameCallbackData *callback_data;

	callback_data = (RenameCallbackData *) data;

	if (result != E_STORAGE_OK) {
		e_notice (callback_data->shell_view, GTK_MESSAGE_ERROR,
			  _("Cannot rename folder:\n%s"), e_storage_result_to_string (result));
	} else {
		EFolder *folder;
		EShell *shell;
		EStorageSet *storage_set;

		shell = e_shell_view_get_shell (callback_data->shell_view);
		storage_set = e_shell_get_storage_set (shell);
		folder = e_storage_set_get_folder (storage_set, callback_data->new_path);

		if (folder != NULL) {
			char *base_name = g_path_get_basename (callback_data->new_path);

			e_folder_set_name (folder, base_name);
			g_free (base_name);
		}
	}

	rename_callback_data_free (callback_data);
}

void
e_shell_command_rename_folder (EShell *shell,
			       EShellView *shell_view,
			       const char *folder_path)
{
	EStorageSet *storage_set;
	EFolder *folder;
	RenameCallbackData *callback_data;
	const char *old_name;
	char *prompt;
	char *new_name;
	char *old_base_path;
	char *new_path;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	storage_set = e_shell_get_storage_set (shell);

	if (folder_path == NULL)
		folder_path = e_shell_view_get_current_path (shell_view);

	folder = e_storage_set_get_folder (storage_set, folder_path);
	g_return_if_fail (folder != NULL);

	old_name = e_folder_get_name (folder);
	prompt = g_strdup_printf (_("Rename the \"%s\" folder to:"), old_name);

	while (1) {
		const char *reason;

		new_name = e_request_string (shell_view != NULL ? GTK_WINDOW (shell_view) : NULL,
					     _("Rename Folder"), prompt, old_name);

		if (new_name == NULL)
			return;

		if (e_shell_folder_name_is_valid (new_name, &reason))
			break;

		e_notice (shell_view, GTK_MESSAGE_ERROR,
			  _("The specified folder name is not valid: %s"), reason);
	}

	g_free (prompt);

	if (strcmp (old_name, new_name) == 0) {
		g_free (new_name);
		return;
	}

	old_base_path = g_path_get_dirname (folder_path);
	new_path = g_build_filename (old_base_path, new_name, NULL);

	callback_data = rename_callback_data_new (shell_view, new_path);
	e_storage_set_async_xfer_folder (storage_set, folder_path, new_path, TRUE, rename_cb, callback_data);

	g_free (old_base_path);
	g_free (new_path);
	g_free (new_name);
}


static void
remove_shared_cb (EStorageSet *storage_set,
		  EStorageResult result,
		  void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	if (result == E_STORAGE_NOTIMPLEMENTED ||
	    result == E_STORAGE_UNSUPPORTEDOPERATION)
		e_notice (shell_view, GTK_MESSAGE_ERROR,
			  _("Selected folder does not belong to another user"));
	else if (result != E_STORAGE_OK)
		e_notice (shell_view, GTK_MESSAGE_ERROR,
			  _("Cannot remove folder:\n%s"), e_storage_result_to_string (result));
}

void
e_shell_command_remove_shared_folder (EShell *shell,
				      EShellView *shell_view,
				      const char *folder_path)
{
	EStorageSet *storage_set;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL || folder_path != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (folder_path != NULL || g_path_is_absolute (folder_path));

	storage_set = e_shell_get_storage_set (shell);

	if (folder_path == NULL)
		folder_path = e_shell_view_get_current_path (shell_view);

	e_storage_set_async_remove_shared_folder (storage_set, folder_path,
						  remove_shared_cb, shell_view);
}


void
e_shell_command_add_to_shortcut_bar (EShell *shell,
				     EShellView *shell_view,
				     const char *folder_path)
{
	EShortcuts *shortcuts;
	EStorageSet *storage_set;
	EFolder *folder;
	int group_num;
	char *uri;
	int unread_count;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (folder_path == NULL || g_path_is_absolute (folder_path));

	shortcuts = e_shell_get_shortcuts (shell);
	group_num = e_shell_view_get_current_shortcuts_group_num (shell_view);

	if (folder_path == NULL)
		uri = g_strdup (e_shell_view_get_current_uri (shell_view));
	else
		uri = g_strconcat (E_SHELL_URI_PREFIX, folder_path, NULL);

	unread_count = get_folder_unread (shell, e_shell_view_get_current_path (shell_view));

	storage_set = e_shell_get_storage_set (shell);
	folder = e_storage_set_get_folder (storage_set, e_shell_view_get_current_path (shell_view));

	e_shortcuts_add_shortcut (shortcuts, group_num, -1, uri, NULL,
				  unread_count,
				  e_folder_get_type_string (folder),
				  e_folder_get_custom_icon_name (folder));

	g_free (uri);
}
