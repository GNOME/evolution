/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright 2000, 2001, 2002 Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * CalPrefsDialog - a GtkObject which handles a libglade-loaded dialog
 * to edit the calendar preference settings.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../e-timezone-entry.h"
#include "cal-prefs-dialog.h"
#include "../calendar-config.h"
#include "url-editor-dialog.h"

#include <gtk/gtk.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktogglebutton.h>
#include <libxml/tree.h>
#include <string.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-color-picker.h>
#include <glade/glade.h>
#include <gal/util/e-util.h>
#include <e-util/e-dialog-widgets.h>
#include <widgets/misc/e-dateedit.h>


static const int week_start_day_map[] = {
	1, 2, 3, 4, 5, 6, 0, -1
};

static const int time_division_map[] = {
	60, 30, 15, 10, 5, -1
};

/* The following two are kept separate in case we need to re-order each menu individually */
static const int hide_completed_units_map[] = {
	CAL_MINUTES, CAL_HOURS, CAL_DAYS, -1
};

static const int default_reminder_units_map[] = {
	CAL_MINUTES, CAL_HOURS, CAL_DAYS, -1
};

static gboolean get_widgets (DialogData *data);

static void setup_changes (DialogData *data);

static void init_widgets (DialogData *data);
static void show_config (DialogData *data);

static void config_control_destroy_callback (DialogData *dialog_data, GObject *deadbeef);

static void cal_prefs_dialog_url_add_clicked (GtkWidget *button, DialogData *dialog_data);
static void cal_prefs_dialog_url_edit_clicked (GtkWidget *button, DialogData *dialog_data);
static void cal_prefs_dialog_url_remove_clicked (GtkWidget *button, DialogData *dialog_data);
static void cal_prefs_dialog_url_enable_clicked (GtkWidget *button, DialogData *dialog_data);
static void cal_prefs_dialog_url_list_change (GtkTreeSelection *selection, DialogData *dialog_data);
static void cal_prefs_dialog_url_list_enable_toggled (GtkCellRendererToggle *renderer, const char *path_string, DialogData *dialog_data);
static void cal_prefs_dialog_url_list_double_click(GtkTreeView *treeview, 
						   GtkTreePath *path, 
						   GtkTreeViewColumn *column, 
						   DialogData *dialog_data);
static void show_fb_config (DialogData *dialog_data);

GtkWidget *cal_prefs_dialog_create_time_edit (void);

#define PREFS_WINDOW(dialog_data) GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (dialog_data), GTK_TYPE_WINDOW))

/**
 * cal_prefs_dialog_new:
 *
 * Creates a new #CalPrefsDialog.
 *
 * Return value: a new #CalPrefsDialog.
 **/
EvolutionConfigControl *
cal_prefs_dialog_new (void)
{
	DialogData *dialog_data;
	EvolutionConfigControl *config_control;

	dialog_data = g_new0 (DialogData, 1);

	/* Load the content widgets */

	dialog_data->xml = glade_xml_new (EVOLUTION_GLADEDIR "/cal-prefs-dialog.glade", NULL, NULL);
	if (!dialog_data->xml) {
		g_message ("cal_prefs_dialog_construct(): Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (dialog_data)) {
		g_message ("cal_prefs_dialog_construct(): Could not find all widgets in the XML file!");
		return NULL;
	}

	init_widgets (dialog_data);
	show_config (dialog_data);

	gtk_widget_ref (dialog_data->page);
	gtk_container_remove (GTK_CONTAINER (dialog_data->page->parent), dialog_data->page);
	config_control = evolution_config_control_new (dialog_data->page);
	gtk_widget_unref (dialog_data->page);
	
	g_object_weak_ref ((GObject *) config_control, (GWeakNotify) config_control_destroy_callback, dialog_data);
	
	setup_changes (dialog_data);

	return config_control;
}

/* Returns a pointer to a static string with an X color spec for the current
 * value of a color picker.
 */
static const char *
spec_from_picker (GtkWidget *picker)
{
	static char spec[8];
	guint8 r, g, b;

	gnome_color_picker_get_i8 (GNOME_COLOR_PICKER (picker), &r, &g, &b, NULL);
	g_snprintf (spec, sizeof (spec), "#%02x%02x%02x", r, g, b);

	return spec;
}

static void
working_days_changed (GtkWidget *widget, DialogData *dialog_data)
{
	CalWeekdays working_days = 0;
	guint32 mask = 1;
	int day;
	
	for (day = 0; day < 7; day++) {
		if (e_dialog_toggle_get (dialog_data->working_days[day]))
			working_days |= mask;
		mask <<= 1;
	}
	
	calendar_config_set_working_days (working_days);
}

static void
timezone_changed (GtkWidget *widget, DialogData *dialog_data)
{
	icaltimezone *zone;
	
	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (dialog_data->timezone));
	calendar_config_set_timezone (icaltimezone_get_location (zone));
}

static void
start_of_day_changed (GtkWidget *widget, DialogData *dialog_data)
{
	int start_hour, start_minute, end_hour, end_minute;
	EDateEdit *start, *end;
	
	start = E_DATE_EDIT (dialog_data->start_of_day);
	end = E_DATE_EDIT (dialog_data->end_of_day);
	
	e_date_edit_get_time_of_day (start, &start_hour, &start_minute);
	e_date_edit_get_time_of_day (end, &end_hour, &end_minute);
	
	if ((start_hour > end_hour) || (start_hour == end_hour && start_minute > end_minute)) {
		if (start_hour < 23)
			e_date_edit_set_time_of_day (end, start_hour + 1, start_minute);
		else
			e_date_edit_set_time_of_day (end, 23, 59);
		
		return;
	}
	
	calendar_config_set_day_start_hour (start_hour);
	calendar_config_set_day_start_minute (start_minute);
}

static void
end_of_day_changed (GtkWidget *widget, DialogData *dialog_data)
{
	int start_hour, start_minute, end_hour, end_minute;
	EDateEdit *start, *end;
	
	start = E_DATE_EDIT (dialog_data->start_of_day);
	end = E_DATE_EDIT (dialog_data->end_of_day);
	
	e_date_edit_get_time_of_day (start, &start_hour, &start_minute);
	e_date_edit_get_time_of_day (end, &end_hour, &end_minute);
	
	if ((end_hour < start_hour) || (end_hour == start_hour && end_minute < start_minute)) {
		if (end_hour < 1)
			e_date_edit_set_time_of_day (start, 0, 0);
		else
			e_date_edit_set_time_of_day (start, end_hour - 1, end_minute);
		
		return;
	}
	
	calendar_config_set_day_end_hour (end_hour);
	calendar_config_set_day_end_minute (end_minute);
}

static void
week_start_day_changed (GtkWidget *widget, DialogData *dialog_data)
{
	int week_start_day;
	
	week_start_day = e_dialog_option_menu_get (dialog_data->week_start_day, week_start_day_map);
	calendar_config_set_week_start_day (week_start_day);
}

static void
use_24_hour_toggled (GtkToggleButton *toggle, DialogData *dialog_data)
{
	gboolean use_24_hour;
	
	use_24_hour = gtk_toggle_button_get_active (toggle);
	
	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (dialog_data->start_of_day), use_24_hour);
	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (dialog_data->end_of_day), use_24_hour);
	
	calendar_config_set_24_hour_format (use_24_hour);
}

static void
time_divisions_changed (GtkWidget *widget, DialogData *dialog_data)
{
	int time_divisions;
	
	time_divisions = e_dialog_option_menu_get (dialog_data->time_divisions, time_division_map);
	calendar_config_set_time_divisions (time_divisions);
}

static void
show_end_times_toggled (GtkToggleButton *toggle, DialogData *dialog_data)
{
	calendar_config_set_show_event_end (gtk_toggle_button_get_active (toggle));
}

static void
compress_weekend_toggled (GtkToggleButton *toggle, DialogData *dialog_data)
{
	calendar_config_set_compress_weekend (gtk_toggle_button_get_active (toggle));
}

static void
dnav_show_week_no_toggled (GtkToggleButton *toggle, DialogData *dialog_data)
{
	calendar_config_set_dnav_show_week_no (gtk_toggle_button_get_active (toggle));
}

static void
hide_completed_tasks_toggled (GtkToggleButton *toggle, DialogData *dialog_data)
{
	gboolean hide;
	
	hide = gtk_toggle_button_get_active (toggle);
	
	gtk_widget_set_sensitive (dialog_data->tasks_hide_completed_spinbutton, hide);
	gtk_widget_set_sensitive (dialog_data->tasks_hide_completed_optionmenu, hide);
	
	calendar_config_set_hide_completed_tasks (hide);
}

static void
hide_completed_tasks_changed (GtkWidget *widget, DialogData *dialog_data)
{
	calendar_config_set_hide_completed_tasks_value (e_dialog_spin_get_int (dialog_data->tasks_hide_completed_spinbutton));
}

static void
hide_completed_tasks_units_changed (GtkWidget *widget, DialogData *dialog_data)
{
	calendar_config_set_hide_completed_tasks_units (
		e_dialog_option_menu_get (dialog_data->tasks_hide_completed_optionmenu, hide_completed_units_map));
}

static void
tasks_due_today_set_color (GnomeColorPicker *picker, guint r, guint g, guint b, guint a, DialogData *dialog_data)
{
	calendar_config_set_tasks_due_today_color (spec_from_picker (dialog_data->tasks_due_today_color));
}

static void
tasks_overdue_set_color (GnomeColorPicker *picker, guint r, guint g, guint b, guint a, DialogData *dialog_data)
{
	calendar_config_set_tasks_overdue_color (spec_from_picker (dialog_data->tasks_overdue_color));
}

static void
confirm_delete_toggled (GtkToggleButton *toggle, DialogData *dialog_data)
{
	calendar_config_set_confirm_delete (gtk_toggle_button_get_active (toggle));
}

static void
default_reminder_toggled (GtkToggleButton *toggle, DialogData *dialog_data)
{
	calendar_config_set_use_default_reminder (gtk_toggle_button_get_active (toggle));
}

static void
default_reminder_interval_changed (GtkWidget *widget, DialogData *dialog_data)
{
	calendar_config_set_default_reminder_interval (
		e_dialog_spin_get_int (dialog_data->default_reminder_interval));
}

static void
default_reminder_units_changed (GtkWidget *widget, DialogData *dialog_data)
{
	calendar_config_set_default_reminder_units (
		e_dialog_option_menu_get (dialog_data->default_reminder_units, default_reminder_units_map));
}

static void
url_list_changed (DialogData *dialog_data)
{
	GtkListStore *model = NULL;
	GSList *url_list = NULL;
	GtkTreeIter iter;
	gboolean valid;
	
	url_list = NULL;
	
	model = (GtkListStore *) gtk_tree_view_get_model (dialog_data->url_list);
	
	valid = gtk_tree_model_get_iter_first ((GtkTreeModel *) model, &iter);
	while (valid) {
		EPublishUri *url;
		char *xml;
		
		gtk_tree_model_get ((GtkTreeModel *) model, &iter, 
				    URL_LIST_FREE_BUSY_URL_COLUMN, &url, 
				    -1);
		
		if ((xml = e_pub_uri_to_xml (url)))
			url_list = g_slist_append (url_list, xml);
		
		valid = gtk_tree_model_iter_next ((GtkTreeModel *) model, &iter);
	}
	
	calendar_config_set_free_busy (url_list);
	
	g_slist_free (url_list);
}

static void
setup_changes (DialogData *dialog_data)
{
	int i;
	
	for (i = 0; i < 7; i ++)
		g_signal_connect (dialog_data->working_days[i], "toggled", G_CALLBACK (working_days_changed), dialog_data);
	
	g_signal_connect (dialog_data->timezone, "changed", G_CALLBACK (timezone_changed), dialog_data);
	
	g_signal_connect (dialog_data->start_of_day, "changed", G_CALLBACK (start_of_day_changed), dialog_data);
	g_signal_connect (dialog_data->end_of_day, "changed", G_CALLBACK (end_of_day_changed), dialog_data);
	
	g_signal_connect (GTK_OPTION_MENU (dialog_data->week_start_day)->menu, "selection-done",
			  G_CALLBACK (week_start_day_changed), dialog_data);
	
	g_signal_connect (dialog_data->use_24_hour, "toggled", G_CALLBACK (use_24_hour_toggled), dialog_data);
	
	g_signal_connect (GTK_OPTION_MENU (dialog_data->time_divisions)->menu, "selection-done",
			  G_CALLBACK (time_divisions_changed), dialog_data);
	
	g_signal_connect (dialog_data->show_end_times, "toggled", G_CALLBACK (show_end_times_toggled), dialog_data);
	g_signal_connect (dialog_data->compress_weekend, "toggled", G_CALLBACK (compress_weekend_toggled), dialog_data);
	g_signal_connect (dialog_data->dnav_show_week_no, "toggled", G_CALLBACK (dnav_show_week_no_toggled), dialog_data);
	
	g_signal_connect (dialog_data->tasks_hide_completed_checkbutton, "toggled",
			  G_CALLBACK (hide_completed_tasks_toggled), dialog_data);
	g_signal_connect (dialog_data->tasks_hide_completed_spinbutton, "value-changed",
			  G_CALLBACK (hide_completed_tasks_changed), dialog_data);
	g_signal_connect (GTK_OPTION_MENU (dialog_data->tasks_hide_completed_optionmenu)->menu, "selection-done",
			  G_CALLBACK (hide_completed_tasks_units_changed), dialog_data);
	g_signal_connect (dialog_data->tasks_due_today_color, "color-set",
			  G_CALLBACK (tasks_due_today_set_color), dialog_data);
	g_signal_connect (dialog_data->tasks_overdue_color, "color-set",
			  G_CALLBACK (tasks_overdue_set_color), dialog_data);
	
	g_signal_connect (dialog_data->confirm_delete, "toggled", G_CALLBACK (confirm_delete_toggled), dialog_data);
	g_signal_connect (dialog_data->default_reminder, "toggled", G_CALLBACK (default_reminder_toggled), dialog_data);
	g_signal_connect (dialog_data->default_reminder_interval, "changed",
			  G_CALLBACK (default_reminder_interval_changed), dialog_data);
	g_signal_connect (GTK_OPTION_MENU (dialog_data->default_reminder_units)->menu, "selection-done",
			  G_CALLBACK (default_reminder_units_changed), dialog_data);
}

/* Gets the widgets from the XML file and returns if they are all available.
 */
static gboolean
get_widgets (DialogData *data)
{
#define GW(name) glade_xml_get_widget (data->xml, name)

	data->page = GW ("toplevel-notebook");

	/* The indices must be 0 (Sun) to 6 (Sat). */
	data->working_days[0] = GW ("sun_button");
	data->working_days[1] = GW ("mon_button");
	data->working_days[2] = GW ("tue_button");
	data->working_days[3] = GW ("wed_button");
	data->working_days[4] = GW ("thu_button");
	data->working_days[5] = GW ("fri_button");
	data->working_days[6] = GW ("sat_button");

	data->timezone = GW ("timezone");
	data->week_start_day = GW ("first_day_of_week");
	data->start_of_day = GW ("start_of_day");
	gtk_widget_show (data->start_of_day);
	data->end_of_day = GW ("end_of_day");
	gtk_widget_show (data->end_of_day);
	data->use_12_hour = GW ("use_12_hour");
	data->use_24_hour = GW ("use_24_hour");
	data->time_divisions = GW ("time_divisions");
	data->show_end_times = GW ("show_end_times");
	data->compress_weekend = GW ("compress_weekend");
	data->dnav_show_week_no = GW ("dnav_show_week_no");

	data->tasks_due_today_color = GW ("tasks_due_today_color");
	data->tasks_overdue_color = GW ("tasks_overdue_color");

	data->tasks_hide_completed_checkbutton = GW ("tasks-hide-completed-checkbutton");
	data->tasks_hide_completed_spinbutton = GW ("tasks-hide-completed-spinbutton");
	data->tasks_hide_completed_optionmenu = GW ("tasks-hide-completed-optionmenu");

	data->confirm_delete = GW ("confirm-delete");
	data->default_reminder = GW ("default-reminder");
	data->default_reminder_interval = GW ("default-reminder-interval");
	data->default_reminder_units = GW ("default-reminder-units");
	
	data->url_add = GW ("url_add");
	data->url_edit = GW ("url_edit");
	data->url_remove = GW ("url_remove");
	data->url_enable = GW ("url_enable");
	data->url_list = GTK_TREE_VIEW (GW ("url_list"));

#undef GW

	return (data->page
		&& data->timezone
		&& data->working_days[0]
		&& data->working_days[1]
		&& data->working_days[2]
		&& data->working_days[3]
		&& data->working_days[4]
		&& data->working_days[5]
		&& data->working_days[6]
		&& data->week_start_day
		&& data->start_of_day
		&& data->end_of_day
		&& data->use_12_hour
		&& data->use_24_hour
		&& data->time_divisions
		&& data->show_end_times
		&& data->compress_weekend
		&& data->dnav_show_week_no
		&& data->tasks_due_today_color
		&& data->tasks_overdue_color
		&& data->tasks_hide_completed_checkbutton
		&& data->tasks_hide_completed_spinbutton
		&& data->tasks_hide_completed_optionmenu
		&& data->confirm_delete
		&& data->default_reminder
		&& data->default_reminder_interval
		&& data->default_reminder_units
		&& data->url_add
		&& data->url_edit
		&& data->url_remove
		&& data->url_enable
		&& data->url_list);
}


static void
config_control_destroy_callback (DialogData *dialog_data, GObject *deadbeef)
{
	g_object_unref (dialog_data->xml);
	
	g_free (dialog_data);
}

/* Called by libglade to create our custom EDateEdit widgets. */
GtkWidget *
cal_prefs_dialog_create_time_edit (void)
{
	GtkWidget *dedit;

	dedit = e_date_edit_new ();

	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (dedit), calendar_config_get_24_hour_format ());
	e_date_edit_set_time_popup_range (E_DATE_EDIT (dedit), 0, 24);
	e_date_edit_set_show_date (E_DATE_EDIT (dedit), FALSE);

	return dedit;
}


/* Connects any necessary signal handlers. */
static void
init_widgets (DialogData *dialog_data)
{
	GtkCellRenderer *renderer = NULL;
	GtkTreeSelection *selection;
	GtkListStore *model;
	
	dialog_data->url_editor = FALSE;
	dialog_data->url_editor_dlg =NULL;
	
	/* Free/Busy ... */
	g_signal_connect (dialog_data->url_add, "clicked",
			  G_CALLBACK (cal_prefs_dialog_url_add_clicked),
			  dialog_data);
	
	g_signal_connect (dialog_data->url_edit, "clicked",
			  G_CALLBACK (cal_prefs_dialog_url_edit_clicked),
			  dialog_data);
	
	g_signal_connect (dialog_data->url_remove, "clicked",
			  G_CALLBACK (cal_prefs_dialog_url_remove_clicked),
			  dialog_data);
	
	g_signal_connect (dialog_data->url_enable, "clicked",
			  G_CALLBACK (cal_prefs_dialog_url_enable_clicked),
			  dialog_data);
	
	/* Free/Busy Listview */
	renderer = gtk_cell_renderer_toggle_new();
	g_object_set ((GObject *) renderer, "activatable", TRUE, NULL);
	
	model = gtk_list_store_new (URL_LIST_N_COLUMNS, G_TYPE_BOOLEAN,
				    G_TYPE_STRING, G_TYPE_POINTER);
	
	gtk_tree_view_set_model (dialog_data->url_list, 
				 (GtkTreeModel *) model);

	gtk_tree_view_insert_column_with_attributes (dialog_data->url_list, -1,
						    _("Enabled"), renderer,
						    "active", 
						     URL_LIST_ENABLED_COLUMN, 
						    NULL);

	g_signal_connect (renderer, "toggled", 
			 G_CALLBACK (cal_prefs_dialog_url_list_enable_toggled),
			 dialog_data);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (dialog_data->url_list, -1, 
						    _("Location"), renderer,
						    "text", 
						    URL_LIST_LOCATION_COLUMN, 
						    NULL);
	
	selection = gtk_tree_view_get_selection ((GtkTreeView *) dialog_data->url_list);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_view_set_headers_visible ((GtkTreeView *) dialog_data->url_list, TRUE);
	
	g_signal_connect (dialog_data->url_list, "row-activated",
			 G_CALLBACK (cal_prefs_dialog_url_list_double_click),
			 dialog_data);
}

/* Sets the color in a color picker from an X color spec */
static void
set_color_picker (GtkWidget *picker, const char *spec)
{
	GdkColor color;

	if (!spec || !gdk_color_parse (spec, &color))
		color.red = color.green = color.blue = 0;

	gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (picker),
				    color.red,
				    color.green,
				    color.blue,
				    65535);
}

static void
cal_prefs_dialog_url_add_clicked (GtkWidget *button, DialogData *dialog_data)
{
	EPublishUri *url = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	
	model = gtk_tree_view_get_model (dialog_data->url_list);
	url = g_new0 (EPublishUri, 1);
	url->enabled = TRUE;
	url->location = "";
	
	if (!dialog_data->url_editor) {
		dialog_data->url_editor = url_editor_dialog_new (dialog_data, 
								 url);
		
		if (url->location != "") {
			gtk_list_store_append(GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE(model), &iter, 
					   URL_LIST_ENABLED_COLUMN, 
					   url->enabled,
					   URL_LIST_LOCATION_COLUMN, 
					   g_strdup (url->location),
					   URL_LIST_FREE_BUSY_URL_COLUMN, url,
					   -1);
			
			url_list_changed (dialog_data);
			
			if (!GTK_WIDGET_SENSITIVE ((GtkWidget *) dialog_data->url_remove)) {
				selection = gtk_tree_view_get_selection ((GtkTreeView *) dialog_data->url_list);
				gtk_tree_model_get_iter_first ((GtkTreeModel *) model, &iter);
				gtk_widget_set_sensitive ((GtkWidget*) dialog_data->url_remove, TRUE);
				gtk_tree_selection_select_iter (selection, &iter);
			}	
		}
		dialog_data->url_editor = FALSE;
		dialog_data->url_editor_dlg = NULL;
	} else {
		gdk_window_raise (dialog_data->url_editor_dlg->window);
	}	
}

static void
cal_prefs_dialog_url_edit_clicked (GtkWidget *button, DialogData *dialog_data)
{
	if (!dialog_data->url_editor) {
		GtkTreeSelection *selection;
		EPublishUri *url = NULL;
		GtkTreeModel *model;
		GtkTreeIter iter;
		
		selection = gtk_tree_view_get_selection ((GtkTreeView *) dialog_data->url_list);
		if (gtk_tree_selection_get_selected (selection, &model, &iter)){
			gtk_tree_model_get (model, &iter, 
					    URL_LIST_FREE_BUSY_URL_COLUMN, 
					    &url, 
					    -1);

		}

		if (url) {
			dialog_data->url_editor = url_editor_dialog_new (dialog_data, url);
			
			gtk_list_store_set ((GtkListStore *) model, &iter, 
					   URL_LIST_LOCATION_COLUMN, 
					   g_strdup (url->location), 
					   URL_LIST_ENABLED_COLUMN, 
					   url->enabled, 
					   URL_LIST_FREE_BUSY_URL_COLUMN, url,
					   -1);
			
			url_list_changed (dialog_data);
			
			if (!GTK_WIDGET_SENSITIVE ((GtkWidget *) dialog_data->url_remove)) {
				selection = gtk_tree_view_get_selection ((GtkTreeView *) dialog_data->url_list);
				gtk_tree_model_get_iter_first ((GtkTreeModel *) model, &iter);
				gtk_widget_set_sensitive ((GtkWidget*) dialog_data->url_remove, TRUE);
				gtk_tree_selection_select_iter (selection, &iter);
			}
			dialog_data->url_editor = FALSE;
			dialog_data->url_editor_dlg = NULL;
		}
	} else {
		gdk_window_raise (dialog_data->url_editor_dlg->window);
	}	
}

static void
cal_prefs_dialog_url_remove_clicked (GtkWidget *button, DialogData *dialog_data)
{
	EPublishUri *url = NULL;
	GtkTreeSelection * selection;
	GtkTreeModel *model;
	GtkWidget *confirm;
	GtkTreeIter iter;
	int ans;
	
	selection = gtk_tree_view_get_selection (dialog_data->url_list);
	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (model, &iter, 
				    URL_LIST_FREE_BUSY_URL_COLUMN, &url, 
				    -1);
	
	/* make sure we have a valid account selected and that 
	   we aren't editing anything... */
	if (url == NULL || dialog_data->url_editor)
		return;
	
	confirm = gtk_message_dialog_new (PREFS_WINDOW (dialog_data),
					  GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
					  GTK_MESSAGE_QUESTION, 
					  GTK_BUTTONS_NONE,
					  _("Are you sure you want to remove this URL?"));
	
	button = gtk_button_new_from_stock (GTK_STOCK_YES);
	gtk_button_set_label ((GtkButton *) button, _("Remove"));
	gtk_dialog_add_action_widget ((GtkDialog *) confirm, (GtkWidget *) button, GTK_RESPONSE_YES);
	gtk_widget_show ((GtkWidget *) button);
	
	button = gtk_button_new_from_stock (GTK_STOCK_NO);
	gtk_button_set_label ((GtkButton *) button, _("Don't Remove"));
	gtk_dialog_add_action_widget ((GtkDialog *) confirm, 
				      (GtkWidget *) button, GTK_RESPONSE_NO);

	gtk_widget_show ((GtkWidget *) button);
	
	ans = gtk_dialog_run ((GtkDialog *) confirm);
	gtk_widget_destroy (confirm);
	
	if (ans == GTK_RESPONSE_YES) {
		int len;
		
		gtk_list_store_remove ((GtkListStore *) model, &iter);
		
		len = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), NULL);
		if (len > 0) {
			gtk_tree_selection_select_iter (selection, &iter);
		} else {
			gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_edit), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_remove), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_enable), FALSE);
		}
		g_free (url);
		url_list_changed (dialog_data);
	}
}

static void
cal_prefs_dialog_url_enable_clicked (GtkWidget *button, DialogData *dialog_data)
{
	EPublishUri *url = NULL;
	GtkTreeSelection * selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	selection = gtk_tree_view_get_selection (dialog_data->url_list);
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, 
				    URL_LIST_FREE_BUSY_URL_COLUMN, &url, 
				    -1);
		url->enabled = !url->enabled;
		
		gtk_tree_selection_select_iter (selection, &iter);
		
		gtk_list_store_set ((GtkListStore *) model, &iter, 
				    URL_LIST_ENABLED_COLUMN, url->enabled, 
				    -1);
		
		gtk_button_set_label ((GtkButton *) dialog_data->url_enable, 
				      url->enabled ? _("Disable") : _("Enable"));
		
		url_list_changed (dialog_data);
	}
}
 
static void
cal_prefs_dialog_url_list_enable_toggled (GtkCellRendererToggle *renderer, 
					  const char *path_string, 
					  DialogData *dialog_data)
{
	GtkTreeSelection * selection;
	EPublishUri *url = NULL;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	path = gtk_tree_path_new_from_string (path_string);
	model = gtk_tree_view_get_model (dialog_data->url_list);
	selection = gtk_tree_view_get_selection (dialog_data->url_list);

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_model_get (model, &iter, 
				    URL_LIST_FREE_BUSY_URL_COLUMN, &url, 
				    -1);
		
		url->enabled = !url->enabled;
		gtk_list_store_set((GtkListStore *) model, &iter, 
				   URL_LIST_ENABLED_COLUMN,
				   url->enabled, -1);
		
		if (gtk_tree_selection_iter_is_selected (selection, &iter))
			gtk_button_set_label ((GtkButton *) dialog_data->url_enable, 
					      url->enabled ? _("Disable") : _("Enable"));
		
		url_list_changed (dialog_data);
	}

	gtk_tree_path_free (path);
}

static void
cal_prefs_dialog_url_list_double_click (GtkTreeView *treeview, 
					GtkTreePath *path, 
					GtkTreeViewColumn *column, 
					DialogData *dialog_data)
{
	cal_prefs_dialog_url_edit_clicked  (NULL, dialog_data);
}				

static void
cal_prefs_dialog_url_list_change (GtkTreeSelection *selection, 
				  DialogData *dialog_data)
{
	EPublishUri *url = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	int state;
	
	state = gtk_tree_selection_get_selected (selection, &model, &iter);
	if (state) {
		gtk_tree_model_get (model, &iter, 
				    URL_LIST_FREE_BUSY_URL_COLUMN, &url, 
				    -1);

		if (url->location && url->enabled)
			gtk_button_set_label ((GtkButton *) dialog_data->url_enable, _("Disable"));
		else
			gtk_button_set_label ((GtkButton *) dialog_data->url_enable, _("Enable"));
	} else {
		gtk_widget_grab_focus (GTK_WIDGET (dialog_data->url_add));
	}
	
	gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_edit), state);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_remove), state);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_enable), state);
}

/* Shows the current Free/Busy settings in the dialog */
static void
show_fb_config (DialogData *dialog_data)
{
	GSList *url_config_list = NULL;
	GtkListStore *model;
	GtkTreeIter iter;
	
	model = (GtkListStore *) gtk_tree_view_get_model (dialog_data->url_list);
	gtk_list_store_clear (model);
	
	/* restore urls from gconf */
	url_config_list = calendar_config_get_free_busy();

	if (!url_config_list) {
		/* list is empty-disable edit, remove, and enable buttons */
		gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_edit), 
					 FALSE);

		gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_remove), 
					 FALSE);

		gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_enable), 
					 FALSE);
	}	else {
		gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_edit), 
					 TRUE);

		gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_remove), 
					 TRUE);

		gtk_widget_set_sensitive (GTK_WIDGET (dialog_data->url_enable), 
					 TRUE);
	}
	
	while (url_config_list) {
		gchar *xml = (gchar *)url_config_list->data;
		EPublishUri *url;
		url = g_new0 (EPublishUri, 1);
		
		e_pub_uri_from_xml (url, xml);
		if (url->location) {
			gtk_list_store_append (model, &iter);
			gtk_list_store_set (model, &iter, 
					   URL_LIST_ENABLED_COLUMN, 
					   url->enabled,
					   URL_LIST_LOCATION_COLUMN, 
					   url->location,
					   URL_LIST_FREE_BUSY_URL_COLUMN, url,
					   -1);
		}

		url_config_list = g_slist_next (url_config_list);
		g_free (xml);
	}

	g_slist_foreach (url_config_list, (GFunc) g_free, NULL);
	g_slist_free (url_config_list);
}

/* Shows the current task list settings in the dialog */
static void
show_task_list_config (DialogData *dialog_data)
{
	CalUnits units;
	gboolean hide_completed_tasks;

	set_color_picker (dialog_data->tasks_due_today_color, calendar_config_get_tasks_due_today_color ());
	set_color_picker (dialog_data->tasks_overdue_color, calendar_config_get_tasks_overdue_color ());

	/* Hide Completed Tasks. */
	hide_completed_tasks = calendar_config_get_hide_completed_tasks ();
	e_dialog_toggle_set (dialog_data->tasks_hide_completed_checkbutton,
			     hide_completed_tasks);

	/* Hide Completed Tasks Units. */
	units = calendar_config_get_hide_completed_tasks_units ();
	e_dialog_option_menu_set (dialog_data->tasks_hide_completed_optionmenu,
				  units, hide_completed_units_map);

	/* Hide Completed Tasks Value. */
	e_dialog_spin_set (dialog_data->tasks_hide_completed_spinbutton,
			   calendar_config_get_hide_completed_tasks_value ());

	gtk_widget_set_sensitive (dialog_data->tasks_hide_completed_spinbutton,
				  hide_completed_tasks);
	gtk_widget_set_sensitive (dialog_data->tasks_hide_completed_optionmenu,
				  hide_completed_tasks);
}

/* Shows the current config settings in the dialog. */
static void
show_config (DialogData *dialog_data)
{
	CalWeekdays working_days;
	gint mask, day, week_start_day, time_divisions;
	icaltimezone *zone;
	gboolean sensitive;

	/* Timezone. */
	zone = calendar_config_get_icaltimezone ();
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (dialog_data->timezone),
				       zone);

	/* Working Days. */
	working_days = calendar_config_get_working_days ();
	mask = 1 << 0;
	for (day = 0; day < 7; day++) {
		e_dialog_toggle_set (dialog_data->working_days[day], (working_days & mask) ? TRUE : FALSE);
		mask <<= 1;
	}

	/* Week Start Day. */
	week_start_day = calendar_config_get_week_start_day ();
	e_dialog_option_menu_set (dialog_data->week_start_day, week_start_day,
				  week_start_day_map);

	/* Start of Day. */
	e_date_edit_set_time_of_day (E_DATE_EDIT (dialog_data->start_of_day),
				     calendar_config_get_day_start_hour (),
				     calendar_config_get_day_start_minute ());

	/* End of Day. */
	e_date_edit_set_time_of_day (E_DATE_EDIT (dialog_data->end_of_day),
				     calendar_config_get_day_end_hour (),
				     calendar_config_get_day_end_minute ());

	/* 12/24 Hour Format. */
	if (calendar_config_get_24_hour_format ())
		e_dialog_toggle_set (dialog_data->use_24_hour, TRUE);
	else
		e_dialog_toggle_set (dialog_data->use_12_hour, TRUE);

	sensitive = calendar_config_locale_supports_12_hour_format ();
	gtk_widget_set_sensitive (dialog_data->use_12_hour, sensitive);
	gtk_widget_set_sensitive (dialog_data->use_24_hour, sensitive);


	/* Time Divisions. */
	time_divisions = calendar_config_get_time_divisions ();
	e_dialog_option_menu_set (dialog_data->time_divisions, time_divisions,
				  time_division_map);

	/* Show Appointment End Times. */
	e_dialog_toggle_set (dialog_data->show_end_times, calendar_config_get_show_event_end ());

	/* Compress Weekend. */
	e_dialog_toggle_set (dialog_data->compress_weekend, calendar_config_get_compress_weekend ());

	/* Date Navigator - Show Week Numbers. */
	e_dialog_toggle_set (dialog_data->dnav_show_week_no, calendar_config_get_dnav_show_week_no ());

	/* Task list */

	show_task_list_config (dialog_data);
	
	/* Free/Busy */
	show_fb_config (dialog_data);

	/* Other page */

	e_dialog_toggle_set (dialog_data->confirm_delete, calendar_config_get_confirm_delete ());

	e_dialog_toggle_set (dialog_data->default_reminder,
			     calendar_config_get_use_default_reminder ());
	e_dialog_spin_set (dialog_data->default_reminder_interval,
			   calendar_config_get_default_reminder_interval ());
	e_dialog_option_menu_set (dialog_data->default_reminder_units,
				  calendar_config_get_default_reminder_units (),
				  default_reminder_units_map);
}
