/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Alarm page of the calendar component dialogs
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *          Hans Petter Jansson <hpj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkoptionmenu.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-time-utils.h"
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-icon-factory.h"
#include "alarm-dialog.h"
#include "alarm-list-dialog.h"



typedef struct {
	/* Glade XML data */
	GladeXML *xml;

	/* The client */
	ECal *ecal;

	/* The list store */
	EAlarmList *list_store;
	
	/* Toplevel */
	GtkWidget *toplevel;

	GtkWidget *list;
	GtkWidget *add;
	GtkWidget *delete;
} Dialog;

/* Gets the widgets from the XML file and returns TRUE if they are all available. */
static gboolean
get_widgets (Dialog *dialog)
{
#define GW(name) glade_xml_get_widget (dialog->xml, name)

	dialog->toplevel = GW ("alarm-list-dialog");
	if (!dialog->toplevel)
		return FALSE;

	dialog->list = GW ("list");
	dialog->add = GW ("add"); 
	dialog->delete = GW ("delete");

#undef GW

	return (dialog->list
		&& dialog->add
		&& dialog->delete);
}

static void
sensitize_buttons (Dialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gboolean have_selected, read_only = FALSE;
	GError *error = NULL;

	if (!e_cal_is_read_only (dialog->ecal, &read_only, &error)) {
		if (error->code != E_CALENDAR_STATUS_BUSY)
			read_only = TRUE;
		g_error_free (error);
	}

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->list));
	have_selected = gtk_tree_selection_get_selected (selection, NULL, &iter);

	if ((e_cal_get_one_alarm_only (dialog->ecal) && have_selected) || read_only)
		gtk_widget_set_sensitive (dialog->add, FALSE);
	else
		gtk_widget_set_sensitive (dialog->add, TRUE);
	gtk_widget_set_sensitive (dialog->delete, have_selected && !read_only);
}

/* Callback used for the "add reminder" button */
static void
add_clicked_cb (GtkButton *button, gpointer data)
{
	Dialog *dialog = data;
	ECalComponentAlarm *alarm;
	GtkTreeView *view;
	GtkTreeIter iter;
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	
	view = GTK_TREE_VIEW (dialog->list);

	alarm = e_cal_component_alarm_new ();

	icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
	icalprop = icalproperty_new_x ("1");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-NEEDS-DESCRIPTION");
        icalcomponent_add_property (icalcomp, icalprop);
	
	if (alarm_dialog_run (dialog->toplevel, dialog->ecal, alarm)) {
		e_alarm_list_append (dialog->list_store, &iter, alarm);
		gtk_tree_selection_select_iter (gtk_tree_view_get_selection (view), &iter);
	} else {
		e_cal_component_alarm_free (alarm);
	}

	sensitize_buttons (dialog);
}

/* Callback used for the "delete reminder" button */
static void
delete_clicked_cb (GtkButton *button, gpointer data)
{
	Dialog *dialog = data;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean valid_iter;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->list));
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		g_warning ("Could not get a selection to delete.");
		return;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (dialog->list_store), &iter);
	e_alarm_list_remove (dialog->list_store, &iter);

	/* Select closest item after removal */
	valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (dialog->list_store), &iter, path);
	if (!valid_iter) {
		gtk_tree_path_prev (path);
		valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (dialog->list_store), &iter, path);
	}

	if (valid_iter)
		gtk_tree_selection_select_iter (selection, &iter);

	sensitize_buttons (dialog);

	gtk_tree_path_free (path);
}

static void
selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
	Dialog *dialog = data;

	sensitize_buttons (dialog);
}

/* Hooks the widget signals */
static void
init_widgets (Dialog *dialog)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;

	/* View */
	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->list),
				 GTK_TREE_MODEL (dialog->list_store));

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Action/Trigger"));
	cell_renderer = GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", E_ALARM_LIST_COLUMN_DESCRIPTION);
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->list), column);

	/* Reminder buttons */
	g_signal_connect (dialog->add, "clicked",
			  G_CALLBACK (add_clicked_cb), dialog);
	g_signal_connect (dialog->delete, "clicked",
			  G_CALLBACK (delete_clicked_cb), dialog);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->list)), "changed",
			  G_CALLBACK (selection_changed_cb), dialog);
}

gboolean
alarm_list_dialog_run (GtkWidget *parent, ECal *ecal, EAlarmList *list_store)
{
	Dialog dialog;
	int response_id;
	GList *icon_list;
	
	dialog.ecal = ecal;
	dialog.list_store = list_store;	

	dialog.xml = glade_xml_new (EVOLUTION_GLADEDIR "/alarm-list-dialog.glade", NULL, NULL);
	if (!dialog.xml) {
		g_message (G_STRLOC ": Could not load the Glade XML file!");
		return FALSE;
	}

	if (!get_widgets (&dialog)) {
		g_object_unref(dialog.xml);
		return FALSE;
	}

	init_widgets (&dialog);

	sensitize_buttons (&dialog);
	
	icon_list = e_icon_factory_get_icon_list ("stock_calendar");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (dialog.toplevel), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}

	gtk_window_set_transient_for (GTK_WINDOW (dialog.toplevel),
				      GTK_WINDOW (parent));

	response_id = gtk_dialog_run (GTK_DIALOG (dialog.toplevel));
	gtk_widget_hide (dialog.toplevel);

	gtk_widget_destroy (dialog.toplevel);
	g_object_unref (dialog.xml);

	return response_id == GTK_RESPONSE_OK ? TRUE : FALSE;
}
