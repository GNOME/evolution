/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Commands for the calendar GUI control
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
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
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include <bonobo/bonobo-ui-util.h>
#include <cal-util/timeutil.h>
#include "calendar-commands.h"
#include "gnome-cal.h"
#include "goto.h"
#include "print.h"
#include "dialogs/cal-prefs-dialog.h"


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

/* Prints the calendar at its current view and time range */
static void
print (GnomeCalendar *gcal, gboolean preview)
{
	time_t start;
	const char *view;
	PrintView print_view;

	gnome_calendar_get_current_time_range (gcal, &start, NULL);
	view = gnome_calendar_get_current_view_name (gcal);

	if (strcmp (view, "dayview") == 0)
		print_view = PRINT_VIEW_DAY;
	else if (strcmp (view, "workweekview") == 0 || strcmp (view, "weekview") == 0)
		print_view = PRINT_VIEW_WEEK;
	else if (strcmp (view, "monthview") == 0)
		print_view = PRINT_VIEW_MONTH;
	else {
		g_assert_not_reached ();
		print_view = PRINT_VIEW_DAY;
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

	gnome_calendar_set_view (gcal, "dayview", FALSE, TRUE);
}

static void
show_work_week_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, "workweekview", FALSE, TRUE);
}

static void
show_week_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, "weekview", FALSE, TRUE);
}

static void
show_month_view_clicked (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gnome_calendar_set_view (gcal, "monthview", FALSE, TRUE);
}


static void
new_calendar_cmd (BonoboUIComponent *uic, gpointer data, const char *path)
{
	new_calendar ();
}

static void
open_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	GtkWidget *error_dialog;
	int ret;
	if(!g_file_exists (gtk_file_selection_get_filename (fs))) {
		error_dialog = gnome_message_box_new (
			_("File not found"),
			GNOME_MESSAGE_BOX_ERROR,
			GNOME_STOCK_BUTTON_OK,
			NULL);

		gnome_dialog_set_parent (GNOME_DIALOG (error_dialog), GTK_WINDOW (fs));
		ret = gnome_dialog_run (GNOME_DIALOG (error_dialog));
	} else {
		/* FIXME: find out who owns this calendar and use that name */
#ifndef NO_WARNINGS
#warning "FIXME: find out who owns this calendar and use that name"
#endif
		/*
		new_calendar ("Somebody", gtk_file_selection_get_filename (fs));
		*/
		gtk_widget_destroy (GTK_WIDGET (fs));
	}
}

static void
open_calendar_cmd (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GtkFileSelection *fs;

	fs = GTK_FILE_SELECTION (gtk_file_selection_new (_("Open calendar")));

	gtk_signal_connect (GTK_OBJECT (fs->ok_button), "clicked",
			    (GtkSignalFunc) open_ok,
			    fs);
	gtk_signal_connect_object (GTK_OBJECT (fs->cancel_button), "clicked",
				   (GtkSignalFunc) gtk_widget_destroy,
				   GTK_OBJECT (fs));

	gtk_widget_show (GTK_WIDGET (fs));
	gtk_grab_add (GTK_WIDGET (fs)); /* Yes, it is modal, so sue me */
}

static void
save_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	GnomeCalendar *gcal;
	gchar *fname;

	gcal = GNOME_CALENDAR (gtk_object_get_user_data (GTK_OBJECT (fs)));
	gtk_window_set_wmclass (GTK_WINDOW (gcal), "gnomecal", "gnomecal");

	fname = g_strdup (gtk_file_selection_get_filename (fs));
	g_free(fname);
	gtk_main_quit ();
}

static gint
close_save (GtkWidget *w)
{
	gtk_main_quit ();
	return TRUE;
}

static void
save_as_calendar_cmd (BonoboUIComponent *uic, gpointer data, const char *path)
{
	GnomeCalendar *gcal;
	GtkFileSelection *fs;

	gcal = GNOME_CALENDAR (data);

	fs = GTK_FILE_SELECTION (gtk_file_selection_new (_("Save calendar")));
	gtk_object_set_user_data (GTK_OBJECT (fs), gcal);

	gtk_signal_connect (GTK_OBJECT (fs->ok_button), "clicked",
			    (GtkSignalFunc) save_ok,
			    fs);
	gtk_signal_connect_object (GTK_OBJECT (fs->cancel_button), "clicked",
				   (GtkSignalFunc) close_save,
				   GTK_OBJECT (fs));
	gtk_signal_connect_object (GTK_OBJECT (fs), "delete_event",
				   GTK_SIGNAL_FUNC (close_save),
				   GTK_OBJECT (fs));
	gtk_widget_show (GTK_WIDGET (fs));
	gtk_grab_add (GTK_WIDGET (fs)); /* Yes, it is modal, so sue me even more */
	gtk_main ();
	gtk_widget_destroy (GTK_WIDGET (fs));
}

static void
properties_cmd (BonoboUIComponent *uic, gpointer data, const char *path)
{
	if (!preferences_dialog)
		preferences_dialog = cal_prefs_dialog_new ();
	else
		cal_prefs_dialog_show (preferences_dialog);
}


static BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("CalendarNew", new_calendar_cmd),
	BONOBO_UI_VERB ("CalendarOpen", open_calendar_cmd),
	BONOBO_UI_VERB ("CalendarSaveAs", save_as_calendar_cmd),
	BONOBO_UI_VERB ("CalendarPrint", file_print_cb),
	BONOBO_UI_VERB ("CalendarPrintPreview", file_print_preview_cb),
	BONOBO_UI_VERB ("EditNewAppointment", new_appointment_cb),
	BONOBO_UI_VERB ("EditNewEvent", new_event_cb),
	BONOBO_UI_VERB ("CalendarPreferences", properties_cmd),
		  
	BONOBO_UI_VERB ("CalendarPrev", previous_clicked),
	BONOBO_UI_VERB ("CalendarToday", today_clicked),
	BONOBO_UI_VERB ("CalendarNext", next_clicked),
	BONOBO_UI_VERB ("CalendarGoto", goto_clicked),
		  
	BONOBO_UI_VERB ("ShowDayView", show_day_view_clicked),
	BONOBO_UI_VERB ("ShowWorkWeekView", show_work_week_view_clicked),
	BONOBO_UI_VERB ("ShowWeekView", show_week_view_clicked),
	BONOBO_UI_VERB ("ShowMonthView", show_month_view_clicked),

	BONOBO_UI_VERB_END
};

static void
set_pixmap (BonoboUIComponent *uic,
	    const char        *xml_path,
	    const char        *icon)
{
	char *path;
	GdkPixbuf *pixbuf;

	path = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution", icon);

	pixbuf = gdk_pixbuf_new_from_file (path);
	if (pixbuf == NULL) {
		g_warning ("Cannot load image -- %s", path);
		g_free (path);
		return;
	}

	bonobo_ui_util_set_pixbuf (uic, xml_path, pixbuf);

	gdk_pixbuf_unref (pixbuf);

	g_free (path);
}

static void
update_pixmaps (BonoboUIComponent *uic)
{
	set_pixmap (uic, "/Toolbar/New",	  "buttons/new_appointment.png");

	set_pixmap (uic, "/Toolbar/DayView",	  "buttons/dayview.xpm");
	set_pixmap (uic, "/Toolbar/WorkWeekView", "buttons/workweekview.xpm");
	set_pixmap (uic, "/Toolbar/WeekView",	  "buttons/weekview.xpm");
	set_pixmap (uic, "/Toolbar/MonthView",	  "buttons/monthview.xpm");
}

void
calendar_control_activate (BonoboControl *control,
			   GnomeCalendar *cal)
{
	Bonobo_UIContainer remote_uih;
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	remote_uih = bonobo_control_get_remote_ui_container (control);
	bonobo_ui_component_set_container (uic, remote_uih);
	bonobo_object_release_unref (remote_uih, NULL);

#if 0
	/* FIXME: Need to update this to use new Bonobo ui stuff somehow.
	   Also need radio buttons really. */

	/* Note that these indices should correspond with the button indices
	   in the gnome_toolbar_view_buttons UIINFO struct. */
	gnome_calendar_set_view_buttons (cal,
					 gnome_toolbar_view_buttons[0].widget,
					 gnome_toolbar_view_buttons[1].widget,
					 gnome_toolbar_view_buttons[2].widget,
					 gnome_toolbar_view_buttons[3].widget);

	/* This makes the appropriate radio button in the toolbar active. */
	gnome_calendar_update_view_buttons (cal);
#endif
	
	bonobo_ui_component_add_verb_list_with_data (
		uic, verbs, cal);

	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, EVOLUTION_DATADIR,
			       "evolution-calendar.xml",
			       "evolution-calendar");

	update_pixmaps (uic);

	bonobo_ui_component_thaw (uic, NULL);
}

void
calendar_control_deactivate (BonoboControl *control)
{
	BonoboUIComponent *uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

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
