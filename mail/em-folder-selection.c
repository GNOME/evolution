/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>

#include "em-folder-tree.h"
#include "em-folder-selector.h"
#include "em-folder-selection.h"
#include "mail-component.h"

/* TODO: rmeove this file, it could just go on em-folder-selection or em-utils */

struct _select_folder_data {
	void (*done) (const gchar *uri, gpointer data);
	gpointer data;
};

static void
emfs_selector_response(EMFolderSelector *emfs, gint response, struct _select_folder_data *d)
{
	if (response == GTK_RESPONSE_OK) {
		const gchar *uri = em_folder_selector_get_selected_uri(emfs);

		d->done(uri, d->data);
	}

	gtk_widget_destroy((GtkWidget *)emfs);
}

void
em_select_folder (GtkWindow *parent_window, const gchar *title, const gchar *oklabel, const gchar *default_uri,
		  EMFTExcludeFunc exclude,
		  void (*done) (const gchar *uri, gpointer user_data), gpointer user_data)
{
	struct _select_folder_data *d;
	EMFolderTreeModel *model;
	GtkWidget *dialog;
	EMFolderTree *emft;

	model = mail_component_peek_tree_model (mail_component_peek ());
	emft = (EMFolderTree *) em_folder_tree_new_with_model (model);
	if (exclude)
		em_folder_tree_set_excluded_func(emft, exclude, user_data);
	else
		em_folder_tree_set_excluded (emft, EMFT_EXCLUDE_NOSELECT|EMFT_EXCLUDE_VIRTUAL|EMFT_EXCLUDE_VTRASH);

	dialog = em_folder_selector_new(emft, EM_FOLDER_SELECTOR_CAN_CREATE, title, NULL, oklabel);

	d = g_malloc0(sizeof(*d));
	d->data = user_data;
	d->done = done;
	g_signal_connect(dialog, "response", G_CALLBACK (emfs_selector_response), d);
	g_object_set_data_full((GObject *)dialog, "e-select-data", d, (GDestroyNotify)g_free);
	gtk_widget_show(dialog);

	if (default_uri)
		em_folder_selector_set_selected((EMFolderSelector *)dialog, default_uri);
}
