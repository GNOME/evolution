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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>

#include "em-folder-tree.h"
#include "em-folder-selection.h"

#include "mail-component.h"
#include "mail-tools.h"


static void
folder_selected_cb (EMFolderTree *emft, const char *path, const char *uri, GtkDialog *dialog)
{
	gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, TRUE);
}

static GtkWidget *
create_dialog (GtkWindow *parent, EMFolderTree *emft, const char *title, const char *default_uri, gboolean allow_create)
{
	GtkWidget *dialog;
	
	dialog = gtk_dialog_new ();
	
	if (parent)
		gtk_window_set_transient_for ((GtkWindow *) dialog, parent);
	
	gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 300);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_title (GTK_WINDOW (dialog), title);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 6);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 6);
	
	if (allow_create)
		gtk_dialog_add_buttons (GTK_DIALOG (dialog), GTK_STOCK_NEW, GTK_RESPONSE_APPLY, NULL);
	
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	
	if (default_uri == NULL)
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
	else
		em_folder_tree_set_selected (emft, default_uri);
	
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), GTK_WIDGET (emft), TRUE, TRUE, 6);
	
	g_signal_connect (emft, "folder-selected", G_CALLBACK (folder_selected_cb), dialog);
	
	return dialog;
}

CamelFolder *
em_folder_selection_run_dialog (GtkWindow *parent_window, const char *title, CamelFolder *default_folder)
{
	CamelFolder *selected_folder;
	EMFolderTreeModel *model;
	GtkWidget *dialog;
	GtkWidget *emft;
	const char *uri;
	int response;
	
	model = mail_component_get_tree_model (mail_component_peek ());
	emft = em_folder_tree_new_with_model (model);
	gtk_widget_show (emft);
	
	if (default_folder) {
		char *default_uri;
		
		default_uri = mail_tools_folder_to_url (default_folder);
		dialog = create_dialog (parent_window, (EMFolderTree *) emft, title, default_uri, FALSE);
		g_free (default_uri);
	} else {
		dialog = create_dialog (parent_window, (EMFolderTree *) emft, title, NULL, FALSE);
	}
	
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return NULL;
	}
	
	if (!(uri = em_folder_tree_get_selected_uri ((EMFolderTree *) emft))) {
		gtk_widget_destroy (dialog);
		return NULL;
	}
	
	selected_folder = mail_tool_uri_to_folder (uri, 0, NULL);
	gtk_widget_destroy (dialog);
	
	return selected_folder;
}

/* FIXME: This isn't the way to do it, but then neither is the above, really ... */
char *
em_folder_selection_run_dialog_uri (GtkWindow *parent_window, const char *title, const char *default_uri)
{
	EMFolderTreeModel *model;
	GtkWidget *dialog;
	GtkWidget *emft;
	const char *uri;
	int response;
	char *ret;
	
	model = mail_component_get_tree_model (mail_component_peek ());
	emft = em_folder_tree_new_with_model (model);
	gtk_widget_show (emft);
	
	if (default_uri)
		dialog = create_dialog (parent_window, (EMFolderTree *) emft, title, default_uri, FALSE);
	else
		dialog = create_dialog (parent_window, (EMFolderTree *) emft, title, NULL, FALSE);
	
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return NULL;
	}
	
	if (!(uri = em_folder_tree_get_selected_uri ((EMFolderTree *) emft))) {
		gtk_widget_destroy (dialog);
		return NULL;
	}
	
	ret = g_strdup (uri);
	gtk_widget_destroy (dialog);
	
	return ret;
}


struct _select_folder_data {
	void (*done) (const char *uri, void *data);
	void *data;
};

static void
emfs_response (GtkWidget *dialog, int response, struct _select_folder_data *d)
{
	EMFolderTree *emft = g_object_get_data ((GObject *) dialog, "emft");
	const char *uri;
	
	gtk_widget_hide (dialog);
	
	if (response == GTK_RESPONSE_OK) {
		uri = em_folder_tree_get_selected_uri (emft);
		d->done (uri, d->data);
	}
	
	gtk_widget_destroy (dialog);
}

void
em_select_folder (GtkWindow *parent_window, const char *title, const char *default_uri,
		  void (*done) (const char *uri, void *user_data), void *user_data)
{
	struct _select_folder_data *d;
	EMFolderTreeModel *model;
	GtkWidget *dialog;
	GtkWidget *emft;
	
	model = mail_component_get_tree_model (mail_component_peek ());
	emft = em_folder_tree_new_with_model (model);
	gtk_widget_show (emft);
	
	if (default_uri)
		dialog = create_dialog (parent_window, (EMFolderTree *) emft, title, default_uri, FALSE);
	else
		dialog = create_dialog (parent_window, (EMFolderTree *) emft, title, NULL, FALSE);
	
	d = g_malloc0 (sizeof (*d));
	d->data = user_data;
	d->done = done;
	
	/* ugh, painful api ... */
	g_object_set_data ((GObject *) dialog, "emft", emft);
	g_object_set_data_full ((GObject *) dialog, "emfs_data", d, g_free);
	g_signal_connect (dialog, "response", G_CALLBACK (emfs_response), d);
	
	gtk_widget_show (dialog);
}
