/*
 * Evolution calendar - Alarm page of the calendar component dialogs
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Miguel de Icaza <miguel@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      JP Rosevear <jpr@ximian.com>
 *      Hans Petter Jansson <hpj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libedataserver/e-time-utils.h>
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include "e-util/e-util.h"
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-util-private.h"
#include "alarm-dialog.h"
#include "alarm-list-dialog.h"



typedef struct {
	GtkBuilder *builder;

	/* The client */
	ECal *ecal;

	/* The list store */
	EAlarmList *list_store;

	/* Toplevel */
	GtkWidget *toplevel;

	GtkWidget *list;
	GtkWidget *add;
	GtkWidget *edit;
	GtkWidget *delete;
	GtkWidget *box;

} Dialog;

/* Gets the widgets from the XML file and returns TRUE if they are all available. */
static gboolean
get_widgets (Dialog *dialog)
{
#define GW(name) e_builder_get_widget (dialog->builder, name)

	dialog->toplevel = GW ("alarm-list-dialog");
	if (!dialog->toplevel)
		return FALSE;

	dialog->box = GW ("vbox53");
	dialog->list = GW ("list");
	dialog->add = GW ("add");
	dialog->edit = GW ("edit");
	dialog->delete = GW ("delete");

#undef GW

	return (dialog->list
		&& dialog->add
		&& dialog->edit
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
	gtk_widget_set_sensitive (dialog->edit, have_selected && !read_only);
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

/* Callback used for the "edit reminder" button */
static void
edit_clicked_cb (GtkButton *button, gpointer data)
{
	Dialog *dialog = data;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	ECalComponentAlarm *alarm;
	GtkTreeView *view;

	view = GTK_TREE_VIEW (dialog->list);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->list));
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		g_warning ("Could not get a selection to edit.");
		return;
	}

	alarm = (ECalComponentAlarm *)e_alarm_list_get_alarm (dialog->list_store, &iter);
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (dialog->list_store), &iter);

	if (alarm_dialog_run (dialog->toplevel, dialog->ecal, alarm)) {
		gtk_tree_selection_select_iter (gtk_tree_view_get_selection (view), &iter);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (dialog->list_store), path, &iter);
	}
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

void
alarm_list_dialog_set_client (GtkWidget *dlg_box, ECal *client)
{
	Dialog *dialog;

	if (!dlg_box) return;

	dialog = g_object_get_data (G_OBJECT (dlg_box), "dialog");
	if (dialog) {
		dialog->ecal = client;
		sensitize_buttons (dialog);
	}
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
	g_signal_connect (dialog->edit, "clicked",
			  G_CALLBACK (edit_clicked_cb), dialog);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->list)), "changed",
			  G_CALLBACK (selection_changed_cb), dialog);
}

gboolean
alarm_list_dialog_run (GtkWidget *parent, ECal *ecal, EAlarmList *list_store)
{
	Dialog dialog;
	GtkWidget *container;
	gint response_id;

	dialog.ecal = ecal;
	dialog.list_store = list_store;

	dialog.builder = gtk_builder_new ();
	e_load_ui_builder_definition (dialog.builder, "alarm-list-dialog.ui");

	if (!get_widgets (&dialog)) {
		g_object_unref(dialog.builder);
		return FALSE;
	}

	init_widgets (&dialog);

	sensitize_buttons (&dialog);

	gtk_widget_ensure_style (dialog.toplevel);

	container = gtk_dialog_get_action_area (GTK_DIALOG (dialog.toplevel));
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog.toplevel));
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	gtk_window_set_icon_name (
		GTK_WINDOW (dialog.toplevel), "x-office-calendar");

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog.toplevel),
		GTK_WINDOW (parent));

	response_id = gtk_dialog_run (GTK_DIALOG (dialog.toplevel));
	gtk_widget_hide (dialog.toplevel);

	gtk_widget_destroy (dialog.toplevel);
	g_object_unref (dialog.builder);

	return response_id == GTK_RESPONSE_OK ? TRUE : FALSE;
}

GtkWidget *
alarm_list_dialog_peek (ECal *ecal, EAlarmList *list_store)
{
	Dialog *dialog;

	dialog = (Dialog *)g_new (Dialog, 1);
	dialog->ecal = ecal;
	dialog->list_store = list_store;

	dialog->builder = gtk_builder_new ();
	e_load_ui_builder_definition (dialog->builder, "alarm-list-dialog.ui");

	if (!get_widgets (dialog)) {
		g_object_unref(dialog->builder);
		return NULL;
	}

	init_widgets (dialog);

	sensitize_buttons (dialog);

	g_object_unref (dialog->builder);

	/* Free the other stuff when the parent really gets destroyed. */
	g_object_set_data_full (G_OBJECT (dialog->box), "toplevel", dialog->toplevel, (GDestroyNotify) gtk_widget_destroy);
	g_object_set_data_full (G_OBJECT (dialog->box), "dialog", dialog, g_free);

	return dialog->box;
}
