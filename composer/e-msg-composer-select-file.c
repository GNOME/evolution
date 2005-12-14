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
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkversion.h>

#ifdef USE_GTKFILECHOOSER
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkstock.h>
#else
#include <gtk/gtkfilesel.h>
#endif

#include <libgnomeui/gnome-uidefs.h>
#include <libgnome/gnome-i18n.h>

#include "e-msg-composer-select-file.h"
#include <e-util/e-icon-factory.h>
#include "e-msg-composer.h"
#include "e-attachment-bar.h"

enum {
	SELECTOR_MODE_MULTI    = (1 << 0),
	SELECTOR_MODE_SAVE     = (1 << 1),
	SELECTOR_SHOW_INLINE = 1<<2
};

/* this is a mess */

static GtkWidget*
get_selector(struct _EMsgComposer *composer, const char *title, guint32 flags)
{
	GtkWidget *selection;
	GtkWidget *showinline = NULL;
	char *path;
	GList *icon_list;
	
	path = g_object_get_data ((GObject *) composer, "attach_path");
	
#ifdef USE_GTKFILECHOOSER
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
							 _("A_ttach"), GTK_RESPONSE_OK,
							 NULL);
	
	gtk_dialog_set_default_response (GTK_DIALOG (selection), GTK_RESPONSE_OK);
	
	if ((flags & SELECTOR_MODE_SAVE) == 0)
		gtk_file_chooser_set_select_multiple ((GtkFileChooser *) selection, (flags & SELECTOR_MODE_MULTI));
	
	/* restore last path used */
	if (!path)
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (selection), g_get_home_dir ());
	else
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (selection), path);
	
        if (flags & SELECTOR_SHOW_INLINE) {
		showinline = gtk_check_button_new_with_label (_("Suggest automatic display of attachment"));
		gtk_widget_show (showinline);
		gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (selection), showinline);
		g_object_set_data((GObject *)selection, "show-inline", showinline);
        }
#else
	selection = gtk_file_selection_new (title);
	
	gtk_file_selection_set_select_multiple ((GtkFileSelection *) selection, (flags & SELECTOR_MODE_MULTI));
	
	/* restore last path used */
	if (!path) {
		path = g_strdup_printf ("%s/", g_get_home_dir ());
		gtk_file_selection_set_filename (GTK_FILE_SELECTION (selection), path);
		g_free (path);
	} else {
		gtk_file_selection_set_filename (GTK_FILE_SELECTION (selection), path);
	}
	
        if (flags & SELECTOR_SHOW_INLINE) {
		showinline = gtk_check_button_new_with_label (_("Suggest automatic display of attachment"));
		gtk_widget_show (showinline);
		gtk_box_pack_end (GTK_BOX (GTK_FILE_SELECTION (selection)->main_vbox), showinline, FALSE, FALSE, 4);
		g_object_set_data((GObject *)selection, "show-inline", showinline);
	}
#endif
	
	gtk_window_set_transient_for ((GtkWindow *) selection, (GtkWindow *) composer);
	gtk_window_set_wmclass ((GtkWindow *) selection, "fileselection", "Evolution:composer");
	gtk_window_set_modal ((GtkWindow *) selection, FALSE);
	
	icon_list = e_icon_factory_get_icon_list ("stock_mail-compose");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (selection), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}
	
	return selection;
}

static void
select_file_response(GtkWidget *selector, guint response, struct _EMsgComposer *composer)
{
	if (response == GTK_RESPONSE_OK) {
		const char *name;
		char *path;
		EMsgComposerSelectFileFunc func = g_object_get_data((GObject *)selector, "callback");

#ifdef USE_GTKFILECHOOSER
		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selector));
		path = g_path_get_dirname (gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selector)));
#else
		name = gtk_file_selection_get_filename (GTK_FILE_SELECTION (selector));
		path = g_path_get_dirname (gtk_file_selection_get_filename (GTK_FILE_SELECTION (selector)));
#endif
		g_object_set_data_full ((GObject *) composer, "attach_path", path, g_free);

		func(composer, name);
	}

	gtk_widget_destroy(selector);
}

/**
 * e_msg_composer_select_file:
 * @composer: a composer
 * @w: widget pointer, so same dialog is not re-shown
 * @func: callback invoked if the user selected a file
 * @title: the title for the file selection dialog box
 * @save: whether the file selection box should be shown in save mode or not
 *
 * This pops up a file selection dialog box with the given title
 * and allows the user to select a single file.
 *
 **/
void e_msg_composer_select_file(struct _EMsgComposer *composer, GtkWidget **w, EMsgComposerSelectFileFunc func, const char *title, int save)
{
	if (*w) {
		gtk_window_present((GtkWindow *)*w);			
		return;
	}

	*w = get_selector (composer, title, save ? SELECTOR_MODE_SAVE : 0);
	g_signal_connect(*w, "response", G_CALLBACK(select_file_response), composer);
	g_signal_connect(*w, "destroy", G_CALLBACK(gtk_widget_destroyed), w);
	g_object_set_data((GObject *)*w, "callback", func);
	gtk_widget_show(*w);
}


static void
select_attach_response(GtkWidget *selector, guint response, struct _EMsgComposer *composer)
{
	if (response == GTK_RESPONSE_OK) {
		GSList *names;
		EMsgComposerSelectAttachFunc func = g_object_get_data((GObject *)selector, "callback");
		GtkToggleButton *showinline = g_object_get_data((GObject *)selector, "show-inline");
		char *path;

#ifdef USE_GTKFILECHOOSER
		char *filename;
		names = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (selector));
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selector));
		path = g_path_get_dirname (filename);
		g_free (filename);
#else
		char **files;
		int i;
		
		names = NULL;
		if ((files = gtk_file_selection_get_selections (GTK_FILE_SELECTION (selector)))) {
			for (i = 0; files[i]; i++)
				names = g_slist_prepend(names, files[i]);
			
			g_free (files);
			names = g_slist_reverse(names);
		}

		path = g_path_get_dirname (gtk_file_selection_get_filename (GTK_FILE_SELECTION (selector)));		
#endif
		g_object_set_data_full ((GObject *) composer, "attach_path", path, g_free);

		func(composer, names, gtk_toggle_button_get_active(showinline));
		
		e_msg_composer_show_attachments_ui (composer);


		g_slist_foreach(names, (GFunc)g_free, NULL);
		g_slist_free(names);
	}

	gtk_widget_destroy(selector);
}

void e_msg_composer_select_file_attachments(struct _EMsgComposer *composer, GtkWidget **w, EMsgComposerSelectAttachFunc func)
{
	if (*w) {
		gtk_window_present((GtkWindow *)*w);			
		return;
	}

	*w = get_selector (composer, _("Insert Attachment"), SELECTOR_MODE_MULTI|SELECTOR_SHOW_INLINE);
	g_signal_connect(*w, "response", G_CALLBACK(select_attach_response), composer);
	g_signal_connect(*w, "destroy", G_CALLBACK(gtk_widget_destroyed), w);
	g_object_set_data((GObject *)*w, "callback", func);
	gtk_widget_show(*w);
}
