/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: JP Rosevear <jpr@novell.com>
 *
 *  Copyright 2004 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-message.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-time-util.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <mail/em-format-hook.h>
#include <mail/em-format-html.h>
#include <e-util/e-account-list.h>
#include <e-util/e-icon-factory.h>
#include <calendar/gui/itip-utils.h>
#include "itip-view.h"

#define CLASSID "itip://"

void format_itip (EPlugin *ep, EMFormatHookTarget *target);

typedef struct {
	EMFormatHTMLPObject pobject;

	GtkWidget *view;
	
	ESourceList *source_lists[E_CAL_SOURCE_TYPE_LAST];
	GHashTable *ecals[E_CAL_SOURCE_TYPE_LAST];	

	ECal *current_ecal;
	ECalSourceType type;

	char action;
	gboolean rsvp;

	GtkWidget *ok;
	GtkWidget *hbox;
	GtkWidget *vbox;
	
	char *vcalendar;
	ECalComponent *comp;
	icalcomponent *main_comp;
	icalcomponent *ical_comp;
	icalcomponent *top_level;
	icalcompiter iter;
	icalproperty_method method;

	int current;
	int total;

	gchar *calendar_uid;

	EAccountList *accounts;

	gchar *from_address;
	gchar *delegator_address;
	gchar *delegator_name;
	gchar *my_address;
	gint   view_only;
} FormatItipPObject;

static icalcomponent *
get_next (icalcompiter *iter)
{
	icalcomponent *ret = NULL;
	icalcomponent_kind kind;

	do {
		icalcompiter_next (iter);
		ret = icalcompiter_deref (iter);
		if (ret == NULL)
			break;
		kind = icalcomponent_isa (ret);
	} while (ret != NULL 
		 && kind != ICAL_VEVENT_COMPONENT
		 && kind != ICAL_VTODO_COMPONENT
		 && kind != ICAL_VFREEBUSY_COMPONENT);

	return ret;
}

static void
extract_itip_data (FormatItipPObject *pitip) 
{
	CamelDataWrapper *content;
	CamelStream *mem;
	icalproperty *prop;
	icalcomponent_kind kind = ICAL_NO_COMPONENT;
	icalcomponent *tz_comp;
	icalcompiter tz_iter;

/* FIXME try and match sender with organizer/attendee/sentby?
	pd->from_address = camel_address_encode ((CamelAddress *)from);		
	g_message ("Detected from address %s", pd->from_address);
*/

	content = camel_medium_get_content_object ((CamelMedium *) pitip->pobject.part);
	mem = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream (content, mem);
	
	pitip->vcalendar = g_strndup (((CamelStreamMem *) mem)->buffer->data, ((CamelStreamMem *) mem)->buffer->len);

	/* FIXME unref the content object as well? */
	camel_object_unref (mem);	

	pitip->top_level = e_cal_util_new_top_level ();

	pitip->main_comp = icalparser_parse_string (pitip->vcalendar);
	if (pitip->main_comp == NULL) {
//		write_error_html (itip, _("The attachment does not contain a valid calendar message"));
		return;
	}

	prop = icalcomponent_get_first_property (pitip->main_comp, ICAL_METHOD_PROPERTY);
	if (prop == NULL) {
		pitip->method = ICAL_METHOD_PUBLISH;
	} else {
		pitip->method = icalproperty_get_method (prop);
	}

	tz_iter = icalcomponent_begin_component (pitip->main_comp, ICAL_VTIMEZONE_COMPONENT);
	while ((tz_comp = icalcompiter_deref (&tz_iter)) != NULL) {
		icalcomponent *clone;

		clone = icalcomponent_new_clone (tz_comp);
		icalcomponent_add_component (pitip->top_level, clone);

		icalcompiter_next (&tz_iter);
	}

	pitip->iter = icalcomponent_begin_component (pitip->main_comp, ICAL_ANY_COMPONENT);
	pitip->ical_comp = icalcompiter_deref (&pitip->iter);
	if (pitip->ical_comp != NULL) {
		kind = icalcomponent_isa (pitip->ical_comp);
		if (kind != ICAL_VEVENT_COMPONENT
		    && kind != ICAL_VTODO_COMPONENT
		    && kind != ICAL_VFREEBUSY_COMPONENT)
			pitip->ical_comp = get_next (&pitip->iter);
	}

	if (pitip->ical_comp == NULL) {
//		write_error_html (itip, _("The attachment has no viewable calendar items"));		
		return;
	}

	pitip->total = icalcomponent_count_components (pitip->main_comp, ICAL_VEVENT_COMPONENT);
	pitip->total += icalcomponent_count_components (pitip->main_comp, ICAL_VTODO_COMPONENT);
	pitip->total += icalcomponent_count_components (pitip->main_comp, ICAL_VFREEBUSY_COMPONENT);

	if (pitip->total > 0)
		pitip->current = 1;
	else
		pitip->current = 0;

	pitip->comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (pitip->comp, pitip->ical_comp)) {
//		write_error_html (itip, _("The message does not appear to be properly formed"));
		g_object_unref (pitip->comp);
		pitip->comp = NULL;
		return;
	};

//	show_current (itip);	
}

static gboolean
format_itip_object (EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	FormatItipPObject *pitip = (FormatItipPObject *) pobject;
	ECalComponentText text;
	ECalComponentOrganizer organizer;
	ECalComponentDateTime datetime;
	icaltimezone *from_zone, *to_zone;
	const char *string;

	/* FIXME Error handling? */
	extract_itip_data (pitip);

	pitip->view = itip_view_new ();
	gtk_widget_show (pitip->view);

	itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_REQUEST);

	e_cal_component_get_organizer (pitip->comp, &organizer);
	itip_view_set_organizer (ITIP_VIEW (pitip->view), organizer.cn ? organizer.cn : itip_strip_mailto (organizer.value));
	/* FIXME, do i need to strip the sentby somehow? Maybe with camel? */
	itip_view_set_sentby (ITIP_VIEW (pitip->view), organizer.sentby);

	e_cal_component_get_summary (pitip->comp, &text);
	itip_view_set_summary (ITIP_VIEW (pitip->view), text.value ? text.value : _("None"));

	e_cal_component_get_location (pitip->comp, &string);
	itip_view_set_location (ITIP_VIEW (pitip->view), string);

	to_zone = calendar_config_get_icaltimezone ();
	
	e_cal_component_get_dtstart (pitip->comp, &datetime);
	if (datetime.value) {
		struct tm start_tm;
		
		/* FIXME Handle tzid that is not in the component - treat as "local" time */
		if (!datetime.value->is_utc && datetime.tzid) 
			from_zone = icalcomponent_get_timezone (pitip->top_level, datetime.tzid);
		else
			from_zone = NULL;
		
		start_tm = icaltimetype_to_tm_with_zone (datetime.value, from_zone, to_zone);

		itip_view_set_start (ITIP_VIEW (pitip->view), &start_tm);
	}
	e_cal_component_free_datetime (&datetime);

	e_cal_component_get_dtend (pitip->comp, &datetime);
	if (datetime.value) {
		struct tm end_tm;
		
		/* FIXME Handle tzid that is not in the component - treat as "local" time */
		if (!datetime.value->is_utc && datetime.tzid) 
			from_zone = icalcomponent_get_timezone (pitip->top_level, datetime.tzid);
		else
			from_zone = NULL;
		
		end_tm = icaltimetype_to_tm_with_zone (datetime.value, from_zone, to_zone);

		itip_view_set_end (ITIP_VIEW (pitip->view), &end_tm);
	}
	e_cal_component_free_datetime (&datetime);

	gtk_container_add (GTK_CONTAINER (eb), pitip->view);

	return TRUE;
}

void
format_itip (EPlugin *ep, EMFormatHookTarget *target)
{
	FormatItipPObject *pitip;

	calendar_config_init ();
	
	pitip = (FormatItipPObject *) em_format_html_add_pobject ((EMFormatHTML *) target->format, sizeof (FormatItipPObject), CLASSID, target->part, format_itip_object);
	// FIXME set the free function
//	pitip->object.free = pitip_free;
	camel_stream_printf (target->stream, "<table border=0 width=\"100%%\" cellpadding=3><tr>");
	camel_stream_printf (target->stream, "<td valign=top><object classid=\"%s\"></object></td><td width=100%% valign=top>", CLASSID);
	camel_stream_printf (target->stream, "</td></tr></table>");
}
