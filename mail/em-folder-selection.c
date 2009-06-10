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

/* TODO: rmeove this file, it could just go on em-folder-selection or em-utils */

void
em_select_folder (EMFolderTreeModel *model,
                  const gchar *title,
                  const gchar *oklabel,
                  const gchar *default_uri,
                  EMFTExcludeFunc exclude,
                  void (*done) (const gchar *uri, gpointer user_data),
                  gpointer user_data)
{
	GtkWidget *dialog;
	EMFolderTree *emft;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (done != NULL);

	/* XXX Do we leak this reference? */
	emft = (EMFolderTree *) em_folder_tree_new_with_model (model);
	em_folder_tree_clone_expanded (emft);

	if (exclude)
		em_folder_tree_set_excluded_func (emft, exclude, user_data);
	else
		em_folder_tree_set_excluded (
			emft, EMFT_EXCLUDE_NOSELECT |
			EMFT_EXCLUDE_VIRTUAL | EMFT_EXCLUDE_VTRASH);

	dialog = em_folder_selector_new (
		emft, EM_FOLDER_SELECTOR_CAN_CREATE, title, NULL, oklabel);

	if (default_uri != NULL)
		em_folder_selector_set_selected (
			EM_FOLDER_SELECTOR (dialog), default_uri);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		const gchar *uri;

		uri = em_folder_selector_get_selected_uri (
			EM_FOLDER_SELECTOR (dialog));
		done (uri, user_data);
	}

	gtk_widget_destroy (dialog);
}
