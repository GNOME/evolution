/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Rodrigo Moya <rodrigo@novell.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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

/* This is prototype code only, this may, or may not, use undocumented
 * unstable or private internal function calls. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#ifdef USE_GTKFILECHOOSER
#  include <gtk/gtkfilechooser.h>
#  include <gtk/gtkfilechooserdialog.h>
#else
#  include <gtk/gtkfilesel.h>
#endif
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtk.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <calendar/gui/e-cal-popup.h>
#include <libgnomevfs/gnome-vfs.h>
#include <string.h>

#include "format-handler.h"

enum {  /* GtkComboBox enum */
	DEST_NAME_COLUMN,
	DEST_HANDLER,
	N_DEST_COLUMNS

};

void org_gnome_save_calendar (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_save_tasks (EPlugin *ep, ECalPopupTargetSource *target);


static void 
extra_widget_foreach_hide (GtkWidget *widget, gpointer data)
{
	if (widget != data)
		gtk_widget_hide (widget);
}

static void 
on_type_combobox_changed (GtkComboBox *combobox, gpointer data)
{
	FormatHandler *handler = NULL;
	GtkWidget *extra_widget = data;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_combo_box_get_model (combobox);

	gtk_container_foreach (GTK_CONTAINER (extra_widget), 
		extra_widget_foreach_hide, combobox);

	gtk_combo_box_get_active_iter (combobox, &iter);

	gtk_tree_model_get (model, &iter, 
		DEST_HANDLER, &handler, -1);


	if (handler->options_widget)
	{
		gtk_widget_show (handler->options_widget);
	} 

}

static void 
format_handlers_foreach_free (gpointer data, gpointer user_data)
{
	FormatHandler *handler = data;

	if (handler->options_widget)
		gtk_widget_destroy (handler->options_widget);

	if (handler->data)
		g_free (handler->data);

	g_free (data);
}

static void 
ask_destination_and_save (EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type)
{
	FormatHandler *handler = NULL;

	GtkWidget *extra_widget = gtk_vbox_new (FALSE, 0);
	GtkComboBox *combo = GTK_COMBO_BOX(gtk_combo_box_new ());
	GtkTreeModel *model = GTK_TREE_MODEL (gtk_list_store_new 
		(N_DEST_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER));
	GtkCellRenderer *renderer=NULL;
	GtkListStore *store = GTK_LIST_STORE (model);
	GtkTreeIter iter;
	GtkWidget *dialog = NULL;
	char *dest_uri = NULL;
	gboolean proceed = FALSE;

	GList *format_handlers = NULL;

	/* The available formathandlers */
	format_handlers = g_list_append (format_handlers, 
		ical_format_handler_new ());
	format_handlers = g_list_append (format_handlers, 
		csv_format_handler_new ());
	format_handlers = g_list_append (format_handlers, 
		rdf_format_handler_new ());


	/* The Type GtkComboBox */
	gtk_box_pack_start (GTK_BOX (extra_widget), GTK_WIDGET (combo), 
		TRUE, TRUE, 0);
	gtk_combo_box_set_model (combo, model);

	gtk_list_store_clear (store);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), 
			renderer, "text", DEST_NAME_COLUMN, NULL);

	while (format_handlers) {
		FormatHandler *handler = format_handlers->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, DEST_NAME_COLUMN, 
			handler->combo_label, -1);
		gtk_list_store_set (store, &iter, DEST_HANDLER, handler, -1);

		if (handler->options_widget) {
			gtk_box_pack_start (GTK_BOX (extra_widget), 
				GTK_WIDGET (handler->options_widget), TRUE, TRUE, 0);
			gtk_widget_hide (handler->options_widget);
		}

		if (handler->isdefault) {
			gtk_combo_box_set_active_iter (combo, &iter);
			if (handler->options_widget)
				gtk_widget_show (handler->options_widget);
		}

		format_handlers = g_list_next (format_handlers);
	}


	g_signal_connect (G_OBJECT(combo), "changed", 
		G_CALLBACK (on_type_combobox_changed), extra_widget);

#ifdef USE_GTKFILECHOOSER

	dialog = gtk_file_chooser_dialog_new (_("Select destination file"),
					      NULL,
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_SAVE_AS, GTK_RESPONSE_OK,
		 			      NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), extra_widget);
#else
	dialog = gtk_file_selection_new (_("Select destination file"));
	gtk_box_pack_start (GTK_BOX (GTK_FILE_SELECTION (dialog)->main_vbox), extra_widget, FALSE, TRUE, 0);
#endif
	gtk_widget_show (GTK_WIDGET(combo));
	gtk_widget_show (extra_widget);


	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		char *tmp = NULL;

		gtk_combo_box_get_active_iter (combo, &iter);
		gtk_tree_model_get (model, &iter, 
			DEST_HANDLER, &handler, -1);

#ifdef USE_GTKFILECHOOSER
	       dest_uri = gtk_file_chooser_get_uri 
			(GTK_FILE_CHOOSER (dialog));
#else
	       dest_uri = g_strdup (gtk_file_selection_get_filename 
			(GTK_FILE_SELECTION (dialog)));
#endif

		tmp = strstr (dest_uri, handler->filename_ext);

		if (tmp && *(tmp + strlen (handler->filename_ext)) == '\0') {

			proceed = TRUE;

		} else {

			GtkWidget *warning = 
				gtk_message_dialog_new (NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_YES_NO,
				_("The suggested filename extension of this filetype (%s)"
				  " is unused in the chosen filename. Do you want to "
				  "continue?"), handler->filename_ext);

			if (gtk_dialog_run (GTK_DIALOG (warning)) == GTK_RESPONSE_YES)
				proceed = TRUE;

			gtk_widget_destroy (warning);

		}

		if (proceed) {
			handler->save (handler, ep, target, type, dest_uri);
			/* Free the handlers */
			g_list_foreach (format_handlers, format_handlers_foreach_free, NULL);
			g_list_free (format_handlers);

			/* Now we can destroy it */
			gtk_widget_destroy (dialog);	
			g_free (dest_uri);
		}

	} else {
		/* Free the handlers */
		g_list_foreach (format_handlers, format_handlers_foreach_free, NULL);
		g_list_free (format_handlers);

		/* Now we can destroy it */
		gtk_widget_destroy (dialog);	
		g_free (dest_uri);
	}
}

void
org_gnome_save_calendar (EPlugin *ep, ECalPopupTargetSource *target)
{
	ask_destination_and_save (ep, target, E_CAL_SOURCE_TYPE_EVENT);
}

void
org_gnome_save_tasks (EPlugin *ep, ECalPopupTargetSource *target)
{
	ask_destination_and_save (ep, target, E_CAL_SOURCE_TYPE_TODO);
}
