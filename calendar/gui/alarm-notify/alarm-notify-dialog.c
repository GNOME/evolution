/* Evolution calendar - alarm notification dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#if 0 
#  include <libgnomeui/gnome-winhints.h>
#endif
#include <glade/glade.h>
#include <libedataserver/e-time-utils.h>
#include <libecal/e-cal-time-util.h>
#include "alarm-notify-dialog.h"
#include "config-data.h"
#include "util.h"
#include <e-util/e-icon-factory.h>


	
enum {
	ALARM_DISPLAY_COLUMN,
	ALARM_SUMMARY_COLUMN,
	ALARM_DESCRIPTION_COLUMN,
	ALARM_LOCATION_COLUMN,

	ALARM_START_COLUMN,
	ALARM_END_COLUMN,
	
	ALARM_FUNCINFO_COLUMN,

	N_ALARM_COLUMNS
};

/* The useful contents of the alarm notify dialog */

typedef struct {
	AlarmNotifyFunc func;
	gpointer func_data;
} AlarmFuncInfo;

typedef struct {
	GladeXML *xml;

	GtkWidget *dialog;
	GtkWidget *snooze_time;
	GtkWidget *minutes_label;
	GtkWidget *description;
	GtkWidget *location;
	GtkWidget *treeview;
	GtkWidget *scrolledwindow;
	
	AlarmFuncInfo *cur_funcinfo;
	
} AlarmNotify;



static void
tree_selection_changed_cb (GtkTreeSelection *selection, gpointer data);

static void
fill_in_labels (AlarmNotify *an, const gchar *summary, const gchar *description, 
			const gchar *location, time_t occur_start, time_t occur_end);
static void 
edit_pressed_cb (GtkButton *button, gpointer user_data);

static void
snooze_pressed_cb (GtkButton *button, gpointer user_data);


AlarmNotify *an = NULL;
gboolean have_one = FALSE;



static void
an_update_minutes_label (GtkSpinButton *sb, gpointer data)
{
	AlarmNotify *an;
	char *new_label;
	int snooze_timeout;

	an = (AlarmNotify *) data;

	snooze_timeout = gtk_spin_button_get_value_as_int (sb);
	new_label = g_strdup (ngettext ("minute", "minutes", snooze_timeout));
	gtk_label_set_text (GTK_LABEL (an->minutes_label), new_label);
	g_free (new_label);
}

static void
dialog_response_cb (GtkDialog *dialog, guint response_id, gpointer user_data)
{
	AlarmNotify *an = user_data;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	AlarmFuncInfo *funcinfo = NULL;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (an->treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (model, &iter, ALARM_FUNCINFO_COLUMN, &funcinfo, -1);	

	g_return_if_fail (funcinfo);

	switch (response_id) {
	case GTK_RESPONSE_CLOSE:
	case GTK_RESPONSE_DELETE_EVENT:
		(* funcinfo->func) (ALARM_NOTIFY_CLOSE, -1, funcinfo->func_data);
		break;
	}

	return;
}

static void 
edit_pressed_cb (GtkButton *button, gpointer user_data)
{
	AlarmNotify *an = user_data;
	AlarmFuncInfo *funcinfo = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (an->treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (model, &iter, ALARM_FUNCINFO_COLUMN, &funcinfo, -1);	

	g_return_if_fail (funcinfo);
	
	(* funcinfo->func) (ALARM_NOTIFY_EDIT, -1, funcinfo->func_data);
}

static void 
snooze_pressed_cb (GtkButton *button, gpointer user_data)
{
	int snooze_timeout;
	AlarmNotify *an = user_data;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	AlarmFuncInfo *funcinfo = NULL;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (an->treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (model, &iter, ALARM_FUNCINFO_COLUMN, &funcinfo, -1);	

	g_return_if_fail (funcinfo);

	snooze_timeout = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (an->snooze_time));
	(* funcinfo->func) (ALARM_NOTIFY_SNOOZE, snooze_timeout, funcinfo->func_data);

}

static void
dialog_destroyed_cb (GtkWidget *dialog, gpointer user_data)
{
	AlarmNotify *an = user_data;

	g_object_unref (an->xml);
	g_free (an);
}

/**
 * notified_alarms_dialog_new:
 *
 * Return value: a new dialog in which you can add alarm notifications
 **/
AlarmNotificationsDialog *
notified_alarms_dialog_new (void)
{
	GtkWidget *edit_btn;
	GtkWidget *snooze_btn;
	GtkWidget *image;
	char *icon_path;
	GList *icon_list;
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	AlarmNotificationsDialog *na = NULL;
	AlarmNotify *an = g_new0 (AlarmNotify, 1);
	GtkTreeViewColumn *column = NULL;
	GtkTreeSelection *selection = NULL;
	GtkTreeModel *model = GTK_TREE_MODEL (gtk_list_store_new (
			N_ALARM_COLUMNS, 
		
			G_TYPE_STRING, /* Display */
			G_TYPE_STRING, /* Summary */
			G_TYPE_STRING, /* Description */
			G_TYPE_STRING, /* Location */
		
			G_TYPE_POINTER, /* Start */
			G_TYPE_POINTER, /* End */
		
			G_TYPE_POINTER /* FuncInfo*/));
	 
	an->xml = glade_xml_new (EVOLUTION_GLADEDIR "/alarm-notify.glade", NULL, NULL);
	if (!an->xml) {
		g_message ("alarm_notify_dialog(): Could not load the Glade XML file!");
		g_free (an);
		return NULL;
	}
	
	an->dialog = glade_xml_get_widget (an->xml, "alarm-notify");
	an->snooze_time = glade_xml_get_widget (an->xml, "snooze-time");
	an->minutes_label = glade_xml_get_widget (an->xml, "minutes-label");
	an->description = glade_xml_get_widget (an->xml, "description-label");
	an->location = glade_xml_get_widget (an->xml, "location-label");
	an->treeview = glade_xml_get_widget (an->xml, "appointments-treeview");
	an->scrolledwindow = glade_xml_get_widget (an->xml, "treeview-scrolledwindow");
	snooze_btn = glade_xml_get_widget (an->xml, "snooze-button");
	edit_btn = glade_xml_get_widget (an->xml, "edit-button");

	if (!(an->dialog && an->scrolledwindow && an->treeview && an->snooze_time
	      && an->description && an->location && edit_btn && snooze_btn)) {
		g_message ("alarm_notify_dialog(): Could not find all widgets in Glade file!");
		g_object_unref (an->xml);
		g_free (an);
		return NULL;
	}

	gtk_tree_view_set_model (GTK_TREE_VIEW(an->treeview), model);

	gtk_window_set_keep_above (GTK_WINDOW (an->dialog), TRUE);

	column = gtk_tree_view_column_new_with_attributes (_("Start time"),
					renderer, "text", ALARM_DISPLAY_COLUMN, NULL);

	gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", ALARM_DISPLAY_COLUMN, NULL);
	
	gtk_tree_view_append_column (GTK_TREE_VIEW (an->treeview), column);
		
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (an->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (selection), "changed",
		G_CALLBACK (tree_selection_changed_cb), an);
	
	gtk_widget_realize (an->dialog);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (an->dialog)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (an->dialog)->action_area), 12);
		image = glade_xml_get_widget (an->xml, "alarm-image");
	icon_path = e_icon_factory_get_icon_filename ("stock_alarm", E_ICON_SIZE_DIALOG);
	gtk_image_set_from_file (GTK_IMAGE (image), icon_path);
	g_free (icon_path);

	g_signal_connect (edit_btn, "pressed", G_CALLBACK (edit_pressed_cb), an);
	g_signal_connect (snooze_btn, "pressed", G_CALLBACK (snooze_pressed_cb), an);
	g_signal_connect (G_OBJECT (an->dialog), "response", G_CALLBACK (dialog_response_cb), an);
	g_signal_connect (G_OBJECT (an->dialog), "destroy", G_CALLBACK (dialog_destroyed_cb), an);
	
	if (!GTK_WIDGET_REALIZED (an->dialog))
	gtk_widget_realize (an->dialog);
		icon_list = e_icon_factory_get_icon_list ("stock_alarm");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (an->dialog), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}
	
	/* Set callback for updating the snooze "minutes" label */
	g_signal_connect (G_OBJECT (an->snooze_time), "value_changed",
	 		G_CALLBACK (an_update_minutes_label), an);
	
	
	na = g_new0 (AlarmNotificationsDialog, 1);

	na->treeview = an->treeview;
	na->dialog = an->dialog;
	
	return na;
}
 
/**
 * add_alarm_to_notified_alarms_dialog:
 * @na: Pointer to the dialog-info
 * @trigger: Trigger time for the alarm.
 * @occur_start: Start of occurrence time for the event.
 * @occur_end: End of occurrence time for the event.
 * @vtype: Type of the component which corresponds to the alarm.
 * @summary: Short summary of the appointment
 * @description: Long description of the appointment
 * @location: Location of the appointment
 * @func: Function to be called when a dialog action is invoked.
 * @func_data: Closure data for @func.
 *
 * The specified @func will be used to notify the client about result of
 * the actions in the dialog.
 *
 * Return value: the iter in the treeview of the dialog
 **/
 
GtkTreeIter 
add_alarm_to_notified_alarms_dialog (AlarmNotificationsDialog *na, time_t trigger, 
				time_t occur_start, time_t occur_end,
				ECalComponentVType vtype, const char *summary,
				const char *description, const char *location,
				AlarmNotifyFunc func, gpointer func_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (na->treeview));
	AlarmFuncInfo *funcinfo = NULL;
	gchar *to_display = NULL, *start, *end;
	icaltimezone *current_zone;
	
	g_return_val_if_fail (trigger != -1, iter);

	/* Only VEVENTs or VTODOs can have alarms */
	g_return_val_if_fail (vtype == E_CAL_COMPONENT_EVENT || vtype == E_CAL_COMPONENT_TODO, iter);
	g_return_val_if_fail (summary != NULL, iter);
	g_return_val_if_fail (description != NULL, iter);
	g_return_val_if_fail (location != NULL, iter);
	g_return_val_if_fail (func != NULL, iter);

	funcinfo = g_new0 (AlarmFuncInfo, 1);
	funcinfo->func = func;
	funcinfo->func_data = func_data;
	
	gtk_list_store_append (GTK_LIST_STORE(model), &iter);

	current_zone = config_data_get_timezone ();
	start = timet_to_str_with_zone (occur_start, current_zone);
	end = timet_to_str_with_zone (occur_end, current_zone);
	to_display = g_strdup_printf (_("<big><b>%s</b></big>\n%s until %s"), 
		summary, start, end);
	g_free (start);
	g_free (end);
	gtk_list_store_set (GTK_LIST_STORE(model), &iter, 
		ALARM_DISPLAY_COLUMN, to_display, -1);
	g_free (to_display);
	
	gtk_list_store_set (GTK_LIST_STORE(model), &iter, ALARM_SUMMARY_COLUMN, summary, -1);
	gtk_list_store_set (GTK_LIST_STORE(model), &iter, ALARM_DESCRIPTION_COLUMN, description, -1);
	gtk_list_store_set (GTK_LIST_STORE(model), &iter, ALARM_LOCATION_COLUMN, location, -1);
	gtk_list_store_set (GTK_LIST_STORE(model), &iter, ALARM_START_COLUMN, occur_start, -1);
	gtk_list_store_set (GTK_LIST_STORE(model), &iter, ALARM_END_COLUMN, occur_end, -1);
	gtk_list_store_set (GTK_LIST_STORE(model), &iter, ALARM_FUNCINFO_COLUMN, funcinfo, -1);
	
	return iter;
}

static void
tree_selection_changed_cb (GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		AlarmNotify *an = user_data;
		gchar *summary, *description, *location;
		time_t occur_start, occur_end;
		
		gtk_tree_model_get (model, &iter, ALARM_SUMMARY_COLUMN, &summary, -1);
		gtk_tree_model_get (model, &iter, ALARM_DESCRIPTION_COLUMN, &description, -1);
		gtk_tree_model_get (model, &iter, ALARM_LOCATION_COLUMN, &location, -1);
		gtk_tree_model_get (model, &iter, ALARM_START_COLUMN, &occur_start, -1);
		gtk_tree_model_get (model, &iter, ALARM_END_COLUMN, &occur_end, -1);\
		gtk_tree_model_get (model, &iter, ALARM_FUNCINFO_COLUMN, &an->cur_funcinfo, -1);
		
		fill_in_labels (an, summary, description, location, occur_start, occur_end);
	}
}



static void 
fill_in_labels (AlarmNotify *an, const gchar *summary, const gchar *description, 
		const gchar *location, time_t occur_start, time_t occur_end)
{
	GtkTextTagTable *table = gtk_text_tag_table_new ();
	GtkTextBuffer *buffer =  gtk_text_buffer_new (table);
	gtk_text_buffer_set_text (buffer, description, -1);
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (an->description), buffer);
	gtk_label_set_text (GTK_LABEL (an->location), location);
	g_object_unref (table);
	g_object_unref (buffer);
}
