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
		path = g_strdup_printf ("/Mail/Accounts/identity_name_%ld", i);
		a->name = bonobo_config_get_string (db, path, NULL);
		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/identity_address_%ld", i);
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

typedef struct {
	GHashTable *tzids;
	icalcomponent *icomp;	
} ItipUtilTZData;

static void
foreach_tzid_callback (icalparameter *param, gpointer data)
{
	ItipUtilTZData *tz_data = data;	
	const char *tzid;
	icaltimezone *zone;
	icalcomponent *vtimezone_comp;

	/* Get the TZID string from the parameter. */
	tzid = icalparameter_get_tzid (param);
	if (!tzid || g_hash_table_lookup (tz_data->tzids, tzid))
		return;

	/* Check if it is a builtin timezone. If it isn't, return. */
	zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	if (!zone)
		return;

	/* Convert it to a string and add it to the hash. */
	vtimezone_comp = icaltimezone_get_component (zone);
	if (!vtimezone_comp)
		return;

	icalcomponent_add_component (tz_data->icomp, icalcomponent_new_clone (vtimezone_comp));
	g_hash_table_insert (tz_data->tzids, (char *)tzid, (char *)tzid);	
}

static GNOME_Evolution_Composer_RecipientList *
comp_to_list (CalComponentItipMethod method, CalComponent *comp)
{
	GNOME_Evolution_Composer_RecipientList *to_list;
	GNOME_Evolution_Composer_Recipient *recipient;
	CalComponentOrganizer organizer;
	GSList *attendees, *l;
	gint cntr, len;

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
			return NULL;
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
	CORBA_sequence_set_release (to_list, TRUE);

	return to_list;	
}
	
static CORBA_char *
comp_subject (CalComponent *comp) 
{
	CalComponentText caltext;

	cal_component_get_summary (comp, &caltext);
	if (caltext.value != NULL)	
		return CORBA_string_dup (caltext.value);

	switch (cal_component_get_vtype (comp)) {
	case CAL_COMPONENT_EVENT:
		return CORBA_string_dup ("Event information");
	case CAL_COMPONENT_TODO:
		return CORBA_string_dup ("Task information");
	case CAL_COMPONENT_JOURNAL:
		return CORBA_string_dup ("Journal information");
	case CAL_COMPONENT_FREEBUSY:
		return CORBA_string_dup ("Free/Busy information");
	default:
		return CORBA_string_dup ("Calendar information");
	}		
}

static CORBA_char *
comp_content_type (CalComponentItipMethod method)
{
	char tmp[256];	

	sprintf (tmp, "text/calendar;METHOD=%s", itip_methods[method]);
	return CORBA_string_dup (tmp);

}

static CORBA_char *
comp_filename (CalComponent *comp)
{
	switch (cal_component_get_vtype (comp)) {
	case CAL_COMPONENT_FREEBUSY:
		return CORBA_string_dup ("freebusy.ifb");
	default:
		return CORBA_string_dup ("calendar.ics");
	}	
}

static CORBA_char *
comp_description (CalComponent *comp)
{
	CORBA_char *description;	
	CalComponentDateTime dt;
	char *start = NULL, *end = NULL;

	switch (cal_component_get_vtype (comp)) {
	case CAL_COMPONENT_EVENT:
		return CORBA_string_dup ("Event information");
	case CAL_COMPONENT_TODO:
		return CORBA_string_dup ("Task information");
	case CAL_COMPONENT_JOURNAL:
		return CORBA_string_dup ("Journal information");
	case CAL_COMPONENT_FREEBUSY:
		cal_component_get_dtstart (comp, &dt);
		if (dt.value) {
			start = get_label (dt.value);
			cal_component_get_dtend (comp, &dt);
			if (dt.value)
				end = get_label (dt.value);
		}
		if (start != NULL && end != NULL) {
			char *tmp = g_strdup_printf ("Free/Busy information (%s to %s)", start, end);
			description = CORBA_string_dup (tmp);
			g_free (tmp);			
		} else {
			description = CORBA_string_dup ("Free/Busy information");
		}
		g_free (start);
		g_free (end);
		return description;		
	default:
		return CORBA_string_dup ("iCalendar information");
	}
}

static char *
comp_string (CalComponentItipMethod method, CalComponent *comp)
{
	CalComponent *clone;		
	icalcomponent *icomp, *iclone;
	icalproperty *prop;
	icalvalue *value;
	gchar *ical_string;
	ItipUtilTZData tz_data;
		
	icomp = cal_util_new_top_level ();

	prop = icalproperty_new (ICAL_METHOD_PROPERTY);
	value = icalvalue_new_method (itip_methods_enum[method]);
	icalproperty_set_value (prop, value);
	icalcomponent_add_property (icomp, prop);

	/* Strip off attributes barred from appearing */
	clone = cal_component_clone (comp);
	switch (method) {
	case CAL_COMPONENT_METHOD_PUBLISH:
		cal_component_set_attendee_list (clone, NULL);
		break;			
	case CAL_COMPONENT_METHOD_REPLY:
	case CAL_COMPONENT_METHOD_CANCEL:
	case CAL_COMPONENT_METHOD_REFRESH:
	case CAL_COMPONENT_METHOD_DECLINECOUNTER:
		cal_component_remove_all_alarms (clone);
		break;
	default:
	}

	iclone = cal_component_get_icalcomponent (clone);
		
	/* Add the timezones */
	tz_data.tzids = g_hash_table_new (g_str_hash, g_str_equal);
	tz_data.icomp = icomp;		
	icalcomponent_foreach_tzid (iclone, foreach_tzid_callback, &tz_data);
	g_hash_table_destroy (tz_data.tzids);

	icalcomponent_add_component (icomp, iclone);
	ical_string = icalcomponent_as_ical_string (icomp);
	icalcomponent_remove_component (icomp, iclone);
	
	icalcomponent_free (icomp);
	gtk_object_unref (GTK_OBJECT (clone));	
	
	return ical_string;	
}

void
itip_send_comp (CalComponentItipMethod method, CalComponent *comp)
{
	BonoboObjectClient *bonobo_server;
	GNOME_Evolution_Composer composer_server;
	GNOME_Evolution_Composer_RecipientList *to_list = NULL;
	GNOME_Evolution_Composer_RecipientList *cc_list = NULL;
	GNOME_Evolution_Composer_RecipientList *bcc_list = NULL;
	CORBA_char *subject = NULL, *content_type = NULL;
	CORBA_char *filename = NULL, *description = NULL;
	GNOME_Evolution_Composer_AttachmentData *attach_data = NULL;
	CORBA_boolean show_inline;
	char *ical_string;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	/* Obtain an object reference for the Composer. */
	bonobo_server = bonobo_object_activate (GNOME_EVOLUTION_COMPOSER_OAFIID, 0);
	g_return_if_fail (bonobo_server != NULL);
	composer_server = BONOBO_OBJREF (bonobo_server);

	to_list = comp_to_list (method, comp);
	if (to_list == NULL)
		goto cleanup;
	
	cc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
	cc_list->_maximum = cc_list->_length = 0;
	bcc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
	bcc_list->_maximum = bcc_list->_length = 0;
	
	/* Subject information */
	subject = comp_subject (comp);
	
	/* Set recipients, subject */
	GNOME_Evolution_Composer_setHeaders (composer_server, to_list, cc_list, bcc_list, subject, &ev);
	if (BONOBO_EX (&ev)) {		
		g_warning ("Unable to set composer headers while sending iTip message");
		goto cleanup;
	}

	/* Content type, suggested file name, description */
	content_type = comp_content_type (method);
	filename = comp_filename (comp);	
	description = comp_description (comp);	
	show_inline = TRUE;

	ical_string = comp_string (method, comp);	
	attach_data = GNOME_Evolution_Composer_AttachmentData__alloc ();
	attach_data->_length = strlen (ical_string);
	attach_data->_maximum = attach_data->_length;	
	attach_data->_buffer = CORBA_sequence_CORBA_char_allocbuf (attach_data->_length);	
	strcpy (attach_data->_buffer, ical_string);

	GNOME_Evolution_Composer_attachData (composer_server, 
					content_type, filename, description,
					show_inline, attach_data,
					&ev);
	
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to attach data to the composer while sending iTip message");
		goto cleanup;
	}
	
	if (method == CAL_COMPONENT_METHOD_PUBLISH) {
		GNOME_Evolution_Composer_show (composer_server, &ev);
		if (BONOBO_EX (&ev))
			g_warning ("Unable to show the composer while sending iTip message");
	} else {		
		GNOME_Evolution_Composer_send (composer_server, &ev);
		if (BONOBO_EX (&ev))
			g_warning ("Unable to send iTip message");
	}
	
 cleanup:
	CORBA_exception_free (&ev);

	if (to_list != NULL)
		CORBA_free (to_list);
	if (cc_list != NULL)
		CORBA_free (cc_list);
	if (bcc_list != NULL)
		CORBA_free (bcc_list);

	if (subject != NULL)
		CORBA_free (subject);
	if (content_type != NULL)
		CORBA_free (content_type);
	if (filename != NULL)
		CORBA_free (filename);
	if (description != NULL)
		CORBA_free (description);
	if (attach_data != NULL)
		CORBA_free (attach_data);
}

