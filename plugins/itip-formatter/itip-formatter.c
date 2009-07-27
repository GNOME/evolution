/*
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
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-folder.h>
#include <camel/camel-vee-folder.h>
#include <camel/camel-multipart.h>
#include <camel/camel-service.h>
#include <camel/camel-store.h>
#include <camel/camel-db.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserverui/e-source-selector.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <mail/em-format-hook.h>
#include <mail/em-config.h>
#include <mail/em-format.h>
#include <mail/em-format-html.h>
#include <mail/em-utils.h>
#include <mail/mail-folder-cache.h>
#include <mail/mail-tools.h>
#include <mail/mail-mt.h>
#include <libedataserver/e-account-list.h>
#include <e-util/e-error.h>
#include <calendar/gui/calendar-config.h>
#include <calendar/gui/itip-utils.h>
#include <calendar/common/authentication.h>
#include "itip-view.h"

#define CLASSID "itip://"
#define GCONF_KEY_DELETE "/apps/evolution/itip/delete_processed"

#define d(x)

struct _itip_puri {
	EMFormatPURI puri;

	const EMFormatHandler *handle;
	CamelFolder *folder;
	CamelMimeMessage *msg;
	CamelMimePart *part;

	gchar *uid;
	GtkWidget *view;

	ESourceList *source_lists[E_CAL_SOURCE_TYPE_LAST];
	GHashTable *ecals[E_CAL_SOURCE_TYPE_LAST];

	ECal *current_ecal;
	ECalSourceType type;

	gchar *vcalendar;
	ECalComponent *comp;
	icalcomponent *main_comp;
	icalcomponent *ical_comp;
	icalcomponent *top_level;
	icalcompiter iter;
	icalproperty_method method;
	time_t start_time;
	time_t end_time;

	gint current;
	gint total;

	gchar *calendar_uid;

	EAccountList *accounts;

	gchar *from_address;
	gchar *from_name;
	gchar *to_address;
	gchar *to_name;
	gchar *delegator_address;
	gchar *delegator_name;
	gchar *my_address;
	gint   view_only;

	guint progress_info_id;

	gboolean delete_message;
	/* a reply can only be sent if and only if there is an organizer */
	gboolean has_organizer;
	/*
	 * Usually replies are sent unless the user unchecks that option.
	 * There are some cases when the default is not to sent a reply
	 * (but the user can still chose to do so by checking the option):
	 * - the organizer explicitly set RSVP=FALSE for the current user
	 * - the event has no ATTENDEEs: that's the case for most non-meeting
	 *   events
	 *
	 * The last case is meant for forwarded non-meeting
	 * events. Traditionally Evolution hasn't offered to send a
	 * reply, therefore the updated implementation mimics that
	 * behavior.
	 *
	 * Unfortunately some software apparently strips all ATTENDEEs
	 * when forwarding a meeting; in that case sending a reply is
	 * also unchecked by default. So the check for ATTENDEEs is a
	 * tradeoff between sending unwanted replies in cases where
	 * that wasn't done in the past and not sending a possibly
	 * wanted reply where that wasn't possible in the past
	 * (because replies to forwarded events were not
	 * supported). Overall that should be an improvement, and the
	 * user can always override the default.
	 */
	gboolean no_reply_wanted;

};

void format_itip (EPlugin *ep, EMFormatHookTarget *target);
GtkWidget *itip_formatter_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
static void itip_attachment_frame(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri);

typedef struct {
	struct _itip_puri *puri;
	gchar *uid;
	gchar *rid;

	gchar *sexp;

	gint count;
} FormatItipFindData;

typedef void (* FormatItipOpenFunc) (ECal *ecal, ECalendarStatus status, gpointer data);

static gboolean check_is_instance (icalcomponent *icalcomp);

static icalproperty *
find_attendee (icalcomponent *ical_comp, const gchar *address)
{
	icalproperty *prop;

	if (address == NULL)
		return NULL;

	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY)) {
		gchar *attendee;
		gchar *text;

		attendee = icalproperty_get_value_as_string_r (prop);

		 if (!attendee)
			continue;

		text = g_strdup (itip_strip_mailto (attendee));
		text = g_strstrip (text);
		if (text && !g_ascii_strcasecmp (address, text)) {
			g_free (text);
			g_free (attendee);
			break;
		}
		g_free (text);
		g_free (attendee);
	}

	return prop;
}

static icalproperty *
find_attendee_if_sentby (icalcomponent *ical_comp, const gchar *address)
{
	icalproperty *prop;

	if (address == NULL)
		return NULL;

	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY)) {
		icalparameter *param;
		const gchar *attendee_sentby;
		gchar *text;

		param = icalproperty_get_first_parameter (prop, ICAL_SENTBY_PARAMETER);
		if (!param)
			continue;

		attendee_sentby = icalparameter_get_sentby (param);

		if (!attendee_sentby)
			continue;

		text = g_strdup (itip_strip_mailto (attendee_sentby));
		text = g_strstrip (text);
		if (text && !g_ascii_strcasecmp (address, text)) {
			g_free (text);
			break;
		}
		g_free (text);
	}

	return prop;
}

static void
find_to_address (struct _itip_puri *pitip, icalcomponent *ical_comp, icalparameter_partstat *status)
{
	EIterator *it;

	it = e_list_get_iterator((EList *)pitip->accounts);

	/* Look through the list of attendees to find the user's address */

	if (!pitip->my_address)
		while (e_iterator_is_valid(it)) {
			const EAccount *account = e_iterator_get(it);
			icalproperty *prop = NULL;

			if (!account->enabled) {
				e_iterator_next(it);
				continue;
			}

			prop = find_attendee (ical_comp, account->id->address);

			if (prop) {
				gchar *text;
				icalparameter *param;

				param = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
				if (param)
					pitip->to_name = g_strdup (icalparameter_get_cn (param));

				text = icalproperty_get_value_as_string_r (prop);

				pitip->to_address = g_strdup (itip_strip_mailto (text));
				g_free (text);
				g_strstrip (pitip->to_address);

				pitip->my_address = g_strdup (account->id->address);

				param = icalproperty_get_first_parameter (prop, ICAL_RSVP_PARAMETER);
				if (param &&
				    icalparameter_get_rsvp (param) == ICAL_RSVP_FALSE) {
					pitip->no_reply_wanted = TRUE;
				}

				if (status) {
					param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
					*status = param ? icalparameter_get_partstat (param) : ICAL_PARTSTAT_NEEDSACTION;
				}

				break;
			}
			e_iterator_next(it);
		}

	e_iterator_reset (it);

	/* If the user's address was not found in the attendee's list, then the user
	 * might be responding on behalf of his/her delegator. In this case, we
	 * would want to go through the SENT-BY fields of the attendees to find
	 * the user's address.
	 *
	 * Note: This functionality could have been (easily) implemented in the
	 * previous loop, but it would hurt the performance for all providers in
	 * general. Hence, we choose to iterate through the accounts list again.
	 */

	if (!pitip->my_address)
		while (e_iterator_is_valid(it)) {
			const EAccount *account = e_iterator_get(it);
			icalproperty *prop = NULL;

			if (!account->enabled) {
				e_iterator_next(it);
				continue;
			}

			prop = find_attendee_if_sentby (ical_comp, account->id->address);

			if (prop) {
				gchar *text;
				icalparameter *param;

				param = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
				if (param)
					pitip->to_name = g_strdup (icalparameter_get_cn (param));

				text = icalproperty_get_value_as_string_r (prop);

				pitip->to_address = g_strdup (itip_strip_mailto (text));
				g_free (text);
				g_strstrip (pitip->to_address);

				pitip->my_address = g_strdup (account->id->address);

				param = icalproperty_get_first_parameter (prop, ICAL_RSVP_PARAMETER);
				if (param &&
				    ICAL_RSVP_FALSE == icalparameter_get_rsvp (param)) {
					pitip->no_reply_wanted = TRUE;
				}

				if (status) {
					param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
					*status = param ? icalparameter_get_partstat (param) : ICAL_PARTSTAT_NEEDSACTION;
				}

				break;
			}
			e_iterator_next(it);
		}

	g_object_unref(it);
}

static void
find_from_address (struct _itip_puri *pitip, icalcomponent *ical_comp)
{
	EIterator *it;
	icalproperty *prop;
	gchar *organizer;
	icalparameter *param;
	const gchar *organizer_sentby;
	gchar *organizer_clean = NULL;
	gchar *organizer_sentby_clean = NULL;

	prop = icalcomponent_get_first_property (ical_comp, ICAL_ORGANIZER_PROPERTY);

	if (!prop)
		return;

	organizer = icalproperty_get_value_as_string_r (prop);
	if (organizer) {
		organizer_clean = g_strdup (itip_strip_mailto (organizer));
		organizer_clean = g_strstrip (organizer_clean);
		g_free (organizer);
	}

	param = icalproperty_get_first_parameter (prop, ICAL_SENTBY_PARAMETER);
	if (param) {
		organizer_sentby = icalparameter_get_sentby (param);
		if (organizer_sentby) {
			organizer_sentby_clean = g_strdup (itip_strip_mailto (organizer_sentby));
			organizer_sentby_clean = g_strstrip (organizer_sentby_clean);
		}
	}

	if (!(organizer_sentby_clean || organizer_clean))
		return;

	pitip->from_address = g_strdup (organizer_clean);

	param = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
	if (param)
		pitip->from_name = g_strdup (icalparameter_get_cn (param));

	it = e_list_get_iterator((EList *)pitip->accounts);
	while (e_iterator_is_valid(it)) {
		const EAccount *account = e_iterator_get(it);

		if (!account->enabled) {
			e_iterator_next(it);
			continue;
		}

		if ((organizer_clean && !g_ascii_strcasecmp (organizer_clean, account->id->address))
		    || (organizer_sentby_clean && !g_ascii_strcasecmp (organizer_sentby_clean, account->id->address))) {
			pitip->my_address = g_strdup (account->id->address);

			break;
		}
		e_iterator_next(it);
	}
	g_object_unref(it);
	g_free (organizer_sentby_clean);
	g_free (organizer_clean);
}

static ECalComponent *
get_real_item (struct _itip_puri *pitip)
{
	ECalComponent *comp;
	icalcomponent *icalcomp;
	gboolean found = FALSE;
	const gchar *uid;

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
adjust_item (struct _itip_puri *pitip, ECalComponent *comp)
{
	ECalComponent *real_comp;

	real_comp = get_real_item (pitip);
	if (real_comp != NULL) {
		ECalComponentText text;
		const gchar *string;
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
set_buttons_sensitive (struct _itip_puri *pitip)
{
	gboolean read_only = TRUE;

	if (pitip->current_ecal)
		e_cal_is_read_only (pitip->current_ecal, &read_only, NULL);

	itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), pitip->current_ecal != NULL && !read_only);
}

static void
cal_opened_cb (ECal *ecal, ECalendarStatus status, gpointer data)
{
	struct _itip_puri *pitip = data;
	ESource *source;
	ECalSourceType source_type;

	source_type = e_cal_get_source_type (ecal);
	source = e_cal_get_source (ecal);

	g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, cal_opened_cb, NULL);

	if (status != E_CALENDAR_STATUS_OK) {
		d(printf ("Failed opening itip formatter calendar '%s' during non-search opening\n", e_source_peek_name (source)));
		itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view),
						      ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
						      _("Failed to load the calendar '%s'"),
						      e_source_peek_name (source));

		if (pitip->current_ecal == ecal) {
			pitip->current_ecal = NULL;
			itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), FALSE);
		}

		g_hash_table_remove (pitip->ecals[source_type], e_source_peek_uid (source));
		return;
	}

	if (e_cal_get_static_capability (ecal, CAL_STATIC_CAPABILITY_RECURRENCES_NO_MASTER)) {
		icalcomponent *icalcomp = e_cal_component_get_icalcomponent (pitip->comp);

		if (check_is_instance (icalcomp))
			itip_view_set_show_recur_check (ITIP_VIEW (pitip->view), TRUE);
		else
			itip_view_set_show_recur_check (ITIP_VIEW (pitip->view), FALSE);
	}

	if (pitip->type == E_CAL_SOURCE_TYPE_JOURNAL) {
		if (e_cal_get_static_capability (ecal, CAL_STATIC_CAPABILITY_HAS_UNACCEPTED_MEETING))
			itip_view_set_needs_decline (ITIP_VIEW (pitip->view), TRUE);
		else
			itip_view_set_needs_decline (ITIP_VIEW (pitip->view), FALSE);
		itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_PUBLISH);
	}

	pitip->current_ecal = ecal;

	set_buttons_sensitive (pitip);
}

static ECal *
start_calendar_server (struct _itip_puri *pitip, ESource *source, ECalSourceType type, FormatItipOpenFunc func, gpointer data)
{
	ECal *ecal;
	icaltimezone *zone = NULL;

	g_return_val_if_fail (source != NULL, NULL);

	ecal = g_hash_table_lookup (pitip->ecals[type], e_source_peek_uid (source));
	if (ecal) {
		pitip->current_ecal = ecal;

		itip_view_remove_lower_info_item (ITIP_VIEW (pitip->view), pitip->progress_info_id);
		pitip->progress_info_id = 0;

		set_buttons_sensitive (pitip);

		return ecal;
	}

	ecal = auth_new_cal_from_source (source, type);

	if (!ecal)
		return NULL;

	g_signal_connect (G_OBJECT (ecal), "cal_opened", G_CALLBACK (func), data);

	g_hash_table_insert (pitip->ecals[type], g_strdup (e_source_peek_uid (source)), ecal);

	zone = calendar_config_get_icaltimezone ();
	e_cal_set_default_timezone (ecal, zone, NULL);

	e_cal_open_async (ecal, TRUE);

	return ecal;
}

static ECal *
start_calendar_server_by_uid (struct _itip_puri *pitip, const gchar *uid, ECalSourceType type)
{
	gint i;

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
	struct _itip_puri *pitip = data;

	itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), FALSE);

	g_return_if_fail (source != NULL);

	start_calendar_server (pitip, source, pitip->type, cal_opened_cb, pitip);
}

static void
find_cal_opened_cb (ECal *ecal, ECalendarStatus status, gpointer data)
{
	FormatItipFindData *fd = data;
	struct _itip_puri *pitip = fd->puri;
	ESource *source;
	ECalSourceType source_type;
	icalcomponent *icalcomp;
	GList *objects = NULL;

	source_type = e_cal_get_source_type (ecal);
	source = e_cal_get_source (ecal);

	fd->count--;

	g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, find_cal_opened_cb, NULL);

	if (status != E_CALENDAR_STATUS_OK) {
		/* FIXME Do we really want to warn here?  If we fail
		 * to find the item, this won't be cleared but the
		 * selector might be shown */
		d(printf ("Failed opening itip formatter calendar '%s' during search opening... ", e_source_peek_name (source)));
		itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view),
						      ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
						      _("Failed to load the calendar '%s'"),
						      e_source_peek_name (source));
		if (pitip->current_ecal == ecal) {
			pitip->current_ecal = NULL;
			itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), FALSE);
		}

		g_hash_table_remove (pitip->ecals[source_type], e_source_peek_uid (source));
		goto cleanup;
	}

	/* Check for conflicts */
	/* If the query fails, we'll just ignore it */
	/* FIXME What happens for recurring conflicts? */
	if (pitip->type == E_CAL_SOURCE_TYPE_EVENT
	    && e_source_get_property (E_SOURCE (source), "conflict")
	    && !g_ascii_strcasecmp (e_source_get_property (E_SOURCE (source), "conflict"), "true")
	    && e_cal_get_object_list (ecal, fd->sexp, &objects, NULL)
	    && g_list_length (objects) > 0) {
		itip_view_add_upper_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
						      _("An appointment in the calendar '%s' conflicts with this meeting"), e_source_peek_name (source));

		e_cal_free_object_list (objects);
	}

	/* search for a master object if the detached object doesn't exist in the calendar */
	if (!pitip->current_ecal && (e_cal_get_object (ecal, fd->uid, fd->rid, &icalcomp, NULL) || (fd->rid && e_cal_get_object (ecal, fd->uid, NULL, &icalcomp, NULL)))) {
		if ((pitip->method == ICAL_METHOD_PUBLISH || pitip->method ==  ICAL_METHOD_REQUEST) &&
		    (icalcomponent_get_first_component (icalcomp, ICAL_VALARM_COMPONENT) ||
		    icalcomponent_get_first_component (icalcomp, ICAL_XAUDIOALARM_COMPONENT) ||
		    icalcomponent_get_first_component (icalcomp, ICAL_XDISPLAYALARM_COMPONENT) ||
		    icalcomponent_get_first_component (icalcomp, ICAL_XPROCEDUREALARM_COMPONENT) ||
		    icalcomponent_get_first_component (icalcomp, ICAL_XEMAILALARM_COMPONENT)))
			itip_view_set_show_keep_alarm_check (ITIP_VIEW (pitip->view), TRUE);
		else
			itip_view_set_show_keep_alarm_check (ITIP_VIEW (pitip->view), FALSE);

		icalcomponent_free (icalcomp);

		pitip->current_ecal = ecal;

		/* Provide extra info, since its not in the component */
		/* FIXME Check sequence number of meeting? */
		/* FIXME Do we need to adjust elsewhere for the delegated calendar item? */
		/* FIXME Need to update the fields in the view now */
		if (pitip->method == ICAL_METHOD_REPLY || pitip->method == ICAL_METHOD_REFRESH)
			adjust_item (pitip, pitip->comp);

		/* We clear everything because we don't really care
		 * about any other info/warnings now we found an
		 * existing versions */
		itip_view_clear_lower_info_items (ITIP_VIEW (pitip->view));
		pitip->progress_info_id = 0;

		/* FIXME Check read only state of calendar? */
		itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO,
						      _("Found the appointment in the calendar '%s'"), e_source_peek_name (source));

		set_buttons_sensitive (pitip);
	} else if (!pitip->current_ecal)
		itip_view_set_show_keep_alarm_check (ITIP_VIEW (pitip->view), FALSE);

	if (pitip->current_ecal) {
		if (e_cal_get_static_capability (pitip->current_ecal, CAL_STATIC_CAPABILITY_RECURRENCES_NO_MASTER)) {
			icalcomponent *icalcomp = e_cal_component_get_icalcomponent (pitip->comp);

			if (check_is_instance (icalcomp))
				itip_view_set_show_recur_check (ITIP_VIEW (pitip->view), TRUE);
			else
				itip_view_set_show_recur_check (ITIP_VIEW (pitip->view), FALSE);
		}

		if (pitip->type == E_CAL_SOURCE_TYPE_JOURNAL) {
			/* TODO The static capability should be made generic to convey that the calendar contains unaccepted items */
			if (e_cal_get_static_capability (pitip->current_ecal, CAL_STATIC_CAPABILITY_HAS_UNACCEPTED_MEETING))
				itip_view_set_needs_decline (ITIP_VIEW (pitip->view), TRUE);
			else
				itip_view_set_needs_decline (ITIP_VIEW (pitip->view), FALSE);

			itip_view_set_mode (ITIP_VIEW (pitip->view), ITIP_VIEW_MODE_PUBLISH);
		}
	}

 cleanup:
	d(printf ("Decreasing itip formatter search count to %d\n", fd->count));

	if (fd->count == 0) {
		gboolean rsvp_enabled = FALSE;

		itip_view_remove_lower_info_item (ITIP_VIEW (pitip->view), pitip->progress_info_id);
		pitip->progress_info_id = 0;

		/*
		 * Only allow replies if backend doesn't do that automatically.
                 * Only enable it for forwarded invitiations (PUBLISH) or direct
                 * invitiations (REQUEST), but not replies (REPLY).
		 * Replies only make sense for events with an organizer.
		 */
		if (!e_cal_get_static_capability (ecal, CAL_STATIC_CAPABILITY_SAVE_SCHEDULES) &&
                    (pitip->method == ICAL_METHOD_PUBLISH || pitip->method ==  ICAL_METHOD_REQUEST) &&
		    pitip->has_organizer) {
			rsvp_enabled = TRUE;
		}
		itip_view_set_show_rsvp (ITIP_VIEW (pitip->view), rsvp_enabled);

		/* default is chosen in extract_itip_data() based on content of the VEVENT */
		itip_view_set_rsvp (ITIP_VIEW (pitip->view), !pitip->no_reply_wanted);

		if ((pitip->method == ICAL_METHOD_PUBLISH || pitip->method ==  ICAL_METHOD_REQUEST)
		    && !pitip->current_ecal) {
			/* Reuse already declared one or rename? */
			ESource *source = NULL;
			gchar *uid;

			switch (pitip->type) {
			case E_CAL_SOURCE_TYPE_EVENT:
				uid = calendar_config_get_primary_calendar ();
				break;
			case E_CAL_SOURCE_TYPE_TODO:
				uid = calendar_config_get_primary_tasks ();
				break;
			case E_CAL_SOURCE_TYPE_JOURNAL:
				uid = calendar_config_get_primary_memos ();
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

			if (source) {
				itip_view_set_source (ITIP_VIEW (pitip->view), source);

				/* FIXME Shouldn't the buttons be sensitized here? */
			} else {
				itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, _("Unable to find any calendars"));
				itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), FALSE);
			}
		} else if (!pitip->current_ecal) {
			switch (pitip->type) {
			case E_CAL_SOURCE_TYPE_EVENT:
				itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
								      _("Unable to find this meeting in any calendar"));
				break;
			case E_CAL_SOURCE_TYPE_TODO:
				itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
								      _("Unable to find this task in any task list"));
				break;
			case E_CAL_SOURCE_TYPE_JOURNAL:
				itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
								      _("Unable to find this memo in any memo list"));
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}

		g_free (fd->uid);
		g_free (fd->rid);
		g_free (fd);
	}
}

static void
find_server (struct _itip_puri *pitip, ECalComponent *comp)
{
	FormatItipFindData *fd = NULL;
	GSList *groups, *l, *sources_conflict = NULL, *all_sources = NULL;
	const gchar *uid;
	gchar *rid = NULL;
	CamelURL *url;
	gchar *uri;
	ESource *source = NULL, *current_source = NULL;

	e_cal_component_get_uid (comp, &uid);
	rid = e_cal_component_get_recurid_as_string (comp);

        url = CAMEL_SERVICE (pitip->folder->parent_store)->url;
        uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);

	itip_view_set_buttons_sensitive (ITIP_VIEW (pitip->view), FALSE);

	groups = e_source_list_peek_groups (pitip->source_lists[pitip->type]);
	for (l = groups; l; l = l->next) {
		ESourceGroup *group;
		GSList *sources, *m;

		group = l->data;

		sources = e_source_group_peek_sources (group);
		for (m = sources; m; m = m->next) {
                        gchar *source_uri = NULL;

			source = m->data;

			if (e_source_get_property (source, "conflict"))
				sources_conflict = g_slist_prepend (sources_conflict, source);

			if (current_source)
				continue;

			source_uri = e_source_get_uri (source);
			if (source_uri && (strcmp (uri, source_uri) == 0)) {
				current_source = source;
				sources_conflict = g_slist_prepend (sources_conflict, source);

				g_free (source_uri);
				continue;
			}

			all_sources = g_slist_prepend (all_sources, source);
			g_free (source_uri);

		}
	}

	if (current_source) {
		l = sources_conflict;

		pitip->progress_info_id = itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS,
				_("Opening the calendar. Please wait.."));
	} else {
		pitip->progress_info_id = itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS,
				_("Searching for an existing version of this appointment"));

		l = all_sources;
	}

	for (; l != NULL; l = l->next) {
		ECal *ecal;
		source = l->data;

		if (!fd) {
			gchar *start = NULL, *end = NULL;

			fd = g_new0 (FormatItipFindData, 1);
			fd->puri = pitip;
			fd->uid = g_strdup (uid);
			fd->rid = rid;
			/* avoid free this at the end */
			rid = NULL;

			if (pitip->start_time && pitip->end_time) {
				start = isodate_from_time_t (pitip->start_time);
				end = isodate_from_time_t (pitip->end_time);

				fd->sexp = g_strdup_printf ("(and (occur-in-time-range? (make-time \"%s\") (make-time \"%s\")) (not (uid? \"%s\")))",
						start, end, icalcomponent_get_uid (pitip->ical_comp));
			}

			g_free (start);
			g_free (end);
		}
		fd->count++;
		d(printf ("Increasing itip formatter search count to %d\n", fd->count));

		if (current_source == source)
			pitip->current_ecal = start_calendar_server (pitip, source, pitip->type, find_cal_opened_cb, fd);
		else
			ecal = start_calendar_server (pitip, source, pitip->type, find_cal_opened_cb, fd);

	}

	g_slist_free (all_sources);
	g_slist_free (sources_conflict);
	g_free (uri);
	g_free (rid);
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

static gboolean
change_status (icalcomponent *ical_comp, const gchar *address, icalparameter_partstat status)
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
message_foreach_part (CamelMimePart *part, GSList **part_list)
{
	CamelDataWrapper *containee;
	gint parts, i;
	gint go = TRUE;

	if (!part)
		return;

	*part_list = g_slist_append (*part_list, part);

	containee = camel_medium_get_content_object (CAMEL_MEDIUM (part));

	if (containee == NULL)
		return;

	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART (containee)) {
		parts = camel_multipart_get_number (CAMEL_MULTIPART (containee));
		for (i = 0; go && i < parts; i++) {
			/* Reuse already declared *parts? */
			CamelMimePart *part = camel_multipart_get_part (CAMEL_MULTIPART (containee), i);

			message_foreach_part (part, part_list);
		}
	} else if (CAMEL_IS_MIME_MESSAGE (containee)) {
		message_foreach_part ((CamelMimePart *)containee, part_list);
	}
}

static gboolean
update_item (struct _itip_puri *pitip, ItipViewResponse response)
{
	struct icaltimetype stamp;
	icalproperty *prop;
	icalcomponent *clone;
	ECalComponent *clone_comp;
	ESource *source;
	GError *error = NULL;
	gboolean result = TRUE;
	gchar *str;

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
	str = icaltime_as_ical_string_r (stamp);
	prop = icalproperty_new_x (str);
	g_free (str);
	icalproperty_set_x_name (prop, "X-MICROSOFT-CDO-REPLYTIME");
	icalcomponent_add_property (pitip->ical_comp, prop);

	clone = icalcomponent_new_clone (pitip->ical_comp);
	icalcomponent_add_component (pitip->top_level, clone);
	icalcomponent_set_method (pitip->top_level, pitip->method);

	if (!itip_view_get_inherit_alarm_check_state (ITIP_VIEW (pitip->view))) {
		icalcomponent *alarm_comp;
		icalcompiter alarm_iter;

		alarm_iter = icalcomponent_begin_component (clone, ICAL_VALARM_COMPONENT);
		while ((alarm_comp = icalcompiter_deref (&alarm_iter)) != NULL) {
			icalcompiter_next (&alarm_iter);

			icalcomponent_remove_component (clone, alarm_comp);
			icalcomponent_free (alarm_comp);
		}
	}

	clone_comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (clone_comp, clone)) {
		itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, _("Unable to parse item"));
		result = FALSE;
		goto cleanup;
	}
	source = e_cal_get_source (pitip->current_ecal);

	if (itip_view_get_keep_alarm_check_state (ITIP_VIEW (pitip->view))) {
		ECalComponent *real_comp;
		GList *alarms, *l;
		ECalComponentAlarm *alarm;

		real_comp = get_real_item (pitip);
		if (real_comp != NULL) {
			alarms = e_cal_component_get_alarm_uids (real_comp);

			for (l = alarms; l; l = l->next) {
				alarm = e_cal_component_get_alarm (real_comp, (const gchar *)l->data);

				if (alarm) {
					ECalComponentAlarm *aclone = e_cal_component_alarm_clone (alarm);

					if (aclone) {
						e_cal_component_add_alarm (clone_comp, aclone);
						e_cal_component_alarm_free (aclone);
					}

					e_cal_component_alarm_free (alarm);
				}
			}

			cal_obj_uid_list_free (alarms);
			g_object_unref (real_comp);
		}
	}

	if ((response != ITIP_VIEW_RESPONSE_CANCEL)
		&& (response != ITIP_VIEW_RESPONSE_DECLINE)) {
		GSList *attachments = NULL, *new_attachments = NULL, *l;
		CamelMimeMessage *msg = pitip->msg;

		e_cal_component_get_attachment_list (clone_comp, &attachments);

		for (l = attachments; l; l = l->next) {
			GSList *parts = NULL, *m;
			gchar *uri, *new_uri;
			CamelMimePart *part;

			uri = l->data;

			if (!g_ascii_strncasecmp (uri, "cid:...", 7)) {
				message_foreach_part ((CamelMimePart *) msg, &parts);

				for (m = parts; m; m = m->next) {
					part = m->data;

					/* Skip the actual message and the text/calendar part */
					/* FIXME Do we need to skip anything else? */
					if (part == (CamelMimePart *) msg || part == pitip->part)
						continue;

					new_uri = em_utils_temp_save_part (NULL, part, FALSE);
					new_attachments = g_slist_append (new_attachments, new_uri);
				}

				g_slist_free (parts);

			} else if (!g_ascii_strncasecmp (uri, "cid:", 4)) {
				part = camel_mime_message_get_part_by_content_id (msg, uri + 4);
				if (part) {
					new_uri = em_utils_temp_save_part (NULL, part, FALSE);
					new_attachments = g_slist_append (new_attachments, new_uri);
				}

			} else {
				/* Preserve existing non-cid ones */
				new_attachments = g_slist_append (new_attachments, g_strdup (uri));
			}
		}

		g_slist_foreach (attachments, (GFunc) g_free, NULL);
		g_slist_free (attachments);

		e_cal_component_set_attachment_list (clone_comp, new_attachments);
	}

	if (!e_cal_receive_objects (pitip->current_ecal, pitip->top_level, &error)) {
		itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO,
						      _("Unable to send item to calendar '%s'.  %s"),
						      e_source_peek_name (source), error->message);
		g_error_free (error);
		result = FALSE;
	} else {
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
							      _("Sent to calendar '%s' as canceled"), e_source_peek_name (source));
			break;
		default:
			g_assert_not_reached ();
			break;
		}

		/* FIXME Should we hide or desensitize the buttons now? */
	}

 cleanup:
	icalcomponent_remove_component (pitip->top_level, clone);
	g_object_unref (clone_comp);
	return result;
}

/* TODO These operations should be available in e-cal-component.c */
static void
set_attendee (ECalComponent *comp, const gchar *address)
{
	icalproperty *prop;
	icalcomponent *icalcomp;
	gboolean found = FALSE;

	icalcomp = e_cal_component_get_icalcomponent (comp);

	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
			prop;
			prop = icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		const gchar *attendee = icalproperty_get_attendee (prop);

		if (!(g_str_equal (itip_strip_mailto (attendee), address)))
			icalcomponent_remove_property (icalcomp, prop);
		else
			found = TRUE;
	}

	if (!found) {
		icalparameter *param;
		gchar *temp = g_strdup_printf ("MAILTO:%s", address);

		prop = icalproperty_new_attendee ((const gchar *) temp);
		icalcomponent_add_property (icalcomp, prop);

		param = icalparameter_new_partstat (ICAL_PARTSTAT_NEEDSACTION);
		icalproperty_add_parameter (prop, param);

		param = icalparameter_new_role (ICAL_ROLE_REQPARTICIPANT);
		icalproperty_add_parameter (prop, param);

		param = icalparameter_new_cutype (ICAL_CUTYPE_INDIVIDUAL);
		icalproperty_add_parameter (prop, param);

		param = icalparameter_new_rsvp (ICAL_RSVP_TRUE);
		icalproperty_add_parameter (prop, param);

		g_free (temp);
	}

}

static gboolean
send_comp_to_attendee (ECalComponentItipMethod method, ECalComponent *comp, const gchar *user, ECal *client, const gchar *comment)
{
	gboolean status;
	ECalComponent *send_comp = e_cal_component_clone (comp);

	set_attendee (send_comp, user);

	if (comment) {
		GSList comments;
		ECalComponentText text;

		text.value = comment;
		text.altrep = NULL;

		comments.data = &text;
		comments.next = NULL;

		e_cal_component_set_comment_list (send_comp, &comments);
	}

	/* FIXME send the attachments in the request */
	status = itip_send_comp (method, send_comp, client, NULL, NULL, NULL, TRUE, FALSE);

	g_object_unref (send_comp);

	return status;
}

static void
remove_delegate (struct _itip_puri *pitip, const gchar *delegate, const gchar *delegator, ECalComponent *comp)
{
	gboolean status;
	gchar *comment = g_strdup_printf (_("Organizer has removed the delegate %s "), itip_strip_mailto (delegate));

	/* send cancellation notice to delegate */
	status = send_comp_to_attendee (E_CAL_COMPONENT_METHOD_CANCEL, pitip->comp, delegate, pitip->current_ecal, comment);
	if (status)
		send_comp_to_attendee (E_CAL_COMPONENT_METHOD_REQUEST, pitip->comp, delegator, pitip->current_ecal, comment);
	if (status) {
		itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("Sent a cancelation notice to the delegate"));
	} else
		itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("Could not send the cancelation notice to the delegate"));

	g_free (comment);

}

static void
update_x (ECalComponent *pitip_comp, ECalComponent *comp)
{
	icalcomponent *itip_icalcomp = e_cal_component_get_icalcomponent (pitip_comp);
	icalcomponent *icalcomp = e_cal_component_get_icalcomponent (comp);

	icalproperty *prop = icalcomponent_get_first_property (itip_icalcomp, ICAL_X_PROPERTY);
	while (prop) {
		const gchar *name = icalproperty_get_x_name (prop);
		if (!g_ascii_strcasecmp (name, "X-EVOLUTION-IS-REPLY")) {
			icalproperty *new_prop = icalproperty_new_x (icalproperty_get_x (prop));
			icalproperty_set_x_name (new_prop, "X-EVOLUTION-IS-REPLY");
			icalcomponent_add_property (icalcomp, new_prop);
		}
		prop = icalcomponent_get_next_property (itip_icalcomp, ICAL_X_PROPERTY);
	}

	e_cal_component_set_icalcomponent (comp, icalcomp);
}

static void
update_attendee_status (struct _itip_puri *pitip)
{
	ECalComponent *comp = NULL;
	icalcomponent *icalcomp = NULL, *org_icalcomp;
	const gchar *uid;
	gchar *rid = NULL;
	const gchar *delegate;
	GError *error = NULL;

	/* Obtain our version */
	e_cal_component_get_uid (pitip->comp, &uid);
	org_icalcomp = e_cal_component_get_icalcomponent (pitip->comp);

	rid = e_cal_component_get_recurid_as_string (pitip->comp);

	/* search for a master object if the detached object doesn't exist in the calendar */
	if (e_cal_get_object (pitip->current_ecal, uid, rid, &icalcomp, NULL) || (rid && e_cal_get_object (pitip->current_ecal, uid, NULL, &icalcomp, NULL))) {
		GSList *attendees;

		comp = e_cal_component_new ();
		if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
			icalcomponent_free (icalcomp);

			itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, "The meeting is invalid and cannot be updated");
		} else {
			e_cal_component_get_attendee_list (pitip->comp, &attendees);
			if (attendees != NULL) {
				ECalComponentAttendee *a = attendees->data;
				icalproperty *prop, *del_prop;

				prop = find_attendee (icalcomp, itip_strip_mailto (a->value));
				if ((a->status == ICAL_PARTSTAT_DELEGATED) && (del_prop = find_attendee (org_icalcomp, itip_strip_mailto (a->delto))) && !(find_attendee (icalcomp, itip_strip_mailto (a->delto)))) {
					gint response;
					delegate = icalproperty_get_attendee (del_prop);
					response = e_error_run (NULL, "org.gnome.itip-formatter:add-delegate",
								itip_strip_mailto (a->value),
								itip_strip_mailto (delegate), NULL);
					if (response == GTK_RESPONSE_YES) {
						icalcomponent_add_property (icalcomp, icalproperty_new_clone (del_prop));
						e_cal_component_rescan (comp);
					} else if (response == GTK_RESPONSE_NO) {
						remove_delegate (pitip, delegate, itip_strip_mailto (a->value), comp);
						goto cleanup;
					} else {
						goto cleanup;
					}
				}

				if (prop == NULL) {
					gint response;

					if (a->delfrom && *a->delfrom) {
						response = e_error_run (NULL, "org.gnome.itip-formatter:add-delegate",
									itip_strip_mailto (a->delfrom),
									itip_strip_mailto (a->value), NULL);
						if (response == GTK_RESPONSE_YES) {
							/* Already declared in this function */
							icalproperty *prop = find_attendee (icalcomp, itip_strip_mailto (a->value));
							icalcomponent_add_property (icalcomp,icalproperty_new_clone (prop));
							e_cal_component_rescan (comp);
						} else if (response == GTK_RESPONSE_NO) {
							remove_delegate (pitip,
									 itip_strip_mailto (a->value),
									 itip_strip_mailto (a->delfrom),
									 comp);
							goto cleanup;
						} else {
							goto cleanup;
						}
					}

					response = e_error_run (NULL, "org.gnome.itip-formatter:add-unknown-attendee", NULL);

					if (response == GTK_RESPONSE_YES) {
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
					if (a->status == ICAL_PARTSTAT_DELEGATED) {
						/* *prop already declared in this function */
						icalproperty *prop, *new_prop;

						prop = find_attendee (icalcomp, itip_strip_mailto (a->value));
						icalcomponent_remove_property (icalcomp, prop);

						new_prop = find_attendee (org_icalcomp, itip_strip_mailto (a->value));
						icalcomponent_add_property (icalcomp, icalproperty_new_clone (new_prop));
					} else
						change_status (icalcomp, itip_strip_mailto (a->value), a->status);

					e_cal_component_rescan (comp);
				}
			}
		}

		update_x (pitip->comp, comp);

		if (itip_view_get_update (ITIP_VIEW (pitip->view))) {
			e_cal_component_commit_sequence (comp);
			itip_send_comp (E_CAL_COMPONENT_METHOD_REQUEST, comp, pitip->current_ecal, NULL, NULL, NULL, TRUE, FALSE);
		}

		if (!e_cal_modify_object (pitip->current_ecal, icalcomp, rid ? CALOBJ_MOD_THIS : CALOBJ_MOD_ALL, &error)) {
			itip_view_add_lower_info_item_printf (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
							      _("Unable to update attendee. %s"), error->message);

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
	g_free (rid);
}

static void
send_item (struct _itip_puri *pitip)
{
	ECalComponent *comp;

	comp = get_real_item (pitip);

	if (comp != NULL) {
		itip_send_comp (E_CAL_COMPONENT_METHOD_REQUEST, comp, pitip->current_ecal, NULL, NULL, NULL, TRUE, FALSE);
		g_object_unref (comp);

		switch (pitip->type) {
		case E_CAL_SOURCE_TYPE_EVENT:
			itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("Meeting information sent"));
			break;
		case E_CAL_SOURCE_TYPE_TODO:
			itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("Task information sent"));
			break;
		case E_CAL_SOURCE_TYPE_JOURNAL:
			itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("Memo information sent"));
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	} else {
		switch (pitip->type) {
		case E_CAL_SOURCE_TYPE_EVENT:
			itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, _("Unable to send meeting information, the meeting does not exist"));
			break;
		case E_CAL_SOURCE_TYPE_TODO:
			itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, _("Unable to send task information, the task does not exist"));
			break;
		case E_CAL_SOURCE_TYPE_JOURNAL:
			itip_view_add_lower_info_item (ITIP_VIEW (pitip->view), ITIP_VIEW_INFO_ITEM_TYPE_ERROR, _("Unable to send memo information, the memo does not exist"));
			break;
		default:
			g_assert_not_reached ();
			break;
		}
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
set_itip_error (struct _itip_puri *pitip, GtkContainer *container, const gchar *primary, const gchar *secondary)
{
	GtkWidget *vbox, *label;
	gchar *message;

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (vbox);

	message = g_strdup_printf ("<b>%s</b>", primary);
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_markup (GTK_LABEL (label), message);
	g_free (message);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	label = gtk_label_new (secondary);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	gtk_container_add (container, vbox);
}

static gboolean
extract_itip_data (struct _itip_puri *pitip, GtkContainer *container, gboolean *have_alarms)
{
	icalproperty *prop;
	icalcomponent_kind kind = ICAL_NO_COMPONENT;
	icalcomponent *tz_comp;
	icalcompiter tz_iter;
	icalcomponent *alarm_comp;
	icalcompiter alarm_iter;
	ECalComponent *comp;
	gchar *my_address;

	if (!pitip->vcalendar) {
		set_itip_error (pitip, container,
				_("The calendar attached is not valid"),
				_("The message claims to contain a calendar, but the calendar is not a valid iCalendar."));

		return FALSE;
	}

	pitip->top_level = e_cal_util_new_top_level ();

	pitip->main_comp = icalparser_parse_string (pitip->vcalendar);
	if (pitip->main_comp == NULL || !is_icalcomp_valid (pitip->main_comp)) {
		set_itip_error (pitip, container,
				_("The calendar attached is not valid"),
				_("The message claims to contain a calendar, but the calendar is not a valid iCalendar."));

		if (pitip->main_comp) {
			icalcomponent_free (pitip->main_comp);
			pitip->main_comp = NULL;
		}

		return FALSE;
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
		    && kind != ICAL_VFREEBUSY_COMPONENT
		    && kind != ICAL_VJOURNAL_COMPONENT)
			pitip->ical_comp = get_next (&pitip->iter);
	}

	if (pitip->ical_comp == NULL) {
		set_itip_error (pitip, container,
				_("The item in the calendar is not valid"),
				_("The message does contain a calendar, but the calendar contains no events, tasks or free/busy information"));

		return FALSE;
	}

	switch (icalcomponent_isa (pitip->ical_comp)) {
	case ICAL_VEVENT_COMPONENT:
		pitip->type = E_CAL_SOURCE_TYPE_EVENT;
		pitip->has_organizer = icalcomponent_get_first_property (pitip->ical_comp, ICAL_ORGANIZER_PROPERTY) != NULL;
		if (icalcomponent_get_first_property (pitip->ical_comp, ICAL_ATTENDEE_PROPERTY) == NULL) {
			/* no attendees: assume that that this is not a meeting and organizer doesn't want a reply */
			pitip->no_reply_wanted = TRUE;
		} else {
			/*
			 * if we have attendees, then find_to_address() will check for our RSVP
			 * and set no_reply_wanted=TRUE if RSVP=FALSE for the current user
			 */
		}
		break;
	case ICAL_VTODO_COMPONENT:
		pitip->type = E_CAL_SOURCE_TYPE_TODO;
		break;
	case ICAL_VJOURNAL_COMPONENT:
		pitip->type = E_CAL_SOURCE_TYPE_JOURNAL;
		break;
	default:
		set_itip_error (pitip, container,
				_("The item in the calendar is not valid"),
				_("The message does contain a calendar, but the calendar contains no events, tasks or free/busy information"));
		return FALSE;
	}

	pitip->total = icalcomponent_count_components (pitip->main_comp, ICAL_VEVENT_COMPONENT);
	pitip->total += icalcomponent_count_components (pitip->main_comp, ICAL_VTODO_COMPONENT);
	pitip->total += icalcomponent_count_components (pitip->main_comp, ICAL_VFREEBUSY_COMPONENT);
	pitip->total += icalcomponent_count_components (pitip->main_comp, ICAL_VJOURNAL_COMPONENT);

	if (pitip->total > 1) {
		set_itip_error (pitip, container,
				_("The calendar attached contains multiple items"),
				_("To process all of these items, the file should be saved and the calendar imported"));

		return FALSE;
	} if (pitip->total > 0) {
		pitip->current = 1;
	} else {
		pitip->current = 0;
	}

	if (icalcomponent_isa (pitip->ical_comp) != ICAL_VJOURNAL_COMPONENT) {
		prop = NULL;
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (pitip->ical_comp));
		my_address = itip_get_comp_attendee (comp, NULL);
		g_object_unref (comp);
		comp = NULL;

		if (!prop)
			prop = find_attendee (pitip->ical_comp, my_address);
		if (!prop)
			prop = find_attendee_if_sentby (pitip->ical_comp, my_address);
		if (prop) {
			icalparameter *param;
			const gchar * delfrom;

			if ((param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDFROM_PARAMETER))) {
				delfrom = icalparameter_get_delegatedfrom (param);

				pitip->delegator_address = g_strdup (itip_strip_mailto (delfrom));
			}
		}
		prop = NULL;

		/* Determine any delegate sections */
		prop = icalcomponent_get_first_property (pitip->ical_comp, ICAL_X_PROPERTY);
		while (prop) {
			const gchar *x_name, *x_val;

			x_name = icalproperty_get_x_name (prop);
			x_val = icalproperty_get_x (prop);

			if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-CALENDAR-UID"))
				pitip->calendar_uid = g_strdup (x_val);
			else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-CALENDAR-URI"))
				g_warning (G_STRLOC ": X-EVOLUTION-DELEGATOR-CALENDAR-URI used");
			else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-ADDRESS"))
				pitip->delegator_address = g_strdup (x_val);
			else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-NAME"))
				pitip->delegator_name = g_strdup (x_val);

			prop = icalcomponent_get_next_property (pitip->ical_comp, ICAL_X_PROPERTY);
		}

		/* Strip out procedural alarms for security purposes */
		alarm_iter = icalcomponent_begin_component (pitip->ical_comp, ICAL_VALARM_COMPONENT);
		while ((alarm_comp = icalcompiter_deref (&alarm_iter)) != NULL) {
			icalproperty *p;

			icalcompiter_next (&alarm_iter);

			p = icalcomponent_get_first_property (alarm_comp, ICAL_ACTION_PROPERTY);
			if (icalproperty_get_action (p) == ICAL_ACTION_PROCEDURE)
				icalcomponent_remove_component (pitip->ical_comp, alarm_comp);

			icalcomponent_free (alarm_comp);
		}

		if (have_alarms) {
			alarm_iter = icalcomponent_begin_component (pitip->ical_comp, ICAL_VALARM_COMPONENT);
			*have_alarms = icalcompiter_deref (&alarm_iter) != NULL;
		}
	}

	pitip->comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (pitip->comp, pitip->ical_comp)) {
		g_object_unref (pitip->comp);
		pitip->comp = NULL;

		set_itip_error (pitip, container,
				_("The item in the calendar is not valid"),
				_("The message does contain a calendar, but the calendar contains no events, tasks or free/busy information"));

		return FALSE;
	};

	/* Add default reminder if the config says so */
	if (calendar_config_get_use_default_reminder ()) {
		ECalComponentAlarm *acomp;
		gint interval;
		CalUnits units;
		ECalComponentAlarmTrigger trigger;

		interval = calendar_config_get_default_reminder_interval ();
		units = calendar_config_get_default_reminder_units ();

		acomp = e_cal_component_alarm_new ();

		e_cal_component_alarm_set_action (acomp, E_CAL_COMPONENT_ALARM_DISPLAY);

		trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;
		memset (&trigger.u.rel_duration, 0, sizeof (trigger.u.rel_duration));

		trigger.u.rel_duration.is_neg = TRUE;

		switch (units) {
			case CAL_MINUTES:
				trigger.u.rel_duration.minutes = interval;
				break;
			case CAL_HOURS:
				trigger.u.rel_duration.hours = interval;
				break;
			case CAL_DAYS:
				trigger.u.rel_duration.days = interval;
				break;
			default:
				g_assert_not_reached ();
		}

		e_cal_component_alarm_set_trigger (acomp, trigger);
		e_cal_component_add_alarm (pitip->comp, acomp);

		e_cal_component_alarm_free (acomp);
	}

	find_from_address (pitip, pitip->ical_comp);
	find_to_address (pitip, pitip->ical_comp, NULL);

	return TRUE;
}

struct _opencal_msg {
	MailMsg base;

	gchar *command; /* command line to run */
};

static gchar *
open_calendar__desc (struct _opencal_msg *m, gint complete)
{
	return g_strdup (_("Opening calendar"));
}

static void
open_calendar__exec (struct _opencal_msg *m)
{
	if (!g_spawn_command_line_async (m->command, NULL)) {
		g_warning ("Could not launch %s", m->command);
	}
}

static void
open_calendar__free (struct _opencal_msg *m)
{
	g_free (m->command);
	m->command = NULL;
}

static MailMsgInfo open_calendar_info = {
	sizeof (struct _opencal_msg),
	(MailMsgDescFunc) open_calendar__desc,
	(MailMsgExecFunc) open_calendar__exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) open_calendar__free,
};

static gboolean
idle_open_cb (gpointer data)
{
	struct _itip_puri *pitip = data;
	struct _opencal_msg *m;
	gchar *start, *end;

	start = isodate_from_time_t (pitip->start_time);
	end = isodate_from_time_t (pitip->end_time);
	m = mail_msg_new (&open_calendar_info);
	m->command = g_strdup_printf ("evolution \"calendar://?startdate=%s&enddate=%s\"", start, end);
	mail_msg_slow_ordered_push (m);

	g_free (start);
	g_free (end);

	return FALSE;
}

static void
view_response_cb (GtkWidget *widget, ItipViewResponse response, gpointer data)
{
	struct _itip_puri *pitip = data;
	gboolean status = FALSE, delete_invitation_from_cache = FALSE;
	icalproperty *prop;
	ECalComponentTransparency trans;
	gboolean flag, save_schedules;

	if (pitip->method == ICAL_METHOD_PUBLISH || pitip->method ==  ICAL_METHOD_REQUEST) {
		if (itip_view_get_free_time_check_state (ITIP_VIEW (pitip->view)))
			e_cal_component_set_transparency (pitip->comp, E_CAL_COMPONENT_TRANSP_TRANSPARENT);
		else
			e_cal_component_set_transparency (pitip->comp, E_CAL_COMPONENT_TRANSP_OPAQUE);
	} else {
		e_cal_component_get_transparency (pitip->comp, &trans);

		if (trans == E_CAL_COMPONENT_TRANSP_NONE)
			e_cal_component_set_transparency (pitip->comp, E_CAL_COMPONENT_TRANSP_OPAQUE);
	}

	if (!pitip->to_address && pitip->current_ecal != NULL)
		e_cal_get_cal_address (pitip->current_ecal, &pitip->to_address, NULL);

	/* check if it is a  recur instance (no master object) and
	 * add a property */
	if (itip_view_get_recur_check_state (ITIP_VIEW (pitip->view))) {
		prop = icalproperty_new_x ("All");
		icalproperty_set_x_name (prop, "X-GW-RECUR-INSTANCES-MOD-TYPE");
		icalcomponent_add_property (pitip->ical_comp, prop);
	}

	/*FIXME Save schedules is misused here, remove it */
	save_schedules = e_cal_get_static_capability (pitip->current_ecal,
                                                      CAL_STATIC_CAPABILITY_SAVE_SCHEDULES);

	switch (response) {
		case ITIP_VIEW_RESPONSE_ACCEPT:
			if (pitip->type != E_CAL_SOURCE_TYPE_JOURNAL)
				status = change_status (pitip->ical_comp, pitip->to_address,
					ICAL_PARTSTAT_ACCEPTED);
			else
				status = TRUE;
			if (status) {
				e_cal_component_rescan (pitip->comp);
				flag = update_item (pitip, response);
				if (save_schedules && flag)
					delete_invitation_from_cache = TRUE;
			}
			break;
		case ITIP_VIEW_RESPONSE_TENTATIVE:
			status = change_status (pitip->ical_comp, pitip->to_address,
					ICAL_PARTSTAT_TENTATIVE);
			if (status) {
				e_cal_component_rescan (pitip->comp);
				flag = update_item (pitip, response);
				if (save_schedules && flag)
					delete_invitation_from_cache = TRUE;

			}
			break;
		case ITIP_VIEW_RESPONSE_DECLINE:
			if (pitip->type != E_CAL_SOURCE_TYPE_JOURNAL)
				status = change_status (pitip->ical_comp, pitip->to_address,
					ICAL_PARTSTAT_DECLINED);
			else {
				prop = icalproperty_new_x ("1");
				icalproperty_set_x_name (prop, "X-GW-DECLINED");
				icalcomponent_add_property (pitip->ical_comp, prop);
				status = TRUE;
			}

			if (status) {
				e_cal_component_rescan (pitip->comp);
				flag = update_item (pitip, response);
				if (save_schedules && flag)
					delete_invitation_from_cache = TRUE;
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
		case ITIP_VIEW_RESPONSE_OPEN:
			g_idle_add (idle_open_cb, pitip);
			return;
		default:
			break;
	}

	/* FIXME Remove this and handle this at the groupwise mail provider */
	if (delete_invitation_from_cache) {
		CamelFolderChangeInfo *changes = NULL;
		const gchar *tag = NULL;
		CamelMessageInfo *mi;
		mi = camel_folder_summary_uid (pitip->folder->summary, pitip->uid);
		if (mi) {
			changes = camel_folder_change_info_new ();

			if (itip_view_get_recur_check_state (ITIP_VIEW (pitip->view))) {
				/* Recurring appointment and "apply-to-all" is selected */
				camel_message_info_ref (mi);
				tag = camel_message_info_user_tag (mi, "recurrence-key");
				camel_message_info_free (mi);
				if (tag) {
					gint i = 0, count;
					GSList *list = NULL;

					count = camel_folder_summary_count (pitip->folder->summary);
					for (i = 0; i < count; i++) {
						mi = camel_folder_summary_index (pitip->folder->summary, i);
						if (!mi)
							continue;
						camel_message_info_ref (mi);
						if ( camel_message_info_user_tag (mi, "recurrence-key") && g_str_equal (camel_message_info_user_tag (mi, "recurrence-key"), tag)) {
							camel_folder_summary_remove_uid_fast (pitip->folder->summary, (gchar *)(mi->uid));
							camel_folder_change_info_remove_uid (changes, (gchar *) mi->uid);
							list = g_slist_prepend (list, (gpointer) mi->uid);

							/* step back once to have the right index */
							count--;
							i--;
						}
						camel_message_info_free (mi);
					}
					camel_db_delete_uids (pitip->folder->parent_store->cdb_w, pitip->folder->full_name, list, NULL);
					g_slist_free (list);
				}
			} else {
				/* Either not a recurring appointment or "apply-to-all" is not selected. So just delete this instance alone */
				camel_folder_summary_remove_uid(pitip->folder->summary, pitip->uid);
				camel_folder_change_info_remove_uid (changes, pitip->uid);
			}
			camel_object_trigger_event (pitip->folder, "folder_changed", changes);
			camel_folder_change_info_free (changes);
		}
	}

	if (!save_schedules && pitip->delete_message) {
		g_message ("Deleting!");
		camel_folder_delete_message (pitip->folder, pitip->uid);
	}

        if (itip_view_get_rsvp (ITIP_VIEW (pitip->view)) && status) {
                ECalComponent *comp = NULL;
                icalcomponent *ical_comp;
                icalvalue *value;
                const gchar *attendee, *comment;
                GSList *l, *list = NULL;
		gboolean found;

                comp = e_cal_component_clone (pitip->comp);
                if (comp == NULL)
                        return;

                if (pitip->to_address == NULL)
                        find_to_address (pitip, pitip->ical_comp, NULL);
                g_assert (pitip->to_address != NULL);

                ical_comp = e_cal_component_get_icalcomponent (comp);

		/* Remove all attendees except the one we are responding as */
		found = FALSE;
                for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
                     prop != NULL;
                     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY))
                {
                        gchar *text;

                        value = icalproperty_get_value (prop);
                        if (!value)
                                continue;

                        attendee = icalvalue_get_string (value);

                        text = g_strdup (itip_strip_mailto (attendee));
                        text = g_strstrip (text);

			/* We do this to ensure there is at most one
			 * attendee in the response */
			if (found || g_ascii_strcasecmp (pitip->to_address, text))
				list = g_slist_prepend (list, prop);
			else if (!g_ascii_strcasecmp (pitip->to_address, text))
				found = TRUE;
                        g_free (text);
                }

                for (l = list; l; l = l->next) {
                        prop = l->data;
                        icalcomponent_remove_property (ical_comp, prop);
                        icalproperty_free (prop);
                }
                g_slist_free (list);

		/* Add a comment if there user set one */
		comment = itip_view_get_rsvp_comment (ITIP_VIEW (pitip->view));
		if (comment) {
			GSList comments;
			ECalComponentText text;

			text.value = comment;
			text.altrep = NULL;

			comments.data = &text;
			comments.next = NULL;

			e_cal_component_set_comment_list (comp, &comments);
		}

                e_cal_component_rescan (comp);
                if (itip_send_comp (E_CAL_COMPONENT_METHOD_REPLY, comp, pitip->current_ecal, pitip->top_level, NULL, NULL, TRUE, FALSE)) {
			camel_folder_set_message_flags (pitip->folder, pitip->uid, CAMEL_MESSAGE_ANSWERED, CAMEL_MESSAGE_ANSWERED);
		}

                g_object_unref (comp);

        }
}

static gboolean
check_is_instance (icalcomponent *icalcomp)
{
	icalproperty *icalprop;

	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	while (icalprop) {
		const gchar *x_name;

		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp (x_name, "X-GW-RECURRENCE-KEY")) {
			return TRUE;
		}
		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}

	return FALSE;
}

static gboolean
in_proper_folder (CamelFolder *folder)
{
	gboolean res = TRUE;
	gint flags = 0;
	gchar *uri;

	if (!folder)
		return res;

	uri = mail_tools_folder_to_url (folder);

	if (mail_folder_cache_get_folder_info_flags (folder, &flags)) {
		/* it should be neither trash nor junk folder, */
		res = ((flags & CAMEL_FOLDER_TYPE_TRASH) !=  CAMEL_FOLDER_TYPE_TRASH &&
		       (flags & CAMEL_FOLDER_TYPE_JUNK) != CAMEL_FOLDER_TYPE_JUNK &&
			  /* it can be Inbox */
			( (flags & CAMEL_FOLDER_TYPE_INBOX) == CAMEL_FOLDER_TYPE_INBOX ||
			  /* or any other virtual folder */
			  CAMEL_IS_VEE_FOLDER (folder) ||
			  /* or anything else except of sent, outbox or drafts folder */
			  (!em_utils_folder_is_sent (folder, uri) &&
			   !em_utils_folder_is_outbox (folder, uri) &&
			   !em_utils_folder_is_drafts (folder, uri))
			));
	} else {
		/* cannot check for Inbox folder here */
		res = (folder->folder_flags & (CAMEL_FOLDER_IS_TRASH | CAMEL_FOLDER_IS_JUNK)) == 0 && (
		      (CAMEL_IS_VEE_FOLDER (folder)) || (
		      !em_utils_folder_is_sent (folder, uri) &&
		      !em_utils_folder_is_outbox (folder, uri) &&
		      !em_utils_folder_is_drafts (folder, uri)));
	}

	g_free (uri);

	return res;
}

static gboolean
format_itip_object (EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	struct _itip_puri *info;
	ECalComponentText text;
	ECalComponentOrganizer organizer;
	ECalComponentDateTime datetime;
	icaltimezone *from_zone, *to_zone;
	GString *gstring = NULL;
	GSList *list, *l;
	icalcomponent *icalcomp;
	const gchar *string, *org;
	gint i;
	gboolean response_enabled;
	gboolean have_alarms = FALSE;

	info = (struct _itip_puri *) em_format_find_puri((EMFormat *)efh, pobject->classid);

	/* Accounts */
	info->accounts = itip_addresses_get ();

	/* Source Lists and open ecal clients */
	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		if (!e_cal_get_sources (&info->source_lists[i], i, NULL))
			/* FIXME More error handling? */
			info->source_lists[i] = NULL;

		/* Initialize the ecal hashes */
		info->ecals[i] = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, cleanup_ecal);
	}

	/* FIXME Handle multiple VEVENTS with the same UID, ie detached instances */
	if (!extract_itip_data (info, GTK_CONTAINER (eb), &have_alarms))
		return TRUE;

	info->view = itip_view_new ();
	gtk_container_add (GTK_CONTAINER (eb), info->view);
	gtk_widget_show (info->view);

	response_enabled = in_proper_folder (((EMFormat*)efh)->folder);

	if (!response_enabled) {
		itip_view_set_mode (ITIP_VIEW (info->view), ITIP_VIEW_MODE_HIDE_ALL);
	} else {
		itip_view_set_show_inherit_alarm_check (ITIP_VIEW (info->view), have_alarms && (info->method == ICAL_METHOD_PUBLISH || info->method ==  ICAL_METHOD_REQUEST));

		switch (info->method) {
		case ICAL_METHOD_PUBLISH:
		case ICAL_METHOD_REQUEST:
			/*
			 * Treat meeting request (sent by organizer directly) and
			 * published evend (forwarded by organizer or attendee) alike:
			 * if the event has an organizer, then it can be replied to and
			 * we show the "accept/tentative/decline" choice.
			 * Otherwise only show "accept".
			 */
			itip_view_set_mode (ITIP_VIEW (info->view),
					    info->has_organizer ?
					    ITIP_VIEW_MODE_REQUEST :
					    ITIP_VIEW_MODE_PUBLISH);
			break;
		case ICAL_METHOD_REPLY:
			itip_view_set_mode (ITIP_VIEW (info->view), ITIP_VIEW_MODE_REPLY);
			break;
		case ICAL_METHOD_ADD:
			itip_view_set_mode (ITIP_VIEW (info->view), ITIP_VIEW_MODE_ADD);
			break;
		case ICAL_METHOD_CANCEL:
			itip_view_set_mode (ITIP_VIEW (info->view), ITIP_VIEW_MODE_CANCEL);
			break;
		case ICAL_METHOD_REFRESH:
			itip_view_set_mode (ITIP_VIEW (info->view), ITIP_VIEW_MODE_REFRESH);
			break;
		case ICAL_METHOD_COUNTER:
			itip_view_set_mode (ITIP_VIEW (info->view), ITIP_VIEW_MODE_COUNTER);
			break;
		case ICAL_METHOD_DECLINECOUNTER:
			itip_view_set_mode (ITIP_VIEW (info->view), ITIP_VIEW_MODE_DECLINECOUNTER);
			break;
		case ICAL_METHOD_X :
			/* Handle appointment requests from Microsoft Live. This is
			 * a best-at-hand-now handling. Must be revisited when we have
			 * better access to the source of such meetings */
			info->method = ICAL_METHOD_REQUEST;
			itip_view_set_mode (ITIP_VIEW (info->view), ITIP_VIEW_MODE_REQUEST);
			break;
		default:
			return FALSE;
		}
	}

	itip_view_set_item_type (ITIP_VIEW (info->view), info->type);

	if (response_enabled) {
		switch (info->method) {
		case ICAL_METHOD_REQUEST:
			/* FIXME What about the name? */
			itip_view_set_delegator (ITIP_VIEW (info->view), info->delegator_name ? info->delegator_name : info->delegator_address);
		case ICAL_METHOD_PUBLISH:
		case ICAL_METHOD_ADD:
		case ICAL_METHOD_CANCEL:
		case ICAL_METHOD_DECLINECOUNTER:
			itip_view_set_show_update (ITIP_VIEW (info->view), FALSE);

			/* An organizer sent this */
			e_cal_component_get_organizer (info->comp, &organizer);
			org = organizer.cn ? organizer.cn : itip_strip_mailto (organizer.value);

			itip_view_set_organizer (ITIP_VIEW (info->view), org);
			if (organizer.sentby)
				itip_view_set_organizer_sentby (ITIP_VIEW (info->view), itip_strip_mailto (organizer.sentby));

			if (info->my_address) {
				if (!(organizer.value && !g_ascii_strcasecmp (itip_strip_mailto (organizer.value), info->my_address))
				&& !(organizer.sentby && !g_ascii_strcasecmp (itip_strip_mailto (organizer.sentby), info->my_address))
				&& (info->to_address && g_ascii_strcasecmp (info->to_address, info->my_address)))
					itip_view_set_proxy (ITIP_VIEW (info->view), info->to_name ? info->to_name : info->to_address);
			}
			break;
		case ICAL_METHOD_REPLY:
		case ICAL_METHOD_REFRESH:
		case ICAL_METHOD_COUNTER:
			itip_view_set_show_update (ITIP_VIEW (info->view), TRUE);

			/* An attendee sent this */
			e_cal_component_get_attendee_list (info->comp, &list);
			if (list != NULL) {
				ECalComponentAttendee *attendee;

				attendee = list->data;

				itip_view_set_attendee (ITIP_VIEW (info->view), attendee->cn ? attendee->cn : itip_strip_mailto (attendee->value));

				if (attendee->sentby)
					itip_view_set_attendee_sentby (ITIP_VIEW (info->view), itip_strip_mailto (attendee->sentby));

				if (info->my_address) {
					if (!(attendee->value && !g_ascii_strcasecmp (itip_strip_mailto (attendee->value), info->my_address))
					&& !(attendee->sentby && !g_ascii_strcasecmp (itip_strip_mailto (attendee->sentby), info->my_address))
					&& (info->from_address && g_ascii_strcasecmp (info->from_address, info->my_address)))
						itip_view_set_proxy (ITIP_VIEW (info->view), info->from_name ? info->from_name : info->from_address);
				}

				e_cal_component_free_attendee_list (list);
			}
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}

	e_cal_component_get_summary (info->comp, &text);
	itip_view_set_summary (ITIP_VIEW (info->view), text.value ? text.value : _("None"));

	e_cal_component_get_location (info->comp, &string);
	itip_view_set_location (ITIP_VIEW (info->view), string);

	/* Status really only applies for REPLY */
	if (response_enabled && info->method == ICAL_METHOD_REPLY) {
		e_cal_component_get_attendee_list (info->comp, &list);
		if (list != NULL) {
			ECalComponentAttendee *a = list->data;

			switch (a->status) {
			case ICAL_PARTSTAT_ACCEPTED:
				itip_view_set_status (ITIP_VIEW (info->view), _("Accepted"));
				break;
			case ICAL_PARTSTAT_TENTATIVE:
				itip_view_set_status (ITIP_VIEW (info->view), _("Tentatively Accepted"));
				break;
			case ICAL_PARTSTAT_DECLINED:
				itip_view_set_status (ITIP_VIEW (info->view), _("Declined"));
				break;
			case ICAL_PARTSTAT_DELEGATED:
				itip_view_set_status (ITIP_VIEW (info->view), _("Delegated"));
				break;
			default:
				itip_view_set_status (ITIP_VIEW (info->view), _("Unknown"));
			}
		}
		e_cal_component_free_attendee_list (list);
	}

	if (info->method == ICAL_METHOD_REPLY
	    || info->method == ICAL_METHOD_COUNTER
	    || info->method == ICAL_METHOD_DECLINECOUNTER) {
		/* FIXME Check spec to see if multiple comments are actually valid */
		/* Comments for iTIP are limited to one per object */
		e_cal_component_get_comment_list (info->comp, &list);
		if (list) {
			ECalComponentText *text = list->data;

			if (text->value)
				itip_view_set_comment (ITIP_VIEW (info->view), text->value);
		}
		e_cal_component_free_text_list (list);
	}

	e_cal_component_get_description_list (info->comp, &list);
	for (l = list; l; l = l->next) {
		ECalComponentText *text = l->data;

		if (!gstring && text->value)
			gstring = g_string_new (text->value);
		else if (text->value)
			g_string_append_printf (gstring, "\n\n%s", text->value);
	}
	e_cal_component_free_text_list (list);

	if (gstring) {
		itip_view_set_description (ITIP_VIEW (info->view), gstring->str);
		g_string_free (gstring, TRUE);
	}

	to_zone = calendar_config_get_icaltimezone ();

	e_cal_component_get_dtstart (info->comp, &datetime);
	info->start_time = 0;
	if (datetime.value) {
		struct tm start_tm;

		/* If the timezone is not in the component, guess the local time */
		/* Should we guess if the timezone is an olsen name somehow? */
		if (datetime.value->is_utc)
			from_zone = icaltimezone_get_utc_timezone ();
		else if (!datetime.value->is_utc && datetime.tzid)
			from_zone = icalcomponent_get_timezone (info->top_level, datetime.tzid);
		else
			from_zone = NULL;

		start_tm = icaltimetype_to_tm_with_zone (datetime.value, from_zone, to_zone);

		itip_view_set_start (ITIP_VIEW (info->view), &start_tm);
		info->start_time = icaltime_as_timet_with_zone (*datetime.value, from_zone);
	}

	icalcomp = e_cal_component_get_icalcomponent (info->comp);

	/* Set the recurrence id */
	if (check_is_instance (icalcomp) && datetime.value) {
		ECalComponentRange *recur_id;
		struct icaltimetype icaltime = icaltime_convert_to_zone (*datetime.value, to_zone);

		recur_id = g_new0 (ECalComponentRange, 1);
		recur_id->type = E_CAL_COMPONENT_RANGE_SINGLE;
		recur_id->datetime.value = &icaltime;
		recur_id->datetime.tzid = icaltimezone_get_tzid (to_zone);
		e_cal_component_set_recurid (info->comp, recur_id);
		g_free (recur_id); /* it's ok to call g_free here */
	}
	e_cal_component_free_datetime (&datetime);

	e_cal_component_get_dtend (info->comp, &datetime);
	info->end_time = 0;
	if (datetime.value) {
		struct tm end_tm;

		/* If the timezone is not in the component, guess the local time */
		/* Should we guess if the timezone is an olsen name somehow? */
		if (datetime.value->is_utc)
			from_zone = icaltimezone_get_utc_timezone ();
		else if (!datetime.value->is_utc && datetime.tzid)
			from_zone = icalcomponent_get_timezone (info->top_level, datetime.tzid);
		else
			from_zone = NULL;

		end_tm = icaltimetype_to_tm_with_zone (datetime.value, from_zone, to_zone);

		itip_view_set_end (ITIP_VIEW (info->view), &end_tm);
		info->end_time = icaltime_as_timet_with_zone (*datetime.value, from_zone);
	}
	e_cal_component_free_datetime (&datetime);

	/* Recurrence info */
	/* FIXME Better recurring description */
	if (e_cal_component_has_recurrences (info->comp)) {
		/* FIXME Tell the user we don't support recurring tasks */
		switch (info->type) {
		case E_CAL_SOURCE_TYPE_EVENT:
			itip_view_add_upper_info_item (ITIP_VIEW (info->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("This meeting recurs"));
			break;
		case E_CAL_SOURCE_TYPE_TODO:
			itip_view_add_upper_info_item (ITIP_VIEW (info->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("This task recurs"));
			break;
		case E_CAL_SOURCE_TYPE_JOURNAL:
			itip_view_add_upper_info_item (ITIP_VIEW (info->view), ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("This memo recurs"));
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}

	if (response_enabled) {
		g_signal_connect (info->view, "response", G_CALLBACK (view_response_cb), info);

		itip_view_set_show_free_time_check (ITIP_VIEW (info->view), info->type == E_CAL_SOURCE_TYPE_EVENT && (info->method == ICAL_METHOD_PUBLISH || info->method ==  ICAL_METHOD_REQUEST));

		if (info->calendar_uid)
			info->current_ecal = start_calendar_server_by_uid (info, info->calendar_uid, info->type);
		else {
			find_server (info, info->comp);
			set_buttons_sensitive (info);
		}
	}

	return TRUE;
}

static void
puri_free (EMFormatPURI *puri)
{
	struct _itip_puri *pitip = (struct _itip_puri*) puri;
	gint i;

	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		if (pitip->source_lists[i])
		g_object_unref (pitip->source_lists[i]);
		pitip->source_lists[i] = NULL;

		g_hash_table_destroy (pitip->ecals[i]);
		pitip->ecals[i] = NULL;
	}

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

	g_free (pitip->calendar_uid);
	pitip->calendar_uid = NULL;

	g_free (pitip->from_address);
	pitip->from_address = NULL;
	g_free (pitip->from_name);
	pitip->from_name = NULL;
	g_free (pitip->to_address);
	pitip->to_address = NULL;
	g_free (pitip->to_name);
	pitip->to_name = NULL;
	g_free (pitip->delegator_address);
	pitip->delegator_address = NULL;
	g_free (pitip->delegator_name);
	pitip->delegator_name = NULL;
	g_free (pitip->my_address);
	pitip->my_address = NULL;
	g_free (pitip->uid);
}

void
format_itip (EPlugin *ep, EMFormatHookTarget *target)
{
	EMFormatHTMLPObject *pobj;
	GConfClient *gconf;
	gchar *classid;
	struct _itip_puri *puri;
	CamelDataWrapper *content;
	CamelStream *mem;

	classid = g_strdup_printf("itip:///%s", ((EMFormat *) target->format)->part_id->str);

	/* mark message as containing calendar, thus it will show the icon in message list now on */
	if (target->format->uid && target->format->folder &&
	    !camel_folder_get_message_user_flag (target->format->folder, target->format->uid, "$has_cal"))
		camel_folder_set_message_user_flag (target->format->folder, target->format->uid, "$has_cal", TRUE);

	puri = (struct _itip_puri *)em_format_add_puri(target->format, sizeof(struct _itip_puri), classid, target->part, itip_attachment_frame);

	pobj = em_format_html_add_pobject ((EMFormatHTML *) target->format, sizeof (EMFormatHTMLPObject), classid, target->part, format_itip_object);

	gconf = gconf_client_get_default ();
	puri->delete_message = gconf_client_get_bool (gconf, GCONF_KEY_DELETE, NULL);
	puri->has_organizer = FALSE;
	puri->no_reply_wanted = FALSE;
	puri->folder = ((EMFormat *) target->format)->folder;
	puri->uid = g_strdup (((EMFormat *) target->format)->uid);
	puri->msg = ((EMFormat *) target->format)->message;
	puri->part = target->part;
	puri->puri.free = puri_free;
	g_object_unref (gconf);

	/* This is non-gui thread. Download the part for using in the main thread */
	content = camel_medium_get_content_object ((CamelMedium *) target->part);
	mem = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream (content, mem);

	if (((CamelStreamMem *) mem)->buffer->len == 0)
		puri->vcalendar = NULL;
	else
		puri->vcalendar = g_strndup ((gchar *)((CamelStreamMem *) mem)->buffer->data, ((CamelStreamMem *) mem)->buffer->len);
	camel_object_unref (mem);

	camel_stream_printf (target->stream, "<table border=0 width=\"100%%\" cellpadding=3><tr>");
	camel_stream_printf (target->stream, "<td valign=top><object classid=\"%s\"></object></td><td width=100%% valign=top>", classid);
	camel_stream_printf (target->stream, "</td></tr></table>");

	g_free (classid);
}

static void
delete_toggled_cb (GtkWidget *widget, gpointer data)
{
	EMConfigTargetPrefs *target = data;

	gconf_client_set_bool (target->gconf, GCONF_KEY_DELETE, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)), NULL);
}

static void
initialize_selection (ESourceSelector *selector, ESourceList *source_list)
{
	GSList *groups;

	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;
		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);
			const gchar *completion = e_source_get_property (source, "conflict");
			if (completion && !g_ascii_strcasecmp (completion, "true"))
				e_source_selector_select_source (selector, source);
		}
	}
}

static void
source_selection_changed (ESourceSelector *selector, gpointer data)
{
	ESourceList *source_list = data;
	GSList *selection;
	GSList *l;
	GSList *groups;

	/* first we clear all the completion flags from all sources */
	g_message ("Clearing selection");
	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;
		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);

			g_message ("Unsetting for %s", e_source_peek_name (source));
			e_source_set_property (source, "conflict", NULL);
		}
	}

	/* then we loop over the selector's selection, setting the
	   property on those sources */
	selection = e_source_selector_get_selection (selector);
	for (l = selection; l; l = l->next) {
		g_message ("Setting for %s", e_source_peek_name (E_SOURCE (l->data)));
		e_source_set_property (E_SOURCE (l->data), "conflict", "true");
	}
	e_source_selector_free_selection (selection);

	/* FIXME show an error if this fails? */
	e_source_list_sync (source_list, NULL);
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
	GtkWidget *label;
	GtkWidget *ess;
	GtkWidget *scrolledwin;
	ESourceList *source_list;
	gchar *str;

	/* Create a new notebook page */
	page = gtk_vbox_new (FALSE, 0);
	GTK_CONTAINER (page)->border_width = 12;
	tab_label = gtk_label_new (_("Calendar and Tasks"));
	gtk_notebook_append_page (GTK_NOTEBOOK (hook_data->parent), page, tab_label);

	/* Frame */
	frame = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, FALSE, FALSE, 0);

	/* "General" */
	frame_label = gtk_label_new ("");
	str = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("General"));
	gtk_label_set_markup (GTK_LABEL (frame_label), str);
	g_free (str);
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
	str = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("Conflict Search"));
	gtk_label_set_markup (GTK_LABEL (frame_label), str);
	g_free (str);
	GTK_MISC (frame_label)->xalign = 0.0;
	gtk_box_pack_start (GTK_BOX (frame), frame_label, FALSE, FALSE, 0);

	/* Indent/padding */
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (frame), hbox, TRUE, TRUE, 0);
	padding_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), padding_label, FALSE, FALSE, 0);
	inner_vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (hbox), inner_vbox, TRUE, TRUE, 0);

	/* Source selector */
	label = gtk_label_new (_("Select the calendars to search for meeting conflicts"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (inner_vbox), label, FALSE, FALSE, 0);

	if (!e_cal_get_sources (&source_list, E_CAL_SOURCE_TYPE_EVENT, NULL)) {
	    /* FIXME Error handling */;
	}

	scrolledwin = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwin),
					     GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (inner_vbox), scrolledwin, TRUE, TRUE, 0);

	ess = e_source_selector_new (source_list);
	atk_object_set_name (gtk_widget_get_accessible (ess), _("Conflict Search"));
	gtk_container_add (GTK_CONTAINER (scrolledwin), ess);

	initialize_selection (E_SOURCE_SELECTOR (ess), source_list);

	g_signal_connect (ess, "selection_changed", G_CALLBACK (source_selection_changed), source_list);
	g_object_weak_ref (G_OBJECT (page), (GWeakNotify) g_object_unref, source_list);

	gtk_widget_show_all (page);

	return page;
}

static void
itip_attachment_frame(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri)
{
	struct _itip_puri *info = (struct _itip_puri *)puri;

	d(printf("writing to frame content, handler is '%s'\n", info->handle->mime_type));
	info->handle->handler(emf, stream, info->puri.part, info->handle);
	camel_stream_close(stream);
}

