/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder-selection-dialog.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-folder-selection-dialog.h"

#include "e-shell-marshal.h"
#include "e-storage-set-view.h"
#include "e-storage-set.h"

#include <libgnome/gnome-i18n.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>

#include <gtk/gtkbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>

#include <string.h>


#define PARENT_TYPE (gtk_dialog_get_type ())
static GtkDialogClass *parent_class = NULL;

struct _EFolderSelectionDialogPrivate {
	GList *allowed_types;
	EStorageSet *storage_set;
	GtkWidget *storage_set_view;

	gboolean allow_creation;
};

enum {
	FOLDER_SELECTED,
	CANCELLED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
	RESPONSE_NEW
};


/* Utility functions.  */

static gboolean
check_folder_type_valid (EFolderSelectionDialog *folder_selection_dialog)
{
	EFolderSelectionDialogPrivate *priv;
	const char *selected_path;
	EFolder *folder;
	const char *folder_type;
	GList *p;

	priv = folder_selection_dialog->priv;
	if (priv->allowed_types == NULL)
		return TRUE;

	selected_path = e_folder_selection_dialog_get_selected_path (folder_selection_dialog);
	if (selected_path == NULL)
		return FALSE;

	folder = e_storage_set_get_folder (priv->storage_set, selected_path);
	if (folder == NULL)
		return FALSE;

	folder_type = e_folder_get_type_string (folder);

	for (p = priv->allowed_types; p != NULL; p = p->next) {
		const char *type, *slash;

		type = (const char *) p->data;
		if (strcmp (folder_type, type) == 0)
			return TRUE;
		slash = strchr (type, '/');
		if (slash && slash[1] == '*' && strncmp (folder_type, type, slash - type) == 0)
			return TRUE;
	}

	return FALSE;
}


/* Folder creation dialog callback.  */

#if 0				/* EPFIXME */
static void
folder_creation_dialog_result_cb (EShell *shell,
				  EShellFolderCreationDialogResult result,
				  const char *path,
				  void *data)
{
	EFolderSelectionDialog *dialog;
	EFolderSelectionDialogPrivate *priv;

	dialog = E_FOLDER_SELECTION_DIALOG (data);
	priv = dialog->priv;

	if (priv == NULL) {
		g_warning ("dialog->priv is NULL, and should not be");
		return;
	}

	if (result == E_SHELL_FOLDER_CREATION_DIALOG_RESULT_SUCCESS)
		e_storage_set_view_set_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view),
						       path);
}
#endif


/* GtkObject methods.  */

static void
impl_dispose (GObject *object)
{
	EFolderSelectionDialog *folder_selection_dialog;
	EFolderSelectionDialogPrivate *priv;

	folder_selection_dialog = E_FOLDER_SELECTION_DIALOG (object);
	priv = folder_selection_dialog->priv;

	if (priv->storage_set != NULL) {
		g_object_unref (priv->storage_set);
		priv->storage_set = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EFolderSelectionDialog *folder_selection_dialog;
	EFolderSelectionDialogPrivate *priv;

	folder_selection_dialog = E_FOLDER_SELECTION_DIALOG (object);
	priv = folder_selection_dialog->priv;

	e_free_string_list (priv->allowed_types);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* GtkDialog methods.  */

static void
impl_response (GtkDialog *dialog,
	       int response)
{
	EFolderSelectionDialog *folder_selection_dialog;
	EFolderSelectionDialogPrivate *priv;
	EStorageSetView *storage_set_view;
	const char *default_parent_folder;
	const char *default_subtype;
	char *default_type;

	folder_selection_dialog = E_FOLDER_SELECTION_DIALOG (dialog);
	priv = folder_selection_dialog->priv;

	switch (response) {
	case GTK_RESPONSE_OK:
		if (check_folder_type_valid (folder_selection_dialog)) {
			g_signal_emit (folder_selection_dialog, signals[FOLDER_SELECTED], 0,
				       e_folder_selection_dialog_get_selected_path (folder_selection_dialog));
		}
		break;

	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_DELETE_EVENT:
		g_signal_emit (folder_selection_dialog, signals[CANCELLED], 0);
		break;

	case RESPONSE_NEW:
		storage_set_view = E_STORAGE_SET_VIEW (priv->storage_set_view);
		default_parent_folder = e_storage_set_view_get_current_folder (storage_set_view);

		/* The default type in the folder creation dialog will be the
		   first of the allowed types.  If all types are allowed,
		   hardcode to "mail".  */
		if (priv->allowed_types == NULL)
			default_type = g_strdup ("mail");
		else {
			default_subtype = (const char *) priv->allowed_types->data;
			default_type = g_strndup (default_subtype,
						  strcspn (default_subtype, "/"));
		}

#if 0				/* EPFIXME */
		e_shell_show_folder_creation_dialog (priv->shell, GTK_WINDOW (dialog),
						     default_parent_folder,
						     default_type,
						     folder_creation_dialog_result_cb,
						     dialog);
#endif

		g_free (default_type);

		break;
	}
}


/* GTK+ type initialization.  */

static void
class_init (EFolderSelectionDialogClass *klass)
{
	GObjectClass *object_class;
	GtkDialogClass *dialog_class;

	parent_class = g_type_class_ref(PARENT_TYPE);
	object_class = G_OBJECT_CLASS (klass);
	dialog_class = GTK_DIALOG_CLASS (klass);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	dialog_class->response = impl_response;

	signals[FOLDER_SELECTED]
		= g_signal_new ("folder_selected",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EFolderSelectionDialogClass, folder_selected),
				NULL, NULL,
				e_shell_marshal_NONE__STRING,
				G_TYPE_NONE, 1,
				G_TYPE_STRING);

	signals[CANCELLED]
		= g_signal_new ("cancelled",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EFolderSelectionDialogClass, cancelled),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);
}

static void
init (EFolderSelectionDialog *folder_selection_dialog)
{
	EFolderSelectionDialogPrivate *priv;

	priv = g_new (EFolderSelectionDialogPrivate, 1);
	priv->storage_set      = NULL;
	priv->storage_set_view = NULL;
	priv->allowed_types    = NULL;
	priv->allow_creation   = TRUE;

	folder_selection_dialog->priv = priv;
}


/* ETable callbacks.  */

static void
folder_selected_cb (EStorageSetView *storage_set_view,
		    const char *path,
		    void *data)
{
	EFolderSelectionDialog *dialog;

	dialog = E_FOLDER_SELECTION_DIALOG (data);

	if (check_folder_type_valid (dialog))
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
	else
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
}

static void
double_click_cb (EStorageSetView *essv,
		 int row,
		 ETreePath path,
		 int col,
		 GdkEvent *event,
		 EFolderSelectionDialog *folder_selection_dialog)
{
	g_return_if_fail (folder_selection_dialog != NULL);

	if (check_folder_type_valid (folder_selection_dialog)) {
		g_signal_emit (folder_selection_dialog, signals[FOLDER_SELECTED], 0,
			       e_folder_selection_dialog_get_selected_path (folder_selection_dialog));
	}
}


/**
 * e_folder_selection_dialog_construct:
 * @folder_selection_dialog: A folder selection dialog widget
 * @storage_set: The storage set this folder selection dialog is for
 * @title: Title of the window
 * @caption: A brief text to be put on top of the storage view
 * @default_path: The path of the folder to be selected by default
 * @allowed_types: List of the names of the allowed types
 * 
 * Construct @folder_selection_dialog.
 **/
/* EPFIXME: Should be allowed to get the expanded state from a model
   EStorageSetView.  */
void
e_folder_selection_dialog_construct (EFolderSelectionDialog *folder_selection_dialog,
				     EStorageSet *storage_set,
				     const char *title,
				     const char *caption,
				     const char *default_path,
				     const char *allowed_types[],
				     gboolean allow_creation)
{
	EFolderSelectionDialogPrivate *priv;
	GtkWidget *scrolled_window;
	GtkWidget *caption_label;
	int i;

	g_return_if_fail (E_IS_FOLDER_SELECTION_DIALOG (folder_selection_dialog));
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	priv = folder_selection_dialog->priv;

	/* Basic dialog setup.  */

	gtk_window_set_default_size (GTK_WINDOW (folder_selection_dialog), 350, 300);
	gtk_window_set_modal (GTK_WINDOW (folder_selection_dialog), TRUE);
	gtk_window_set_title (GTK_WINDOW (folder_selection_dialog), title);
	gtk_container_set_border_width (GTK_CONTAINER (folder_selection_dialog), 6);

	if (allow_creation)
		gtk_dialog_add_buttons (GTK_DIALOG (folder_selection_dialog),
					GTK_STOCK_NEW, RESPONSE_NEW,
					NULL);

	gtk_dialog_add_buttons (GTK_DIALOG (folder_selection_dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (folder_selection_dialog), GTK_RESPONSE_OK, FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (folder_selection_dialog), GTK_RESPONSE_OK);

	/* Set up the label.  */

	if (caption != NULL) {
		caption_label = gtk_label_new (caption);
		gtk_label_set_justify (GTK_LABEL (caption_label), GTK_JUSTIFY_LEFT); 
		gtk_widget_show (caption_label);

		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (folder_selection_dialog)->vbox),
				    caption_label, FALSE, TRUE, 6);
		gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (folder_selection_dialog)->vbox),
				     6);


	}

	/* Set up the storage set and its view.  */

	priv->storage_set = storage_set;
	g_object_ref (storage_set);

	priv->storage_set_view = e_storage_set_create_new_view (priv->storage_set, NULL);
	e_storage_set_view_set_allow_dnd (E_STORAGE_SET_VIEW (priv->storage_set_view), FALSE);
	e_storage_set_view_enable_search (E_STORAGE_SET_VIEW (priv->storage_set_view), TRUE);

	g_signal_connect (priv->storage_set_view, "double_click", G_CALLBACK (double_click_cb), folder_selection_dialog);
	g_signal_connect (priv->storage_set_view, "folder_selected", G_CALLBACK (folder_selected_cb), folder_selection_dialog);

	g_assert (priv->allowed_types == NULL);
	if (allowed_types != NULL) {
		for (i = 0; allowed_types[i] != NULL; i++)
			priv->allowed_types = g_list_prepend (priv->allowed_types,
							      g_strdup (allowed_types[i]));

		/* Preserve the order so we can use the first type listed as
		   the default for the folder creation dialog invoked by the
		   "New..." button.  */
		priv->allowed_types = g_list_reverse (priv->allowed_types);
	}

	if (default_path != NULL)
		e_storage_set_view_set_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view), default_path);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (scrolled_window), priv->storage_set_view);
        
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (folder_selection_dialog)->vbox),
			    scrolled_window, TRUE, TRUE, 6);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (folder_selection_dialog)->vbox), 6); 
	
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (folder_selection_dialog)->vbox), 6);
	
	gtk_widget_show (priv->storage_set_view);
	gtk_widget_show (scrolled_window);

	GTK_WIDGET_SET_FLAGS (priv->storage_set_view, GTK_CAN_FOCUS);
	gtk_widget_grab_focus (priv->storage_set_view);
}

/**
 * e_folder_selection_dialog_new:
 * @storage_set: The storage set this folder selection dialog is for
 * @title: Title of the window
 * @caption: A brief text to be put on top of the storage view
 * @default_path: The path of the folder to be selected by default
 * @allowed_types: List of the names of the allowed types
 * 
 * Create a new folder selection dialog widget.
 * 
 * Return value: 
 **/
GtkWidget *
e_folder_selection_dialog_new (EStorageSet *storage_set,
			       const char *title,
			       const char *caption,
			       const char *default_path,
			       const char *allowed_types[],
			       gboolean allow_creation)
{
	EFolderSelectionDialog *folder_selection_dialog;

	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	folder_selection_dialog = g_object_new (e_folder_selection_dialog_get_type (), NULL);
	e_folder_selection_dialog_construct (folder_selection_dialog, storage_set,
					     title, caption, default_path, allowed_types,
					     allow_creation);

	return GTK_WIDGET (folder_selection_dialog);
}


const char *
e_folder_selection_dialog_get_selected_path (EFolderSelectionDialog *folder_selection_dialog)
{
	EFolderSelectionDialogPrivate *priv;

	g_return_val_if_fail (E_IS_FOLDER_SELECTION_DIALOG (folder_selection_dialog), NULL);

	priv = folder_selection_dialog->priv;

	return e_storage_set_view_get_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view));
}


E_MAKE_TYPE (e_folder_selection_dialog, "EFolderSelectionDialog", EFolderSelectionDialog, class_init, init, PARENT_TYPE)
