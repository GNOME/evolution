/* Evolution calendar - Widget utilities
 *
 * Copyright (C) 2000 Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <string.h>
#include <ical.h>
#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <liboaf/liboaf.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>
#include <gal/unicode/gunicode.h>
#include <ebook/e-destination.h>
#include <e-util/e-time-utils.h>
#include <cal-util/timeutil.h>
#include "../calendar-config.h"
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
comp_editor_dates (CompEditorPageDates *dates, CalComponent *comp)
{
	CalComponentDateTime dt;

	dates->start = NULL;
	dates->end = NULL;
	dates->due = NULL;
	dates->complete = NULL;
	
	/* Note that the CalComponentDateTime's returned contain allocated
	   icaltimetype and tzid values, so we just take over ownership of
	   those. */
	cal_component_get_dtstart (comp, &dt);
	if (dt.value) {
		dates->start = g_new (CalComponentDateTime, 1);
		*dates->start = dt;
	}

	cal_component_get_dtend (comp, &dt);
	if (dt.value) {
		dates->end = g_new (CalComponentDateTime, 1);
		*dates->end = dt;
	}

	cal_component_get_due (comp, &dt);
	if (dt.value) {
		dates->due = g_new (CalComponentDateTime, 1);
		*dates->due = dt;
	}

	cal_component_get_completed (comp, &dates->complete);
}


/* This frees the dates in the CompEditorPageDates struct. But it doesn't free
 * the struct (as that is usually static).
 */
void
comp_editor_free_dates (CompEditorPageDates *dates)
{
	/* Note that cal_component_free_datetime() only frees the fields in
	   the struct. It doesn't free the struct itself, so we do that. */
	if (dates->start) {
		cal_component_free_datetime (dates->start);
		g_free (dates->start);
	}

	if (dates->end) {
		cal_component_free_datetime (dates->end);
		g_free (dates->end);
	}

	if (dates->due) {
		cal_component_free_datetime (dates->due);
		g_free (dates->due);
	}

	if (dates->complete)
		cal_component_free_icaltimetype (dates->complete);
}


/* dtstart is only passed in if tt is the dtend. */
static void
write_label_piece (struct icaltimetype *tt, char *buffer, int size,
		   char *stext, char *etext, struct icaltimetype *dtstart)
{
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt_copy = *tt;
	int len;
	
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
	char buffer[1024];
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

	dedit = E_DATE_EDIT (e_date_edit_new ());

	e_date_edit_set_show_date (dedit, show_date);
	e_date_edit_set_show_time (dedit, show_time);
#if 0
	e_date_edit_set_make_time_insensitive (dedit, make_time_insensitive);
#else
	e_date_edit_set_make_time_insensitive (dedit, FALSE);
#endif
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



/*
 * These are utility functions to handle the SelectNames control we use
 * for the contacts field, and its related dialog.
 */

#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames"

GNOME_Evolution_Addressbook_SelectNames
comp_editor_create_contacts_component (void)
{
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	corba_select_names = oaf_activate_from_id (SELECT_NAMES_OAFID, 0,
						   NULL, &ev);

	/* OAF seems to be broken -- it can return a CORBA_OBJECT_NIL without
           raising an exception in `ev'.  */
	if (ev._major != CORBA_NO_EXCEPTION
	    || corba_select_names == CORBA_OBJECT_NIL) {
		g_warning ("Cannot activate -- %s", SELECT_NAMES_OAFID);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);

	return corba_select_names;
}


GtkWidget *
comp_editor_create_contacts_control (GNOME_Evolution_Addressbook_SelectNames corba_select_names)
{
	Bonobo_Control corba_control;
	GtkWidget *control_widget;
	CORBA_Environment ev;
	char *name = _("Contacts");

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_addSection (
		corba_select_names, name, name, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	corba_control =
		GNOME_Evolution_Addressbook_SelectNames_getEntryBySection (
			corba_select_names, name, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	control_widget = bonobo_widget_new_control_from_objref (
		corba_control, CORBA_OBJECT_NIL);

	gtk_widget_show (control_widget);

	return control_widget;
}


Bonobo_EventSource_ListenerId
comp_editor_connect_contacts_changed (GtkWidget *contacts_entry,
				      BonoboListenerCallbackFn changed_cb,
				      gpointer changed_cb_data)
{
	BonoboControlFrame *cf;
	Bonobo_PropertyBag pb = CORBA_OBJECT_NIL;

	cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (contacts_entry));
	pb = bonobo_control_frame_get_control_property_bag (cf, NULL);

	return bonobo_event_source_client_add_listener (
		pb, changed_cb,
		"Bonobo/Property:change:entry_changed",
		NULL, changed_cb_data);
}


void
comp_editor_show_contacts_dialog (GNOME_Evolution_Addressbook_SelectNames corba_select_names)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	GNOME_Evolution_Addressbook_SelectNames_activateDialog (
		corba_select_names, _("Contacts"), &ev);
	CORBA_exception_free (&ev);
}


/* A simple 'name <email>' parser. Input should be UTF8.
   FIXME: Should probably use camel functions or something. */
static void
parse_contact_string (const char *value, char **name, char **email)
{
	char *lbracket, *rbracket, *name_end, *tmp_name, *tmp_email;

	if (!value) {
		*name = g_strdup ("");
		*email = g_strdup ("");
		return;
	}

	lbracket = g_utf8_strchr (value, '<');
	rbracket = g_utf8_strchr (value, '>');

	if (!lbracket || !rbracket || rbracket < lbracket) {
		*name = g_strdup (value);
		*email = g_strdup ("");
		return;
	}

	name_end = g_utf8_prev_char (lbracket);
	while (name_end > value && g_unichar_isspace (g_utf8_get_char (name_end)))
		name_end = g_utf8_prev_char (name_end);

	tmp_name = g_malloc (name_end - value + 2);
	strncpy (tmp_name, value, name_end - value + 1);
	tmp_name[name_end - value + 1] = '\0';
	*name = tmp_name;

	tmp_email = g_malloc (rbracket - lbracket);
	strncpy (tmp_email, lbracket + 1, rbracket - lbracket - 1);
	tmp_email[rbracket - lbracket - 1] = '\0';
	*email = tmp_email;

#if 0
	g_print ("Parsed: %s\n  Name:'%s'\nEmail:'%s'\n",
		 value, *name, *email);
#endif
}


void
comp_editor_contacts_to_widget (GtkWidget *contacts_entry,
				CalComponent *comp)
{
	GPtrArray *dest_array;
	EDestination *dest;
	GSList *contact_list, *elem;
	char *contacts_string;
	int i;

	cal_component_get_contact_list (comp, &contact_list);
	dest_array = g_ptr_array_new ();
	for (elem = contact_list; elem; elem = elem->next) {
		CalComponentText *t = elem->data;
		char *name, *email;

		parse_contact_string (t->value, &name, &email);

		dest = e_destination_new ();
		e_destination_set_name (dest, name);
		e_destination_set_email (dest, email);
		g_ptr_array_add (dest_array, dest);
		g_free (name);
		g_free (email);
	}
	cal_component_free_text_list (contact_list);

	/* we need destv to be NULL terminated */
	g_ptr_array_add (dest_array, NULL);

	contacts_string = e_destination_exportv ((EDestination**) dest_array->pdata);
#if 0
	g_print ("Destinations: %s\n", contacts_string ? contacts_string : "");
#endif

	bonobo_widget_set_property (BONOBO_WIDGET (contacts_entry),
				    "destinations", contacts_string, NULL);

	g_free (contacts_string);

	/* We free all dest_array except the last NULL we added. */
	for (i = 0; i < dest_array->len - 1; i++) {
		dest = g_ptr_array_index (dest_array, i);
		gtk_object_unref (GTK_OBJECT (dest));
	}
	g_ptr_array_free (dest_array, TRUE);
}


void
comp_editor_contacts_to_component (GtkWidget *contacts_entry,
				   CalComponent *comp)
{
	EDestination **contact_destv;
	GSList *contact_list = NULL, *elem;
	char *contacts_string = NULL;
	CalComponentText *t;
	const char *name, *email;
	int i;

	bonobo_widget_get_property (BONOBO_WIDGET (contacts_entry),
				    "destinations", &contacts_string, NULL);
#if 0
	g_print ("Contacts string: %s\n", contacts_string ? contacts_string : "");
#endif

	contact_destv = e_destination_importv (contacts_string);
	g_free (contacts_string);

	if (contact_destv) {
		for (i = 0; contact_destv[i] != NULL; i++) {
			name = e_destination_get_name (contact_destv[i]);
			email = e_destination_get_email (contact_destv[i]);

			t = g_new0 (CalComponentText, 1);
			t->altrep = NULL;

			/* If both name and email are given, use the standard
			   '"name" <email>' form, otherwise use just the name
			   or the email address.
			   FIXME: I'm not sure this is correct syntax etc. */
			if (name && name[0] && email && email[0])
				t->value = g_strdup_printf ("\"%s\" <%s>",
							    name, email);
			else if (name && name[0])
				t->value = g_strdup_printf ("\"%s\"",
							    name);
			else
				t->value = g_strdup_printf ("<%s>",
							    email);

			contact_list = g_slist_prepend (contact_list, t);

			gtk_object_unref (GTK_OBJECT (contact_destv[i]));
		}
	}
	g_free (contact_destv);

	contact_list = g_slist_reverse (contact_list);
	cal_component_set_contact_list (comp, contact_list);

	for (elem = contact_list; elem; elem = elem->next) {
		t = elem->data;
		g_free ((char*)t->value);
		g_free (t);
	}
	g_slist_free (contact_list);
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
char *
comp_editor_strip_categories (const char *categories)
{
	char *new_categories;
	const char *start, *end;
	const char *p;
	char *new_p;

	if (!categories)
		return NULL;

	new_categories = g_new (char, strlen (categories) + 1);

	start = end = NULL;
	new_p = new_categories;

	for (p = categories; *p; p++) {
		int c;

		c = *p;

		if (isspace (c))
			continue;
		else if (c == ',') {
			int len;

			if (!start)
				continue;

			g_assert (start <= end);

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
				end = p;
		}
	}

	if (start) {
		int len;

		g_assert (start <= end);

		len = end - start + 1;
		strncpy (new_p, start, len);
		new_p += len;
	}

	*new_p = '\0';

	return new_categories;
}
