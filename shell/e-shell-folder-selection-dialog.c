/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-folder-selection-dialog.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnome/gnome-i18n.h>

#include "e-util/e-gui-utils.h"
#include "e-util/e-util.h"
#include "widgets/misc/e-scroll-frame.h"

#include "e-shell-constants.h"
#include "e-storage-set-view.h"
#include "e-storage-set.h"

#include "e-shell-folder-creation-dialog.h"

#include "e-shell-folder-selection-dialog.h"


#define PARENT_TYPE (gnome_dialog_get_type ())
static GnomeDialogClass *parent_class = NULL;

struct _EShellFolderSelectionDialogPrivate {
	EShell *shell;
	GList *allowed_types;
	EStorageSet *storage_set;
	GtkWidget *storage_set_view;
};

enum {
	FOLDER_SELECTED,
	CANCELLED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static gboolean
check_folder_type (EShellFolderSelectionDialog *folder_selection_dialog)
{
	EShellFolderSelectionDialogPrivate *priv;
	const char *selected_path;
	EFolder *folder;
	const char *folder_type;
	GList *p;

	priv = folder_selection_dialog->priv;
	if (priv->allowed_types == NULL)
		return TRUE;

	selected_path = e_shell_folder_selection_dialog_get_selected_path (folder_selection_dialog);
	if (selected_path == NULL)
		return FALSE;

	folder = e_storage_set_get_folder (priv->storage_set, selected_path);
	if (folder == NULL)
		return FALSE;

	folder_type = e_folder_get_type_string (folder);

	for (p = priv->allowed_types; p != NULL; p = p->next) {
		const char *type;

		type = (const char *) p->data;
		if (strcasecmp (folder_type, type) == 0)
			return TRUE;
	}

	e_notice (GTK_WINDOW (folder_selection_dialog), GNOME_MESSAGE_BOX_ERROR,
		  _("The type of the selected folder is not valid for\nthe requested operation."));

	return FALSE;
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EShellFolderSelectionDialog *folder_selection_dialog;
	EShellFolderSelectionDialogPrivate *priv;

	folder_selection_dialog = E_SHELL_FOLDER_SELECTION_DIALOG (object);
	priv = folder_selection_dialog->priv;

	if (priv->storage_set != NULL)
		gtk_object_unref (GTK_OBJECT (priv->storage_set));

	e_free_string_list (priv->allowed_types);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* GnomeDialog methods.  */

static void
impl_clicked (GnomeDialog *dialog,
	      int button_number)
{
	EShellFolderSelectionDialog *folder_selection_dialog;
	EShellFolderSelectionDialogPrivate *priv;
	EStorageSetView *storage_set_view;
	const char *default_parent_folder;

	folder_selection_dialog = E_SHELL_FOLDER_SELECTION_DIALOG (dialog);
	priv = folder_selection_dialog->priv;

	switch (button_number) {
	case 0:			/* OK */
		if (check_folder_type (folder_selection_dialog)) {
			gtk_signal_emit (GTK_OBJECT (folder_selection_dialog), signals[FOLDER_SELECTED],
					 e_shell_folder_selection_dialog_get_selected_path (folder_selection_dialog));
			gtk_widget_destroy (GTK_WIDGET (dialog));
		}
		break;
	case 1:			/* Cancel */
		gtk_signal_emit (GTK_OBJECT (folder_selection_dialog), signals[CANCELLED]);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case 2:			/* Add */
		storage_set_view = E_STORAGE_SET_VIEW (priv->storage_set_view);
		default_parent_folder = e_storage_set_view_get_current_folder (storage_set_view);

		e_shell_show_folder_creation_dialog (priv->shell, GTK_WINDOW (dialog),
						     default_parent_folder);

		break;
	}
}


/* GTK+ type initialization.  */

static void
class_init (EShellFolderSelectionDialogClass *klass)
{
	GtkObjectClass *object_class;
	GnomeDialogClass *dialog_class;

	parent_class = gtk_type_class (PARENT_TYPE);
	object_class = GTK_OBJECT_CLASS (klass);
	dialog_class = GNOME_DIALOG_CLASS (klass);

	object_class->destroy = impl_destroy;

	dialog_class->clicked = impl_clicked;

	signals[FOLDER_SELECTED]
		= gtk_signal_new ("folder_selected",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShellFolderSelectionDialogClass, folder_selected),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_STRING);

	signals[CANCELLED]
		= gtk_signal_new ("cancelled",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShellFolderSelectionDialogClass, cancelled),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EShellFolderSelectionDialog *shell_folder_selection_dialog)
{
	EShellFolderSelectionDialogPrivate *priv;

	priv = g_new (EShellFolderSelectionDialogPrivate, 1);
	priv->shell            = NULL;
	priv->storage_set      = NULL;
	priv->storage_set_view = NULL;
	priv->allowed_types    = NULL;

	shell_folder_selection_dialog->priv = priv;
}


static void
set_default_folder (EShellFolderSelectionDialog *shell_folder_selection_dialog,
		    const char *default_uri)
{
	EShellFolderSelectionDialogPrivate *priv;
	char *default_path;

	g_assert (default_uri != NULL);

	priv = shell_folder_selection_dialog->priv;

	if (strncmp (default_uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0) {
		/* `evolution:' URI.  */
		default_path = g_strdup (default_uri + E_SHELL_URI_PREFIX_LEN);
	} else {
		/* Physical URI.  */
		default_path = e_storage_set_get_path_for_physical_uri (priv->storage_set,
									default_uri);
	}

	e_storage_set_view_set_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view),
					       default_path);

	g_free (default_path);
}


/**
 * e_shell_folder_selection_dialog_construct:
 * @folder_selection_dialog: A folder selection dialog widget
 * @shell: The this folder selection dialog is for
 * @title: Title of the window
 * @default_uri: The URI of the folder to be selected by default
 * @allowed_types: List of the names of the allowed types
 * 
 * Construct @folder_selection_dialog.
 **/
void
e_shell_folder_selection_dialog_construct (EShellFolderSelectionDialog *folder_selection_dialog,
					   EShell *shell,
					   const char *title,
					   const char *default_uri,
					   const char *allowed_types[])
{
	EShellFolderSelectionDialogPrivate *priv;
	GtkWidget *scroll_frame;
	int i;

	g_return_if_fail (folder_selection_dialog != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_SELECTION_DIALOG (folder_selection_dialog));
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	priv = folder_selection_dialog->priv;

	gtk_window_set_modal (GTK_WINDOW (folder_selection_dialog), TRUE);
	gtk_window_set_title (GTK_WINDOW (folder_selection_dialog), title);

	gnome_dialog_append_buttons (GNOME_DIALOG (folder_selection_dialog),
				     GNOME_STOCK_BUTTON_OK,
				     GNOME_STOCK_BUTTON_CANCEL,
				     _("New..."),
				     NULL);
	gnome_dialog_set_default (GNOME_DIALOG (folder_selection_dialog), 0);

	gtk_window_set_policy (GTK_WINDOW (folder_selection_dialog), TRUE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (folder_selection_dialog), 350, 300);

	priv->shell = shell;
	gtk_signal_connect_object_while_alive (GTK_OBJECT (shell), "destroy",
					       GTK_SIGNAL_FUNC (gtk_widget_destroy),
					       GTK_OBJECT (folder_selection_dialog));

	priv->storage_set = e_shell_get_storage_set (shell);
	gtk_object_ref (GTK_OBJECT (priv->storage_set));

	priv->storage_set_view = e_storage_set_new_view (priv->storage_set);
	GTK_WIDGET_SET_FLAGS (priv->storage_set_view, GTK_CAN_FOCUS);

	g_assert (priv->allowed_types == NULL);
	if (allowed_types != NULL) {
		for (i = 0; allowed_types[i] != NULL; i++)
			priv->allowed_types = g_list_prepend (priv->allowed_types,
							      g_strdup (allowed_types[i]));
	}

	if (default_uri != NULL)
		set_default_folder (folder_selection_dialog, default_uri);

	scroll_frame = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (scroll_frame),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (scroll_frame), priv->storage_set_view);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (folder_selection_dialog)->vbox),
			    scroll_frame, TRUE, TRUE, 2);

	gtk_widget_show (scroll_frame);
	gtk_widget_show (priv->storage_set_view);
}

/**
 * e_shell_folder_selection_dialog_new:
 * @shell: The this folder selection dialog is for
 * @title: Title of the window
 * @default_uri: The URI of the folder to be selected by default
 * @allowed_types: List of the names of the allowed types
 * 
 * Create a new folder selection dialog widget.  @default_uri can be either an
 * `evolution:' URI or a physical URI (all the non-`evoluion:' URIs are
 * considered to be physical URIs).
 * 
 * Return value: 
 **/
GtkWidget *
e_shell_folder_selection_dialog_new (EShell *shell,
				     const char *title,
				     const char *default_uri,
				     const char *allowed_types[])
{
	EShellFolderSelectionDialog *folder_selection_dialog;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	folder_selection_dialog = gtk_type_new (e_shell_folder_selection_dialog_get_type ());
	e_shell_folder_selection_dialog_construct (folder_selection_dialog, shell,
						   title, default_uri, allowed_types);

	return GTK_WIDGET (folder_selection_dialog);
}


const char *
e_shell_folder_selection_dialog_get_selected_path (EShellFolderSelectionDialog *folder_selection_dialog)
{
	EShellFolderSelectionDialogPrivate *priv;

	g_return_val_if_fail (folder_selection_dialog != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_FOLDER_SELECTION_DIALOG (folder_selection_dialog), NULL);

	priv = folder_selection_dialog->priv;

	return e_storage_set_view_get_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view));
}


E_MAKE_TYPE (e_shell_folder_selection_dialog, "EShellFolderSelectionDialog", EShellFolderSelectionDialog,
	     class_init, init, PARENT_TYPE)
