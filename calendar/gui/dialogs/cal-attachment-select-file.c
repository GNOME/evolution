/*
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
 *		Harish Krishnaswamy <kharish@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* Much of this file has been shamelessly copied from the mail attachment
 * handling code, including the fixmes. Modifications/Additions that are
 * specific to the calendar component have been flagged by some comments
 * fwiw */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <glib/gi18n.h>

#include "cal-attachment-select-file.h"

enum {
	SELECTOR_MODE_MULTI    = (1 << 0),
	SELECTOR_MODE_SAVE     = (1 << 1)
};

static GtkWidget*
run_selector(CompEditor *editor, const gchar *title, guint32 flags, gboolean *showinline_p)
{
	GtkWidget *selection;
	GtkWidget *showinline = NULL;
	gchar *path;

	path = g_object_get_data ((GObject *) editor, "attach_path");

	if (flags & SELECTOR_MODE_SAVE)
		selection = gtk_file_chooser_dialog_new (title,
							 NULL,
							 GTK_FILE_CHOOSER_ACTION_SAVE,
							 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							 GTK_STOCK_SAVE, GTK_RESPONSE_OK,
							 NULL);
	else
		selection = gtk_file_chooser_dialog_new (title,
							 NULL,
							 GTK_FILE_CHOOSER_ACTION_OPEN,
							 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							 GTK_STOCK_OPEN, GTK_RESPONSE_OK,
							 NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (selection), GTK_RESPONSE_OK);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (selection), FALSE);

	if ((flags & SELECTOR_MODE_SAVE) == 0)
		gtk_file_chooser_set_select_multiple ((GtkFileChooser *) selection, (flags & SELECTOR_MODE_MULTI));

	/* restore last path used */
	if (!path)
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (selection), g_get_home_dir ());
	else
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (selection), path);

        if (showinline_p) {
		showinline = gtk_check_button_new_with_mnemonic (_("_Suggest automatic display of attachment"));
		gtk_widget_show (showinline);
		gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (selection), showinline);
        }

	gtk_window_set_transient_for ((GtkWindow *) selection, (GtkWindow *) editor);
	gtk_window_set_wmclass ((GtkWindow *) selection, "fileselection", "Evolution:editor");
	gtk_window_set_modal ((GtkWindow *) selection, TRUE);

	gtk_window_set_icon_name (GTK_WINDOW (selection), "mail-message-new");

	if (gtk_dialog_run ((GtkDialog *) selection) == GTK_RESPONSE_OK) {
		if (showinline_p)
			*showinline_p = gtk_toggle_button_get_active ((GtkToggleButton *) showinline);

		path = g_path_get_dirname (gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selection)));

		g_object_set_data_full ((GObject *) editor, "attach_path", g_strdup_printf ("%s/", path), g_free);
		g_free (path);
	} else {
		gtk_widget_destroy (selection);
		selection = NULL;
	}

	return selection;
}

/**
 * comp_editor_select_file:
 * @editor: a editor
 * @title: the title for the file selection dialog box
 * @save_mode: whether the file selection box should be shown in save mode or not
 *
 * This pops up a file selection dialog box with the given title
 * and allows the user to select a file.
 *
 * Return value: the selected filename, or %NULL if the user
 * cancelled.
 **/
gchar *
comp_editor_select_file (CompEditor *editor, const gchar *title, gboolean save_mode)
{
	guint32 flags = save_mode ? SELECTOR_MODE_SAVE : SELECTOR_MODE_MULTI;
	GtkWidget *selection;
	gchar *name = NULL;

	selection = run_selector (editor, title, flags, NULL);
	if (selection) {
		name = g_strdup (gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selection)));
		gtk_widget_destroy (selection);
	}

	return name;
}

GPtrArray *
comp_editor_select_file_attachments (CompEditor *editor, gboolean *showinline_p)
{
	GtkWidget *selection;
	GPtrArray *list = NULL;

	selection = run_selector (editor, _("Attach file(s)"), SELECTOR_MODE_MULTI, showinline_p);

	if (selection) {
		GSList *l, *n;

		if ((l = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (selection)))) {
			list = g_ptr_array_new ();

			while (l) {
				n = l->next;
				g_ptr_array_add (list, l->data);
				g_slist_free_1 (l);
				l = n;
			}
		}

		gtk_widget_destroy (selection);
	}

	return list;
}

