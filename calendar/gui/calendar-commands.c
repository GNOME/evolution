/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Commands for the calendar GUI control
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
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
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-exception.h>
#include <gal/util/e-util.h>
#include <libecal/e-cal-time-util.h>
#include "shell/Evolution.h"
#include "calendar-commands.h"
#include "calendar-config.h"
#include "e-day-view.h"
#include "e-week-view.h"
#include "gnome-cal.h"
#include "goto.h"
#include "print.h"
#include "dialogs/cal-prefs-dialog.h"
#include "itip-utils.h"
#include "e-pub-utils.h"
#include "e-cal-list-view.h"
#include "evolution-shell-component-utils.h"
#include "e-util/e-icon-factory.h"

/* Focusing information for the calendar view.  We have to keep track of this
 * ourselves because with Bonobo controls, we may get unpaired focus_out events.
 */
typedef struct {
	guint calendar_focused : 1;
	guint taskpad_focused : 1;
} FocusData;

static void
file_open_event_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	e_calendar_view_open_event (gnome_calendar_get_current_view_widget (gcal));
}


/* Prints the calendar at its current view and time range */
static void
print (GnomeCalendar *gcal, gboolean preview)
{
	time_t start;
	GnomeCalendarViewType view_type;
	PrintView print_view;
	ECalListView *list_view;
	ETable *etable;
	
	gnome_calendar_get_current_time_range (gcal, &start, NULL);
	view_type = gnome_calendar_get_view (gcal);

	switch (view_type) {
	case GNOME_CAL_DAY_VIEW:
		print_view = PRINT_VIEW_DAY;
		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
	case GNOME_CAL_WEEK_VIEW:
		print_view = PRINT_VIEW_WEEK;
		break;

	case GNOME_CAL_MONTH_VIEW:
		print_view = PRINT_VIEW_MONTH;
		break;

	case GNOME_CAL_LIST_VIEW:
		list_view = E_CAL_LIST_VIEW (gnome_calendar_get_current_view_widget (gcal));
		etable = e_table_scrolled_get_table (list_view->table_scrolled);
		print_table (etable, _("Calendar"), preview);
		return;

	default:
		g_assert_not_reached ();
		return;
	}

	print_calendar (gcal, preview, start, print_view);
}

/* File/Print callback */
static void
file_print_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);
	print (gcal, FALSE);
}

static void
file_print_preview_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);
	print (gcal, TRUE);
}

/* Sets a clock cursor for the specified calendar window */
static void
set_clock_cursor (GnomeCalendar *gcal)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor (GTK_WIDGET (gcal)->window, cursor);
	gdk_cursor_destroy (cursor);
	gdk_flush ();
}

/* Resets the normal cursor for the specified calendar window */
static void
set_normal_cursor (GnomeCalendar *gcal)
{
	gdk_window_set_cursor (GTK_WIDGET (gcal)->window, NULL);
	gdk_flush ();
}

static void
previous_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	set_clock_cursor (gcal);
	gnome_calendar_previous (gcal);
	set_normal_cursor (gcal);
}

static void
next_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	set_clock_cursor (gcal);
	gnome_calendar_next (gcal);
	set_normal_cursor (gcal);
}

void
calendar_goto_today (GnomeCalendar *gcal)
{
	set_clock_cursor (gcal);
	gnome_calendar_goto_today (gcal);
	set_normal_cursor (gcal);
}

static void
today_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	calendar_goto_today (gcal);
}

static void
goto_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	goto_dialog (gcal);
}

static void
show_day_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, GNOME_CAL_DAY_VIEW);
}

static void
show_work_week_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, GNOME_CAL_WORK_WEEK_VIEW);
}

static void
show_week_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, GNOME_CAL_WEEK_VIEW);
}

static void
show_month_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, GNOME_CAL_MONTH_VIEW);
}


static void
show_list_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, GNOME_CAL_LIST_VIEW);
}


static void
cut_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);
	set_clock_cursor (gcal);
	gnome_calendar_cut_clipboard (gcal);
	set_normal_cursor (gcal);
}

static void
copy_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	set_clock_cursor (gcal);
	gnome_calendar_copy_clipboard (gcal);
	set_normal_cursor (gcal);
}

static void
paste_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	set_clock_cursor (gcal);
	gnome_calendar_paste_clipboard (gcal);
	set_normal_cursor (gcal);
}

static void
delete_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	set_clock_cursor (gcal);
	gnome_calendar_delete_selection (gcal);
	set_normal_cursor (gcal);
}

static void
delete_occurrence_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	set_clock_cursor (gcal);
	gnome_calendar_delete_selected_occurrence (gcal);
	set_normal_cursor (gcal);
}

static void
publish_freebusy_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	e_pub_publish (TRUE);
}

static void
purge_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;
	GtkWidget *dialog, *parent, *box, *label, *spin;
	int response;

	gcal = GNOME_CALENDAR (data);

	/* create the dialog */
	parent = gtk_widget_get_toplevel (GTK_WIDGET (gcal));
	dialog = gtk_message_dialog_new (
		(GtkWindow *)parent,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_WARNING,
		GTK_BUTTONS_OK_CANCEL,
		_("This operation will permanently erase all events older than the selected amount of time. If you continue, you will not be able to recover these events."));
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	box = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), box, TRUE, FALSE, 6);

	label = gtk_label_new (_("Purge events older than"));
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, FALSE, 6);
	spin = gtk_spin_button_new_with_range (0.0, 1000.0, 1.0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 60.0);
	gtk_box_pack_start (GTK_BOX (box), spin, FALSE, FALSE, 6);
	label = gtk_label_new (_("days"));
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, FALSE, 6);

	gtk_widget_show_all (box);

	/* run the dialog */
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response == GTK_RESPONSE_OK) {
		gint days;
		time_t tt;

		days = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
		tt = time (NULL);
		tt -= (days * (24 * 3600));

		gnome_calendar_purge (gcal, tt);
	}

	gtk_widget_destroy (dialog);
}


const gchar *
calendar_get_text_for_folder_bar_label (GnomeCalendar *gcal)
{
	icaltimezone *zone;
	struct icaltimetype start_tt, end_tt;
	time_t start_time, end_time;
	struct tm start_tm, end_tm;
	static char buffer[512];
	char end_buffer[256];
	GnomeCalendarViewType view;

	gnome_calendar_get_visible_time_range (gcal, &start_time, &end_time);
	zone = gnome_calendar_get_timezone (gcal);

	start_tt = icaltime_from_timet_with_zone (start_time, FALSE, zone);
	start_tm.tm_year = start_tt.year - 1900;
	start_tm.tm_mon = start_tt.month - 1;
	start_tm.tm_mday = start_tt.day;
	start_tm.tm_hour = start_tt.hour;
	start_tm.tm_min = start_tt.minute;
	start_tm.tm_sec = start_tt.second;
	start_tm.tm_isdst = -1;
	start_tm.tm_wday = time_day_of_week (start_tt.day, start_tt.month - 1,
					     start_tt.year);

	/* Take one off end_time so we don't get an extra day. */
	end_tt = icaltime_from_timet_with_zone (end_time - 1, FALSE, zone);
	end_tm.tm_year = end_tt.year - 1900;
	end_tm.tm_mon = end_tt.month - 1;
	end_tm.tm_mday = end_tt.day;
	end_tm.tm_hour = end_tt.hour;
	end_tm.tm_min = end_tt.minute;
	end_tm.tm_sec = end_tt.second;
	end_tm.tm_isdst = -1;
	end_tm.tm_wday = time_day_of_week (end_tt.day, end_tt.month - 1,
					   end_tt.year);

	view = gnome_calendar_get_view (gcal);

	switch (view) {
	case GNOME_CAL_DAY_VIEW:
	case GNOME_CAL_WORK_WEEK_VIEW:
	case GNOME_CAL_WEEK_VIEW:
		if (start_tm.tm_year == end_tm.tm_year
		    && start_tm.tm_mon == end_tm.tm_mon
		    && start_tm.tm_mday == end_tm.tm_mday) {
			e_utf8_strftime (buffer, sizeof (buffer),
				  _("%A %d %B %Y"), &start_tm);
		} else if (start_tm.tm_year == end_tm.tm_year) {
			e_utf8_strftime (buffer, sizeof (buffer),
				  _("%a %d %b"), &start_tm);
			e_utf8_strftime (end_buffer, sizeof (end_buffer),
				  _("%a %d %b %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		} else {
			e_utf8_strftime (buffer, sizeof (buffer),
				  _("%a %d %b %Y"), &start_tm);
			e_utf8_strftime (end_buffer, sizeof (end_buffer),
				  _("%a %d %b %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		}
		break;
	case GNOME_CAL_MONTH_VIEW:
	case GNOME_CAL_LIST_VIEW:
		if (start_tm.tm_year == end_tm.tm_year) {
			if (start_tm.tm_mon == end_tm.tm_mon) {
				if (start_tm.tm_mday == end_tm.tm_mday) {
					buffer [0] = '\0';
				} else {
					e_utf8_strftime (buffer, sizeof (buffer),
							 "%d", &start_tm);
					strcat (buffer, " - ");
				}
				e_utf8_strftime (end_buffer, sizeof (end_buffer),
					  _("%d %B %Y"), &end_tm);
				strcat (buffer, end_buffer);
			} else {
				e_utf8_strftime (buffer, sizeof (buffer),
					  _("%d %B"), &start_tm);
				e_utf8_strftime (end_buffer, sizeof (end_buffer),
					  _("%d %B %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			}
		} else {
			e_utf8_strftime (buffer, sizeof (buffer),
				  _("%d %B %Y"), &start_tm);
			e_utf8_strftime (end_buffer, sizeof (end_buffer),
				  _("%d %B %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		}
		break;
	default:
		g_assert_not_reached ();
		return NULL;
	}
	return buffer;
}


/* Sensitizes the UI Component menu/toolbar calendar commands based on the
 * number of selected events. (This will always be 0 or 1 currently.)  If enable
 * is FALSE, all will be disabled.  Otherwise, the currently-selected number of
 * events will be used.
 */
void
calendar_control_sensitize_calendar_commands (BonoboControl *control, GnomeCalendar *gcal, gboolean enable)
{
	BonoboUIComponent *uic;
	ECalendarViewEvent *event;
	GList *list;
	int n_selected;
	GtkWidget *view;
	ECal *e_cal;
	gboolean selected_read_only = FALSE, default_read_only = FALSE, has_recurrences;
	
	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	if (bonobo_ui_component_get_container (uic) == CORBA_OBJECT_NIL)
		return;
	
	view = gnome_calendar_get_current_view_widget (gcal);
	list = e_calendar_view_get_selected_events (E_CALENDAR_VIEW (view));

	n_selected = enable ? g_list_length (list) : 0;

	event = (ECalendarViewEvent *) list ? list->data : NULL;
	if (event && event->comp_data)
		e_cal_is_read_only (event->comp_data->client, &selected_read_only, NULL);
	else
		selected_read_only = TRUE;

	/* retrieve read-onlyness of the default client */
	e_cal = e_cal_model_get_default_client (gnome_calendar_get_calendar_model (gcal));
	if (e_cal)
		e_cal_is_read_only (e_cal, &default_read_only, NULL);
	else
		default_read_only = TRUE;

	bonobo_ui_component_set_prop (uic, "/commands/EventOpen", "sensitive",
				      n_selected != 1 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/Cut", "sensitive",
				      n_selected == 0 || selected_read_only ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/Copy", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/Paste", "sensitive",
				      default_read_only ? "0" : "1",
				      NULL);

	/* occurrence-related menu items */
	has_recurrences = FALSE;
	if (n_selected > 0 && !selected_read_only) {
		if (list) {
			event = (ECalendarViewEvent *) list->data;
			if (e_cal_util_component_has_recurrences (event->comp_data->icalcomp))
				has_recurrences = TRUE;
		}
	}

	bonobo_ui_component_set_prop (uic, "/commands/Delete", "sensitive",
				      n_selected == 0 || selected_read_only || has_recurrences ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/DeleteOccurrence", "sensitive",
				      has_recurrences ? "1" : "0",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/DeleteAllOccurrences", "sensitive",
				      has_recurrences ? "1" : "0",
				      NULL);

	/* free memory */
	if (list)
		g_list_free (list);
}

/* Sensitizes the UI Component menu/toolbar tasks commands based on the number
 * of selected tasks.  If enable is FALSE, all will be disabled.  Otherwise, the
 * currently-selected number of tasks will be used.
 */
static void
sensitize_taskpad_commands (GnomeCalendar *gcal, BonoboControl *control, gboolean enable)
{
	BonoboUIComponent *uic;
	int n_selected;
	ECalendarTable *task_pad;
	ECalModel *model;
	ECal *e_cal;
	gboolean read_only = TRUE;
	
	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	n_selected = enable ? gnome_calendar_get_num_tasks_selected (gcal) : 0;
	task_pad = gnome_calendar_get_task_pad (gcal);
	model = e_calendar_table_get_model (task_pad);
	e_cal = e_cal_model_get_default_client (model);
	
	if (e_cal)
		e_cal_is_read_only (e_cal, &read_only, NULL);
	else
		read_only = TRUE;

	bonobo_ui_component_set_prop (uic, "/commands/Cut", "sensitive",
				      n_selected == 0 || read_only ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/Copy", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/Paste", "sensitive",
				      enable && !read_only ? "1" : "0",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/Delete", "sensitive",
				      n_selected == 0 || read_only ? "0" : "1",
				      NULL);
}

/* Callback used when the selection in the calendar views changes */
static void
gcal_calendar_selection_changed_cb (GnomeCalendar *gcal, gpointer data)
{
	BonoboControl *control;

	control = BONOBO_CONTROL (data);

	calendar_control_sensitize_calendar_commands (control, gcal, TRUE);
}

/* Callback used when the selection in the taskpad changes */
static void
gcal_taskpad_selection_changed_cb (GnomeCalendar *gcal, gpointer data)
{
	BonoboControl *control;

	control = BONOBO_CONTROL (data);

	sensitize_taskpad_commands (gcal, control, TRUE);
}

/* Callback used when the focus changes for a calendar view */
static void
gcal_calendar_focus_change_cb (GnomeCalendar *gcal, gboolean in, gpointer data)
{
	BonoboControl *control;
	FocusData *focus;

	control = BONOBO_CONTROL (data);

	focus = g_object_get_data (G_OBJECT (control), "focus_data");
	g_assert (focus != NULL);

	if (in) {
		g_signal_connect (gcal, "calendar_selection_changed",
				  G_CALLBACK (gcal_calendar_selection_changed_cb), control);
		calendar_control_sensitize_calendar_commands (control, gcal, TRUE);
		focus->calendar_focused = TRUE;
	} else if (focus->calendar_focused) {
		gtk_signal_disconnect_by_func (GTK_OBJECT (gcal),
					       G_CALLBACK (gcal_calendar_selection_changed_cb),
					       control);
		calendar_control_sensitize_calendar_commands (control, gcal, FALSE);
		focus->calendar_focused = FALSE;
	}
}

/* Callback used when the taskpad focus changes */
static void
gcal_taskpad_focus_change_cb (GnomeCalendar *gcal, gboolean in, gpointer data)
{
	BonoboControl *control;
	FocusData *focus;

	control = BONOBO_CONTROL (data);

	focus = g_object_get_data (G_OBJECT (control), "focus_data");
	g_assert (focus != NULL);

	if (in) {
		g_signal_connect (gcal, "taskpad_selection_changed",
				  G_CALLBACK (gcal_taskpad_selection_changed_cb), control);
		sensitize_taskpad_commands (gcal, control, TRUE);
		focus->taskpad_focused = TRUE;
	} else if (focus->taskpad_focused) {
		/* With Bonobo controls, we may get unpaired focus_out events.
		 * That is why we have to keep track of this ourselves instead
		 * of blindly assumming that we are getting this event because
		 * the taskpad was in fact focused.
		 */
		gtk_signal_disconnect_by_func (GTK_OBJECT (gcal),
					       G_CALLBACK (gcal_taskpad_selection_changed_cb),
					       control);
		sensitize_taskpad_commands (gcal, control, FALSE);
		focus->taskpad_focused = FALSE;
	}

}


static BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("EventOpen", file_open_event_cb),
	BONOBO_UI_VERB ("CalendarPrint", file_print_cb),
	BONOBO_UI_VERB ("CalendarPrintPreview", file_print_preview_cb),

	BONOBO_UI_VERB ("Cut", cut_cmd),
	BONOBO_UI_VERB ("Copy", copy_cmd),
	BONOBO_UI_VERB ("Paste", paste_cmd),
	BONOBO_UI_VERB ("Delete", delete_cmd),
	BONOBO_UI_VERB ("DeleteOccurrence", delete_occurrence_cmd),
	BONOBO_UI_VERB ("DeleteAllOccurrences", delete_cmd),

	BONOBO_UI_VERB ("CalendarPrev", previous_clicked),
	BONOBO_UI_VERB ("CalendarToday", today_clicked),
	BONOBO_UI_VERB ("CalendarNext", next_clicked),
	BONOBO_UI_VERB ("CalendarGoto", goto_clicked),

	BONOBO_UI_VERB ("ShowDayView", show_day_view_clicked),
	BONOBO_UI_VERB ("ShowWorkWeekView", show_work_week_view_clicked),
	BONOBO_UI_VERB ("ShowWeekView", show_week_view_clicked),
	BONOBO_UI_VERB ("ShowMonthView", show_month_view_clicked),
	BONOBO_UI_VERB ("ShowListView", show_list_view_clicked),

	BONOBO_UI_VERB ("PublishFreeBusy", publish_freebusy_cmd),
	BONOBO_UI_VERB ("CalendarPurge", purge_cmd),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] =
{
	E_PIXMAP ("/Toolbar/DayView",	      "stock_calendar-view-day",       E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/WorkWeekView",    "stock_calendar-view-work-week", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/WeekView",	      "stock_calendar-view-week",      E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MonthView",	      "stock_calendar-view-month",     E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/ListView",	      "stock_calendar-view-list",      E_ICON_SIZE_LARGE_TOOLBAR),

	E_PIXMAP_END
};

void
calendar_control_activate (BonoboControl *control,
			   GnomeCalendar *gcal)
{
	Bonobo_UIContainer remote_uih;
	BonoboUIComponent *uic;
	FocusData *focus;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	remote_uih = bonobo_control_get_remote_ui_container (control, NULL);
	bonobo_ui_component_set_container (uic, remote_uih, NULL);
	bonobo_object_release_unref (remote_uih, NULL);

	gnome_calendar_set_ui_component (gcal, uic);

	bonobo_ui_component_add_verb_list_with_data (uic, verbs, gcal);

	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-calendar.xml",
			       "evolution-calendar",
			       NULL);

	e_pixmaps_update (uic, pixmaps);

	gnome_calendar_setup_view_menus (gcal, uic);

	g_signal_connect (gcal, "calendar_focus_change",
			  G_CALLBACK (gcal_calendar_focus_change_cb), control);
	g_signal_connect (gcal, "taskpad_focus_change",
			  G_CALLBACK (gcal_taskpad_focus_change_cb), control);

	calendar_control_sensitize_calendar_commands (control, gcal, TRUE);
	sensitize_taskpad_commands (gcal, control, TRUE);

	bonobo_ui_component_thaw (uic, NULL);

	/* Show the dialog for setting the timezone if the user hasn't chosen
	   a default timezone already. This is done in the startup wizard now,
	   so we don't do it here. */
#if 0
	calendar_config_check_timezone_set ();
#endif

	focus = g_new (FocusData, 1);
	focus->calendar_focused = FALSE;
	focus->taskpad_focused = FALSE;

	g_object_set_data (G_OBJECT (control), "focus_data", focus);
}

void
calendar_control_deactivate (BonoboControl *control, GnomeCalendar *gcal)
{
	FocusData *focus;
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	gnome_calendar_set_ui_component (gcal, NULL);

	focus = g_object_get_data (G_OBJECT (control), "focus_data");
	g_assert (focus != NULL);

	g_object_set_data (G_OBJECT (control), "focus_data", NULL);
	g_free (focus);

	gnome_calendar_discard_view_menus (gcal);

	g_signal_handlers_disconnect_matched (gcal, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, control);

	bonobo_ui_component_rm (uic, "/", NULL);
 	bonobo_ui_component_unset_container (uic, NULL);
}
