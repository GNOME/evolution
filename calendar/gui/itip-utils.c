/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *  JP Rosevear <jpr@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-object-client.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo-conf/bonobo-config-database.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <gtk/gtkwidget.h>
#include <gal/util/e-util.h>
#include <e-util/e-unicode-i18n.h>
#include <ical.h>
#include <Evolution-Composer.h>
#include <e-util/e-time-utils.h>
#include <cal-util/timeutil.h>
#include <cal-util/cal-util.h>
#include "calendar-config.h"
#include "itip-utils.h"

#define GNOME_EVOLUTION_COMPOSER_OAFIID "OAFIID:GNOME_Evolution_Mail_Composer"

static gchar *itip_methods[] = {
	"PUBLISH",
	"REQUEST",
	"REPLY",
	"ADD",
	"CANCEL",
	"RERESH",
	"COUNTER",
	"DECLINECOUNTER"
};

static icalproperty_method itip_methods_enum[] = {
    ICAL_METHOD_PUBLISH,
    ICAL_METHOD_REQUEST,
    ICAL_METHOD_REPLY,
    ICAL_METHOD_ADD,
    ICAL_METHOD_CANCEL,
    ICAL_METHOD_REFRESH,
    ICAL_METHOD_COUNTER,
    ICAL_METHOD_DECLINECOUNTER,
};

static void
error_dialog (gchar *str) 
{
	GtkWidget *dlg;
	
	dlg = gnome_error_dialog (str);
	gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
}

GList *
itip_addresses_get (void)
{
	static Bonobo_ConfigDatabase db = NULL;
	CORBA_Environment ev;
	GList *addresses = NULL;
	glong len, def, i;

	if (db == NULL) {
		CORBA_exception_init (&ev);
 
		db = bonobo_get_object ("wombat:", 
					"Bonobo/ConfigDatabase", 
					&ev);
	
		if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
			CORBA_exception_free (&ev);
			return NULL;
		}
		
		CORBA_exception_free (&ev);
	}
	
	len = bonobo_config_get_long_with_default (db, "/Mail/Accounts/num", 0, NULL);
	def = bonobo_config_get_long_with_default (db, "/Mail/Accounts/default_account", 0, NULL);

	for (i = 0; i < len; i++) {
		ItipAddress *a;
		gchar *path;
		
		a = g_new0 (ItipAddress, 1);

		/* get the identity info */
		path = g_strdup_printf ("/Mail/Accounts/identity_name_%d", i);
		a->name = bonobo_config_get_string (db, path, NULL);
		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/identity_address_%d", i);
		a->address = bonobo_config_get_string (db, path, NULL);
		g_free (path);

		if (i == def)
			a->default_address = TRUE;

		a->full = g_strdup_printf ("%s <%s>", a->name, a->address);
		addresses = g_list_append (addresses, a);
	}

	return addresses;
}

void
itip_addresses_free (GList *addresses)
{
	GList *l;
	
	for (l = addresses; l != NULL; l = l->next) {
		ItipAddress *a = l->data;

		g_free (a->name);
		g_free (a->address);
		g_free (a->full);
		g_free (a);
	}
	g_list_free (addresses);
}

const gchar *
itip_strip_mailto (const gchar *address) 
{
	const gchar *text;
	
	if (address == NULL)
		return NULL;
	
	text = e_strstrcase (address, "mailto:");
	if (text != NULL && strlen (address) > 7)
		address += 7;

	return address;
}

static char *
get_label (struct icaltimetype *tt)
{
	char buffer[1000];
	struct tm tmp_tm = { 0 };
	
	tmp_tm.tm_year = tt->year - 1900;
	tmp_tm.tm_mon = tt->month - 1;
	tmp_tm.tm_mday = tt->day;
	tmp_tm.tm_hour = tt->hour;
	tmp_tm.tm_min = tt->minute;
	tmp_tm.tm_sec = tt->second;
	tmp_tm.tm_isdst = -1;

	tmp_tm.tm_wday = time_day_of_week (tt->day, tt->month - 1, tt->year);

	e_time_format_date_and_time (&tmp_tm,
				     calendar_config_get_24_hour_format (), 
				     FALSE, FALSE,
				     buffer, 1000);
	
	return g_strdup (buffer);
}

static void
foreach_tzid_callback (icalparameter *param, gpointer data)
{
	icalcomponent *icomp = data;
	const char *tzid;
	icaltimezone *zone;
	icalcomponent *vtimezone_comp;

	/* Get the TZID string from the parameter. */
	tzid = icalparameter_get_tzid (param);
	if (!tzid)
		return;

	/* Check if it is a builtin timezone. If it isn't, return. */
	zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	if (!zone)
		return;

	/* Convert it to a string and add it to the hash. */
	vtimezone_comp = icaltimezone_get_component (zone);
	if (!vtimezone_comp)
		return;

	icalcomponent_add_component (icomp, icalcomponent_new_clone (vtimezone_comp));
}

void
itip_send_comp (CalComponentItipMethod method, CalComponent *comp)
{
	BonoboObjectClient *bonobo_server;
	GNOME_Evolution_Composer composer_server;
	CORBA_Environment ev;
	GSList *attendees, *l;
	GNOME_Evolution_Composer_RecipientList *to_list, *cc_list, *bcc_list;
	GNOME_Evolution_Composer_Recipient *recipient;
	CORBA_char *subject;
	gint cntr, len;
	CalComponentVType type;	
	CalComponentText caltext;
	CalComponentOrganizer organizer;
	CORBA_char *content_type, *filename, *description;
	GNOME_Evolution_Composer_AttachmentData *attach_data;
	CORBA_boolean show_inline;
	char tempstr[200];
	
	CORBA_exception_init (&ev);

	/* Obtain an object reference for the Composer. */
	bonobo_server = bonobo_object_activate (GNOME_EVOLUTION_COMPOSER_OAFIID, 0);
	g_return_if_fail (bonobo_server != NULL);

	composer_server = BONOBO_OBJREF (bonobo_server);

	/* Type for later use */
	type = cal_component_get_vtype (comp);

	/* Create list of recipients */
	switch (method) {
	case CAL_COMPONENT_METHOD_REQUEST:
	case CAL_COMPONENT_METHOD_CANCEL:
		cal_component_get_attendee_list (comp, &attendees);
		len = g_slist_length (attendees);
		
		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = len;
		to_list->_length = len;
		to_list->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (len);
		
		for (cntr = 0, l = attendees; cntr < len; cntr++, l = l->next) {
			CalComponentAttendee *att = l->data;
			
			recipient = &(to_list->_buffer[cntr]);
			if (att->cn)
				recipient->name = CORBA_string_dup (att->cn);
			else
				recipient->name = CORBA_string_dup ("");
			recipient->address = CORBA_string_dup (itip_strip_mailto (att->value));
		}
		break;

	case CAL_COMPONENT_METHOD_REPLY:
	case CAL_COMPONENT_METHOD_ADD:
	case CAL_COMPONENT_METHOD_REFRESH:
	case CAL_COMPONENT_METHOD_COUNTER:
	case CAL_COMPONENT_METHOD_DECLINECOUNTER:
		cal_component_get_organizer (comp, &organizer);
		if (organizer.value == NULL) {
			error_dialog (_("An organizer must be set."));
			return;
		}
		
		len = 1;

		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = len;
		to_list->_length = len;
		to_list->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (len);
		recipient = &(to_list->_buffer[0]);

		if (organizer.cn != NULL)
			recipient->name = CORBA_string_dup (organizer.cn);
		else
			recipient->name = CORBA_string_dup ("");
		recipient->address = CORBA_string_dup (itip_strip_mailto (organizer.value));
		break;

	default:
		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = to_list->_length = 0;
		break;
	}

	cc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
	cc_list->_maximum = cc_list->_length = 0;
	bcc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
	bcc_list->_maximum = bcc_list->_length = 0;
	
	/* Subject information */
	cal_component_get_summary (comp, &caltext);
	if (caltext.value != NULL) {		
		subject = CORBA_string_dup (caltext.value);
	} else {
		switch (type) {
		case CAL_COMPONENT_EVENT:
			subject = CORBA_string_dup ("Event information");
			break;
		case CAL_COMPONENT_TODO:
			subject = CORBA_string_dup ("Task information");
			break;
		case CAL_COMPONENT_JOURNAL:
			subject = CORBA_string_dup ("Journal information");
			break;
		case CAL_COMPONENT_FREEBUSY:
			subject = CORBA_string_dup ("Free/Busy information");
			break;
		default:
			subject = CORBA_string_dup ("Calendar information");
		}		
	}
	
	/* Set recipients, subject */
	GNOME_Evolution_Composer_setHeaders (composer_server, to_list, cc_list, bcc_list, subject, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Unable to set composer headers while sending iTip message");
		CORBA_exception_free (&ev);
		return;
	}

	/* Content type, suggested file name, description */
	sprintf (tempstr, "text/calendar;METHOD=%s", itip_methods[method]);
	content_type = CORBA_string_dup (tempstr);
	if (type == CAL_COMPONENT_FREEBUSY)
		filename = CORBA_string_dup ("freebusy.ifb");
	else
		filename = CORBA_string_dup ("calendar.ics");
	switch (type) {
	case CAL_COMPONENT_EVENT:
		description = CORBA_string_dup ("Event information");
		break;
	case CAL_COMPONENT_TODO:
		description = CORBA_string_dup ("Task information");
		break;
	case CAL_COMPONENT_JOURNAL:
		description = CORBA_string_dup ("Journal information");
		break;
	case CAL_COMPONENT_FREEBUSY:
	{
		CalComponentDateTime dt;
		char *start = NULL, *end = NULL;
		
		cal_component_get_dtstart (comp, &dt);
		if (dt.value) {
			start = get_label (dt.value);
			cal_component_get_dtend (comp, &dt);
			if (dt.value)
				end = get_label (dt.value);
		}
		if (start != NULL && end != NULL) {
			snprintf (tempstr, 200, "Free/Busy information (%s to %s)", start, end);
			description = CORBA_string_dup (tempstr);
			g_free (start);
			g_free (end);
		} else {
			description = CORBA_string_dup ("Free/Busy information");
		}
	}
		break;
	default:
		description = CORBA_string_dup ("iCalendar information");
	}	
	show_inline = FALSE;

	/* Create a top level component, and add our component */
	{
		CalComponent *clone;		
		icalcomponent *icomp, *iclone;
		icalproperty *prop;
		icalvalue *value;
		gchar *ical_string;

		icomp = cal_util_new_top_level ();

		prop = icalproperty_new (ICAL_METHOD_PROPERTY);
		value = icalvalue_new_method (itip_methods_enum[method]);
		icalproperty_set_value (prop, value);
		icalcomponent_add_property (icomp, prop);

		/* Strip alarms if necessary */
		clone = cal_component_clone (comp);
		if (method == CAL_COMPONENT_METHOD_REPLY
		    || method == CAL_COMPONENT_METHOD_CANCEL
		    || method == CAL_COMPONENT_METHOD_REFRESH
		    || method == CAL_COMPONENT_METHOD_DECLINECOUNTER)
			cal_component_remove_all_alarms (clone);
		
		iclone = cal_component_get_icalcomponent (clone);
		icalcomponent_add_component (icomp, iclone);

		icalcomponent_foreach_tzid (iclone, foreach_tzid_callback, icomp);

		ical_string = icalcomponent_as_ical_string (icomp);
		attach_data = GNOME_Evolution_Composer_AttachmentData__alloc ();
		attach_data->_maximum = attach_data->_length = strlen (ical_string);		
		attach_data->_buffer = CORBA_sequence_CORBA_char_allocbuf (attach_data->_length);	
		strcpy (attach_data->_buffer, ical_string);

		icalcomponent_free (icomp);
	}

	GNOME_Evolution_Composer_attachData (composer_server, 
					content_type, filename, description,
					show_inline, attach_data,
					&ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Unable to attach data to the composer while sending iTip message");
		CORBA_exception_free (&ev);
		return;
	}
	
	if (method == CAL_COMPONENT_METHOD_PUBLISH)
		GNOME_Evolution_Composer_show (composer_server, &ev);
	else
		GNOME_Evolution_Composer_send (composer_server, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Unable to show the composer while sending iTip message");
		CORBA_exception_free (&ev);
		return;
	}
	
	CORBA_exception_free (&ev);

	/* Beware--depending on whether CORBA_free is recursive, which I
	   think is is, we might have memory leaks, in which case the code
	   below is necessary. */
#if 0
	for (cntr = 0; cntr < priv->numentries; cntr++) {
		recipient = &(to_list->_buffer[cntr]);
		CORBA_free (recipient->name);
		CORBA_free (recipient->address);
		recipient->name = recipient->address = NULL;
	}
#endif

	if (CORBA_sequence_get_release (to_list) != FALSE)
		CORBA_free (to_list->_buffer);

	CORBA_free (to_list);
	CORBA_free (cc_list);
	CORBA_free (bcc_list);

	CORBA_free (subject);
	CORBA_free (content_type);
	CORBA_free (filename);
	CORBA_free (description);
	CORBA_free (attach_data);

	/* bonobo_object_unref (BONOBO_OBJECT (bonobo_server));   */
}

