/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 *  Authors: Ettore Perazzoli <ettore@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *	     Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include <gtk/gtkbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <libgnomeui/gnome-uidefs.h>

#include "e-msg-composer-select-file.h"
#include <e-util/e-icon-factory.h>

static GtkFileSelection *
run_selector(EMsgComposer *composer, const char *title, int multi, gboolean *showinline_p)
{
	GtkFileSelection *selection;
	GtkWidget *showinline = NULL;
	char *path;
	GList *icon_list;

	selection = (GtkFileSelection *)gtk_file_selection_new(title);
	gtk_window_set_transient_for((GtkWindow *)selection, (GtkWindow *)composer);
	gtk_window_set_wmclass((GtkWindow *)selection, "fileselection", "Evolution:composer");
	gtk_window_set_modal((GtkWindow *)selection, TRUE);
	
	icon_list = e_icon_factory_get_icon_list ("stock_mail-compose");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (selection), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}
	
	gtk_file_selection_set_select_multiple((GtkFileSelection *)selection, multi);

	/* restore last path used */
	path = g_object_get_data((GObject *)composer, "attach_path");
	if (path == NULL) {
		path = g_strdup_printf("%s/", g_get_home_dir());
		gtk_file_selection_set_filename(selection, path);
		g_free(path);
	} else {
		gtk_file_selection_set_filename(selection, path);
	}

	if (showinline_p) {
		showinline = gtk_check_button_new_with_label (_("Suggest automatic display of attachment"));
		gtk_widget_show (showinline);
		gtk_box_pack_end (GTK_BOX (selection->main_vbox), showinline, FALSE, FALSE, 4);
	}

	if (gtk_dialog_run((GtkDialog *)selection) == GTK_RESPONSE_OK) {
		if (showinline_p)
			*showinline_p = gtk_toggle_button_get_active((GtkToggleButton *)showinline);
		path = g_path_get_dirname(gtk_file_selection_get_filename(selection));
		g_object_set_data_full((GObject *)composer, "attach_path", g_strdup_printf("%s/", path), g_free);
		g_free(path);
	} else {
		gtk_widget_destroy((GtkWidget *)selection);
		selection = NULL;
	}

	return selection;
}

/**
 * e_msg_composer_select_file:
 * @composer: a composer
 * @title: the title for the file selection dialog box
 *
 * This pops up a file selection dialog box with the given title
 * and allows the user to select a file.
 *
 * Return value: the selected filename, or %NULL if the user
 * cancelled.
 **/
char *
e_msg_composer_select_file (EMsgComposer *composer, const char *title)
{
	GtkFileSelection *selection;
	char *name = NULL;

	selection = run_selector (composer, title, TRUE, NULL);
	if (selection) {
		name = g_strdup(gtk_file_selection_get_filename(selection));
		gtk_widget_destroy((GtkWidget *)selection);
	}

	return name;
}

GPtrArray *
e_msg_composer_select_file_attachments (EMsgComposer *composer, gboolean *showinline_p)
{
	GtkFileSelection *selection;
	GPtrArray *list = NULL;
	char **files;
	int i;

	selection = run_selector(composer, _("Attach file(s)"), TRUE, showinline_p);
	if (selection) {
		files = gtk_file_selection_get_selections(selection);
		
		if (files != NULL) {
			list = g_ptr_array_new ();
			for (i = 0; files[i]; i++)
				g_ptr_array_add (list, g_strdup (files[i]));
			
			g_strfreev (files);
		}
		
		gtk_widget_destroy((GtkWidget *)selection);
	}

	return list;
}

