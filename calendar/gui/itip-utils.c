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

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-object-client.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <gtk/gtkwidget.h>
#include <ical.h>
#include <Evolution-Composer.h>
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

gchar *partstat_values[] = {
	"Needs action",
	"Accepted",
	"Declined",
	"Tentative",
	"Delegated",
	"Completed",
	"In Progress",
	"Unknown"
};

gchar *role_values[] = {
	"Chair",
	"Required Participant",
	"Optional Participant",
	"Non-Participant"
};



/* Note that I have to iterate and check myself because
   ical_property_get_xxx_parameter doesn't take into account the
   kind of parameter for which you wish to search! */
icalparameter *
get_icalparam_by_type (icalproperty *prop, icalparameter_kind kind)
{
	icalparameter *param;

	for (param = icalproperty_get_first_parameter (prop, ICAL_ANY_PARAMETER);
	     param != NULL && icalparameter_isa (param) != kind;
	     param = icalproperty_get_next_parameter (prop, ICAL_ANY_PARAMETER) );

	return param;
}

static void
error_dialog (gchar *str) 
{
	GtkWidget *dlg;
	
	dlg = gnome_error_dialog (str);
	gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
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
	gint cntr;
	gint len;
	CalComponentText caltext;
	CalComponentOrganizer organizer;
	CORBA_char *content_type, *filename, *description, *attach_data;
	CORBA_boolean show_inline;
	CORBA_char tempstr[200];
	
	CORBA_exception_init (&ev);

	/* First, I obtain an object reference that represents the Composer. */
	bonobo_server = bonobo_object_activate (GNOME_EVOLUTION_COMPOSER_OAFIID, 0);
	g_return_if_fail (bonobo_server != NULL);

	composer_server = BONOBO_OBJREF (bonobo_server);

	switch (method) {
	case CAL_COMPONENT_METHOD_PUBLISH:
		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = to_list->_length = 0;
		break;

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
			recipient->address = CORBA_string_dup (att->value);
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
		recipient->address = CORBA_string_dup (organizer.value);
		break;
	}

	cc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
	cc_list->_maximum = cc_list->_length = 0;
	bcc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
	bcc_list->_maximum = bcc_list->_length = 0;
	
	cal_component_get_summary (comp, &caltext);
	subject = CORBA_string_dup (caltext.value);

	GNOME_Evolution_Composer_setHeaders (composer_server, to_list, cc_list, bcc_list, subject, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Unable to set composer headers while sending iTip message");
		CORBA_exception_free (&ev);
		return;
	}

	sprintf (tempstr, "text/calendar;METHOD=%s", itip_methods[method]);
	content_type = CORBA_string_dup (tempstr);
	filename = CORBA_string_dup ("");
	sprintf (tempstr, "Calendar attachment");
	description = CORBA_string_dup (tempstr);
	show_inline = FALSE;

	/* I need to create an encapsulating iCalendar component, and stuff our vEvent
	   into it. */
	{
		icalcomponent *icomp, *clone;
		icalproperty *prop;
		icalvalue *value;
		gchar *ical_string;

		icomp = icalcomponent_new (ICAL_VCALENDAR_COMPONENT);
		
		prop = icalproperty_new (ICAL_PRODID_PROPERTY);
		value = icalvalue_new_text ("-//Ximian/Evolution//EN");
		icalproperty_set_value (prop, value);
		icalcomponent_add_property (icomp, prop);

		prop = icalproperty_new (ICAL_VERSION_PROPERTY);
		value = icalvalue_new_text ("2.0");
		icalproperty_set_value (prop, value);
		icalcomponent_add_property (icomp, prop);

		prop = icalproperty_new (ICAL_METHOD_PROPERTY);
		value = icalvalue_new_method (itip_methods_enum[method]);
		icalproperty_set_value (prop, value);
		icalcomponent_add_property (icomp, prop);

		clone = icalcomponent_new_clone (cal_component_get_icalcomponent (comp));
		icalcomponent_add_component (icomp, clone);

		ical_string = icalcomponent_as_ical_string (icomp);
		attach_data = CORBA_string_dup (ical_string);

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

	/* Let's free shit up. */

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

