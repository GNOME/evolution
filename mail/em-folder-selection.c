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


CamelFolder *
em_folder_selection_run_dialog (GtkWindow *parent_window,
				const char *title,
				const char *caption,
				CamelFolder *default_folder)
{
	CamelFolder *selected_folder;
	EMFolderTreeModel *model;
	GtkWidget *dialog;
	GtkWidget *emft;
	const char *uri;
	int response;
	
	dialog = gtk_dialog_new_with_buttons (title, parent_window, GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_NEW, GTK_RESPONSE_APPLY,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	
	model = mail_component_get_tree_model (mail_component_peek ());
	emft = em_folder_tree_new_with_model (model);
	gtk_widget_show (emft);
	
	gtk_box_pack_start_defaults ((GtkBox *) ((GtkDialog *) dialog)->vbox, emft);
	
	if (default_folder) {
		char *default_uri;
		
		default_uri = mail_tools_folder_to_url (default_folder);
		em_folder_tree_set_selected ((EMFolderTree *) emft, default_uri);
		g_free (default_uri);
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
em_folder_selection_run_dialog_uri (GtkWindow *parent_window,
				    const char *title,
				    const char *caption,
				    const char *default_uri)
{
	EMFolderTreeModel *model;
	GtkWidget *dialog;
	GtkWidget *emft;
	const char *uri;
	int response;
	char *ret;
	
	dialog = gtk_dialog_new_with_buttons (title, parent_window, GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_NEW, GTK_RESPONSE_APPLY,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	
	model = mail_component_get_tree_model (mail_component_peek ());
	emft = em_folder_tree_new_with_model (model);
	gtk_widget_show (emft);
	
	gtk_box_pack_start_defaults ((GtkBox *) ((GtkDialog *) dialog)->vbox, emft);
	
	if (default_uri)
		em_folder_tree_set_selected ((EMFolderTree *) emft, default_uri);
	
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
emfs_response (GtkDialog *dialog, int response, struct _select_folder_data *d)
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
em_select_folder (GtkWindow *parent_window, const char *title, const char *text,
		  const char *default_uri, void (*done) (const char *uri, void *user_data), void *user_data)
{
	struct _select_folder_data *d;
	EMFolderTreeModel *model;
	GtkWidget *dialog;
	GtkWidget *emft;
	
	dialog = gtk_dialog_new_with_buttons (title, parent_window, GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_NEW, GTK_RESPONSE_APPLY,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	
	model = mail_component_get_tree_model (mail_component_peek ());
	emft = em_folder_tree_new_with_model (model);
	gtk_widget_show (emft);
	
	gtk_box_pack_start_defaults ((GtkBox *) ((GtkDialog *) dialog)->vbox, emft);
	
	if (default_uri)
		em_folder_tree_set_selected ((EMFolderTree *) emft, default_uri);
	
	d = g_malloc0 (sizeof (*d));
	d->data = user_data;
	d->done = done;
	
	/* ugh, painful api ... */
	g_object_set_data ((GObject *) dialog, "emft", emft);
	g_object_set_data_full ((GObject *) dialog, "emfs_data", d, g_free);
	g_signal_connect (dialog, "response", G_CALLBACK (emfs_response), d);
	
	gtk_widget_show (dialog);
}
