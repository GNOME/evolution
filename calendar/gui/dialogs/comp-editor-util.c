/* Evolution calendar - Widget utilities
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
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
 * CompEditorPageDates structure
 **/
void
comp_editor_dates (CompEditorPageDates *dates, CalComponent *comp)
{
	CalComponentDateTime dt;
	struct icaltimetype *completed;

	dates->start = 0;
	dates->end = 0;
	dates->due = 0;
	dates->complete = 0;
	
	cal_component_get_dtstart (comp, &dt);
	if (dt.value)
		dates->start = icaltime_as_timet (*dt.value);

	cal_component_get_dtend (comp, &dt);
	if (dt.value)
		dates->end = icaltime_as_timet (*dt.value);

	cal_component_get_due (comp, &dt);
	if (dt.value)
		dates->due = icaltime_as_timet (*dt.value);

	cal_component_get_completed (comp, &completed);
	if (completed) {
		dates->complete = icaltime_as_timet (*completed);
		cal_component_free_icaltimetype (completed);
	}
}

static void
write_label_piece (time_t t, char *buffer, int size, char *stext, char *etext)
{
	struct tm *tmp_tm;
	int len;
	
	tmp_tm = localtime (&t);
	if (stext != NULL)
		strcat (buffer, stext);

	len = strlen (buffer);
	e_time_format_date_and_time (tmp_tm,
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
	static char buffer[1024];

	buffer[0] = '\0';

	if (dates->start > 0)
		write_label_piece (dates->start, buffer, 1024, NULL, NULL);

	if (dates->end > 0 && dates->start > 0)
		write_label_piece (dates->end, buffer, 1024, _(" to "), NULL);

	if (dates->complete > 0) {
		if (dates->start > 0)
			write_label_piece (dates->complete, buffer, 1024, _(" (Completed "), ")");
		else
			write_label_piece (dates->complete, buffer, 1024, _(" Completed "), NULL);
	}
	
	if (dates->due > 0 && dates->complete == 0) {
		if (dates->start > 0)
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
