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
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-ui-util.h>
#include <cal-util/timeutil.h>
#include "shell/Evolution.h"
#include "calendar-commands.h"
#include "calendar-config.h"
#include "gnome-cal.h"
#include "goto.h"
#include "print.h"
#include "dialogs/cal-prefs-dialog.h"
#include "itip-utils.h"
#include "evolution-shell-component-utils.h"

/* A list of all of the calendars started */
static GList *all_calendars = NULL;

/* We have one global preferences dialog. */
static CalPrefsDialog *preferences_dialog = NULL;

/* Callback for the new appointment command */
static void
new_appointment_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);
	gnome_calendar_new_appointment (gcal);
}

static void
new_event_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;
	time_t dtstart, dtend;

	gcal = GNOME_CALENDAR (data);
	gnome_calendar_get_current_time_range (gcal, &dtstart, &dtend);
	gnome_calendar_new_appointment_for (gcal, dtstart, dtend, TRUE);
}

static void
new_task_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);
	gnome_calendar_new_task (gcal);
}

/* Prints the calendar at its current view and time range */
static void
print (GnomeCalendar *gcal, gboolean preview)
{
	time_t start;
	GnomeCalendarViewType view_type;
	PrintView print_view;

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

/* This iterates over each calendar telling them to update their config
   settings. */
void
update_all_config_settings (void)
{
	GList *l;

	for (l = all_calendars; l; l = l->next)
		gnome_calendar_update_config_settings (GNOME_CALENDAR (l->data), FALSE);
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

	gnome_calendar_set_view (gcal, GNOME_CAL_DAY_VIEW, FALSE, TRUE);
}

static void
show_work_week_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, GNOME_CAL_WORK_WEEK_VIEW, FALSE, TRUE);
}

static void
show_week_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, GNOME_CAL_WEEK_VIEW, FALSE, TRUE);
}

static void
show_month_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, GNOME_CAL_MONTH_VIEW, FALSE, TRUE);
}



static void
settings_cmd (BonoboUIComponent *uic, gpointer data, const char *path)
{
	if (!preferences_dialog)
		preferences_dialog = cal_prefs_dialog_new (CAL_PREFS_DIALOG_PAGE_CALENDAR);
	else
		cal_prefs_dialog_show (preferences_dialog, CAL_PREFS_DIALOG_PAGE_CALENDAR);
}

static void
cut_event_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);
	set_clock_cursor (gcal);
	gnome_calendar_cut_clipboard (gcal);
	set_normal_cursor (gcal);
}

static void
copy_event_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	set_clock_cursor (gcal);
	gnome_calendar_copy_clipboard (gcal);
	set_normal_cursor (gcal);
}

static void
paste_event_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	set_clock_cursor (gcal);
	gnome_calendar_paste_clipboard (gcal);
	set_normal_cursor (gcal);
}

static void
delete_event_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	set_clock_cursor (gcal);
	gnome_calendar_delete_event (gcal);
	set_normal_cursor (gcal);
}

static void
publish_freebusy_cmd (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	GnomeCalendar *gcal;
	CalClient *client;
	GList *comp_list;
	icaltimezone *utc;
	time_t start = time (NULL), end;

	gcal = GNOME_CALENDAR (data);

	utc = icaltimezone_get_utc_timezone ();
	start = time_day_begin_with_zone (start, utc);
	end = time_add_week_with_zone (start, 6, utc);

	client = gnome_calendar_get_cal_client (gcal);
	comp_list = cal_client_get_free_busy (client, NULL, start, end);
	if (comp_list) {
		GList *l;

		for (l = comp_list; l; l = l->next) {
			CalComponent *comp = CAL_COMPONENT (l->data);
			itip_send_comp (CAL_COMPONENT_METHOD_PUBLISH, comp);

			gtk_object_unref (GTK_OBJECT (comp));
		}

 		g_list_free (comp_list);
	}
}

/* Does a queryInterface on the control's parent control frame for the ShellView interface */
static GNOME_Evolution_ShellView
get_shell_view_interface (BonoboControl *control)
{
	Bonobo_ControlFrame control_frame;
	GNOME_Evolution_ShellView shell_view;
	CORBA_Environment ev;

	control_frame = bonobo_control_get_control_frame (control);

	g_assert (control_frame != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	shell_view = Bonobo_Unknown_queryInterface (control_frame,
						    "IDL:GNOME/Evolution/ShellView:1.0",
						    &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("get_shell_view_interface(): "
			   "Could not queryInterface() on the control frame");
		shell_view = CORBA_OBJECT_NIL;
		goto out;
	}

	CORBA_exception_free (&ev);

 out:

	return shell_view;
}

/* Displays the currently displayed time range in the folder bar label on the
   shell view, according to which view we are showing. */
void
calendar_set_folder_bar_label (GnomeCalendar *gcal, BonoboControl *control)
{
	icaltimezone *zone;
	struct icaltimetype start_tt, end_tt;
	time_t start_time, end_time;
	struct tm start_tm, end_tm;
	char buffer[512], end_buffer[256];
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
			strftime (buffer, sizeof (buffer),
				  _("%A %d %B %Y"), &start_tm);
		} else if (start_tm.tm_year == end_tm.tm_year) {
			strftime (buffer, sizeof (buffer),
				  _("%a %d %b"), &start_tm);
			strftime (end_buffer, sizeof (end_buffer),
				  _("%a %d %b %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		} else {
			strftime (buffer, sizeof (buffer),
				  _("%a %d %b %Y"), &start_tm);
			strftime (end_buffer, sizeof (end_buffer),
				  _("%a %d %b %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		}
		break;
	case GNOME_CAL_MONTH_VIEW:
		if (start_tm.tm_year == end_tm.tm_year) {
			if (start_tm.tm_mon == end_tm.tm_mon) {
				strftime (buffer, sizeof (buffer),
					  "%d", &start_tm);
				strftime (end_buffer, sizeof (end_buffer),
					  _("%d %B %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			} else {
				strftime (buffer, sizeof (buffer),
					  _("%d %B"), &start_tm);
				strftime (end_buffer, sizeof (end_buffer),
					  _("%d %B %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			}
		} else {
			strftime (buffer, sizeof (buffer),
				  _("%d %B %Y"), &start_tm);
			strftime (end_buffer, sizeof (end_buffer),
				  _("%d %B %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		}
		break;
	default:
		g_assert_not_reached ();
	}

	control_util_set_folder_bar_label (control, buffer);
}

void
control_util_set_folder_bar_label (BonoboControl *control, char *label)
{
	GNOME_Evolution_ShellView shell_view;
	CORBA_Environment ev;

	shell_view = get_shell_view_interface (control);
	if (shell_view == CORBA_OBJECT_NIL)
		return;

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_setFolderBarLabel (shell_view, label, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("control_util_set_folder_bar_label(): Could not set the folder bar label");

	CORBA_exception_free (&ev);
}

/* Sensitizes the UI Component menu/toolbar commands based on the number of
 * selected events. (This will always be 0 or 1 currently.)
 */
static void
sensitize_commands (GnomeCalendar *gcal, BonoboControl *control)
{
	BonoboUIComponent *uic;
	int n_selected;
	
	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	n_selected = gnome_calendar_get_num_events_selected (gcal);

	bonobo_ui_component_set_prop (uic, "/commands/CutEvent", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/CopyEvent", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/DeleteEvent", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
}

/* Callback used when the selection in the calendar views changes */
static void
selection_changed_cb (GnomeCalendar *gcal, gpointer data)
{
	BonoboControl *control;

	control = BONOBO_CONTROL (data);

	sensitize_commands (gcal, control);
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("CalendarPrint", file_print_cb),
	BONOBO_UI_VERB ("CalendarPrintPreview", file_print_preview_cb),

	BONOBO_UI_VERB ("CalendarNewAppointment", new_appointment_cb),
	BONOBO_UI_VERB ("CalendarNewEvent", new_event_cb),
	BONOBO_UI_VERB ("CalendarNewTask", new_task_cb),

	BONOBO_UI_VERB ("CalendarSettings", settings_cmd),

	BONOBO_UI_VERB ("CutEvent", cut_event_cmd),
	BONOBO_UI_VERB ("CopyEvent", copy_event_cmd),
	BONOBO_UI_VERB ("PasteEvent", paste_event_cmd),
	BONOBO_UI_VERB ("DeleteEvent", delete_event_cmd),

	BONOBO_UI_VERB ("CalendarPrev", previous_clicked),
	BONOBO_UI_VERB ("CalendarToday", today_clicked),
	BONOBO_UI_VERB ("CalendarNext", next_clicked),
	BONOBO_UI_VERB ("CalendarGoto", goto_clicked),

	BONOBO_UI_VERB ("ShowDayView", show_day_view_clicked),
	BONOBO_UI_VERB ("ShowWorkWeekView", show_work_week_view_clicked),
	BONOBO_UI_VERB ("ShowWeekView", show_week_view_clicked),
	BONOBO_UI_VERB ("ShowMonthView", show_month_view_clicked),

	BONOBO_UI_VERB ("PublishFreeBusy", publish_freebusy_cmd),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] =
{
	E_PIXMAP ("/menu/File/New/NewFirstItem/NewAppointment",	              "new_appointment.xpm"),
	E_PIXMAP ("/menu/File/New/NewFirstItem/NewTask",	              "new_task-16.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/CutEvent",		      "16_cut.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/CopyEvent",		      "16_copy.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/PasteEvent",		      "16_paste.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/DeleteEvent",		      "evolution-trash-mini.png"),
	E_PIXMAP ("/menu/File/Print/Print",				      "print.xpm"),
	E_PIXMAP ("/menu/File/Print/PrintPreview",			      "print-preview.xpm"),
	E_PIXMAP ("/menu/ComponentActionsPlaceholder/Actions/NewAppointment", "new_appointment.xpm"),
	E_PIXMAP ("/menu/ComponentActionsPlaceholder/Actions/NewEvent",       "new_appointment.xpm"),
	E_PIXMAP ("/menu/ComponentActionsPlaceholder/Actions/NewTask",        "new_task-16.png"),
	E_PIXMAP ("/menu/Tools/ComponentPlaceholder/CalendarSettings",        "configure_16_calendar.xpm"),
	E_PIXMAP ("/menu/View/ViewBegin/Goto",				      "goto-16.png"),

	E_PIXMAP ("/Toolbar/New",					      "buttons/new_appointment.png"),
	E_PIXMAP ("/Toolbar/NewTask",					      "buttons/new_task.png"),
	E_PIXMAP ("/Toolbar/Print",					      "buttons/print.png"),
	E_PIXMAP ("/Toolbar/Delete",					      "buttons/delete-message.png"),
	E_PIXMAP ("/Toolbar/Prev",					      "buttons/arrow-left-24.png"),
	E_PIXMAP ("/Toolbar/Next",					      "buttons/arrow-right-24.png"),
	E_PIXMAP ("/Toolbar/Goto",					      "buttons/goto-24.png"),
	E_PIXMAP ("/Toolbar/DayView",					      "buttons/dayview.xpm"),
	E_PIXMAP ("/Toolbar/WorkWeekView",				      "buttons/workweekview.xpm"),
	E_PIXMAP ("/Toolbar/WeekView",					      "buttons/weekview.xpm"),
	E_PIXMAP ("/Toolbar/MonthView",					      "buttons/monthview.xpm"),

	E_PIXMAP_END
};

void
calendar_control_activate (BonoboControl *control,
			   GnomeCalendar *gcal)
{
	Bonobo_UIContainer remote_uih;
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	remote_uih = bonobo_control_get_remote_ui_container (control);
	bonobo_ui_component_set_container (uic, remote_uih);
	bonobo_object_release_unref (remote_uih, NULL);

	bonobo_ui_component_add_verb_list_with_data (uic, verbs, gcal);

	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, EVOLUTION_DATADIR,
			       "evolution-calendar.xml",
			       "evolution-calendar");

	e_pixmaps_update (uic, pixmaps);

	gnome_calendar_setup_view_menus (gcal, uic);

	gtk_signal_connect (GTK_OBJECT (gcal), "selection_changed",
			    GTK_SIGNAL_FUNC (selection_changed_cb), control);

	sensitize_commands (gcal, control);

	bonobo_ui_component_thaw (uic, NULL);

	/* Show the dialog for setting the timezone if the user hasn't chosen
	   a default timezone already. This is done in the startup wizard now,
	   so we don't do it here. */
#if 0
	calendar_config_check_timezone_set ();
#endif

	calendar_set_folder_bar_label (gcal, control);
}

void
calendar_control_deactivate (BonoboControl *control, GnomeCalendar *gcal)
{
	BonoboUIComponent *uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	gnome_calendar_discard_view_menus (gcal);

	/* Stop monitoring the "selection_changed" signal */
	gtk_signal_disconnect_by_data (GTK_OBJECT (gcal), control);

	bonobo_ui_component_rm (uic, "/", NULL);
 	bonobo_ui_component_unset_container (uic);
}

/* Removes a calendar from our list of all calendars when it is destroyed. */
static void
on_calendar_destroyed (GnomeCalendar *gcal)
{
	all_calendars = g_list_remove (all_calendars, gcal);
}

GnomeCalendar *
new_calendar (void)
{
	GtkWidget *gcal;

	gcal = gnome_calendar_new ();
	if (!gcal) {
		gnome_warning_dialog (_("Could not create the calendar view.  Please check your "
					"ORBit and OAF setup."));
		return NULL;
	}

	gtk_signal_connect (GTK_OBJECT (gcal), "destroy",
			    GTK_SIGNAL_FUNC (on_calendar_destroyed), NULL);

	all_calendars = g_list_prepend (all_calendars, gcal);

	return GNOME_CALENDAR (gcal);
}
