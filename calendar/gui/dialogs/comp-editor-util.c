/* Evolution calendar - Widget utilities
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <ical.h>
#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <e-util/e-time-utils.h>
#include "../calendar-config.h"
#include "comp-editor-util.h"



/**
 * comp_editor_dates:
 * @dates: A structure to be filled out with dates of a component
 * @comp: The component to extract the dates from
 * 
 * Extracts the dates from the calendar component into the
 * CompEditorPageDates structure. Note that it returns pointers to static
 * structs, so these will be overwritten in the next call.
 **/
void
comp_editor_dates (CompEditorPageDates *dates, CalComponent *comp)
{
	static struct icaltimetype start;
	static struct icaltimetype end;
	static struct icaltimetype due;
	static struct icaltimetype complete;

	CalComponentDateTime dt;
	struct icaltimetype *comp_complete;


	dates->start = NULL;
	dates->end = NULL;
	dates->due = NULL;
	dates->complete = NULL;
	
	cal_component_get_dtstart (comp, &dt);
	if (dt.value) {
		start = *dt.value;
		dates->start = &start;
	}
	cal_component_free_datetime (&dt);

	cal_component_get_dtend (comp, &dt);
	if (dt.value) {
		end = *dt.value;
		dates->end = &end;
	}
	cal_component_free_datetime (&dt);

	cal_component_get_due (comp, &dt);
	if (dt.value) {
		due = *dt.value;
		dates->due = &due;
	}
	cal_component_free_datetime (&dt);

	cal_component_get_completed (comp, &comp_complete);
	if (comp_complete) {
		complete = *comp_complete;
		dates->complete = &complete;
	}
	cal_component_free_icaltimetype (comp_complete);
}

static void
write_label_piece (struct icaltimetype *tt, char *buffer, int size,
		   char *stext, char *etext)
{
	struct tm tmp_tm;
	int len;
	
	/* FIXME: May want to convert the time to an appropriate zone. */

	if (stext != NULL)
		strcat (buffer, stext);

	tmp_tm.tm_year = tt->year - 1900;
	tmp_tm.tm_mon = tt->month - 1;
	tmp_tm.tm_mday = tt->day;
	tmp_tm.tm_hour = tt->hour;
	tmp_tm.tm_min = tt->minute;
	tmp_tm.tm_sec = tt->second;
	tmp_tm.tm_isdst = -1;

	len = strlen (buffer);
	e_time_format_date_and_time (&tmp_tm,
				     calendar_config_get_24_hour_format (), 
				     FALSE, FALSE,
				     &buffer[len], size - len);
	if (etext != NULL)
		strcat (buffer, etext);
}

/**
 * comp_editor_date_label:
 * @dates: The dates to use in constructing a label
 * @label: The label whose text is to be set
 * 
 * Set the text of a label based on the dates available and the user's
 * formatting preferences
 **/
void
comp_editor_date_label (CompEditorPageDates *dates, GtkWidget *label)
{
	char buffer[1024];
	gboolean start_set = FALSE, end_set = FALSE;
	gboolean complete_set = FALSE, due_set = FALSE;

	buffer[0] = '\0';

	if (dates->start && !icaltime_is_null_time (*dates->start))
		start_set = TRUE;
	if (dates->end && !icaltime_is_null_time (*dates->end))
		end_set = TRUE;
	if (dates->complete && !icaltime_is_null_time (*dates->complete))
		complete_set = TRUE;
	if (dates->due && !icaltime_is_null_time (*dates->due))
		due_set = TRUE;

	if (start_set)
		write_label_piece (dates->start, buffer, 1024, NULL, NULL);

	if (start_set && end_set)
		write_label_piece (dates->end, buffer, 1024, _(" to "), NULL);

	if (complete_set) {
		if (start_set)
			write_label_piece (dates->complete, buffer, 1024, _(" (Completed "), ")");
		else
			write_label_piece (dates->complete, buffer, 1024, _("Completed "), NULL);
	}
	
	if (due_set && dates->complete == NULL) {
		if (start_set)
			write_label_piece (dates->due, buffer, 1024, _(" (Due "), ")");
		else
			write_label_piece (dates->due, buffer, 1024, _("Due "), NULL);
	}

	gtk_label_set_text (GTK_LABEL (label), buffer);
}

/**
 * comp_editor_new_date_edit:
 * @show_date: Whether to show a date picker in the widget.
 * @show_time: Whether to show a time picker in the widget.
 * 
 * Creates a new #EDateEdit widget, configured using the calendar's preferences.
 * 
 * Return value: A newly-created #EDateEdit widget.
 **/
GtkWidget *
comp_editor_new_date_edit (gboolean show_date, gboolean show_time)
{
	EDateEdit *dedit;

	dedit = E_DATE_EDIT (e_date_edit_new ());

	e_date_edit_set_show_date (dedit, show_date);
	e_date_edit_set_show_time (dedit, show_time);

	calendar_config_configure_e_date_edit (dedit);

	return GTK_WIDGET (dedit);
}


/* Returns the current time, for EDateEdit widgets and ECalendar items in the
   dialogs.
   FIXME: Should probably use the timezone from somewhere in the component
   rather than the current timezone. */
struct tm
comp_editor_get_current_time (GtkObject *object, gpointer data)
{
	char *location;
	icaltimezone *zone;
	struct icaltimetype tt;
	struct tm tmp_tm = { 0 };

	/* Get the current timezone. */
	location = calendar_config_get_timezone ();
	zone = icaltimezone_get_builtin_timezone (location);

	tt = icaltime_from_timet_with_zone (time (NULL), FALSE, zone);

	/* Now copy it to the struct tm and return it. */
	tmp_tm.tm_year  = tt.year - 1900;
	tmp_tm.tm_mon   = tt.month - 1;
	tmp_tm.tm_mday  = tt.day;
	tmp_tm.tm_hour  = tt.hour;
	tmp_tm.tm_min   = tt.minute;
	tmp_tm.tm_sec   = tt.second;
	tmp_tm.tm_isdst = -1;

	return tmp_tm;
}
