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
#include <libedataserverui/e-source-option-menu.h>
#include <libedataserverui/e-source-selector.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <mail/em-format-hook.h>
#include <mail/em-config.h>
#include <mail/em-format-html.h>
#include <e-util/e-account-list.h>
#include <e-util/e-icon-factory.h>
#include <calendar/gui/itip-utils.h>
#include <calendar/common/authentication.h>
#include "itip-view.h"

#define CLASSID "itip://"

void format_itip (EPlugin *ep, EMFormatHookTarget *target);
GtkWidget *itip_formatter_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data);

/* FIXME We should include these properly */
icaltimezone *calendar_config_get_icaltimezone (void);
void	  calendar_config_init			(void);
char     *calendar_config_get_primary_calendar (void);
char     *calendar_config_get_primary_tasks (void);

typedef struct {
	EMFormatHTMLPObject pobject;

	GtkWidget *view;
	
	ESourceList *source_lists[E_CAL_SOURCE_TYPE_LAST];
	GHashTable *ecals[E_CAL_SOURCE_TYPE_LAST];	

	ECal *current_ecal;
	ECalSourceType type;

	gboolean rsvp;
	
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

typedef struct {
	FormatItipPObject *pitip;
	char *uid;
	int count;
	gboolean show_selector;
} EItipControlFindData;


typedef void (* FormatItipOpenFunc) (ECal *ecal, ECalendarStatus status, gpointer data);

static void
cal_opened_cb (ECal *ecal, ECalendarStatus status, gpointer data)
{
	FormatItipPObject *pitip = data;
	ESource *source;
	ECalSourceType source_type;
	icaltimezone *zone;

	source_type = e_cal_get_source_type (ecal);
	source = e_cal_get_source (ecal);
	
	g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, cal_opened_cb, NULL);

	if (status != E_CALENDAR_STATUS_OK) {	
		itip_view_set_progress (ITIP_VIEW (pitip->view), "Failed to load at least one calendar");

		g_hash_table_remove (pitip->ecals[source_type], e_source_peek_uid (source));

		return;
	}

	zone = calendar_config_get_icaltimezone ();
	e_cal_set_default_timezone (ecal, zone, NULL);

	pitip->current_ecal = ecal;

//	set_ok_sens (itip);
}

static ECal *
start_calendar_server (FormatItipPObject *pitip, ESource *source, ECalSourceType type, FormatItipOpenFunc func, gpointer data)
{
	ECal *ecal;
	
	ecal = g_hash_table_lookup (pitip->ecals[type], e_source_peek_uid (source));
	if (ecal) {
		pitip->current_ecal = ecal;

		itip_view_set_progress (ITIP_VIEW (pitip->view), NULL);
//		set_ok_sens (itip);
		return ecal;		
	}
	
	ecal = auth_new_cal_from_source (source, type);
	g_signal_connect (G_OBJECT (ecal), "cal_opened", G_CALLBACK (func), data);

	g_hash_table_insert (pitip->ecals[type], g_strdup (e_source_peek_uid (source)), ecal);
	
	e_cal_open_async (ecal, TRUE);

	return ecal;
}

static ECal *
start_calendar_server_by_uid (FormatItipPObject *pitip, const char *uid, ECalSourceType type)
{
	int i;

	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		ESource *source;

		source = e_source_list_peek_source_by_uid (pitip->source_lists[i], uid);
		if (source)
			return start_calendar_server (pitip, source, type, cal_opened_cb, pitip);
	}
	
	return NULL;
}

static void
source_selected_cb (ESourceOptionMenu *esom, ESource *source, gpointer data)
{
	FormatItipPObject *pitip = data;
	
	g_message ("Source selected");

	/* FIXME turn off buttons while we check the calendar for being open? */

	start_calendar_server (pitip, source, pitip->type, cal_opened_cb, pitip);
}

static void
find_cal_opened_cb (ECal *ecal, ECalendarStatus status, gpointer data)
{
	EItipControlFindData *fd = data;
	FormatItipPObject *pitip = fd->pitip;
	ESource *source;
	ECalSourceType source_type;
	icalcomponent *icalcomp;
	icaltimezone *zone;

	source_type = e_cal_get_source_type (ecal);
	source = e_cal_get_source (ecal);
	
	fd->count--;
	
	g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, find_cal_opened_cb, NULL);

	if (status != E_CALENDAR_STATUS_OK) {
		g_hash_table_remove (pitip->ecals[source_type], e_source_peek_uid (source));

		goto cleanup;
	}

	if (e_cal_get_object (ecal, fd->uid, NULL, &icalcomp, NULL)) {
		icalcomponent_free (icalcomp);
		
		pitip->current_ecal = ecal;

		itip_view_set_progress (ITIP_VIEW (pitip->view), NULL);
//		set_ok_sens (fd->itip);
	}

	zone = calendar_config_get_icaltimezone ();
	e_cal_set_default_timezone (ecal, zone, NULL);

 cleanup:
	if (fd->count == 0) {
		/* FIXME the box check is to see if the buttons are displayed i think */
		if (fd->show_selector && !pitip->current_ecal /*&& pitip->vbox*/) {
			GtkWidget *esom;
			ESource *source = NULL;
			char *uid;

			switch (pitip->type) {
			case E_CAL_SOURCE_TYPE_EVENT:
				uid = calendar_config_get_primary_calendar ();
				break;
			case E_CAL_SOURCE_TYPE_TODO:
				uid = calendar_config_get_primary_tasks ();
				break;
			default:
				uid = NULL;
				g_assert_not_reached ();
			}	
	
			if (uid) {
				source = e_source_list_peek_source_by_uid (pitip->source_lists[pitip->type], uid);
				g_free (uid);
			}

			/* Try to create a default if there isn't one */
			if (!source)
				source = e_source_list_peek_source_any (pitip->source_lists[pitip->type]);

			g_message ("Picking any source");
			esom = e_source_option_menu_new (pitip->source_lists[pitip->type]);
			/* FIXME used to force the data to be kept alive, still do this? */
			g_signal_connect (esom, "source_selected", G_CALLBACK (source_selected_cb), fd->pitip);

			//gtk_box_pack_start (GTK_BOX (pitip->vbox), esom, FALSE, TRUE, 0);
			gtk_widget_show (esom);

			/* FIXME What if there is no source? */
			if (source) {
				e_source_option_menu_select (E_SOURCE_OPTION_MENU (esom), source);
				itip_view_set_progress (ITIP_VIEW (pitip->view), NULL);
			}
		} else {
			/* FIXME Display error message to user */
		}
		
		g_free (fd->uid);
		g_free (fd);
	}
}

static void
find_server (FormatItipPObject *pitip, ECalComponent *comp, gboolean show_selector)
{
	EItipControlFindData *fd = NULL;
	GSList *groups, *l;
	const char *uid;

	e_cal_component_get_uid (comp, &uid);

	itip_view_set_progress (ITIP_VIEW (pitip->view), "Searching for an existing version of this appointment");

	groups = e_source_list_peek_groups (pitip->source_lists[pitip->type]);
	for (l = groups; l; l = l->next) {
		ESourceGroup *group;
		GSList *sources, *m;
		
		group = l->data;

		sources = e_source_group_peek_sources (group);
		for (m = sources; m; m = m->next) {
			ESource *source;
			ECal *ecal;
			
			source = m->data;
			
			if (!fd) {
				fd = g_new0 (EItipControlFindData, 1);
				fd->pitip = pitip;
				fd->uid = g_strdup (uid);
				fd->show_selector = show_selector;
			}
			fd->count++;

			ecal = start_calendar_server (pitip, source, pitip->type, find_cal_opened_cb, fd);
		}		
	}
}

static void
cleanup_ecal (ECal *ecal) 
{
	/* Clean up any signals */
	g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, cal_opened_cb, NULL);
	g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, find_cal_opened_cb, NULL);
	
	g_object_unref (ecal);
}

static icalproperty *
find_attendee (icalcomponent *ical_comp, const char *address)
{
	icalproperty *prop;

	if (address == NULL)
		return NULL;

	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY)) {
		icalvalue *value;
		const char *attendee;
		char *text;

		value = icalproperty_get_value (prop);
		if (!value)
			continue;

		attendee = icalvalue_get_string (value);

		text = g_strdup (itip_strip_mailto (attendee));
		text = g_strstrip (text);
		if (!g_strcasecmp (address, text)) {
			g_free (text);
			break;
		}
		g_free (text);
	}
	
	return prop;
}

static gboolean
change_status (icalcomponent *ical_comp, const char *address, icalparameter_partstat status)
{
	icalproperty *prop;

	prop = find_attendee (ical_comp, address);
	if (prop) {
		icalparameter *param;

		icalproperty_remove_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		param = icalparameter_new_partstat (status);
		icalproperty_add_parameter (prop, param);
	} else {
		icalparameter *param;
		
		if (address != NULL) {
			prop = icalproperty_new_attendee (address);
			icalcomponent_add_property (ical_comp, prop);

			param = icalparameter_new_role (ICAL_ROLE_OPTPARTICIPANT);
			icalproperty_add_parameter (prop, param);

			param = icalparameter_new_partstat (status);
			icalproperty_add_parameter (prop, param);
		} else {
			EAccount *a;

			a = itip_addresses_get_default ();
			
			prop = icalproperty_new_attendee (a->id->address);
			icalcomponent_add_property (ical_comp, prop);
			
			param = icalparameter_new_cn (a->id->name);
			icalproperty_add_parameter (prop, param);	

			param = icalparameter_new_role (ICAL_ROLE_REQPARTICIPANT);
			icalproperty_add_parameter (prop, param);
			
			param = icalparameter_new_partstat (status);
			icalproperty_add_parameter (prop, param);
		}
	}

	return TRUE;
}

static void
update_item (FormatItipPObject *pitip)
{
	struct icaltimetype stamp;
	icalproperty *prop;
	icalcomponent *clone;
//	GtkWidget *dialog;
	GError *error = NULL;

	/* Set X-MICROSOFT-CDO-REPLYTIME to record the time at which
	 * the user accepted/declined the request. (Outlook ignores
	 * SEQUENCE in REPLY reponses and instead requires that each
	 * updated response have a later REPLYTIME than the previous
	 * one.) This also ends up getting saved in our own copy of
	 * the meeting, though there's currently no way to see that
	 * information (unless it's being saved to an Exchange folder
	 * and you then look at it in Outlook).
	 */
	stamp = icaltime_current_time_with_zone (icaltimezone_get_utc_timezone ());
	prop = icalproperty_new_x (icaltime_as_ical_string (stamp));
	icalproperty_set_x_name (prop, "X-MICROSOFT-CDO-REPLYTIME");
	icalcomponent_add_property (pitip->ical_comp, prop);

	clone = icalcomponent_new_clone (pitip->ical_comp);
	icalcomponent_add_component (pitip->top_level, clone);
	icalcomponent_set_method (pitip->top_level, pitip->method);

	if (!e_cal_receive_objects (pitip->current_ecal, pitip->top_level, &error)) {
		/* FIXME e-error */
//		dialog = gnome_warning_dialog (error->message);
		g_error_free (error);
	} else {
		/* FIXME I think we should do nothing */
//		dialog = gnome_ok_dialog (_("Update complete\n"));
	}
//	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

	icalcomponent_remove_component (pitip->top_level, clone);
}

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

static void
view_response_cb (GtkWidget *widget, ItipViewResponse response, gpointer data)
{
	FormatItipPObject *pitip = data;
	gboolean status = FALSE;
	
	switch (response) {
	case ITIP_VIEW_RESPONSE_ACCEPT:
		status = change_status (pitip->ical_comp, pitip->my_address, 
					ICAL_PARTSTAT_ACCEPTED);
		if (status) {
			e_cal_component_rescan (pitip->comp);
			update_item (pitip);
		}
		break;
	case ITIP_VIEW_RESPONSE_TENTATIVE:
		status = change_status (pitip->ical_comp, pitip->my_address,
					ICAL_PARTSTAT_TENTATIVE);
		if (status) {
			e_cal_component_rescan (pitip->comp);
			update_item (pitip);
		}
		break;
	case ITIP_VIEW_RESPONSE_DECLINE:
		status = change_status (pitip->ical_comp, pitip->my_address,
					ICAL_PARTSTAT_DECLINED);
		if (status) {
			e_cal_component_rescan (pitip->comp);
			update_item (pitip);
		}
		break;
	default:
		break;
	}
}

static gboolean
format_itip_object (EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	FormatItipPObject *pitip = (FormatItipPObject *) pobject;
	ECalComponentText text;
	ECalComponentOrganizer organizer;
	ECalComponentDateTime datetime;
	icaltimezone *from_zone, *to_zone;
	GString *gstring = NULL;
	GSList *list, *l;
	const char *string;
	int i;
	
	/* Source Lists and open ecal clients */
	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		if (!e_cal_get_sources (&pitip->source_lists[E_CAL_SOURCE_TYPE_EVENT], E_CAL_SOURCE_TYPE_EVENT, NULL))
			/* FIXME More error handling? */
			pitip->source_lists[i] = NULL;

		/* Initialize the ecal hashes */		
		pitip->ecals[i] = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, cleanup_ecal);
	}
	
	/* FIXME Error handling? */
	/* FIXME Handle multiple VEVENTS with the same UID, ie detached instances */
	extract_itip_data (pitip);

	pitip->view = itip_view_new ();
	gtk_widget_show (pitip->view);

	switch (pitip->method) {
	case ICAL_METHOD_PUBLISH:
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_PUBLISH);
		break;
	case ICAL_METHOD_REQUEST:
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_REQUEST);
		break;
	case ICAL_METHOD_REPLY:
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_REPLY);
		break;
	case ICAL_METHOD_ADD:
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_ADD);
		break;
	case ICAL_METHOD_CANCEL:
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_CANCEL);
		break;
	case ICAL_METHOD_REFRESH:
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_REFRESH);
		break;
	case ICAL_METHOD_COUNTER:
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_COUNTER);
		break;
	case ICAL_METHOD_DECLINECOUNTER:
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_DECLINECOUNTER);
		break;
	default:
		/* FIXME What to do here? */
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_ERROR);
	}
	
	e_cal_component_get_organizer (pitip->comp, &organizer);
	itip_view_set_organizer (ITIP_VIEW (pitip->view), organizer.cn ? organizer.cn : itip_strip_mailto (organizer.value));
	/* FIXME, do i need to strip the sentby somehow? Maybe with camel? */
	itip_view_set_sentby (ITIP_VIEW (pitip->view), organizer.sentby);

	e_cal_component_get_summary (pitip->comp, &text);
	itip_view_set_summary (ITIP_VIEW (pitip->view), text.value ? text.value : _("None"));

	e_cal_component_get_location (pitip->comp, &string);
	itip_view_set_location (ITIP_VIEW (pitip->view), string);

	e_cal_component_get_location (pitip->comp, &string);
	itip_view_set_location (ITIP_VIEW (pitip->view), string);

	e_cal_component_get_description_list (pitip->comp, &list);
	for (l = list; l; l = l->next) {
		ECalComponentText *text = l->data;
		
		if (!gstring)
			gstring = g_string_new (text->value);
		else
			g_string_append_printf (gstring, "\n\n%s", text->value);
	}
	e_cal_component_free_text_list (list);
		
	itip_view_set_description (ITIP_VIEW (pitip->view), gstring->str);
	g_string_free (gstring, TRUE);
	
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

	/* Info area items */
	itip_view_add_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, "This meeting occurs weekly indefinitely");
	itip_view_add_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_WARNING, "An appointment in the calendar conflicts with this meeting");
	
	gtk_container_add (GTK_CONTAINER (eb), pitip->view);
	gtk_widget_set_usize (pitip->view, 640, -1);

	g_signal_connect (pitip->view, "response", G_CALLBACK (view_response_cb), pitip);

	/* FIXME Show selector should be handled in the itip view */
	find_server (pitip, pitip->comp, TRUE);
	
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

GtkWidget *
itip_formatter_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
//	EMConfigTargetPrefs *target = (EMConfigTargetPrefs *) hook_data->config->target;
	GtkWidget *page;
	GtkWidget *tab_label;
	GtkWidget *frame;
	GtkWidget *frame_label;
	GtkWidget *padding_label;
	GtkWidget *hbox;
	GtkWidget *inner_vbox;
	GtkWidget *check;
	GtkWidget *check_gaim;

	/* A structure to pass some stuff around */
//	stuff = g_new0 (struct bbdb_stuff, 1);
//	stuff->target = target;

	/* Create a new notebook page */
	page = gtk_vbox_new (FALSE, 0);
	GTK_CONTAINER (page)->border_width = 12;
	tab_label = gtk_label_new (_("Meetings"));
	gtk_notebook_append_page (GTK_NOTEBOOK (hook_data->parent), page, tab_label);

	/* Frame */
	frame = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, FALSE, FALSE, 0);

	/* "Automatic Contacts" */
	frame_label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (frame_label), _("<span weight=\"bold\">General</span>"));
	GTK_MISC (frame_label)->xalign = 0.0;
	gtk_box_pack_start (GTK_BOX (frame), frame_label, FALSE, FALSE, 0);

	/* Indent/padding */
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (frame), hbox, FALSE, TRUE, 0);
	padding_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), padding_label, FALSE, FALSE, 0);
	inner_vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (hbox), inner_vbox, FALSE, FALSE, 0);
	
	/* Enable BBDB checkbox */
	check = gtk_check_button_new_with_mnemonic (_("_Delete message after acting"));
//	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gconf_client_get_bool (target->gconf, GCONF_KEY_ENABLE, NULL));
//	g_signal_connect (GTK_TOGGLE_BUTTON (check), "toggled", G_CALLBACK (enable_toggled_cb), stuff);
	gtk_box_pack_start (GTK_BOX (inner_vbox), check, FALSE, FALSE, 0);
//	stuff->check = check;

	/* "Instant Messaging Contacts" */
	frame = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, TRUE, TRUE, 24);

	frame_label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (frame_label), _("<span weight=\"bold\">Conflict Search</span>"));
	GTK_MISC (frame_label)->xalign = 0.0;
	gtk_box_pack_start (GTK_BOX (frame), frame_label, FALSE, FALSE, 0);

	/* Indent/padding */
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (frame), hbox, FALSE, TRUE, 0);
	padding_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), padding_label, FALSE, FALSE, 0);
	inner_vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (hbox), inner_vbox, FALSE, FALSE, 0);
	
	/* Enable Gaim Checkbox */
	check_gaim = gtk_label_new (_("Select the calendars to search for meeting conflicts"));
	gtk_box_pack_start (GTK_BOX (inner_vbox), check_gaim, FALSE, FALSE, 0);
//	stuff->check_gaim = check_gaim;

	gtk_widget_show_all (page);

	return page;
}

