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
#include <camel/camel-folder.h>
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
#include <widgets/misc/e-error.h>
#include <calendar/gui/calendar-config.h>
#include <calendar/gui/itip-utils.h>
#include <calendar/common/authentication.h>
#include "itip-view.h"

#define CLASSID "itip://"
#define GCONF_KEY_DELETE "/apps/evolution/itip/delete_processed"

void format_itip (EPlugin *ep, EMFormatHookTarget *target);
GtkWidget *itip_formatter_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data);

typedef struct {
	EMFormatHTMLPObject pobject;

	GtkWidget *view;
	
	ESourceList *source_lists[E_CAL_SOURCE_TYPE_LAST];
	GHashTable *ecals[E_CAL_SOURCE_TYPE_LAST];	

	ECal *current_ecal;
	ECalSourceType type;

	char *vcalendar;
	ECalComponent *comp;
	icalcomponent *main_comp;
	icalcomponent *ical_comp;
	icalcomponent *top_level;
	icalcompiter iter;
	icalproperty_method method;
	time_t start_time;
	time_t end_time;
	
	int current;
	int total;

	gchar *calendar_uid;

	EAccountList *accounts;

	gchar *from_address;
	gchar *delegator_address;
	gchar *delegator_name;
	gchar *my_address;
	gint   view_only;

	guint progress_info_id;

	gboolean delete_message;
} FormatItipPObject;

typedef struct {
	FormatItipPObject *pitip;
	char *uid;

	char *sexp;

	int count;
} EItipControlFindData;


typedef void (* FormatItipOpenFunc) (ECal *ecal, ECalendarStatus status, gpointer data);

static void
find_my_address (FormatItipPObject *pitip, icalcomponent *ical_comp, icalparameter_partstat *status)
{
	icalproperty *prop;
	char *my_alt_address = NULL;
	
	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY)) {
		icalvalue *value;
		icalparameter *param;
		const char *attendee, *name;
		char *attendee_clean, *name_clean;
		EIterator *it;

		value = icalproperty_get_value (prop);
		if (value != NULL) {
			attendee = icalvalue_get_string (value);
			attendee_clean = g_strdup (itip_strip_mailto (attendee));
			attendee_clean = g_strstrip (attendee_clean);
		} else {
			attendee = NULL;
			attendee_clean = NULL;
		}
		
		param = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
		if (param != NULL) {
			name = icalparameter_get_cn (param);
			name_clean = g_strdup (name);
			name_clean = g_strstrip (name_clean);
		} else {
			name = NULL;
			name_clean = NULL;
		}

		if (pitip->delegator_address) {
			char *delegator_clean;
			
			delegator_clean = g_strdup (itip_strip_mailto (attendee));
			delegator_clean = g_strstrip (delegator_clean);
			
			/* If the mailer told us the address to use, use that */
			if (delegator_clean != NULL
			    && !g_ascii_strcasecmp (attendee_clean, delegator_clean)) {
				pitip->my_address = g_strdup (itip_strip_mailto (pitip->delegator_address));
				pitip->my_address = g_strstrip (pitip->my_address);

				if (status) {
					param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
					*status = param ? icalparameter_get_partstat (param) : ICAL_PARTSTAT_NEEDSACTION;
				}
			}

			g_free (delegator_clean);
		} else {
			it = e_list_get_iterator((EList *)pitip->accounts);
			while (e_iterator_is_valid(it)) {
				const EAccount *account = e_iterator_get(it);
				
				/* Check for a matching address */
				if (attendee_clean != NULL
				    && !g_ascii_strcasecmp (account->id->address, attendee_clean)) {
					pitip->my_address = g_strdup (account->id->address);
					if (status) {
						param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
						*status = param ? icalparameter_get_partstat (param) : ICAL_PARTSTAT_NEEDSACTION;
					}
					g_free (attendee_clean);
					g_free (name_clean);
					g_free (my_alt_address);
					g_object_unref(it);
					return;
				}
				
				/* Check for a matching cname to fall back on */
				if (name_clean != NULL 
				    && !g_ascii_strcasecmp (account->id->name, name_clean))
					my_alt_address = g_strdup (attendee_clean);
				
				e_iterator_next(it);
			}
			g_object_unref(it);
		}
		
		g_free (attendee_clean);
		g_free (name_clean);
	}

	pitip->my_address = my_alt_address;
	if (status)
		*status = ICAL_PARTSTAT_NEEDSACTION;
}

static ECalComponent *
get_real_item (FormatItipPObject *pitip)
{
	ECalComponent *comp;
	icalcomponent *icalcomp;
	gboolean found = FALSE;
	const char *uid;

	e_cal_component_get_uid (pitip->comp, &uid);

	found = e_cal_get_object (pitip->current_ecal, uid, NULL, &icalcomp, NULL);
	if (!found)
		return NULL;

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		g_object_unref (comp);
		icalcomponent_free (icalcomp);
		return NULL;
	}

	return comp;
}

static void
adjust_item (FormatItipPObject *pitip, ECalComponent *comp)
{
	ECalComponent *real_comp;
	
	real_comp = get_real_item (pitip);
	if (real_comp != NULL) {
		ECalComponentText text;
		const char *string;
		GSList *l;
		
		e_cal_component_get_summary (real_comp, &text);
		e_cal_component_set_summary (comp, &text);
		e_cal_component_get_location (real_comp, &string);
		e_cal_component_set_location (comp, string);
		e_cal_component_get_description_list (real_comp, &l);
		e_cal_component_set_description_list (comp, l);
		e_cal_component_free_text_list (l);
		
		g_object_unref (real_comp);
	} else {
		ECalComponentText text = {_("Unknown"), NULL};
		
		e_cal_component_set_summary (comp, &text);
	}
}

static void
set_buttons_sensitive (FormatItipPObject *pitip)
{
	gboolean read_only = TRUE;	
	
	if (pitip->current_ecal)
		e_cal_is_read_only (pitip->current_ecal, &read_only, NULL);
	
	itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), pitip->current_ecal != NULL && !read_only);
}


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
		itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
						      "Failed to load the calendar '%s'", e_source_peek_name (source));

		g_hash_table_remove (pitip->ecals[source_type], e_source_peek_uid (source));

		return;
	}

	zone = calendar_config_get_icaltimezone ();
	e_cal_set_default_timezone (ecal, zone, NULL);

	pitip->current_ecal = ecal;

	set_buttons_sensitive (pitip);
}

static ECal *
start_calendar_server (FormatItipPObject *pitip, ESource *source, ECalSourceType type, FormatItipOpenFunc func, gpointer data)
{
	ECal *ecal;
	
	ecal = g_hash_table_lookup (pitip->ecals[type], e_source_peek_uid (source));
	if (ecal) {
		pitip->current_ecal = ecal;

		itip_view_remove_lower_info_item (ITIP_VIEW (pitip->view), pitip->progress_info_id);
		pitip->progress_info_id = 0;

		set_buttons_sensitive (pitip);

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

	itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), FALSE);

	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		ESource *source;

		source = e_source_list_peek_source_by_uid (pitip->source_lists[i], uid);
		if (source)
			return start_calendar_server (pitip, source, type, cal_opened_cb, pitip);
	}
	
	return NULL;
}

static void
source_selected_cb (ItipView *view, ESource *source, gpointer data)
{
	FormatItipPObject *pitip = data;
	
	itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), FALSE);

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
	GList *objects = NULL;
	
	source_type = e_cal_get_source_type (ecal);
	source = e_cal_get_source (ecal);
	
	fd->count--;
	
	g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, find_cal_opened_cb, NULL);

	if (status != E_CALENDAR_STATUS_OK) {
		/* FIXME Do we really want to warn here?  If we fail
		 * to find the item, this won't be cleared but the
		 * selector might be shown */
		itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
						     "Failed to load the calendar '%s'", e_source_peek_name (source));

		g_hash_table_remove (pitip->ecals[source_type], e_source_peek_uid (source));

		goto cleanup;
	}

	/* Check for conflicts */
	/* If the query fails, we'll just ignore it */
	/* FIXME Limit the calendars checked for conflicts? */
	/* FIXME What happens for recurring conflicts? */
	if (e_cal_get_object_list (ecal, fd->sexp, &objects, NULL) && g_list_length (objects) > 0) {
		itip_view_add_upper_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_WARNING, 
						      "An appointment in the calendar '%s' conflicts with this meeting", e_source_peek_name (source));

		e_cal_free_object_list (objects);
	}

	if (e_cal_get_object (ecal, fd->uid, NULL, &icalcomp, NULL)) {
		icalcomponent_free (icalcomp);
		
		pitip->current_ecal = ecal;

		/* Provide extra info, since its not in the component */
		/* FIXME Check sequence number of meeting? */
		/* FIXME Do we need to adjust elsewhere for the delegated calendar item? */
		if (pitip->method == ICAL_METHOD_REPLY || pitip->method == ICAL_METHOD_REFRESH)
			adjust_item (pitip, pitip->comp);

		/* We clear everything because we don't really care
		 * about any other info/warnings now we found an
		 * existing versions */
		itip_view_clear_lower_info_items (ITIP_VIEW (pitip->view));
		pitip->progress_info_id = 0;

		itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, 
						      "Found the appointment in the calendar '%s'", e_source_peek_name (source));

		set_buttons_sensitive (pitip);
	}

	zone = calendar_config_get_icaltimezone ();
	e_cal_set_default_timezone (ecal, zone, NULL);

 cleanup:
	if (fd->count == 0) {
		if ((pitip->method == ICAL_METHOD_PUBLISH || pitip->method ==  ICAL_METHOD_REQUEST) 
		    && !pitip->current_ecal) {
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

			itip_view_set_source_list (ITIP_VIEW (pitip->view), pitip->source_lists[pitip->type]);
			g_signal_connect (pitip->view, "source_selected", G_CALLBACK (source_selected_cb), pitip);

			/* The only method that RSVP makes sense for is REQUEST */
			/* FIXME Default to the suggestion for RSVP for my attendee */
			itip_view_set_rsvp (ITIP_VIEW (pitip->view), TRUE);
			itip_view_set_show_rsvp (ITIP_VIEW (pitip->view), pitip->method == ICAL_METHOD_REQUEST ? TRUE : FALSE );
			
			itip_view_remove_lower_info_item (ITIP_VIEW (pitip->view), pitip->progress_info_id);
			pitip->progress_info_id = 0;

			if (source) {
				itip_view_set_source (ITIP_VIEW (pitip->view), source);

				/* FIXME Shouldn't the buttons be sensitized here? */
			} else {
				itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, "Unable to find any calendars");
				itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), FALSE);
			}
		} else {
			/* FIXME Display error message to user */
		}
		
		g_free (fd->uid);
		g_free (fd);
	}
}

static void
find_server (FormatItipPObject *pitip, ECalComponent *comp)
{
	EItipControlFindData *fd = NULL;
	GSList *groups, *l;
	const char *uid;

	e_cal_component_get_uid (comp, &uid);

	pitip->progress_info_id = itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS, 
								 "Searching for an existing version of this appointment");

	itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), FALSE);

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
				char *start = NULL, *end = NULL;
				
				fd = g_new0 (EItipControlFindData, 1);
				fd->pitip = pitip;
				fd->uid = g_strdup (uid);
				
				if (pitip->start_time && pitip->end_time) {
					start = isodate_from_time_t (pitip->start_time);
					end = isodate_from_time_t (pitip->end_time);
					
					fd->sexp = g_strdup_printf ("(and (occur-in-time-range? (make-time \"%s\") (make-time \"%s\")) (not (uid? \"%s\")))", 
								    start, end, icalcomponent_get_uid (pitip->ical_comp));
					g_message ("Sexp is %s", fd->sexp);
				}
				
				g_free (start);
				g_free (end);
			}
			fd->count++;

			ecal = start_calendar_server (pitip, source, pitip->type, find_cal_opened_cb, fd);
		}		
	}
}

static void
cleanup_ecal (gpointer data)
{
	ECal *ecal = data;
	
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
update_item (FormatItipPObject *pitip, ItipViewResponse response)
{
	struct icaltimetype stamp;
	icalproperty *prop;
	icalcomponent *clone;
	ESource *source;
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

	source = e_cal_get_source (pitip->current_ecal);
		
	if (!e_cal_receive_objects (pitip->current_ecal, pitip->top_level, &error)) {
		itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO,
						      _("Unable to send item to calendar '%s'.  %s"), 
						      e_source_peek_name (source), error->message);
		g_error_free (error);
	} else {
		
		/* FIXME This makes the UI look ugly */
		itip_view_set_source_list (ITIP_VIEW (pitip->view), NULL);

		itip_view_clear_lower_info_items (ITIP_VIEW (pitip->view));

		switch (response) {
		case ITIP_VIEW_RESPONSE_ACCEPT:
			itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, 
							      _("Sent to calendar '%s' as accepted"), e_source_peek_name (source));
			break;
		case ITIP_VIEW_RESPONSE_TENTATIVE:
			itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, 
							      _("Sent to calendar '%s' as tentative"), e_source_peek_name (source));
			break;
		case ITIP_VIEW_RESPONSE_DECLINE:
			/* FIXME some calendars just might not save it at all, is this accurate? */
			itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, 
							      _("Sent to calendar '%s' as declined"), e_source_peek_name (source));
			break;
		case ITIP_VIEW_RESPONSE_CANCEL:
			/* FIXME some calendars just might not save it at all, is this accurate? */
			itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, 
							      _("Sent to calendar '%s' as cancelled"), e_source_peek_name (source));
			break;
		default:
			g_assert_not_reached ();
			break;
		}

		/* FIXME Should we hide or desensitize the buttons now? */
	}

	icalcomponent_remove_component (pitip->top_level, clone);
}

static void
update_attendee_status (FormatItipPObject *pitip)
{
	ECalComponent *comp = NULL;
	icalcomponent *icalcomp = NULL;
	const char *uid;
	GError *error;
	
	/* Obtain our version */
	e_cal_component_get_uid (pitip->comp, &uid);
	if (e_cal_get_object (pitip->current_ecal, uid, NULL, &icalcomp, NULL)) {
		GSList *attendees;

		comp = e_cal_component_new ();
		if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
			icalcomponent_free (icalcomp);

			itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, "The meeting is invalid and cannot be updated");
		} else {
			e_cal_component_get_attendee_list (pitip->comp, &attendees);
			if (attendees != NULL) {
				ECalComponentAttendee *a = attendees->data;
				icalproperty *prop;

				prop = find_attendee (icalcomp, itip_strip_mailto (a->value));

				if (prop == NULL) {
					if (e_error_run (NULL, "org.gnome.itip-formatter:add-unknown-attendee", NULL) == GTK_RESPONSE_YES) {
						change_status (icalcomp, itip_strip_mailto (a->value), a->status);
						e_cal_component_rescan (comp);
					} else {
						goto cleanup;
					}
				} else if (a->status == ICAL_PARTSTAT_NONE || a->status == ICAL_PARTSTAT_X) {
					itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, 
								       _("Attendee status could not be updated because the status is invalid"));
					goto cleanup;
				} else {
					change_status (icalcomp, itip_strip_mailto (a->value), a->status);
					e_cal_component_rescan (comp);			
				}
			}
		}

		if (!e_cal_modify_object (pitip->current_ecal, icalcomp, CALOBJ_MOD_ALL, &error)) {
			itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, 
							      _("Unable to update attendee statusAttendee status updated. %s"), error->message);
			
			g_error_free (error);
		} else {
			itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("Attendee status updated"));
		}
	} else {
		itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_WARNING, 
					       _("Attendee status can not be updated because the item no longer exists"));
	}

 cleanup:
	if (comp != NULL)
		g_object_unref (comp);
}

static void
send_item (FormatItipPObject *pitip)
{
	ECalComponent *comp;

	comp = get_real_item (pitip);
	
	if (comp != NULL) {
		itip_send_comp (E_CAL_COMPONENT_METHOD_REQUEST, comp, pitip->current_ecal, NULL);
		g_object_unref (comp);

		itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, "Meeting information sent");
	} else {
		itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, "Unable to send meeting information, the meeting does not exist");
	}
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

	content = camel_medium_get_content_object ((CamelMedium *) pitip->pobject.part);
	mem = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream (content, mem);

	pitip->vcalendar = g_strndup (((CamelStreamMem *) mem)->buffer->data, ((CamelStreamMem *) mem)->buffer->len);

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
			update_item (pitip, response);
		}
		break;
	case ITIP_VIEW_RESPONSE_TENTATIVE:
		status = change_status (pitip->ical_comp, pitip->my_address,
					ICAL_PARTSTAT_TENTATIVE);
		if (status) {
			e_cal_component_rescan (pitip->comp);
			update_item (pitip, response);
		}
		break;
	case ITIP_VIEW_RESPONSE_DECLINE:
		status = change_status (pitip->ical_comp, pitip->my_address,
					ICAL_PARTSTAT_DECLINED);
		if (status) {
			e_cal_component_rescan (pitip->comp);
			update_item (pitip, response);
		}
		break;
	case ITIP_VIEW_RESPONSE_UPDATE:
		update_attendee_status (pitip);
		break;
	case ITIP_VIEW_RESPONSE_CANCEL:
		update_item (pitip, response);
		break;
	case ITIP_VIEW_RESPONSE_REFRESH:
		send_item (pitip);
		break;
	default:
		break;
	}

	if (pitip->delete_message) {
		g_message ("Deleting!");
		camel_folder_delete_message (((EMFormat *) pitip->pobject.format)->folder, ((EMFormat *) pitip->pobject.format)->uid);
	}
	
        if (e_cal_get_save_schedules (pitip->current_ecal))
                return;

        if (itip_view_get_rsvp (ITIP_VIEW (pitip->view)) && status) {
                ECalComponent *comp = NULL;
                ECalComponentVType vtype;
                icalcomponent *ical_comp;
                icalproperty *prop;
                icalvalue *value;
                const char *attendee;
                GSList *l, *list = NULL;

                comp = e_cal_component_clone (pitip->comp);		
                if (comp == NULL)
                        return;
		
                vtype = e_cal_component_get_vtype (comp);

                if (pitip->my_address == NULL)
                        find_my_address (pitip, pitip->ical_comp, NULL);
                g_assert (pitip->my_address != NULL);

                ical_comp = e_cal_component_get_icalcomponent (comp);
		
                for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
                     prop != NULL;
                     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY))
                {
                        char *text;
			
                        value = icalproperty_get_value (prop);
                        if (!value)
                                continue;
			
                        attendee = icalvalue_get_string (value);
			
                        text = g_strdup (itip_strip_mailto (attendee));
                        text = g_strstrip (text);
                        if (g_strcasecmp (pitip->my_address, text))
                                list = g_slist_prepend (list, prop);
                        g_free (text);			
                }

                for (l = list; l; l = l->next) {
                        prop = l->data;
                        icalcomponent_remove_property (ical_comp, prop);
                        icalproperty_free (prop);
                }
                g_slist_free (list);

                e_cal_component_rescan (comp);
                itip_send_comp (E_CAL_COMPONENT_METHOD_REPLY, comp, pitip->current_ecal, pitip->top_level);

                g_object_unref (comp);
		
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

	/* Accounts */
	pitip->accounts = itip_addresses_get ();
	
	/* Source Lists and open ecal clients */
	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		if (!e_cal_get_sources (&pitip->source_lists[i], i, NULL))
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

	switch (pitip->method) {
	case ICAL_METHOD_PUBLISH:
	case ICAL_METHOD_REQUEST:
	case ICAL_METHOD_ADD:
	case ICAL_METHOD_CANCEL:
		/* An organizer sent this */
		e_cal_component_get_organizer (pitip->comp, &organizer);
		itip_view_set_organizer (ITIP_VIEW (pitip->view), organizer.cn ? organizer.cn : itip_strip_mailto (organizer.value));
		/* FIXME, do i need to strip the sentby somehow? Maybe with camel? */
		itip_view_set_sentby (ITIP_VIEW (pitip->view), organizer.sentby);
	
		/* FIXME try and match sender with organizer/attendee/sentby?
		   pd->from_address = camel_address_encode
		   ((CamelAddress *)from); g_message ("Detected from address %s",
		   pd->from_address);
		*/
		break;
	case ICAL_METHOD_REPLY:
	case ICAL_METHOD_REFRESH:
	case ICAL_METHOD_COUNTER:
	case ICAL_METHOD_DECLINECOUNTER:
		/* An attendee sent this */
		e_cal_component_get_attendee_list (pitip->comp, &list);
		if (list != NULL) {
			ECalComponentAttendee *attendee;
			
			attendee = list->data;
			
			itip_view_set_attendee (ITIP_VIEW (pitip->view), attendee->cn ? attendee->cn : itip_strip_mailto (attendee->value));

			e_cal_component_free_attendee_list (list);
		}
		
		break;		
	default:
		/* FIXME What to do here? */
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_ERROR);
	}	

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

	if (gstring) {
		itip_view_set_description (ITIP_VIEW (pitip->view), gstring->str);
		g_string_free (gstring, TRUE);
	}
	
	to_zone = calendar_config_get_icaltimezone ();
	
	e_cal_component_get_dtstart (pitip->comp, &datetime);
	pitip->start_time = 0;
	if (datetime.value) {
		struct tm start_tm;
		
		/* If the timezone is not in the component, guess the local time */
		/* Should we guess if the timezone is an olsen name somehow? */
		if (!datetime.value->is_utc && datetime.tzid) 
			from_zone = icalcomponent_get_timezone (pitip->top_level, datetime.tzid);
		else
			from_zone = NULL;
		
		start_tm = icaltimetype_to_tm_with_zone (datetime.value, from_zone, to_zone);

		itip_view_set_start (ITIP_VIEW (pitip->view), &start_tm);
		pitip->start_time = icaltime_as_timet_with_zone (*datetime.value, from_zone);
	}
	e_cal_component_free_datetime (&datetime);

	e_cal_component_get_dtend (pitip->comp, &datetime);
	pitip->end_time = 0;
	if (datetime.value) {
		struct tm end_tm;

		/* If the timezone is not in the component, guess the local time */
		/* Should we guess if the timezone is an olsen name somehow? */
		if (!datetime.value->is_utc && datetime.tzid) 
			from_zone = icalcomponent_get_timezone (pitip->top_level, datetime.tzid);
		else
			from_zone = NULL;
		
		end_tm = icaltimetype_to_tm_with_zone (datetime.value, from_zone, to_zone);
		
		itip_view_set_end (ITIP_VIEW (pitip->view), &end_tm);
		pitip->end_time = icaltime_as_timet_with_zone (*datetime.value, from_zone);
	}
	e_cal_component_free_datetime (&datetime);

	/* Recurrence info */
	/* FIXME Better recurring description */
	if (e_cal_component_has_recurrences (pitip->comp)) {
		itip_view_add_upper_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, "This meeting recurs");
	}
	
	gtk_container_add (GTK_CONTAINER (eb), pitip->view);

	g_signal_connect (pitip->view, "response", G_CALLBACK (view_response_cb), pitip);

	/* FIXME Do we always need to search for the server? */
	find_server (pitip, pitip->comp);
	
	return TRUE;
}

static void
pitip_free (EMFormatHTMLPObject *pobject) 
{
	FormatItipPObject *pitip = (FormatItipPObject *) pobject;

	g_free (pitip->vcalendar);
	pitip->vcalendar = NULL;

	if (pitip->comp) {
		g_object_unref (pitip->comp);
		pitip->comp = NULL;
	}

	if (pitip->top_level) {
		icalcomponent_free (pitip->top_level);
		pitip->top_level = NULL;
	}

	if (pitip->main_comp) {
		icalcomponent_free (pitip->main_comp);
		pitip->main_comp = NULL;
	}
	pitip->ical_comp = NULL;

	pitip->current = 0;
	pitip->total = 0;

	g_free (pitip->calendar_uid);
	pitip->calendar_uid = NULL;

	g_free (pitip->from_address);
	pitip->from_address = NULL;
	g_free (pitip->delegator_address);
	pitip->delegator_address = NULL;
	g_free (pitip->delegator_name);
	pitip->delegator_name = NULL;
	g_free (pitip->my_address);
	pitip->my_address = NULL;	
}

void
format_itip (EPlugin *ep, EMFormatHookTarget *target)
{
	FormatItipPObject *pitip;
	GConfClient *gconf;
	
	pitip = (FormatItipPObject *) em_format_html_add_pobject ((EMFormatHTML *) target->format, sizeof (FormatItipPObject), CLASSID, target->part, format_itip_object);
	pitip->pobject.free = pitip_free;

	gconf = gconf_client_get_default ();
	pitip->delete_message = gconf_client_get_bool (gconf, GCONF_KEY_DELETE, NULL);
	g_object_unref (gconf);
	
	camel_stream_printf (target->stream, "<table border=0 width=\"100%%\" cellpadding=3><tr>");
	camel_stream_printf (target->stream, "<td valign=top><object classid=\"%s\"></object></td><td width=100%% valign=top>", CLASSID);
	camel_stream_printf (target->stream, "</td></tr></table>");
}

static void
delete_toggled_cb (GtkWidget *widget, gpointer data)
{
	EMConfigTargetPrefs *target = data;
	
	gconf_client_set_bool (target->gconf, GCONF_KEY_DELETE, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)), NULL);
}

GtkWidget *
itip_formatter_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
	EMConfigTargetPrefs *target = (EMConfigTargetPrefs *) hook_data->config->target;
	GtkWidget *page;
	GtkWidget *tab_label;
	GtkWidget *frame;
	GtkWidget *frame_label;
	GtkWidget *padding_label;
	GtkWidget *hbox;
	GtkWidget *inner_vbox;
	GtkWidget *check;
	GtkWidget *check_gaim;

	/* Create a new notebook page */
	page = gtk_vbox_new (FALSE, 0);
	GTK_CONTAINER (page)->border_width = 12;
	tab_label = gtk_label_new (_("Meetings"));
	gtk_notebook_append_page (GTK_NOTEBOOK (hook_data->parent), page, tab_label);

	/* Frame */
	frame = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, FALSE, FALSE, 0);

	/* "General" */
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
	
	/* Delete message after acting */
	/* FIXME Need a schema for this */
	check = gtk_check_button_new_with_mnemonic (_("_Delete message after acting"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gconf_client_get_bool (target->gconf, GCONF_KEY_DELETE, NULL));
	g_signal_connect (GTK_TOGGLE_BUTTON (check), "toggled", G_CALLBACK (delete_toggled_cb), target);
	gtk_box_pack_start (GTK_BOX (inner_vbox), check, FALSE, FALSE, 0);

	/* "Conflict searching" */
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
	
	/* Source selector */
	check_gaim = gtk_label_new (_("Select the calendars to search for meeting conflicts"));
	gtk_box_pack_start (GTK_BOX (inner_vbox), check_gaim, FALSE, FALSE, 0);

	gtk_widget_show_all (page);

	return page;
}

