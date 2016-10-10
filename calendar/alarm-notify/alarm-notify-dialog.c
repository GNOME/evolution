/*
 * Evolution calendar - alarm notification dialog
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"

#include "alarm-notify-dialog.h"
#include "config-data.h"
#include "util.h"

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
	GtkBuilder *builder;

	GtkWidget *dialog;
	GtkWidget *snooze_time_min;
	GtkWidget *snooze_time_hrs;
	GtkWidget *snooze_time_days;
	GtkWidget *snooze_btn;
	GtkWidget *edit_btn;
	GtkWidget *print_btn;
	GtkWidget *dismiss_btn;
	GtkWidget *minutes_label;
	GtkWidget *hrs_label;
	GtkWidget *days_label;
	GtkWidget *description;
	GtkWidget *location;
	GtkWidget *treeview;

	AlarmFuncInfo *cur_funcinfo;

} AlarmNotify;

static void	tree_selection_changed_cb	(GtkTreeSelection *selection,
						 gpointer data);
static void	fill_in_labels			(AlarmNotify *an,
						 const gchar *summary,
						 const gchar *description,
						 const gchar *location,
						 time_t occur_start,
						 time_t occur_end);

static void	edit_pressed_cb			(GtkButton *button,
						 gpointer user_data);
static void	snooze_pressed_cb		(GtkButton *button,
						 gpointer user_data);

static void
an_update_minutes_label (GtkSpinButton *sb,
                         gpointer data)
{
	AlarmNotify *an;
	gint snooze_timeout_min;

	an = (AlarmNotify *) data;

	snooze_timeout_min = gtk_spin_button_get_value_as_int (sb);
	gtk_label_set_text (GTK_LABEL (an->minutes_label), ngettext ("minute", "minutes", snooze_timeout_min));
}

static void
an_update_hrs_label (GtkSpinButton *sb,
                     gpointer data)
{
	AlarmNotify *an;
	gint snooze_timeout_hrs;

	an = (AlarmNotify *) data;

	snooze_timeout_hrs = gtk_spin_button_get_value_as_int (sb);
	gtk_label_set_text (GTK_LABEL (an->hrs_label), ngettext ("hour", "hours", snooze_timeout_hrs));
}

static void
an_update_days_label (GtkSpinButton *sb,
                      gpointer data)
{
	AlarmNotify *an;
	gint snooze_timeout_days;

	an = (AlarmNotify *) data;

	snooze_timeout_days = gtk_spin_button_get_value_as_int (sb);
	gtk_label_set_text (GTK_LABEL (an->days_label), ngettext ("day", "days", snooze_timeout_days));
}

static void
dialog_response_cb (GtkDialog *dialog,
                    guint response_id,
                    gpointer user_data)
{
	AlarmNotify *an = user_data;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	AlarmFuncInfo *funcinfo = NULL;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (an->treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, ALARM_FUNCINFO_COLUMN, &funcinfo, -1);
	}

	if (!funcinfo) {
		GtkTreeModel *treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW (an->treeview));
		if (!gtk_tree_model_get_iter_first (treemodel, &iter))
			return;

		gtk_tree_model_get (treemodel, &iter, ALARM_FUNCINFO_COLUMN, &funcinfo, -1);
	}

	if (!funcinfo) {
		g_warn_if_reached ();
		return;
	}

	switch (response_id) {
	case GTK_RESPONSE_CLOSE:
	case GTK_RESPONSE_DELETE_EVENT:
		(* funcinfo->func) (ALARM_NOTIFY_CLOSE, -1, funcinfo->func_data);
		break;
	}
}

static void
edit_pressed_cb (GtkButton *button,
                 gpointer user_data)
{
	AlarmNotify *an = user_data;
	AlarmFuncInfo *funcinfo = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (an->treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (model, &iter, ALARM_FUNCINFO_COLUMN, &funcinfo, -1);

	if (!funcinfo) {
		g_warn_if_reached ();
		return;
	}

	(* funcinfo->func) (ALARM_NOTIFY_EDIT, -1, funcinfo->func_data);
}

static void
print_pressed_cb (GtkButton *button,
                 gpointer user_data)
{
	AlarmNotify *an = user_data;
	AlarmFuncInfo *funcinfo = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (an->treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (model, &iter, ALARM_FUNCINFO_COLUMN, &funcinfo, -1);

	if (!funcinfo) {
		g_warn_if_reached ();
		return;
	}

	(* funcinfo->func) (ALARM_NOTIFY_PRINT, -1, funcinfo->func_data);
}

static void
snooze_pressed_cb (GtkButton *button,
                   gpointer user_data)
{
	gint snooze_timeout;
	AlarmNotify *an = user_data;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	AlarmFuncInfo *funcinfo = NULL;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (an->treeview));

	gtk_widget_grab_focus ((GtkWidget *) button);

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (model, &iter, ALARM_FUNCINFO_COLUMN, &funcinfo, -1);

	if (!funcinfo) {
		g_warn_if_reached ();
		return;
	}

	snooze_timeout = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (an->snooze_time_min));
	snooze_timeout += 60 * (gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (an->snooze_time_hrs)));
	snooze_timeout += 60 * 24 * (gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (an->snooze_time_days)));
	if (!snooze_timeout)
		snooze_timeout = config_data_get_default_snooze_minutes ();
	(* funcinfo->func) (ALARM_NOTIFY_SNOOZE, snooze_timeout, funcinfo->func_data);
}

static void
dismiss_pressed_cb (GtkButton *button,
                    gpointer user_data)
{
	AlarmNotify *an = user_data;
	GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (an->treeview));

	g_return_if_fail (model != NULL);

	if (gtk_tree_model_iter_n_children (model, NULL) <= 1) {
		gtk_dialog_response (GTK_DIALOG (an->dialog), GTK_RESPONSE_CLOSE);
	} else {
		GtkTreeIter iter;
		AlarmFuncInfo *funcinfo = NULL;
		GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (an->treeview));

		if (gtk_tree_selection_get_selected (selection, &model, &iter))
			gtk_tree_model_get (model, &iter, ALARM_FUNCINFO_COLUMN, &funcinfo, -1);

		if (!funcinfo) {
			g_warn_if_reached ();
			return;
		}

		(* funcinfo->func) (ALARM_NOTIFY_DISMISS, -1, funcinfo->func_data);
	}
}

static void
dialog_destroyed_cb (GtkWidget *dialog,
                     gpointer user_data)
{
	AlarmNotify *an = user_data;

	g_object_unref (an->builder);
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
	GtkWidget *container;
	GtkWidget *image;
	gint snooze_minutes;
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

			G_TYPE_POINTER /* FuncInfo */));

	an->builder = gtk_builder_new ();
	e_load_ui_builder_definition (an->builder, "alarm-notify.ui");

	an->dialog = e_builder_get_widget (an->builder, "alarm-notify");
	an->snooze_time_min = e_builder_get_widget (an->builder, "snooze-time-min");
	an->minutes_label = e_builder_get_widget (an->builder, "minutes-label");
	an->snooze_time_hrs = e_builder_get_widget (an->builder, "snooze-time-hrs");
	an->hrs_label = e_builder_get_widget (an->builder, "hrs-label");
	an->snooze_time_days = e_builder_get_widget (an->builder, "snooze-time-days");
	an->days_label = e_builder_get_widget (an->builder, "days-label");
	an->description = e_builder_get_widget (an->builder, "description-label");
	an->location = e_builder_get_widget (an->builder, "location-label");
	an->treeview = e_builder_get_widget (an->builder, "appointments-treeview");
	an->snooze_btn = e_builder_get_widget (an->builder, "snooze-button");
	an->edit_btn = e_builder_get_widget (an->builder, "edit-button");
	an->print_btn = e_builder_get_widget (an->builder, "print-button");
	an->dismiss_btn = e_builder_get_widget (an->builder, "dismiss-button");

	if (!(an->dialog && an->treeview
	      && an->snooze_time_min && an->snooze_time_hrs && an->snooze_time_days
	      && an->description && an->location && an->edit_btn && an->print_btn && an->snooze_btn && an->dismiss_btn)) {
		g_warning ("alarm_notify_dialog(): Could not find all widgets in alarm-notify.ui file!");
		g_object_unref (an->builder);
		g_free (an);
		return NULL;
	}

	snooze_minutes = config_data_get_default_snooze_minutes ();
	if (snooze_minutes > 0) {
		gint value;

		value = snooze_minutes / (60 * 24);
		snooze_minutes -= 60 * 24 * value;

		gtk_spin_button_set_value (GTK_SPIN_BUTTON (an->snooze_time_days), value);

		value = snooze_minutes / 60;
		snooze_minutes -= 60 * value;

		gtk_spin_button_set_value (GTK_SPIN_BUTTON (an->snooze_time_hrs), value);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (an->snooze_time_min), snooze_minutes);
	}

	e_buffer_tagger_connect (GTK_TEXT_VIEW (an->description));

	gtk_tree_view_set_model (GTK_TREE_VIEW (an->treeview), model);

	gtk_window_set_keep_above (GTK_WINDOW (an->dialog), TRUE);

	column = gtk_tree_view_column_new_with_attributes (
		_("Start time"),
		renderer, "text", ALARM_DISPLAY_COLUMN, NULL);

	gtk_tree_view_column_set_attributes (
		column, renderer,
		"markup", ALARM_DISPLAY_COLUMN, NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (an->treeview), column);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (an->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (tree_selection_changed_cb), an);
	tree_selection_changed_cb (selection, an);

	gtk_widget_realize (an->dialog);

	container = gtk_dialog_get_action_area (GTK_DIALOG (an->dialog));
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);

	container = gtk_dialog_get_content_area (GTK_DIALOG (an->dialog));
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	image = e_builder_get_widget (an->builder, "alarm-image");
	gtk_image_set_from_icon_name (
		GTK_IMAGE (image), "stock_alarm", GTK_ICON_SIZE_DIALOG);

	g_signal_connect (
		an->edit_btn, "clicked",
		G_CALLBACK (edit_pressed_cb), an);
	g_signal_connect (
		an->print_btn, "clicked",
		G_CALLBACK (print_pressed_cb), an);
	g_signal_connect (
		an->snooze_btn, "clicked",
		G_CALLBACK (snooze_pressed_cb), an);
	g_signal_connect (
		an->dismiss_btn, "clicked",
		G_CALLBACK (dismiss_pressed_cb), an);
	g_signal_connect (
		an->dialog, "response",
		G_CALLBACK (dialog_response_cb), an);
	g_signal_connect (
		an->dialog, "destroy",
		G_CALLBACK (dialog_destroyed_cb), an);

	if (!gtk_widget_get_realized (an->dialog))
		gtk_widget_realize (an->dialog);

	gtk_window_set_icon_name (GTK_WINDOW (an->dialog), "stock_alarm");

	/* Set callback for updating the snooze "minutes" label */
	g_signal_connect (
		an->snooze_time_min, "value_changed",
		G_CALLBACK (an_update_minutes_label), an);

	/* Set callback for updating the snooze "hours" label */
	g_signal_connect (
		an->snooze_time_hrs, "value_changed",
		G_CALLBACK (an_update_hrs_label), an);

	/* Set callback for updating the snooze "days" label */
	g_signal_connect (
		an->snooze_time_days, "value_changed",
		G_CALLBACK (an_update_days_label), an);

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
 * @comp: The #ECalComponent which corresponds to the alarm.
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
add_alarm_to_notified_alarms_dialog (AlarmNotificationsDialog *na,
                                     time_t trigger,
                                     time_t occur_start,
                                     time_t occur_end,
                                     ECalComponent *comp,
                                     const gchar *summary,
                                     const gchar *description,
                                     const gchar *location,
                                     AlarmNotifyFunc func,
                                     gpointer func_data)
{
	GtkTreeIter iter = { 0 };
	GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (na->treeview));
	AlarmFuncInfo *funcinfo = NULL;
	ECalComponentVType vtype;
	gchar *to_display = NULL, *start, *str_time;
	icaltimezone *current_zone;

	/* Iter is not yet defined but still we return it in all the g_return_val_if_fail() calls? */
	g_return_val_if_fail (trigger != -1, iter);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), iter);

	vtype = e_cal_component_get_vtype (comp);

	/* Only VEVENTs or VTODOs can have alarms */
	g_return_val_if_fail (vtype == E_CAL_COMPONENT_EVENT || vtype == E_CAL_COMPONENT_TODO, iter);
	g_return_val_if_fail (summary != NULL, iter);
	g_return_val_if_fail (description != NULL, iter);
	g_return_val_if_fail (location != NULL, iter);
	g_return_val_if_fail (func != NULL, iter);

	funcinfo = g_new0 (AlarmFuncInfo, 1);
	funcinfo->func = func;
	funcinfo->func_data = func_data;

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	current_zone = config_data_get_timezone ();
	start = timet_to_str_with_zone (occur_start, current_zone, datetime_is_date_only (comp, DATETIME_CHECK_DTSTART));
	str_time = calculate_time (occur_start, occur_end);
	to_display = g_markup_printf_escaped (
		"<big><b>%s</b></big>\n%s %s",
		summary, start, str_time);
	g_free (start);
	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		ALARM_DISPLAY_COLUMN, to_display, -1);
	g_free (to_display);
	g_free (str_time);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, ALARM_SUMMARY_COLUMN, summary, -1);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, ALARM_DESCRIPTION_COLUMN, description, -1);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, ALARM_LOCATION_COLUMN, location, -1);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, ALARM_START_COLUMN, occur_start, -1);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, ALARM_END_COLUMN, occur_end, -1);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, ALARM_FUNCINFO_COLUMN, funcinfo, -1);

	return iter;
}

static void
tree_selection_changed_cb (GtkTreeSelection *selection,
                           gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	AlarmNotify *an = user_data;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gchar *summary, *description, *location;
		time_t occur_start, occur_end;

		gtk_widget_set_sensitive (an->snooze_btn, TRUE);
		gtk_widget_set_sensitive (an->edit_btn, TRUE);
		gtk_widget_set_sensitive (an->print_btn, TRUE);
		gtk_widget_set_sensitive (an->dismiss_btn, TRUE);
		gtk_tree_model_get (model, &iter, ALARM_SUMMARY_COLUMN, &summary, -1);
		gtk_tree_model_get (model, &iter, ALARM_DESCRIPTION_COLUMN, &description, -1);
		gtk_tree_model_get (model, &iter, ALARM_LOCATION_COLUMN, &location, -1);
		gtk_tree_model_get (model, &iter, ALARM_START_COLUMN, &occur_start, -1);
		gtk_tree_model_get (model, &iter, ALARM_END_COLUMN, &occur_end, -1);\
		gtk_tree_model_get (model, &iter, ALARM_FUNCINFO_COLUMN, &an->cur_funcinfo, -1);

		fill_in_labels (an, summary, description, location, occur_start, occur_end);

		g_free (summary);
		g_free (description);
		g_free (location);
	} else {
		gtk_widget_set_sensitive (an->snooze_btn, FALSE);
		gtk_widget_set_sensitive (an->edit_btn, FALSE);
		gtk_widget_set_sensitive (an->print_btn, FALSE);
		gtk_widget_set_sensitive (an->dismiss_btn, FALSE);

		fill_in_labels (an, "", "", "", -1, -1);
	}
}

static void
fill_in_labels (AlarmNotify *an,
                const gchar *summary,
                const gchar *description,
                const gchar *location,
                time_t occur_start,
                time_t occur_end)
{
	GtkTextTagTable *table = gtk_text_tag_table_new ();
	GtkTextBuffer *buffer = gtk_text_buffer_new (table);
	gtk_text_buffer_set_text (buffer, description, -1);
	e_buffer_tagger_disconnect (GTK_TEXT_VIEW (an->description));
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (an->description), buffer);
	gtk_label_set_text (GTK_LABEL (an->location), location);
	e_buffer_tagger_connect (GTK_TEXT_VIEW (an->description));
	e_buffer_tagger_update_tags (GTK_TEXT_VIEW (an->description));
	g_object_unref (table);
	g_object_unref (buffer);
}
