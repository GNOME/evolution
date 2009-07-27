/*
 * Evolution calendar - Widget utilities
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <string.h>
#include <libical/ical.h>
#include <glib/gi18n.h>
#include <libedataserver/e-time-utils.h>
#include <libecal/e-cal-time-util.h>
#include "../calendar-config.h"
#include "../e-date-edit-config.h"
#include "../itip-utils.h"
#include "comp-editor-util.h"



/**
 * comp_editor_dates:
 * @dates: A structure to be filled out with dates of a component
 * @comp: The component to extract the dates from
 *
 * Extracts the dates from the calendar component into the
 * CompEditorPageDates structure. Call comp_editor_free_dates() to free the
 * results.
 **/
void
comp_editor_dates (CompEditorPageDates *dates, ECalComponent *comp)
{
	ECalComponentDateTime dt;

	dates->start = NULL;
	dates->end = NULL;
	dates->due = NULL;
	dates->complete = NULL;

	/* Note that the ECalComponentDateTime's returned contain allocated
	   icaltimetype and tzid values, so we just take over ownership of
	   those. */
	e_cal_component_get_dtstart (comp, &dt);
	if (dt.value) {
		dates->start = g_new (ECalComponentDateTime, 1);
		*dates->start = dt;
	}

	e_cal_component_get_dtend (comp, &dt);
	if (dt.value) {
		dates->end = g_new (ECalComponentDateTime, 1);
		*dates->end = dt;
	}

	e_cal_component_get_due (comp, &dt);
	if (dt.value) {
		dates->due = g_new (ECalComponentDateTime, 1);
		*dates->due = dt;
	}

	e_cal_component_get_completed (comp, &dates->complete);
}

/* This frees the dates in the CompEditorPageDates struct. But it doesn't free
 * the struct (as that is usually static).
 */
void
comp_editor_free_dates (CompEditorPageDates *dates)
{
	/* Note that e_cal_component_free_datetime() only frees the fields in
	   the struct. It doesn't free the struct itself, so we do that. */
	if (dates->start) {
		e_cal_component_free_datetime (dates->start);
		g_free (dates->start);
	}

	if (dates->end) {
		e_cal_component_free_datetime (dates->end);
		g_free (dates->end);
	}

	if (dates->due) {
		e_cal_component_free_datetime (dates->due);
		g_free (dates->due);
	}

	if (dates->complete)
		e_cal_component_free_icaltimetype (dates->complete);
}

/* dtstart is only passed in if tt is the dtend. */
static void
write_label_piece (struct icaltimetype *tt,
                   gchar *buffer,
                   gint size,
                   gchar *stext,
                   const gchar *etext,
                   struct icaltimetype *dtstart)
{
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt_copy = *tt;
	gint len;

	/* FIXME: May want to convert the time to an appropriate zone. */

	if (stext != NULL)
		strcat (buffer, stext);

	/* If we are writing the DTEND (i.e. DTSTART is set), and
	   DTEND > DTSTART, subtract 1 day. The DTEND date is not inclusive. */
	if (tt_copy.is_date && dtstart
	    && icaltime_compare_date_only (tt_copy, *dtstart) > 0) {
		icaltime_adjust (&tt_copy, -1, 0, 0, 0);
	}

	tmp_tm.tm_year = tt_copy.year - 1900;
	tmp_tm.tm_mon = tt_copy.month - 1;
	tmp_tm.tm_mday = tt_copy.day;
	tmp_tm.tm_hour = tt_copy.hour;
	tmp_tm.tm_min = tt_copy.minute;
	tmp_tm.tm_sec = tt_copy.second;
	tmp_tm.tm_isdst = -1;

	tmp_tm.tm_wday = time_day_of_week (tt_copy.day, tt_copy.month - 1,
					   tt_copy.year);

	len = strlen (buffer);
	e_time_format_date_and_time (&tmp_tm,
				     calendar_config_get_24_hour_format (),
				     !tt_copy.is_date, FALSE,
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
	gchar buffer[1024];
	gboolean start_set = FALSE, end_set = FALSE;
	gboolean complete_set = FALSE, due_set = FALSE;

	buffer[0] = '\0';

	if (dates->start && !icaltime_is_null_time (*dates->start->value))
		start_set = TRUE;
	if (dates->end && !icaltime_is_null_time (*dates->end->value))
		end_set = TRUE;
	if (dates->complete && !icaltime_is_null_time (*dates->complete))
		complete_set = TRUE;
	if (dates->due && !icaltime_is_null_time (*dates->due->value))
		due_set = TRUE;

	if (start_set)
		write_label_piece (dates->start->value, buffer, 1024,
				   NULL, NULL, NULL);

	if (start_set && end_set)
		write_label_piece (dates->end->value, buffer, 1024,
				   _(" to "), NULL, dates->start->value);

	if (complete_set) {
		if (start_set)
			write_label_piece (dates->complete, buffer, 1024, _(" (Completed "), ")", NULL);
		else
			write_label_piece (dates->complete, buffer, 1024, _("Completed "), NULL, NULL);
	}

	if (due_set && dates->complete == NULL) {
		if (start_set)
			write_label_piece (dates->due->value, buffer, 1024, _(" (Due "), ")", NULL);
		else
			write_label_piece (dates->due->value, buffer, 1024, _("Due "), NULL, NULL);
	}

	gtk_label_set_text (GTK_LABEL (label), buffer);
}

static void
date_edit_destroy_cb (EDateEdit *date_edit, gpointer data)
{
	EDateEditConfig *config = data;

	g_object_unref (config);
}

/**
 * comp_editor_new_date_edit:
 * @show_date: Whether to show a date picker in the widget.
 * @show_time: Whether to show a time picker in the widget.
 * @make_time_insensitive: Whether the time field is made insensitive rather
 * than hiding it. This is useful if you want to preserve the layout of the
 * widgets.
 *
 * Creates a new #EDateEdit widget, configured using the calendar's preferences.
 *
 * Return value: A newly-created #EDateEdit widget.
 **/
GtkWidget *
comp_editor_new_date_edit (gboolean show_date, gboolean show_time,
			   gboolean make_time_insensitive)
{
	EDateEdit *dedit;
	EDateEditConfig *config;

	dedit = E_DATE_EDIT (e_date_edit_new ());

	e_date_edit_set_show_date (dedit, show_date);
	e_date_edit_set_show_time (dedit, show_time);
#if 0
	e_date_edit_set_make_time_insensitive (dedit, make_time_insensitive);
#else
	e_date_edit_set_make_time_insensitive (dedit, FALSE);
#endif

	config = e_date_edit_config_new (dedit);
	g_signal_connect (G_OBJECT (dedit), "destroy", G_CALLBACK (date_edit_destroy_cb), config);

	return GTK_WIDGET (dedit);
}

/* Returns the current time, for EDateEdit widgets and ECalendar items in the
   dialogs.
   FIXME: Should probably use the timezone from somewhere in the component
   rather than the current timezone. */
struct tm
comp_editor_get_current_time (GtkObject *object, gpointer data)
{
	icaltimezone *zone;
	struct icaltimetype tt;
	struct tm tmp_tm = { 0 };

	/* Get the current timezone. */
	zone = calendar_config_get_icaltimezone ();

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

/**
 * comp_editor_strip_categories:
 * @categories: A string of category names entered by the user.
 *
 * Takes a string of the form "categ, categ, categ, ..." and removes the
 * whitespace between categories to result in "categ,categ,categ,..."
 *
 * Return value: The category names stripped of surrounding whitespace
 * and separated with commas.
 **/
gchar *
comp_editor_strip_categories (const gchar *categories)
{
	gchar *new_categories;
	const gchar *start, *end;
	const gchar *p;
	gchar *new_p;

	if (!categories)
		return NULL;

	new_categories = g_new (char, strlen (categories) + 1);

	start = end = NULL;
	new_p = new_categories;

	for (p = categories; *p; p = g_utf8_next_char (p)) {
		gunichar c;

		c = g_utf8_get_char (p);

		if (g_unichar_isspace (c))
			continue;
		else if (c == ',') {
			gint len;

			if (!start)
				continue;

			g_return_val_if_fail (start <= end, NULL);

			len = end - start + 1;
			strncpy (new_p, start, len);
			new_p[len] = ',';
			new_p += len + 1;

			start = end = NULL;
		} else {
			if (!start) {
				start = p;
				end = p;
			} else
				end = g_utf8_next_char(p) - 1;
		}
	}

	if (start) {
		gint len;

		g_return_val_if_fail (start <= end, NULL);

		len = end - start + 1;
		strncpy (new_p, start, len);
		new_p += len;
	}

	*new_p = '\0';

	return new_categories;
}

static GSList *
manage_new_attendees (const GSList *lst, const gchar *eml, gboolean add)
{
	GSList *copy = NULL;
	const GSList *l;
	gboolean found = FALSE;

	g_return_val_if_fail (eml != NULL, NULL);

	for (l = lst; l; l = l->next) {
		const gchar *eml2 = l->data;

		if (!eml2)
			continue;

		if (g_ascii_strcasecmp (eml, eml2) == 0) {
			found = TRUE;
			if (add)
				copy = g_slist_append (copy, g_strdup (eml2));
		} else {
			copy = g_slist_append (copy, g_strdup (eml2));
		}
	}

	if (!found && add) {
		copy = g_slist_append (copy, g_strdup (eml));
	}

	return copy;
}

static void
free_slist_strs (gpointer data)
{
	GSList *lst = data;

	if (lst) {
		g_slist_foreach (lst, (GFunc) g_free, NULL);
		g_slist_free (lst);
	}
}

/**
 * comp_editor_manage_new_attendees:
 * Manages the 'new-attendees' string of new attendees of the component.
 * @param comp: The component.
 * @param ma: An attendee.
 * @param add: TRUE to add attendee's email to new-attendees, FALSE to remove from it.
 *
 * @note The list is just string of emails separated by ';'
 **/
void
comp_editor_manage_new_attendees (ECalComponent *comp, EMeetingAttendee *ma, gboolean add)
{
	const gchar *eml;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (ma != NULL);

	eml = e_meeting_attendee_get_address (ma);
	if (eml)
		eml = itip_strip_mailto (eml);
	g_return_if_fail (eml != NULL);

	g_object_set_data_full (G_OBJECT (comp), "new-attendees", manage_new_attendees (g_object_get_data (G_OBJECT (comp), "new-attendees"), eml, add), free_slist_strs);
}

/**
 * comp_editor_copy_new_attendees:
 * Copies "new-attendees" information from src to des component.
 * @param des: Component, to copy to.
 * @param src: Component, to copy from.
 **/
void
comp_editor_copy_new_attendees (ECalComponent *des, ECalComponent *src)
{
	GSList *copy = NULL, *l;

	g_return_if_fail (src != NULL);
	g_return_if_fail (des != NULL);

	for (l = g_object_get_data (G_OBJECT (src), "new-attendees"); l; l = l->next) {
		copy = g_slist_append (copy, g_strdup (l->data));
	}

	g_object_set_data_full (G_OBJECT (des), "new-attendees", copy, free_slist_strs);
}

/**
 * comp_editor_have_in_new_attendees:
 * @param comp: Component with the "new-attendees" possibly set.
 * @param ma: Meeting attendee to check.
 * @return Whether ma is present in the list of new attendees of the comp.
 **/
gboolean
comp_editor_have_in_new_attendees (ECalComponent *comp, EMeetingAttendee *ma)
{
	const gchar *eml;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (ma != NULL, FALSE);

	eml = e_meeting_attendee_get_address (ma);
	if (eml)
		eml = itip_strip_mailto (eml);
	g_return_val_if_fail (eml != NULL, FALSE);

	return comp_editor_have_in_new_attendees_lst (g_object_get_data (G_OBJECT (comp), "new-attendees"), eml);
}

/**
 * comp_editor_have_in_new_attendees_lst:
 * Same as @ref comp_editor_have_in_new_attendees only parameters are direct GSList and string.
 **/
gboolean
comp_editor_have_in_new_attendees_lst (const GSList *new_attendees, const gchar *eml)
{
	const GSList *l;

	if (!eml)
		return FALSE;

	for (l = new_attendees; l; l = l->next) {
		if (l->data && g_ascii_strcasecmp (eml, l->data) == 0)
			return TRUE;
	}

	return FALSE;
}
