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

#include "em-folder-selection.h"

#include "mail-component.h"
#include "mail-tools.h"

#include "shell/e-folder-selection-dialog.h"


CamelFolder *
em_folder_selection_run_dialog (GtkWindow *parent_window,
				const char *title,
				const char *caption,
				CamelFolder *default_folder)
{
	EStorageSet *storage_set = mail_component_peek_storage_set (mail_component_peek ());
	char *default_path = NULL;
	CamelStore *default_store;
	GtkWidget *dialog;
	EFolder *selected_e_folder;
	CamelFolder *selected_camel_folder;
	int response;

	default_store = camel_folder_get_parent_store (default_folder);
	if (default_store != NULL) {
		EStorage *storage = mail_component_lookup_storage (mail_component_peek (), default_store);

		if (storage != NULL) {
			default_path = g_strconcat ("/",
						    e_storage_get_name (storage),
						    "/",
						    camel_folder_get_full_name (default_folder),
						    NULL);
		}
	}

	/* EPFIXME: Allowed types?  */
	dialog = e_folder_selection_dialog_new (storage_set, title, caption, default_path, NULL, FALSE);
	g_free(default_path);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return NULL;
	}

	selected_e_folder = e_storage_set_get_folder (storage_set,
						      e_folder_selection_dialog_get_selected_path (E_FOLDER_SELECTION_DIALOG (dialog)));
	if (selected_e_folder == NULL) {
		gtk_widget_destroy (dialog);
		return NULL;
	}

	selected_camel_folder = mail_tool_uri_to_folder (e_folder_get_physical_uri (selected_e_folder), 0, NULL);
	gtk_widget_destroy (dialog);

	return selected_camel_folder;
}

/* FIXME: This isn't the way to do it, but then neither is the above, really ... */
char *
em_folder_selection_run_dialog_uri(GtkWindow *parent_window,
				   const char *title,
				   const char *caption,
				   const char *default_folder_uri)
{
	EStorageSet *storage_set = mail_component_peek_storage_set (mail_component_peek ());
	char *default_path;
	GtkWidget *dialog;
	EFolder *selected_e_folder;
	int response;

	default_path = e_storage_set_get_path_for_physical_uri(storage_set, default_folder_uri);
	dialog = e_folder_selection_dialog_new (storage_set, title, caption, default_path, NULL, FALSE);
	g_free(default_path);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return NULL;
	}

	selected_e_folder = e_storage_set_get_folder (storage_set,
						      e_folder_selection_dialog_get_selected_path (E_FOLDER_SELECTION_DIALOG (dialog)));
	gtk_widget_destroy (dialog);
	if (selected_e_folder == NULL)
		return NULL;

	return g_strdup(e_folder_get_physical_uri(selected_e_folder));
}


struct _select_folder_data {
	void (*done)(const char *uri, void *data);
	void *data;
};

static void
emfs_folder_selected(GtkWidget *w, const char *path, struct _select_folder_data *d)
{
	const char *uri = NULL;
	EStorageSet *storage_set = mail_component_peek_storage_set (mail_component_peek ());
	EFolder *folder;
	
	folder = e_storage_set_get_folder(storage_set, path);
	if (folder)
		uri = e_folder_get_physical_uri(folder);

	gtk_widget_hide(w);

	d->done(uri, d->data);

	gtk_widget_destroy(w);
}

static void
emfs_folder_cancelled(GtkWidget *w, struct _select_folder_data *d)
{
	gtk_widget_destroy(w);
}

void
em_select_folder(GtkWindow *parent_window, const char *title, const char *text, const char *default_folder_uri, void (*done)(const char *uri, void *data), void *data)
{
	EStorageSet *storage_set = mail_component_peek_storage_set (mail_component_peek ());
	char *path;
	GtkWidget *dialog;
	struct _select_folder_data *d;

	d = g_malloc0(sizeof(*d));
	d->data = data;
	d->done = done;

	if (default_folder_uri)
		path = e_storage_set_get_path_for_physical_uri(storage_set, default_folder_uri);
	else
		path = NULL;
	dialog = e_folder_selection_dialog_new(storage_set, title, text, path, NULL, TRUE);
	g_free(path);
	/* ugh, painful api ... */
	g_signal_connect(dialog, "folder_selected", G_CALLBACK(emfs_folder_selected), d);
	g_signal_connect(dialog, "cancelled", G_CALLBACK(emfs_folder_cancelled), d);
	g_object_set_data_full((GObject *)dialog, "emfs_data", d, g_free);
	gtk_widget_show(dialog);
}
