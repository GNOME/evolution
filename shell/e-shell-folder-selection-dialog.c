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
#include <libgnome/gnome-i18n.h>

#include "e-util/e-util.h"
#include "widgets/misc/e-scroll-frame.h"

#include "e-storage-set.h"
#include "e-storage-set-view.h"

#include "e-shell-folder-creation-dialog.h"

#include "e-shell-folder-selection-dialog.h"


#define PARENT_TYPE GNOME_TYPE_DIALOG
static GnomeDialogClass *parent_class = NULL;

struct _EShellFolderSelectionDialogPrivate {
	EShell *shell;
	GtkWidget *storage_set_view;
};


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EShellFolderSelectionDialog *folder_selection_dialog;
	EShellFolderSelectionDialogPrivate *priv;

	folder_selection_dialog = E_SHELL_FOLDER_SELECTION_DIALOG (object);
	priv = folder_selection_dialog->priv;

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

	folder_selection_dialog = E_SHELL_FOLDER_SELECTION_DIALOG (dialog);
	priv = folder_selection_dialog->priv;

	/* Check for the "Add..." button.  */
	if (button_number == 2) {
		EStorageSetView *storage_set_view;
		const char *default_parent_folder;

#if 0
		/* (FIXME: The stupid GnomeDialog defines the "clicked" signal as
                   GTK_RUN_LAST so this does not work.  Grrr.)  */
		/* We don't want the user of the widget to handle this directly.  */
		gtk_signal_emit_stop_by_name (GTK_OBJECT (dialog), "clicked");
#endif

		storage_set_view = E_STORAGE_SET_VIEW (priv->storage_set_view);
		default_parent_folder = e_storage_set_view_get_current_folder (storage_set_view);

		e_shell_show_folder_creation_dialog (priv->shell, GTK_WINDOW (dialog),
						     default_parent_folder);
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
}

static void
init (EShellFolderSelectionDialog *shell_folder_selection_dialog)
{
	EShellFolderSelectionDialogPrivate *priv;

	priv = g_new (EShellFolderSelectionDialogPrivate, 1);
	priv->shell            = NULL;
	priv->storage_set_view = NULL;

	shell_folder_selection_dialog->priv = priv;
}


void
e_shell_folder_selection_dialog_construct (EShellFolderSelectionDialog *folder_selection_dialog,
					   EShell *shell,
					   const char *title,
					   const char *default_path)
{
	EShellFolderSelectionDialogPrivate *priv;
	EStorageSet *storage_set;
	GtkWidget *scroll_frame;

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

	storage_set = e_shell_get_storage_set (shell);

	priv->storage_set_view = e_storage_set_new_view (storage_set);
	GTK_WIDGET_SET_FLAGS (priv->storage_set_view, GTK_CAN_FOCUS);

	e_storage_set_view_set_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view),
					       default_path);

	scroll_frame = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (scroll_frame),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (scroll_frame), priv->storage_set_view);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (folder_selection_dialog)->vbox),
			    scroll_frame, TRUE, TRUE, 2);

	gtk_widget_show (scroll_frame);
	gtk_widget_show (priv->storage_set_view);
}

GtkWidget *
e_shell_folder_selection_dialog_new (EShell *shell,
				     const char *title,
				     const char *default_path)
{
	EShellFolderSelectionDialog *folder_selection_dialog;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	folder_selection_dialog = gtk_type_new (e_shell_folder_selection_dialog_get_type ());
	e_shell_folder_selection_dialog_construct (folder_selection_dialog, shell,
						   title, default_path);

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
