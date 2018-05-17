/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libedataserver/libedataserver.h>

#include <shell/e-shell.h>
#include <shell/e-shell-utils.h>

#include <calendar/gui/comp-util.h>
#include <calendar/gui/itip-utils.h>

#include <mail/em-config.h>
#include <mail/em-utils.h>
#include <em-format/e-mail-formatter-utils.h>

#include "itip-view.h"
#include "e-mail-part-itip.h"

#include "itip-view-elements-defines.h"

#include "web-extension/module-itip-formatter-web-extension.h"

#define d(x)

#define MEETING_ICON "stock_people"

#define ITIP_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), ITIP_TYPE_VIEW, ItipViewPrivate))

G_DEFINE_TYPE (ItipView, itip_view, G_TYPE_OBJECT)

typedef struct  {
	ItipViewInfoItemType type;
	gchar *message;

	guint id;
} ItipViewInfoItem;

struct _ItipViewPrivate {
	EClientCache *client_cache;
	gchar *extension_name;

	ESourceRegistry *registry;
	gulong source_added_handler_id;
	gulong source_removed_handler_id;

	ItipViewMode mode;
	ECalClientSourceType type;

        gchar *sender;
	gchar *organizer;
	gchar *organizer_sentby;
	gchar *delegator;
	gchar *attendee;
	gchar *attendee_sentby;
	gchar *proxy;

	gchar *summary;

	gchar *location;
        gchar *status;
	gchar *comment;

	struct tm *start_tm;
	guint start_tm_is_date : 1;
        gchar *start_label;
        const gchar *start_header;

	struct tm *end_tm;
	guint end_tm_is_date : 1;
        gchar *end_label;
        const gchar *end_header;

	GSList *upper_info_items;
	GSList *lower_info_items;

	guint next_info_item_id;

	gchar *description;

	guint buttons_sensitive : 1;

        gboolean is_recur_set;

	guint needs_decline : 1;

        gpointer itip_part_ptr; /* not referenced, only for a "reference" to which part this belongs */

	GDBusProxy *web_extension;
	guint web_extension_watch_name_id;
	guint web_extension_source_changed_cb_signal_id;
	guint web_extension_recur_toggled_signal_id;

	guint64 page_id;
	gchar *part_id;

        gchar *error;
	GWeakRef *web_view_weakref;

	CamelFolder *folder;
	CamelMimeMessage *message;
	gchar *message_uid;
	CamelMimePart *itip_mime_part;
	GCancellable *cancellable;

	ECalClient *current_client;

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
	gboolean with_detached_instances;

	gchar *calendar_uid;

	gchar *from_address;
	gchar *from_name;
	gchar *to_address;
	gchar *to_name;
	gchar *delegator_address;
	gchar *delegator_name;
	gchar *my_address;
	gint   view_only;

	guint progress_info_id;

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

	guint update_item_progress_info_id;
	guint update_item_error_info_id;
	ItipViewResponse update_item_response;
	GHashTable *real_comps; /* ESource's UID -> ECalComponent stored on the server */
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE,
	PROP_EXTENSION_NAME
};

enum {
	SOURCE_SELECTED,
	RESPONSE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
format_date_and_time_x (struct tm *date_tm,
                        struct tm *current_tm,
                        gboolean use_24_hour_format,
                        gboolean show_midnight,
                        gboolean show_zero_seconds,
                        gboolean is_date,
                        gchar *buffer,
                        gint buffer_size)
{
	gchar *format;
	struct tm tomorrow_tm, week_tm;

	/* Calculate a normalized "tomorrow" */
	tomorrow_tm = *current_tm;
	/* Don't need this if date is in the past. Also, year assumption won't fail. */
	if (date_tm->tm_year >= current_tm->tm_year && tomorrow_tm.tm_mday == time_days_in_month (current_tm->tm_year + 1900, current_tm->tm_mon)) {
		tomorrow_tm.tm_mday = 1;
		if (tomorrow_tm.tm_mon == 11) {
			tomorrow_tm.tm_mon = 1;
			tomorrow_tm.tm_year++;
		} else {
			tomorrow_tm.tm_mon++;
		}
	} else {
		tomorrow_tm.tm_mday++;
	}

	/* Calculate a normalized "next seven days" */
	week_tm = *current_tm;
	/* Don't need this if date is in the past. Also, year assumption won't fail. */
	if (date_tm->tm_year >= current_tm->tm_year && week_tm.tm_mday + 6 > time_days_in_month (date_tm->tm_year + 1900, date_tm->tm_mon)) {
		week_tm.tm_mday = (week_tm.tm_mday + 6) % time_days_in_month (date_tm->tm_year + 1900, date_tm->tm_mon);
		if (week_tm.tm_mon == 11) {
			week_tm.tm_mon = 1;
			week_tm.tm_year++;
		} else {
			week_tm.tm_mon++;
		}
	} else {
		week_tm.tm_mday += 6;
	}

	/* Today */
	if (date_tm->tm_mday == current_tm->tm_mday &&
	    date_tm->tm_mon == current_tm->tm_mon &&
	    date_tm->tm_year == current_tm->tm_year) {
		if (is_date || (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0)) {
			/* strftime format of a weekday and a date. */
			format = _("Today");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a time,
				 * in 24-hour format, without seconds. */
				format = _("Today %H:%M");
			else
				/* strftime format of a time,
				 * in 24-hour format. */
				format = _("Today %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a time,
				 * in 12-hour format, without seconds. */
				format = _("Today %l:%M %p");
			else
				/* strftime format of a time,
				 * in 12-hour format. */
				format = _("Today %l:%M:%S %p");
		}

	/* Tomorrow */
	} else if (date_tm->tm_mday == tomorrow_tm.tm_mday &&
		   date_tm->tm_mon == tomorrow_tm.tm_mon &&
		   date_tm->tm_year == tomorrow_tm.tm_year) {
		if (is_date || (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0)) {
			/* strftime format of a weekday and a date. */
			format = _("Tomorrow");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a time,
				 * in 24-hour format, without seconds. */
				format = _("Tomorrow %H:%M");
			else
				/* strftime format of a time,
				 * in 24-hour format. */
				format = _("Tomorrow %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a time,
				 * in 12-hour format, without seconds. */
				format = _("Tomorrow %l:%M %p");
			else
				/* strftime format of a time,
				 * in 12-hour format. */
				format = _("Tomorrow %l:%M:%S %p");
		}

	/* Within 6 days */
	} else if ((date_tm->tm_year >= current_tm->tm_year &&
		    date_tm->tm_mon >= current_tm->tm_mon &&
		    date_tm->tm_mday >= current_tm->tm_mday) &&

		   (date_tm->tm_year < week_tm.tm_year ||

		   (date_tm->tm_year == week_tm.tm_year &&
		    date_tm->tm_mon < week_tm.tm_mon) ||

		   (date_tm->tm_year == week_tm.tm_year &&
		    date_tm->tm_mon == week_tm.tm_mon &&
		    date_tm->tm_mday < week_tm.tm_mday))) {
		if (is_date || (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0)) {
			/* strftime format of a weekday. */
			format = _("%A");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday and a
				 * time, in 24-hour format, without seconds. */
				format = _("%A %H:%M");
			else
				/* strftime format of a weekday and a
				 * time, in 24-hour format. */
				format = _("%A %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday and a
				 * time, in 12-hour format, without seconds. */
				format = _("%A %l:%M %p");
			else
				/* strftime format of a weekday and a
				 * time, in 12-hour format. */
				format = _("%A %l:%M:%S %p");
		}

	/* This Year */
	} else if (date_tm->tm_year == current_tm->tm_year) {
		if (is_date || (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0)) {
			/* strftime format of a weekday and a date
			 * without a year. */
			format = _("%A, %B %e");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date
				 * without a year and a time,
				 * in 24-hour format, without seconds. */
				format = _("%A, %B %e %H:%M");
			else
				/* strftime format of a weekday, a date without a year
				 * and a time, in 24-hour format. */
				format = _("%A, %B %e %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date without a year
				 * and a time, in 12-hour format, without seconds. */
				format = _("%A, %B %e %l:%M %p");
			else
				/* strftime format of a weekday, a date without a year
				 * and a time, in 12-hour format. */
				format = _("%A, %B %e %l:%M:%S %p");
		}
	} else {
		if (is_date || (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0)) {
			/* strftime format of a weekday and a date. */
			format = _("%A, %B %e, %Y");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				 * time, in 24-hour format, without seconds. */
				format = _("%A, %B %e, %Y %H:%M");
			else
				/* strftime format of a weekday, a date and a
				 * time, in 24-hour format. */
				format = _("%A, %B %e, %Y %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				 * time, in 12-hour format, without seconds. */
				format = _("%A, %B %e, %Y %l:%M %p");
			else
				/* strftime format of a weekday, a date and a
				 * time, in 12-hour format. */
				format = _("%A, %B %e, %Y %l:%M:%S %p");
		}
	}

	/* strftime returns 0 if the string doesn't fit, and leaves the buffer
	 * undefined, so we set it to the empty string in that case. */
	if (e_utf8_strftime_fix_am_pm (buffer, buffer_size, format, date_tm) == 0)
		buffer[0] = '\0';
}

static gchar *
dupe_first_bold (const gchar *format,
                 const gchar *first,
                 const gchar *second)
{
	gchar *f, *s, *res;

	f = g_markup_printf_escaped ("<b>%s</b>", first ? first : "");
	s = g_markup_escape_text (second ? second : "", -1);

	res = g_strdup_printf (format, f, s);

	g_free (f);
	g_free (s);

	return res;
}

static gchar *
set_calendar_sender_text (ItipView *view)
{
	ItipViewPrivate *priv;
	const gchar *organizer, *attendee;
	gchar *sender = NULL;
	gchar *on_behalf_of = NULL;

	priv = view->priv;

	organizer = priv->organizer ? priv->organizer : _("An unknown person");
	attendee = priv->attendee ? priv->attendee : _("An unknown person");

	/* The current account ID (i.e. the delegatee) is receiving a copy of the request/response. Here we ask the delegatee to respond/accept on behalf of the delegator. */
	if (priv->organizer && priv->proxy)
		on_behalf_of = dupe_first_bold (_("Please respond on behalf of %s"), priv->proxy, NULL);
	else if (priv->attendee && priv->proxy)
		on_behalf_of = dupe_first_bold (_("Received on behalf of %s"), priv->proxy, NULL);

	switch (priv->mode) {
	case ITIP_VIEW_MODE_PUBLISH:
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s has published the following meeting information:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has published the following meeting information:"), organizer, NULL);
		break;
	case ITIP_VIEW_MODE_REQUEST:
		/* FIXME is the delegator stuff handled correctly here? */
		if (priv->delegator) {
			sender = dupe_first_bold (_("%s has delegated the following meeting to you:"), priv->delegator, NULL);
		} else {
			if (priv->organizer_sentby)
				sender = dupe_first_bold (_("%s through %s requests your presence at the following meeting:"), organizer, priv->organizer_sentby);
			else
				sender = dupe_first_bold (_("%s requests your presence at the following meeting:"), organizer, NULL);
		}
		break;
	case ITIP_VIEW_MODE_ADD:
		/* FIXME What text for this? */
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s wishes to add to an existing meeting:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s wishes to add to an existing meeting:"), organizer, NULL);
		break;
	case ITIP_VIEW_MODE_REFRESH:
		if (priv->attendee_sentby)
			sender = dupe_first_bold (_("%s through %s wishes to receive the latest information for the following meeting:"), attendee, priv->attendee_sentby);
		else
			sender = dupe_first_bold (_("%s wishes to receive the latest information for the following meeting:"), attendee, NULL);
		break;
	case ITIP_VIEW_MODE_REPLY:
		if (priv->attendee_sentby)
			sender = dupe_first_bold (_("%s through %s has sent back the following meeting response:"), attendee, priv->attendee_sentby);
		else
			sender = dupe_first_bold (_("%s has sent back the following meeting response:"), attendee, NULL);
		break;
	case ITIP_VIEW_MODE_CANCEL:
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s has cancelled the following meeting:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has cancelled the following meeting:"), organizer, NULL);
		break;
	case ITIP_VIEW_MODE_COUNTER:
		if (priv->attendee_sentby)
			sender = dupe_first_bold (_("%s through %s has proposed the following meeting changes."), attendee, priv->attendee_sentby);
		else
			sender = dupe_first_bold (_("%s has proposed the following meeting changes:"), attendee, NULL);
		break;
	case ITIP_VIEW_MODE_DECLINECOUNTER:
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s has declined the following meeting changes:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has declined the following meeting changes:"), organizer, NULL);
		break;
	default:
		break;
	}

	if (sender && on_behalf_of) {
		gchar *tmp;
		tmp = g_strjoin (NULL, sender, "\n", on_behalf_of, NULL);
		g_free (sender);
		sender = tmp;
	}

	g_free (on_behalf_of);

	return sender;
}

static gchar *
set_tasklist_sender_text (ItipView *view)
{
	ItipViewPrivate *priv;
	const gchar *organizer, *attendee;
	gchar *sender = NULL;
	gchar *on_behalf_of = NULL;

	priv = view->priv;

	organizer = priv->organizer ? priv->organizer : _("An unknown person");
	attendee = priv->attendee ? priv->attendee : _("An unknown person");

	/* The current account ID (i.e. the delegatee) is receiving a copy of the request/response. Here we ask the delegatee to respond/accept on behalf of the delegator. */
	if (priv->organizer && priv->proxy)
		on_behalf_of = dupe_first_bold (_("Please respond on behalf of %s"), priv->proxy, NULL);
	else if (priv->attendee && priv->proxy)
		on_behalf_of = dupe_first_bold (_("Received on behalf of %s"), priv->proxy, NULL);

	switch (priv->mode) {
	case ITIP_VIEW_MODE_PUBLISH:
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s has published the following task:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has published the following task:"), organizer, NULL);
		break;
	case ITIP_VIEW_MODE_REQUEST:
		/* FIXME is the delegator stuff handled correctly here? */
		if (priv->delegator) {
			sender = dupe_first_bold (_("%s requests the assignment of %s to the following task:"), organizer, priv->delegator);
		} else {
			if (priv->organizer_sentby)
				sender = dupe_first_bold (_("%s through %s has assigned you a task:"), organizer, priv->organizer_sentby);
			else
				sender = dupe_first_bold (_("%s has assigned you a task:"), organizer, NULL);
		}
		break;
	case ITIP_VIEW_MODE_ADD:
		/* FIXME What text for this? */
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s wishes to add to an existing task:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s wishes to add to an existing task:"), organizer, NULL);
		break;
	case ITIP_VIEW_MODE_REFRESH:
		if (priv->attendee_sentby)
			sender = dupe_first_bold (_("%s through %s wishes to receive the latest information for the following assigned task:"), attendee, priv->attendee_sentby);
		else
			sender = dupe_first_bold (_("%s wishes to receive the latest information for the following assigned task:"), attendee, NULL);
		break;
	case ITIP_VIEW_MODE_REPLY:
		if (priv->attendee_sentby)
			sender = dupe_first_bold (_("%s through %s has sent back the following assigned task response:"), attendee, priv->attendee_sentby);
		else
			sender = dupe_first_bold (_("%s has sent back the following assigned task response:"), attendee, NULL);
		break;
	case ITIP_VIEW_MODE_CANCEL:
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s has cancelled the following assigned task:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has cancelled the following assigned task:"), organizer, NULL);
		break;
	case ITIP_VIEW_MODE_COUNTER:
		if (priv->attendee_sentby)
			sender = dupe_first_bold (_("%s through %s has proposed the following task assignment changes:"), attendee, priv->attendee_sentby);
		else
			sender = dupe_first_bold (_("%s has proposed the following task assignment changes:"), attendee, NULL);
		break;
	case ITIP_VIEW_MODE_DECLINECOUNTER:
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s has declined the following assigned task:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has declined the following assigned task:"), organizer, NULL);
		break;
	default:
		break;
	}

	if (sender && on_behalf_of) {
		gchar *tmp;
		tmp = g_strjoin (NULL, sender, "\n", on_behalf_of, NULL);
		g_free (sender);
		sender = tmp;
	}

	g_free (on_behalf_of);

	return sender;
}

static gchar *
set_journal_sender_text (ItipView *view)
{
	ItipViewPrivate *priv;
	const gchar *organizer;
	gchar *sender = NULL;
	gchar *on_behalf_of = NULL;

	priv = view->priv;

	organizer = priv->organizer ? priv->organizer : _("An unknown person");

	/* The current account ID (i.e. the delegatee) is receiving a copy of the request/response. Here we ask the delegatee to respond/accept on behalf of the delegator. */
	if (priv->organizer && priv->proxy)
		on_behalf_of = dupe_first_bold (_("Please respond on behalf of %s"), priv->proxy, NULL);
	else if (priv->attendee && priv->proxy)
		on_behalf_of = dupe_first_bold (_("Received on behalf of %s"), priv->proxy, NULL);

	switch (priv->mode) {
	case ITIP_VIEW_MODE_PUBLISH:
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s has published the following memo:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has published the following memo:"), organizer, NULL);
		break;
	case ITIP_VIEW_MODE_ADD:
		/* FIXME What text for this? */
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s wishes to add to an existing memo:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s wishes to add to an existing memo:"), organizer, NULL);
		break;
	case ITIP_VIEW_MODE_CANCEL:
		if (priv->organizer_sentby)
			sender = dupe_first_bold (_("%s through %s has cancelled the following shared memo:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has cancelled the following shared memo:"), organizer, NULL);
		break;
	default:
		break;
	}

	if (sender && on_behalf_of)
		sender = g_strjoin (NULL, sender, "\n", on_behalf_of, NULL);

	g_free (on_behalf_of);

	return sender;
}

static void
enable_button (ItipView *view,
	       const gchar *button_id,
               gboolean enable)
{
	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"EnableButton",
		g_variant_new ("(tssb)", view->priv->page_id, view->priv->part_id, button_id, enable),
		NULL);
}

static void
show_button (ItipView *view,
             const gchar *id)
{
	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"ShowButton",
		g_variant_new ("(tss)", view->priv->page_id, view->priv->part_id, id),
		NULL);
}

static void
hide_element (ItipView *view,
	      const gchar *element_id,
              gboolean hide)
{
	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"HideElement",
		g_variant_new ("(tssb)", view->priv->page_id, view->priv->part_id, element_id, hide),
		NULL);
}

static gboolean
element_is_hidden (ItipView *view,
                   const gchar *element_id)
{
	GVariant *result;
	gboolean hidden;

	if (!view->priv->web_extension)
		return FALSE;

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
			view->priv->web_extension,
			"ElementIsHidden",
			g_variant_new ("(tss)", view->priv->page_id, view->priv->part_id, element_id),
			NULL);

	if (result) {
		g_variant_get (result, "(b)", &hidden);
		g_variant_unref (result);
		return hidden;
	}

	return FALSE;
}

static void
set_inner_html (ItipView *view,
	        const gchar *element_id,
                const gchar *inner_html)
{
	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"ElementSetInnerHTML",
		g_variant_new ("(tsss)", view->priv->page_id, view->priv->part_id, element_id, inner_html),
		NULL);
}

static void
input_set_checked (ItipView *view,
                   const gchar *input_id,
                   gboolean checked)
{
	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"InputSetChecked",
		g_variant_new ("(tssb)", view->priv->page_id, view->priv->part_id, input_id, checked),
		NULL);
}

static gboolean
input_is_checked (ItipView *view,
                  const gchar *input_id)
{
	GVariant *result;
	gboolean checked;

	if (!view->priv->web_extension)
		return FALSE;

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
			view->priv->web_extension,
			"InputIsChecked",
			g_variant_new ("(tss)", view->priv->page_id, view->priv->part_id, input_id),
			NULL);

	if (result) {
		g_variant_get (result, "(b)", &checked);
		g_variant_unref (result);
		return checked;
	}

	return FALSE;
}

static void
show_checkbox (ItipView *view,
               const gchar *id,
               gboolean show,
	       gboolean update_second)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"ShowCheckbox",
		g_variant_new ("(tssbb)", view->priv->page_id, view->priv->part_id, id, show, update_second),
		NULL);
}

static void
set_area_text (ItipView *view,
               const gchar *id,
               const gchar *text)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"SetAreaText",
		g_variant_new ("(tsss)", view->priv->page_id, view->priv->part_id, id, text ? text : ""),
		NULL);
}

static void
set_sender_text (ItipView *view)
{
	ItipViewPrivate *priv;
	priv = view->priv;

	if (priv->sender)
		g_free (priv->sender);

	switch (priv->type) {
	case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		priv->sender = set_calendar_sender_text (view);
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
		priv->sender = set_tasklist_sender_text (view);
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
		priv->sender = set_journal_sender_text (view);
		break;
	default:
		priv->sender = NULL;
		break;
	}

	if (priv->sender && priv->web_extension)
		set_inner_html (view, TEXT_ROW_SENDER, priv->sender);
}

static void
update_start_end_times (ItipView *view)
{
	ItipViewPrivate *priv;
	gchar buffer[256];
	time_t now;
	struct tm *now_tm;

	priv = view->priv;

	now = time (NULL);
	now_tm = localtime (&now);

	if (priv->start_label)
		g_free (priv->start_label);
	if (priv->end_label)
		g_free (priv->end_label);

	#define is_same(_member) (priv->start_tm->_member == priv->end_tm->_member)
	if (priv->start_tm && priv->end_tm && priv->start_tm_is_date && priv->end_tm_is_date
	    && is_same (tm_mday) && is_same (tm_mon) && is_same (tm_year)) {
		/* it's an all day event in one particular day */
		format_date_and_time_x (priv->start_tm, now_tm, FALSE, TRUE, FALSE, priv->start_tm_is_date, buffer, 256);
		priv->start_label = g_strdup (buffer);
		priv->start_header = _("All day:");
		priv->end_header = NULL;
		priv->end_label = NULL;
	} else {
		if (priv->start_tm) {
			format_date_and_time_x (priv->start_tm, now_tm, FALSE, TRUE, FALSE, priv->start_tm_is_date, buffer, 256);
			priv->start_header = priv->start_tm_is_date ? _("Start day:") : _("Start time:");
			priv->start_label = g_strdup (buffer);
		} else {
			priv->start_header = NULL;
			priv->start_label = NULL;
		}

		if (priv->end_tm) {
			format_date_and_time_x (priv->end_tm, now_tm, FALSE, TRUE, FALSE, priv->end_tm_is_date, buffer, 256);
			priv->end_header = priv->end_tm_is_date ? _("End day:") : _("End time:");
			priv->end_label = g_strdup (buffer);
		} else {
			priv->end_header = NULL;
			priv->end_label = NULL;
		}
	}
	#undef is_same

	if (!priv->web_extension)
		return;

	if (priv->start_header && priv->start_label) {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			priv->web_extension,
			"UpdateTimes",
			g_variant_new (
				"(tssss)",
				view->priv->page_id,
				view->priv->part_id,
				TABLE_ROW_START_DATE,
				priv->start_header,
				priv->start_label),
			NULL);
	} else
		hide_element (view, TABLE_ROW_START_DATE, TRUE);

	if (priv->end_header && priv->end_label) {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			priv->web_extension,
			"UpdateTimes",
			g_variant_new (
				"(tssss)",
				view->priv->page_id,
				view->priv->part_id,
				TABLE_ROW_END_DATE,
				priv->end_header,
				priv->end_label),
			NULL);
	} else
		hide_element (view, TABLE_ROW_END_DATE, TRUE);
}

static void
itip_view_itip_button_clicked_cb (EWebView *web_view,
				  const gchar *element_class,
				  const gchar *element_value,
				  const GtkAllocation *element_position,
				  gpointer user_data)
{
	ItipView *view = user_data;
	gboolean can_use;
	gchar *tmp;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_class && *element_class);
	g_return_if_fail (element_value && *element_value);
	g_return_if_fail (ITIP_IS_VIEW (view));

	tmp = g_strdup_printf ("%p:", view->priv->itip_part_ptr);
	can_use = g_str_has_prefix (element_value, tmp);
	if (can_use)
		element_value += strlen (tmp);
	g_free (tmp);

	if (can_use) {
		gint response = atoi (element_value);

		g_signal_emit (view, signals[RESPONSE], 0, response);
	}
}

static void
itip_view_register_clicked_listener (ItipView *view)
{
	EWebView *web_view;

	g_return_if_fail (ITIP_IS_VIEW (view));

	web_view = itip_view_ref_web_view (view);
	if (web_view) {
		e_web_view_register_element_clicked (web_view, "itip-button",
			itip_view_itip_button_clicked_cb, view);
	}

	g_clear_object (&web_view);
}

static void
recur_toggled_signal_cb (GDBusConnection *connection,
                         const gchar *sender_name,
                         const gchar *object_path,
                         const gchar *interface_name,
                         const gchar *signal_name,
                         GVariant *parameters,
                         ItipView *view)
{
	guint64 page_id = 0;
	const gchar *part_id = NULL;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (g_strcmp0 (signal_name, "RecurToggled") != 0)
		return;

	g_variant_get (parameters, "(t&s)", &page_id, &part_id);

	if (view->priv->page_id == page_id &&
	    g_strcmp0 (view->priv->part_id, part_id) == 0)
		itip_view_set_mode (view, view->priv->mode);
}

static void
source_changed_cb (ItipView *view)
{
	ESource *source;

	source = itip_view_ref_source (view);

	if (source) {
		d (printf ("Source changed to '%s'\n", e_source_get_display_name (source)));
		g_signal_emit (view, signals[SOURCE_SELECTED], 0, source);

		g_object_unref (source);
	}
}

static void
source_changed_cb_signal_cb (GDBusConnection *connection,
			     const gchar *sender_name,
			     const gchar *object_path,
			     const gchar *interface_name,
			     const gchar *signal_name,
			     GVariant *parameters,
			     gpointer user_data)
{
	ItipView *view = user_data;
	guint64 page_id = 0;
	const gchar *part_id = NULL;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (g_strcmp0 (signal_name, "SourceChanged") != 0)
		return;

	g_variant_get (parameters, "(t&s)", &page_id, &part_id);

	if (view->priv->page_id == page_id &&
	    g_strcmp0 (view->priv->part_id, part_id) == 0)
		source_changed_cb (view);
}

static void
append_checkbox_table_row (GString *buffer,
                           const gchar *name,
                           const gchar *label)
{
	gchar *access_key, *html_label;

	html_label = e_mail_formatter_parse_html_mnemonics (label, &access_key);

	g_string_append_printf (
		buffer,
		"<tr id=\"table_row_%s\" hidden=\"\"><td colspan=\"2\">"
		"<input type=\"checkbox\" name=\"%s\" id=\"%s\" value=\"%s\" >"
		"<label for=\"%s\" accesskey=\"%s\">%s</label>"
		"</td></tr>\n",
		name, name, name, name, name,
		access_key ? access_key : "", html_label);

	g_free (html_label);

	if (access_key)
		g_free (access_key);
}

static void
append_text_table_row (GString *buffer,
                       const gchar *id,
                       const gchar *label,
                       const gchar *value)
{
	if (label && *label) {

		g_string_append_printf (
			buffer,
			"<tr id=\"%s\" %s><th%s>%s</th><td>%s</td></tr>\n",
			id, (value && *value) ? "" : "hidden=\"\"",
			g_strcmp0 (id, TABLE_ROW_COMMENT) == 0 ? " style=\"vertical-align: top;\"" : "",
			label, value ? value : "");

	} else {

		g_string_append_printf (
			buffer,
			"<tr id=\"%s\"%s><td colspan=\"2\">%s</td></tr>\n",
			id, g_strcmp0 (id, TABLE_ROW_SUMMARY) == 0 ? "" : " hidden=\"\"", value ? value : "");

	}
}

static void
append_text_table_row_nonempty (GString *buffer,
                                const gchar *id,
                                const gchar *label,
                                const gchar *value)
{
	if (!value || !*value)
		return;

	append_text_table_row (buffer, id, label, value);
}

static void
append_info_item_row (ItipView *view,
                      const gchar *table_id,
                      ItipViewInfoItem *item)
{
	const gchar *icon_name;
	gchar *row_id;

	switch (item->type) {
		case ITIP_VIEW_INFO_ITEM_TYPE_INFO:
			icon_name = "dialog-information";
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_WARNING:
			icon_name = "dialog-warning";
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_ERROR:
			icon_name = "dialog-error";
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS:
			icon_name = "edit-find";
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_NONE:
			default:
			icon_name = NULL;
	}

	row_id = g_strdup_printf ("%s_row_%d", table_id, item->id);

	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"AppendInfoItemRow",
		g_variant_new (
			"(tsssss)",
			view->priv->page_id,
			view->priv->part_id,
			table_id,
			row_id,
			icon_name,
			item->message),
		NULL);

	g_free (row_id);

	d (printf ("Added row %s_row_%d ('%s')\n", table_id, item->id, item->message));
}

static void
remove_info_item_row (ItipView *view,
                      const gchar *table_id,
                      guint id)
{
	gchar *row_id;

	row_id = g_strdup_printf ("%s_row_%d", table_id, id);

	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"RemoveElement",
		g_variant_new ("(tss)", view->priv->page_id, view->priv->part_id, row_id),
		NULL);

	g_free (row_id);

	d (printf ("Removed row %s_row_%d\n", table_id, id));
}

static void
buttons_table_write_button (GString *buffer,
			    gpointer itip_part_ptr,
                            const gchar *name,
                            const gchar *label,
                            const gchar *icon,
                            ItipViewResponse response)
{
	gchar *access_key, *html_label;

	html_label = e_mail_formatter_parse_html_mnemonics (label, &access_key);

	if (icon) {
		gint icon_width, icon_height;

		if (!gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &icon_width, &icon_height)) {
			icon_width = 16;
			icon_height = 16;
		}

		g_string_append_printf (
			buffer,
			"<td><button class=\"itip-button\" type=\"button\" name=\"%s\" value=\"%p:%d\" id=\"%s\" accesskey=\"%s\" hidden disabled>"
			"<div><img src=\"gtk-stock://%s?size=%d\" width=\"%dpx\" height=\"%dpx\"> <span>%s</span></div>"
			"</button></td>\n",
			name, itip_part_ptr, response, name, access_key ? access_key : "" , icon,
			GTK_ICON_SIZE_BUTTON, icon_width, icon_height, html_label);
	} else {
		g_string_append_printf (
			buffer,
			"<td><button class=\"itip-button\" type=\"button\" name=\"%s\" value=\"%p:%d\" id=\"%s\" accesskey=\"%s\" hidden disabled>"
			"<div><span>%s</span></div>"
			"</button></td>\n",
			name, itip_part_ptr, response, name, access_key ? access_key : "" , html_label);
	}

	g_free (html_label);

	if (access_key)
		g_free (access_key);
}

static void
append_buttons_table (GString *buffer,
		      gpointer itip_part_ptr)
{
	g_string_append (
		buffer,
		"<table class=\"itip buttons\" border=\"0\" "
		"id=\"" TABLE_BUTTONS "\" cellspacing=\"6\" "
		"cellpadding=\"0\" >"
		"<tr id=\"" TABLE_ROW_BUTTONS "\">");

        /* Everything gets the open button */
	buttons_table_write_button (
		buffer, itip_part_ptr, BUTTON_OPEN_CALENDAR, _("Ope_n Calendar"),
		"go-jump", ITIP_VIEW_RESPONSE_OPEN);
	buttons_table_write_button (
		buffer, itip_part_ptr, BUTTON_DECLINE_ALL, _("_Decline all"),
		NULL, ITIP_VIEW_RESPONSE_DECLINE);
	buttons_table_write_button (
		buffer, itip_part_ptr, BUTTON_DECLINE, _("_Decline"),
		NULL, ITIP_VIEW_RESPONSE_DECLINE);
	buttons_table_write_button (
		buffer, itip_part_ptr, BUTTON_TENTATIVE_ALL, _("_Tentative all"),
		NULL, ITIP_VIEW_RESPONSE_TENTATIVE);
	buttons_table_write_button (
		buffer, itip_part_ptr, BUTTON_TENTATIVE, _("_Tentative"),
		NULL, ITIP_VIEW_RESPONSE_TENTATIVE);
	buttons_table_write_button (
		buffer, itip_part_ptr, BUTTON_ACCEPT_ALL, _("Acce_pt all"),
		NULL, ITIP_VIEW_RESPONSE_ACCEPT);
	buttons_table_write_button (
		buffer, itip_part_ptr, BUTTON_ACCEPT, _("Acce_pt"),
		NULL, ITIP_VIEW_RESPONSE_ACCEPT);
	buttons_table_write_button (
		buffer, itip_part_ptr, BUTTON_SEND_INFORMATION, _("Send _Information"),
		NULL, ITIP_VIEW_RESPONSE_REFRESH);
	buttons_table_write_button (
		buffer, itip_part_ptr, BUTTON_UPDATE_ATTENDEE_STATUS, _("_Update Attendee Status"),
		NULL, ITIP_VIEW_RESPONSE_UPDATE);
	buttons_table_write_button (
		buffer, itip_part_ptr, BUTTON_UPDATE,  _("_Update"),
		NULL, ITIP_VIEW_RESPONSE_CANCEL);

	g_string_append (buffer, "</tr></table>");
}

static void
itip_view_rebuild_source_list (ItipView *view)
{
	ESourceRegistry *registry;
	GList *list, *link;
	const gchar *extension_name;

	d (printf ("Assigning a new source list!\n"));

	if (!view->priv->web_extension)
		return;

	registry = view->priv->registry;
	extension_name = itip_view_get_extension_name (view);

	if (extension_name == NULL)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"ElementRemoveChildNodes",
		g_variant_new ("(tss)", view->priv->page_id, view->priv->part_id, SELECT_ESOURCE),
		NULL);

	list = e_source_registry_list_enabled (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESource *parent;

		parent = e_source_registry_ref_source (
			registry, e_source_get_parent (source));

		e_util_invoke_g_dbus_proxy_call_with_error_check (
			view->priv->web_extension,
			"RebuildSourceList",
			g_variant_new (
				"(tsssssb)",
				view->priv->page_id,
				view->priv->part_id,
				e_source_get_uid (parent),
				e_source_get_display_name (parent),
				e_source_get_uid (source),
				e_source_get_display_name (source),
				e_source_get_writable (source)),
			NULL);

		g_object_unref (parent);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	source_changed_cb (view);
}

static void
itip_view_source_added_cb (ESourceRegistry *registry,
                           ESource *source,
                           ItipView *view)
{
	const gchar *extension_name;

	extension_name = itip_view_get_extension_name (view);

	/* If we don't have an extension name set
	 * yet then disregard the signal emission. */
	if (extension_name == NULL)
		return;

	if (e_source_has_extension (source, extension_name))
		itip_view_rebuild_source_list (view);
}

static void
itip_view_source_removed_cb (ESourceRegistry *registry,
                             ESource *source,
                             ItipView *view)
{
	const gchar *extension_name;

	extension_name = itip_view_get_extension_name (view);

	/* If we don't have an extension name set
	 * yet then disregard the signal emission. */
	if (extension_name == NULL)
		return;

	if (e_source_has_extension (source, extension_name))
		itip_view_rebuild_source_list (view);
}

static void
itip_view_set_client_cache (ItipView *view,
                            EClientCache *client_cache)
{
	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (view->priv->client_cache == NULL);

	view->priv->client_cache = g_object_ref (client_cache);
}

static void
itip_view_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			itip_view_set_client_cache (
				ITIP_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_EXTENSION_NAME:
			itip_view_set_extension_name (
				ITIP_VIEW (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
itip_view_get_property (GObject *object,
                        guint property_id,
                        GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			g_value_set_object (
				value,
				itip_view_get_client_cache (
				ITIP_VIEW (object)));
			return;

		case PROP_EXTENSION_NAME:
			g_value_set_string (
				value,
				itip_view_get_extension_name (
				ITIP_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
itip_view_dispose (GObject *object)
{
	ItipViewPrivate *priv;

	priv = ITIP_VIEW_GET_PRIVATE (object);

	if (priv->source_added_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->registry,
			priv->source_added_handler_id);
		priv->source_added_handler_id = 0;
	}

	if (priv->source_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->registry,
			priv->source_removed_handler_id);
		priv->source_removed_handler_id = 0;
	}

	if (priv->web_extension_watch_name_id > 0) {
		g_bus_unwatch_name (priv->web_extension_watch_name_id);
		priv->web_extension_watch_name_id = 0;
	}

	if (priv->web_extension_recur_toggled_signal_id > 0) {
		g_dbus_connection_signal_unsubscribe (
			g_dbus_proxy_get_connection (priv->web_extension),
			priv->web_extension_recur_toggled_signal_id);
		priv->web_extension_recur_toggled_signal_id = 0;
	}

	if (priv->web_extension_source_changed_cb_signal_id > 0) {
		g_dbus_connection_signal_unsubscribe (
			g_dbus_proxy_get_connection (priv->web_extension),
			priv->web_extension_source_changed_cb_signal_id);
		priv->web_extension_source_changed_cb_signal_id = 0;
	}

	g_clear_object (&priv->client_cache);
	g_clear_object (&priv->registry);
	g_clear_object (&priv->web_extension);
	g_clear_object (&priv->cancellable);
	g_clear_object (&priv->comp);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (itip_view_parent_class)->dispose (object);
}

static void
itip_view_finalize (GObject *object)
{
	ItipViewPrivate *priv;
	GSList *iter;

	priv = ITIP_VIEW_GET_PRIVATE (object);

	d (printf ("Itip view finalized!\n"));

	g_free (priv->extension_name);
	g_free (priv->sender);
	g_free (priv->organizer);
	g_free (priv->organizer_sentby);
	g_free (priv->delegator);
	g_free (priv->attendee);
	g_free (priv->attendee_sentby);
	g_free (priv->proxy);
	g_free (priv->summary);
	g_free (priv->location);
	g_free (priv->status);
	g_free (priv->comment);
	g_free (priv->start_tm);
	g_free (priv->start_label);
	g_free (priv->end_tm);
	g_free (priv->end_label);
	g_free (priv->description);
	g_free (priv->error);
	g_free (priv->part_id);

	for (iter = priv->lower_info_items; iter; iter = iter->next) {
		ItipViewInfoItem *item = iter->data;
		g_free (item->message);
		g_free (item);
	}

	g_slist_free (priv->lower_info_items);

	for (iter = priv->upper_info_items; iter; iter = iter->next) {
		ItipViewInfoItem *item = iter->data;
		g_free (item->message);
		g_free (item);
	}

	g_slist_free (priv->upper_info_items);

	e_weak_ref_free (priv->web_view_weakref);

	g_free (priv->vcalendar);
	g_free (priv->calendar_uid);
	g_free (priv->from_address);
	g_free (priv->from_name);
	g_free (priv->to_address);
	g_free (priv->to_name);
	g_free (priv->delegator_address);
	g_free (priv->delegator_name);
	g_free (priv->my_address);
	g_free (priv->message_uid);

	g_clear_object (&priv->folder);
	g_clear_object (&priv->message);
	g_clear_object (&priv->itip_mime_part);

	if (priv->top_level != NULL)
		icalcomponent_free (priv->top_level);

	if (priv->main_comp != NULL)
		icalcomponent_free (priv->main_comp);

	g_hash_table_destroy (priv->real_comps);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (itip_view_parent_class)->finalize (object);
}

static void
itip_view_constructed (GObject *object)
{
	ItipView *view;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	gulong handler_id;

	view = ITIP_VIEW (object);
	client_cache = itip_view_get_client_cache (view);
	registry = e_client_cache_ref_registry (client_cache);

	/* Keep our own reference on the ESourceRegistry
	 * to use when disconnecting these signal handlers. */
	view->priv->registry = g_object_ref (registry);

	handler_id = g_signal_connect (
		view->priv->registry, "source-added",
		G_CALLBACK (itip_view_source_added_cb), view);
	view->priv->source_added_handler_id = handler_id;

	handler_id = g_signal_connect (
		view->priv->registry, "source-removed",
		G_CALLBACK (itip_view_source_removed_cb), view);
	view->priv->source_removed_handler_id = handler_id;

	g_object_unref (registry);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (itip_view_parent_class)->constructed (object);
}

static void
itip_view_class_init (ItipViewClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ItipViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = itip_view_set_property;
	object_class->get_property = itip_view_get_property;
	object_class->dispose = itip_view_dispose;
	object_class->finalize = itip_view_finalize;
	object_class->constructed = itip_view_constructed;

	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			"Cache of shared EClient instances",
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_EXTENSION_NAME,
		g_param_spec_string (
			"extension-name",
			"Extension Name",
			"Show only data sources with this extension",
			NULL,
			G_PARAM_READWRITE));

	signals[SOURCE_SELECTED] = g_signal_new (
		"source_selected",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ItipViewClass, source_selected),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_SOURCE);

	signals[RESPONSE] = g_signal_new (
		"response",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ItipViewClass, response),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);
}

EClientCache *
itip_view_get_client_cache (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->client_cache;
}

const gchar *
itip_view_get_extension_name (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->extension_name;
}

void
itip_view_set_extension_name (ItipView *view,
                              const gchar *extension_name)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	/* Avoid unnecessary rebuilds. */
	if (g_strcmp0 (extension_name, view->priv->extension_name) == 0)
		return;

	g_free (view->priv->extension_name);
	view->priv->extension_name = g_strdup (extension_name);

	g_object_notify (G_OBJECT (view), "extension-name");

	itip_view_rebuild_source_list (view);
}

void
itip_view_write (gpointer itip_part_ptr,
		 EMailFormatter *formatter,
                 GString *buffer)
{
	gint icon_width, icon_height;
	gchar *header = e_mail_formatter_get_html_header (formatter);

	g_string_append (buffer, header);
	g_free (header);

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &icon_width, &icon_height)) {
		icon_width = 16;
		icon_height = 16;
	}

	g_string_append_printf (
		buffer,
		"<img src=\"gtk-stock://%s?size=%d\" class=\"itip icon\" width=\"%dpx\" height=\"%dpx\"/>\n",
			MEETING_ICON, GTK_ICON_SIZE_BUTTON, icon_width, icon_height);

	g_string_append (
		buffer,
		"<div class=\"itip content\" id=\"" DIV_ITIP_CONTENT "\">\n");

        /* The first section listing the sender */
        /* FIXME What to do if the send and organizer do not match */
	g_string_append (
		buffer,
		"<div id=\"" TEXT_ROW_SENDER "\" class=\"itip sender\"></div>\n");

	g_string_append (buffer, "<hr>\n");

        /* Elementary event information */
	g_string_append (
		buffer,
		"<table class=\"itip table\" border=\"0\" "
		"cellspacing=\"5\" cellpadding=\"0\">\n");

	append_text_table_row (buffer, TABLE_ROW_SUMMARY, NULL, NULL);
	append_text_table_row (buffer, TABLE_ROW_LOCATION, _("Location:"), NULL);
	append_text_table_row (buffer, TABLE_ROW_START_DATE, _("Start time:"), NULL);
	append_text_table_row (buffer, TABLE_ROW_END_DATE, _("End time:"), NULL);
	append_text_table_row (buffer, TABLE_ROW_STATUS, _("Status:"), NULL);
	append_text_table_row (buffer, TABLE_ROW_COMMENT, _("Comment:"), NULL);

	g_string_append (buffer, "</table>\n");

	/* Upper Info items */
	g_string_append (
		buffer,
		"<table class=\"itip info\" id=\"" TABLE_UPPER_ITIP_INFO "\" border=\"0\" "
		"cellspacing=\"5\" cellpadding=\"0\">");

        /* Description */
	g_string_append (
		buffer,
		"<div id=\"" TABLE_ROW_DESCRIPTION "\" class=\"itip description\" hidden=\"\"></div>\n");

	g_string_append (buffer, "<hr>\n");

	/* Lower Info items */
	g_string_append (
		buffer,
		"<table class=\"itip info\" id=\"" TABLE_LOWER_ITIP_INFO "\" border=\"0\" "
		"cellspacing=\"5\" cellpadding=\"0\">");

	g_string_append (
		buffer,
		"<table class=\"itip table\" border=\"0\" "
		"cellspacing=\"5\" cellpadding=\"0\">\n");

	g_string_append (
		buffer,
		"<tr id=\"" TABLE_ROW_ESCB "\" hidden=\"\""">"
		"<th><label id=\"" TABLE_ROW_ESCB_LABEL "\" for=\"" SELECT_ESOURCE "\"></label></th>"
		"<td><select name=\"" SELECT_ESOURCE "\" id=\"" SELECT_ESOURCE "\"></select></td>"
		"</tr>\n");

	/* RSVP area */
	append_checkbox_table_row (buffer, CHECKBOX_RSVP, _("Send reply to sender"));

        /* Comments */
	g_string_append_printf (
		buffer,
		"<tr id=\"" TABLE_ROW_RSVP_COMMENT "\" hidden=\"\">"
		"<th>%s</th>"
		"<td><textarea name=\"" TEXTAREA_RSVP_COMMENT "\" "
		"id=\"" TEXTAREA_RSVP_COMMENT "\" "
		"rows=\"3\" cols=\"40\" disabled=\"\">"
		"</textarea></td>\n"
		"</tr>\n",
		_("Comment:"));

        /* Updates */
	append_checkbox_table_row (buffer, CHECKBOX_UPDATE, _("Send _updates to attendees"));

        /* The recurrence check button */
	append_checkbox_table_row (buffer, CHECKBOX_RECUR, _("_Apply to all instances"));
	append_checkbox_table_row (buffer, CHECKBOX_FREE_TIME, _("Show time as _free"));
	append_checkbox_table_row (buffer, CHECKBOX_KEEP_ALARM, _("_Preserve my reminder"));
	append_checkbox_table_row (buffer, CHECKBOX_INHERIT_ALARM, _("_Inherit reminder"));

	g_string_append (buffer, "</table>\n");

        /* Buttons table */
	append_buttons_table (buffer, itip_part_ptr);

        /* <div class="itip content" > */
	g_string_append (buffer, "</div>\n");

	g_string_append (buffer, "<div class=\"itip error\" id=\"" DIV_ITIP_ERROR "\"></div>");

	g_string_append (buffer, "</body></html>");
}

void
itip_view_write_for_printing (ItipView *view,
                              GString *buffer)
{
	if (view->priv->error && *view->priv->error) {
		g_string_append (buffer, view->priv->error);
		return;
	}

	g_string_append (
		buffer,
		"<div class=\"itip print_content\" id=\"" DIV_ITIP_CONTENT "\">\n");

        /* The first section listing the sender */
	if (view->priv->sender && *view->priv->sender) {
		/* FIXME What to do if the send and organizer do not match */
		g_string_append_printf (
			buffer,
			"<div id=\"" TEXT_ROW_SENDER "\" class=\"itip sender\">%s</div>\n",
			view->priv->sender);

		g_string_append (buffer, "<hr>\n");
	}

        /* Elementary event information */
	g_string_append (
		buffer,
		"<table class=\"itip table\" border=\"0\" "
		"cellspacing=\"5\" cellpadding=\"0\">\n");

	append_text_table_row_nonempty (
		buffer, TABLE_ROW_SUMMARY,
		NULL, view->priv->summary);
	append_text_table_row_nonempty (
		buffer, TABLE_ROW_LOCATION,
		_("Location:"), view->priv->location);
	append_text_table_row_nonempty (
		buffer, TABLE_ROW_START_DATE,
		view->priv->start_header, view->priv->start_label);
	append_text_table_row_nonempty (
		buffer, TABLE_ROW_END_DATE,
		view->priv->end_header, view->priv->end_label);
	append_text_table_row_nonempty (
		buffer, TABLE_ROW_STATUS,
		_("Status:"), view->priv->status);
	append_text_table_row_nonempty (
		buffer, TABLE_ROW_COMMENT,
		_("Comment:"), view->priv->comment);

	g_string_append (buffer, "</table><br>\n");

        /* Description */
	if (view->priv->description && *view->priv->description) {
		g_string_append_printf (
			buffer,
			"<div id=\"" TABLE_ROW_DESCRIPTION "\" "
			"class=\"itip description\" %s>%s</div>\n",
			view->priv->description ? "" : "hidden=\"\"", view->priv->description);
	}

	g_string_append (buffer, "</div>");
}

static void
web_extension_proxy_created_cb (GDBusProxy *proxy,
                                GAsyncResult *result,
                                ItipView *view)
{
	GError *error = NULL;

	view->priv->web_extension = g_dbus_proxy_new_finish (result, &error);
	if (!view->priv->web_extension) {
		g_warning ("Error creating web extension proxy: %s\n", error->message);
		g_error_free (error);
	}

	view->priv->web_extension_source_changed_cb_signal_id =
		g_dbus_connection_signal_subscribe (
			g_dbus_proxy_get_connection (view->priv->web_extension),
			g_dbus_proxy_get_name (view->priv->web_extension),
			MODULE_ITIP_FORMATTER_WEB_EXTENSION_INTERFACE,
			"SourceChanged",
			MODULE_ITIP_FORMATTER_WEB_EXTENSION_OBJECT_PATH,
			NULL,
			G_DBUS_SIGNAL_FLAGS_NONE,
			source_changed_cb_signal_cb,
			view,
			NULL);

	view->priv->web_extension_recur_toggled_signal_id =
		g_dbus_connection_signal_subscribe (
			g_dbus_proxy_get_connection (view->priv->web_extension),
			g_dbus_proxy_get_name (view->priv->web_extension),
			MODULE_ITIP_FORMATTER_WEB_EXTENSION_INTERFACE,
			"RecurToggled",
			MODULE_ITIP_FORMATTER_WEB_EXTENSION_OBJECT_PATH,
			NULL,
			G_DBUS_SIGNAL_FLAGS_NONE,
			(GDBusSignalCallback) recur_toggled_signal_cb,
			view,
			NULL);

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"CreateDOMBindings",
		g_variant_new ("(ts)", view->priv->page_id, view->priv->part_id),
		NULL);

	itip_view_init_view (view);
}

static void
web_extension_appeared_cb (GDBusConnection *connection,
                           const gchar *name,
                           const gchar *name_owner,
                           ItipView *view)
{
	g_dbus_proxy_new (
		connection,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
		G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
		G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL,
		name,
		MODULE_ITIP_FORMATTER_WEB_EXTENSION_OBJECT_PATH,
		MODULE_ITIP_FORMATTER_WEB_EXTENSION_INTERFACE,
		NULL,
		(GAsyncReadyCallback)web_extension_proxy_created_cb,
		view);
}

static void
web_extension_vanished_cb (GDBusConnection *connection,
                           const gchar *name,
                           ItipView *view)
{
	g_clear_object (&view->priv->web_extension);
}

static void
itip_view_watch_web_extension (ItipView *view)
{
	view->priv->web_extension_watch_name_id =
		g_bus_watch_name (
			G_BUS_TYPE_SESSION,
			MODULE_ITIP_FORMATTER_WEB_EXTENSION_SERVICE_NAME,
			G_BUS_NAME_WATCHER_FLAGS_NONE,
			(GBusNameAppearedCallback) web_extension_appeared_cb,
			(GBusNameVanishedCallback) web_extension_vanished_cb,
			view, NULL);
}

GDBusProxy *
itip_view_get_web_extension_proxy (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->web_extension;
}

static void
itip_view_init (ItipView *view)
{
	EShell *shell;
	EClientCache *client_cache;

	shell = e_shell_get_default ();
	client_cache = e_shell_get_client_cache (shell);

	view->priv = ITIP_VIEW_GET_PRIVATE (view);
	view->priv->web_view_weakref = e_weak_ref_new (NULL);
	view->priv->real_comps = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	view->priv->client_cache = g_object_ref (client_cache);
}

ItipView *
itip_view_new (guint64 page_id,
               const gchar *part_id,
	       gpointer itip_part_ptr,
	       CamelFolder *folder,
	       const gchar *message_uid,
	       CamelMimeMessage *message,
	       CamelMimePart *itip_mime_part,
	       const gchar *vcalendar,
	       GCancellable *cancellable)
{
	ItipView *view;

	view = ITIP_VIEW (g_object_new (ITIP_TYPE_VIEW, NULL));
	view->priv->page_id = page_id;
	view->priv->part_id = g_strdup (part_id);
	view->priv->itip_part_ptr = itip_part_ptr;
	view->priv->folder = g_object_ref (folder);
	view->priv->message_uid = g_strdup (message_uid);
	view->priv->message = g_object_ref (message);
	view->priv->itip_mime_part = g_object_ref (itip_mime_part);
	view->priv->vcalendar = g_strdup (vcalendar);
	view->priv->cancellable = g_object_ref (cancellable);

	itip_view_watch_web_extension (view);

	return view;
}

void
itip_view_set_mode (ItipView *view,
                    ItipViewMode mode)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	view->priv->mode = mode;

	set_sender_text (view);

	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"ElementHideChildNodes",
		g_variant_new ("(tss)", view->priv->page_id, view->priv->part_id, TABLE_ROW_BUTTONS),
		NULL);

	view->priv->is_recur_set = itip_view_get_recur_check_state (view);

        /* Always visible */
	show_button (view, BUTTON_OPEN_CALENDAR);

	switch (mode) {
	case ITIP_VIEW_MODE_PUBLISH:
		if (view->priv->needs_decline) {
			show_button (view, BUTTON_DECLINE);
		}
		show_button (view, BUTTON_ACCEPT);
		break;
	case ITIP_VIEW_MODE_REQUEST:
		show_button (view, view->priv->is_recur_set ? BUTTON_DECLINE_ALL : BUTTON_DECLINE);
		show_button (view, view->priv->is_recur_set ? BUTTON_TENTATIVE_ALL : BUTTON_TENTATIVE);
		show_button (view, view->priv->is_recur_set ? BUTTON_ACCEPT_ALL : BUTTON_ACCEPT);
		break;
	case ITIP_VIEW_MODE_ADD:
		if (view->priv->type != E_CAL_CLIENT_SOURCE_TYPE_MEMOS) {
			show_button (view, BUTTON_DECLINE);
			show_button (view, BUTTON_TENTATIVE);
		}
		show_button (view, BUTTON_ACCEPT);
		break;
	case ITIP_VIEW_MODE_REFRESH:
		show_button (view, BUTTON_SEND_INFORMATION);
		break;
	case ITIP_VIEW_MODE_REPLY:
		show_button (view, BUTTON_UPDATE_ATTENDEE_STATUS);
		break;
	case ITIP_VIEW_MODE_CANCEL:
		show_button (view, BUTTON_UPDATE);
		break;
	case ITIP_VIEW_MODE_COUNTER:
	case ITIP_VIEW_MODE_DECLINECOUNTER:
		show_button (view, BUTTON_DECLINE);
		show_button (view, BUTTON_TENTATIVE);
		show_button (view, BUTTON_ACCEPT);
		break;
	default:
		break;
	}
}

ItipViewMode
itip_view_get_mode (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), ITIP_VIEW_MODE_NONE);

	return view->priv->mode;
}

void
itip_view_set_item_type (ItipView *view,
                         ECalClientSourceType type)
{
	const gchar *header;
	gchar *access_key, *html_label;

	g_return_if_fail (ITIP_IS_VIEW (view));

	view->priv->type = type;

	if (!view->priv->web_extension)
		return;

	switch (view->priv->type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			header = _("_Calendar:");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			header = _("_Tasks:");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			header = _("_Memos:");
			break;
		default:
			header = NULL;
			break;
	}

	if (!header) {
		set_sender_text (view);
		return;
	}

	html_label = e_mail_formatter_parse_html_mnemonics (header, &access_key);

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"ElementSetAccessKey",
		g_variant_new ("(tsss)", view->priv->page_id, view->priv->part_id, TABLE_ROW_ESCB_LABEL, access_key),
		NULL);

	set_inner_html (view, TABLE_ROW_ESCB_LABEL, html_label);

	g_free (html_label);

	if (access_key)
		g_free (access_key);

	set_sender_text (view);
}

ECalClientSourceType
itip_view_get_item_type (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), E_CAL_CLIENT_SOURCE_TYPE_LAST);

	return view->priv->type;
}

void
itip_view_set_organizer (ItipView *view,
                         const gchar *organizer)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->organizer)
		g_free (view->priv->organizer);

	view->priv->organizer = e_utf8_ensure_valid (organizer);

	set_sender_text (view);
}

const gchar *
itip_view_get_organizer (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->organizer;
}

void
itip_view_set_organizer_sentby (ItipView *view,
                                const gchar *sentby)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->organizer_sentby)
		g_free (view->priv->organizer_sentby);

	view->priv->organizer_sentby = e_utf8_ensure_valid (sentby);

	set_sender_text (view);
}

const gchar *
itip_view_get_organizer_sentby (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->organizer_sentby;
}

void
itip_view_set_attendee (ItipView *view,
                        const gchar *attendee)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->attendee)
		g_free (view->priv->attendee);

	view->priv->attendee = e_utf8_ensure_valid (attendee);

	set_sender_text (view);
}

const gchar *
itip_view_get_attendee (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->attendee;
}

void
itip_view_set_attendee_sentby (ItipView *view,
                               const gchar *sentby)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->attendee_sentby)
		g_free (view->priv->attendee_sentby);

	view->priv->attendee_sentby = e_utf8_ensure_valid (sentby);

	set_sender_text (view);
}

const gchar *
itip_view_get_attendee_sentby (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->attendee_sentby;
}

void
itip_view_set_proxy (ItipView *view,
                     const gchar *proxy)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->proxy)
		g_free (view->priv->proxy);

	view->priv->proxy = e_utf8_ensure_valid (proxy);

	set_sender_text (view);
}

const gchar *
itip_view_get_proxy (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->proxy;
}

void
itip_view_set_delegator (ItipView *view,
                         const gchar *delegator)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->delegator)
		g_free (view->priv->delegator);

	view->priv->delegator = e_utf8_ensure_valid (delegator);

	set_sender_text (view);
}

const gchar *
itip_view_get_delegator (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->delegator;
}

void
itip_view_set_summary (ItipView *view,
                       const gchar *summary)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->summary)
		g_free (view->priv->summary);

	view->priv->summary = summary ? g_strstrip (e_utf8_ensure_valid (summary)) : NULL;

	set_area_text (view, TABLE_ROW_SUMMARY, view->priv->summary);
}

const gchar *
itip_view_get_summary (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->summary;
}

void
itip_view_set_location (ItipView *view,
                        const gchar *location)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->location)
		g_free (view->priv->location);

	view->priv->location = location ? g_strstrip (e_utf8_ensure_valid (location)) : NULL;

	set_area_text (view, TABLE_ROW_LOCATION, view->priv->location);
}

const gchar *
itip_view_get_location (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->location;
}

void
itip_view_set_status (ItipView *view,
                      const gchar *status)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->status)
		g_free (view->priv->status);

	view->priv->status = status ? g_strstrip (e_utf8_ensure_valid (status)) : NULL;

	set_area_text (view, TABLE_ROW_STATUS, view->priv->status);
}

const gchar *
itip_view_get_status (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->status;
}

void
itip_view_set_comment (ItipView *view,
                       const gchar *comment)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->comment)
		g_free (view->priv->comment);

	view->priv->comment = comment ? g_strstrip (e_utf8_ensure_valid (comment)) : NULL;

	set_area_text (view, TABLE_ROW_COMMENT, view->priv->comment);
}

const gchar *
itip_view_get_comment (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->comment;
}

static gchar *
itip_plain_text_to_html (const gchar *plain)
{
	return camel_text_to_html (
		plain,
		CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES,
		0);
}

static void
itip_view_extract_attendee_info (ItipView *view)
{
	icalproperty *prop;
	icalcomponent *ical_comp;
	gint num_attendees;
	const gchar *top_comment;
	GString *new_comment = NULL;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->comp)
		return;

	ical_comp = e_cal_component_get_icalcomponent (view->priv->comp);
	if (!ical_comp)
		return;

	num_attendees = icalcomponent_count_properties (ical_comp, ICAL_ATTENDEE_PROPERTY);
	if (num_attendees <= 0)
		return;

	top_comment = icalcomponent_get_comment (ical_comp);

	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY)) {
		gchar *guests_str = NULL;
		guint32 num_guests = 0;
		const gchar *value;

		value = cal_comp_util_find_parameter_xvalue (prop, "X-NUM-GUESTS");
		if (value && *value)
			num_guests = atoi (value);

		value = cal_comp_util_find_parameter_xvalue (prop, "X-RESPONSE-COMMENT");

		if (value && *value && num_attendees == 1 &&
		    g_strcmp0 (value, top_comment) == 0)
			value = NULL;

		if (num_guests)
			guests_str = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "with one guest", "with %d guests", num_guests), num_guests);

		if (num_attendees == 1) {
			if (!value)
				value = top_comment;

			if (value && *value) {
				gchar *html;

				if (num_guests) {
					gchar *plain;

					plain = g_strconcat (guests_str, "; ", value, NULL);
					html = itip_plain_text_to_html (plain);
					g_free (plain);
				} else {
					html = itip_plain_text_to_html (value);
				}

				itip_view_set_comment (view, html);

				g_free (html);
			} else if (guests_str) {
				gchar *html;

				html = itip_plain_text_to_html (guests_str);
				itip_view_set_comment (view, html);
				g_free (html);
			}
		} else if (guests_str || (value && *value)) {
			const gchar *email = icalproperty_get_attendee (prop);
			const gchar *cn = NULL;
			icalparameter *cnparam;

			cnparam = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
			if (cnparam) {
				cn = icalparameter_get_cn (cnparam);
				if (!cn || !*cn)
					cn = NULL;
			}

			email = itip_strip_mailto (email);

			if ((email && *email) || (cn && *cn)) {
				if (!new_comment)
					new_comment = g_string_new ("");
				else
					g_string_append_c (new_comment, '\n');

				if (cn && *cn) {
					g_string_append (new_comment, cn);

					if (g_strcmp0 (email, cn) == 0)
						email = NULL;
				}

				if (email && *email) {
					if (cn && *cn)
						g_string_append_printf (new_comment, " <%s>", email);
					else
						g_string_append (new_comment, email);
				}

				g_string_append (new_comment, ": ");

				if (guests_str) {
					g_string_append (new_comment, guests_str);

					if (value && *value)
						g_string_append (new_comment, "; ");
				}

				if (value && *value)
					g_string_append (new_comment, value);
			}
		}

		g_free (guests_str);
	}

	if (new_comment) {
		gchar *html;

		html = itip_plain_text_to_html (new_comment->str);
		itip_view_set_comment (view, html);
		g_free (html);

		g_string_free (new_comment, TRUE);
	}
}

void
itip_view_set_description (ItipView *view,
                           const gchar *description)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->description)
		g_free (view->priv->description);

	view->priv->description = description ? g_strstrip (e_utf8_ensure_valid (description)) : NULL;

	hide_element (view, TABLE_ROW_DESCRIPTION, (view->priv->description == NULL));
	set_inner_html (
		view,
		TABLE_ROW_DESCRIPTION,
		view->priv->description ? view->priv->description : "");
}

const gchar *
itip_view_get_description (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->description;
}

void
itip_view_set_start (ItipView *view,
                     struct tm *start,
                     gboolean is_date)
{
	ItipViewPrivate *priv;

	g_return_if_fail (ITIP_IS_VIEW (view));

	priv = view->priv;

	if (priv->start_tm && !start) {
		g_free (priv->start_tm);
		priv->start_tm = NULL;
	} else if (start) {
		if (!priv->start_tm)
			priv->start_tm = g_new0 (struct tm, 1);

		*priv->start_tm = *start;
	}

	priv->start_tm_is_date = is_date && start;

	update_start_end_times (view);
}

const struct tm *
itip_view_get_start (ItipView *view,
                     gboolean *is_date)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	if (is_date)
		*is_date = view->priv->start_tm_is_date;

	return view->priv->start_tm;
}

void
itip_view_set_end (ItipView *view,
                   struct tm *end,
                   gboolean is_date)
{
	ItipViewPrivate *priv;

	g_return_if_fail (ITIP_IS_VIEW (view));

	priv = view->priv;

	if (priv->end_tm && !end) {
		g_free (priv->end_tm);
		priv->end_tm = NULL;
	} else if (end) {
		if (!priv->end_tm)
			priv->end_tm = g_new0 (struct tm, 1);

		*priv->end_tm = *end;
	}

	priv->end_tm_is_date = is_date && end;

	update_start_end_times (view);
}

const struct tm *
itip_view_get_end (ItipView *view,
                   gboolean *is_date)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	if (is_date)
		*is_date = view->priv->end_tm_is_date;

	return view->priv->end_tm;
}

guint
itip_view_add_upper_info_item (ItipView *view,
                               ItipViewInfoItemType type,
                               const gchar *message)
{
	ItipViewPrivate *priv;
	ItipViewInfoItem *item;

	g_return_val_if_fail (ITIP_IS_VIEW (view), 0);

	priv = view->priv;

	item = g_new0 (ItipViewInfoItem, 1);

	item->type = type;
	item->message = e_utf8_ensure_valid (message);
	item->id = priv->next_info_item_id++;

	priv->upper_info_items = g_slist_append (priv->upper_info_items, item);

	if (!view->priv->web_extension)
		return item->id;

	append_info_item_row (view, TABLE_UPPER_ITIP_INFO, item);

	return item->id;
}

guint
itip_view_add_upper_info_item_printf (ItipView *view,
                                      ItipViewInfoItemType type,
                                      const gchar *format,
                                      ...)
{
	va_list args;
	gchar *message;
	guint id;

	g_return_val_if_fail (ITIP_IS_VIEW (view), 0);

	va_start (args, format);
	message = g_strdup_vprintf (format, args);
	va_end (args);

	id = itip_view_add_upper_info_item (view, type, message);
	g_free (message);

	return id;
}

void
itip_view_remove_upper_info_item (ItipView *view,
                                  guint id)
{
	ItipViewPrivate *priv;
	GSList *l;

	g_return_if_fail (ITIP_IS_VIEW (view));

	priv = view->priv;

	for (l = priv->upper_info_items; l; l = l->next) {
		ItipViewInfoItem *item = l->data;

		if (item->id == id) {
			priv->upper_info_items = g_slist_remove (priv->upper_info_items, item);

			g_free (item->message);
			g_free (item);

			remove_info_item_row (view, TABLE_UPPER_ITIP_INFO, id);

			return;
		}
	}
}

void
itip_view_clear_upper_info_items (ItipView *view)
{
	ItipViewPrivate *priv;
	GSList *l;

	g_return_if_fail (ITIP_IS_VIEW (view));

	priv = view->priv;

	for (l = priv->upper_info_items; l; l = l->next) {
		ItipViewInfoItem *item = l->data;

		remove_info_item_row (view, TABLE_UPPER_ITIP_INFO, item->id);

		g_free (item->message);
		g_free (item);
	}

	g_slist_free (priv->upper_info_items);
	priv->upper_info_items = NULL;
}

guint
itip_view_add_lower_info_item (ItipView *view,
                               ItipViewInfoItemType type,
                               const gchar *message)
{
	ItipViewPrivate *priv;
	ItipViewInfoItem *item;

	g_return_val_if_fail (ITIP_IS_VIEW (view), 0);

	priv = view->priv;

	item = g_new0 (ItipViewInfoItem, 1);

	item->type = type;
	item->message = e_utf8_ensure_valid (message);
	item->id = priv->next_info_item_id++;

	priv->lower_info_items = g_slist_append (priv->lower_info_items, item);

	if (!view->priv->web_extension)
		return item->id;

	append_info_item_row (view, TABLE_LOWER_ITIP_INFO, item);

	return item->id;
}

guint
itip_view_add_lower_info_item_printf (ItipView *view,
                                      ItipViewInfoItemType type,
                                      const gchar *format,
                                      ...)
{
	va_list args;
	gchar *message;
	guint id;

	g_return_val_if_fail (ITIP_IS_VIEW (view), 0);

	va_start (args, format);
	message = g_strdup_vprintf (format, args);
	va_end (args);

	id = itip_view_add_lower_info_item (view, type, message);
	g_free (message);

	return id;
}

void
itip_view_remove_lower_info_item (ItipView *view,
                                  guint id)
{
	ItipViewPrivate *priv;
	GSList *l;

	g_return_if_fail (ITIP_IS_VIEW (view));

	priv = view->priv;

	for (l = priv->lower_info_items; l; l = l->next) {
		ItipViewInfoItem *item = l->data;

		if (item->id == id) {
			priv->lower_info_items = g_slist_remove (priv->lower_info_items, item);

			g_free (item->message);
			g_free (item);

			remove_info_item_row (view, TABLE_LOWER_ITIP_INFO, id);

			return;
		}
	}
}

void
itip_view_clear_lower_info_items (ItipView *view)
{
	ItipViewPrivate *priv;
	GSList *l;

	g_return_if_fail (ITIP_IS_VIEW (view));

	priv = view->priv;

	for (l = priv->lower_info_items; l; l = l->next) {
		ItipViewInfoItem *item = l->data;

		remove_info_item_row (view, TABLE_LOWER_ITIP_INFO, item->id);

		g_free (item->message);
		g_free (item);
	}

	g_slist_free (priv->lower_info_items);
	priv->lower_info_items = NULL;
}

void
itip_view_set_source (ItipView *view,
                      ESource *source)
{
	ESource *selected_source;

	g_return_if_fail (ITIP_IS_VIEW (view));

	d (printf ("Settings default source '%s'\n", e_source_get_display_name (source)));

	hide_element (view, TABLE_ROW_ESCB, (source == NULL));

	if (!source)
		return;

        /* <select> does not emit 'change' event when already selected
	 * <option> is re-selected, but we need to notify itip formatter,
	 * so that it would make all the buttons sensitive */
	selected_source = itip_view_ref_source (view);
	if (source == selected_source) {
		source_changed_cb (view);
		return;
	}

	if (selected_source != NULL)
		g_object_unref (selected_source);

	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"EnableSelect",
		g_variant_new ("(tssb)", view->priv->page_id, view->priv->part_id, SELECT_ESOURCE, TRUE),
		NULL);

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"SelectSetSelected",
		g_variant_new ("(tsss)", view->priv->page_id, view->priv->part_id, SELECT_ESOURCE, e_source_get_uid (source)),
		NULL);

	source_changed_cb (view);
}

ESource *
itip_view_ref_source (ItipView *view)
{
	ESource *source = NULL;
	gboolean disable = FALSE, enabled = FALSE;
	GVariant *result;

	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	if (!view->priv->web_extension)
		return NULL;

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
			view->priv->web_extension,
			"SelectIsEnabled",
			g_variant_new ("(tss)", view->priv->page_id, view->priv->part_id, SELECT_ESOURCE),
			NULL);

	if (result) {
		g_variant_get (result, "(b)", &enabled);
		g_variant_unref (result);
	}

	if (!enabled) {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			view->priv->web_extension,
			"EnableSelect",
			g_variant_new ("(tssb)", view->priv->page_id, view->priv->part_id, SELECT_ESOURCE, TRUE),
			NULL);

		disable = TRUE;
	}

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		view->priv->web_extension,
		"SelectGetValue",
		g_variant_new ("(tss)", view->priv->page_id, view->priv->part_id, SELECT_ESOURCE),
		NULL);

	if (result) {
		const gchar *uid;

		g_variant_get (result, "(&s)", &uid);
		source = e_source_registry_ref_source (view->priv->registry, uid);
		g_variant_unref (result);
	}

	if (disable) {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			view->priv->web_extension,
			"EnableSelect",
			g_variant_new ("(tssb)", view->priv->page_id, view->priv->part_id, SELECT_ESOURCE, FALSE),
			NULL);
	}

	return source;
}

void
itip_view_set_rsvp (ItipView *view,
                    gboolean rsvp)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->web_extension)
		return;

	input_set_checked (view, CHECKBOX_RSVP, rsvp);

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"EnableTextArea",
		g_variant_new ("(tssb)", view->priv->page_id, view->priv->part_id, TEXTAREA_RSVP_COMMENT, !rsvp),
		NULL);
}

gboolean
itip_view_get_rsvp (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return input_is_checked (view, CHECKBOX_RSVP);
}

void
itip_view_set_show_rsvp_check (ItipView *view,
                               gboolean show)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	show_checkbox (view, CHECKBOX_RSVP, show, FALSE);
	hide_element (view, TABLE_ROW_RSVP_COMMENT, !show);
}

gboolean
itip_view_get_show_rsvp_check (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return !element_is_hidden (view, CHECKBOX_RSVP);
}

void
itip_view_set_update (ItipView *view,
                      gboolean update)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	input_set_checked (view, CHECKBOX_UPDATE, update);
}

gboolean
itip_view_get_update (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return input_is_checked (view, CHECKBOX_UPDATE);
}

void
itip_view_set_show_update_check (ItipView *view,
                                 gboolean show)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	show_checkbox (view, CHECKBOX_UPDATE, show, FALSE);
}

gboolean
itip_view_get_show_update_check (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return !element_is_hidden (view, CHECKBOX_UPDATE);
}

void
itip_view_set_rsvp_comment (ItipView *view,
                            const gchar *comment)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->web_extension)
		return;

	if (comment) {
		e_util_invoke_g_dbus_proxy_call_with_error_check (
			view->priv->web_extension,
			"TextAreaSetValue",
			g_variant_new ("(tsss)", view->priv->page_id, view->priv->part_id, TEXTAREA_RSVP_COMMENT, comment),
			NULL);
	}
}

gchar *
itip_view_get_rsvp_comment (ItipView *view)
{
	GVariant *result;

	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	if (!view->priv->web_extension)
		return NULL;

	if (element_is_hidden (view, TEXTAREA_RSVP_COMMENT))
		return NULL;

	result = e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check (
		view->priv->web_extension,
		"TextAreaGetValue",
		g_variant_new ("(tss)", view->priv->page_id, view->priv->part_id, TEXTAREA_RSVP_COMMENT),
		NULL);

	if (result) {
		gchar *value;

		g_variant_get (result, "(s)", &value);
		g_variant_unref (result);
		return value;
	}

	return NULL;
}

void
itip_view_set_needs_decline (ItipView *view,
                             gboolean needs_decline)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	view->priv->needs_decline = needs_decline;
}

void
itip_view_set_buttons_sensitive (ItipView *view,
                                 gboolean sensitive)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	d (printf ("Settings buttons %s\n", sensitive ? "sensitive" : "insensitive"));

	view->priv->buttons_sensitive = sensitive;

	if (!view->priv->web_extension)
		return;

	e_util_invoke_g_dbus_proxy_call_with_error_check (
		view->priv->web_extension,
		"SetButtonsSensitive",
		g_variant_new ("(tsb)", view->priv->page_id, view->priv->part_id, sensitive),
		NULL);
}

gboolean
itip_view_get_buttons_sensitive (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return view->priv->buttons_sensitive;
}

gboolean
itip_view_get_recur_check_state (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return input_is_checked (view, CHECKBOX_RECUR);
}

void
itip_view_set_show_recur_check (ItipView *view,
                                gboolean show)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	show_checkbox (view, CHECKBOX_RECUR, show, TRUE);
}

void
itip_view_set_show_free_time_check (ItipView *view,
                                    gboolean show)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	show_checkbox (view, CHECKBOX_FREE_TIME, show, TRUE);
}

gboolean
itip_view_get_free_time_check_state (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return input_is_checked (view, CHECKBOX_FREE_TIME);
}

void
itip_view_set_show_keep_alarm_check (ItipView *view,
                                     gboolean show)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	show_checkbox (view, CHECKBOX_KEEP_ALARM, show, TRUE);
}

gboolean
itip_view_get_keep_alarm_check_state (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return input_is_checked (view, CHECKBOX_KEEP_ALARM);
}

void
itip_view_set_show_inherit_alarm_check (ItipView *view,
                                        gboolean show)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	show_checkbox (view, CHECKBOX_INHERIT_ALARM, show, TRUE);
}

gboolean
itip_view_get_inherit_alarm_check_state (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return input_is_checked (view, CHECKBOX_INHERIT_ALARM);
}

void
itip_view_set_error (ItipView *view,
                     const gchar *error_html,
                     gboolean show_save_btn)
{
	GString *str;

	g_return_if_fail (ITIP_IS_VIEW (view));
	g_return_if_fail (error_html);

	str = g_string_new (error_html);

	if (show_save_btn) {
		g_string_append (
			str,
			"<table border=\"0\" width=\"100%\">"
			"<tr width=\"100%\" id=\"" TABLE_ROW_BUTTONS "\">");

		buttons_table_write_button (
			str, view->priv->itip_part_ptr, BUTTON_SAVE, _("Sa_ve"),
			"document-save", ITIP_VIEW_RESPONSE_SAVE);

		g_string_append (str, "</tr></table>");
	}

	view->priv->error = str->str;
	g_string_free (str, FALSE);

	if (!view->priv->web_extension)
		return;

	hide_element (view, DIV_ITIP_CONTENT, TRUE);
	hide_element (view, DIV_ITIP_ERROR, FALSE);
	set_inner_html (view, DIV_ITIP_ERROR, view->priv->error);

	if (show_save_btn) {
		show_button (view, BUTTON_SAVE);
		enable_button (view, BUTTON_SAVE, TRUE);

		itip_view_register_clicked_listener (view);
	}
}

/******************************************************************************/

typedef struct {
        ItipView *view;
	GCancellable *itip_cancellable;
	GCancellable *cancellable;
	gulong cancelled_id;
	gboolean keep_alarm_check;
	GHashTable *conflicts;

	gchar *uid;
	gchar *rid;

	gchar *sexp;

	gint count;
} FormatItipFindData;

static gboolean check_is_instance (icalcomponent *icalcomp);

static icalproperty *
find_attendee (icalcomponent *ical_comp,
               const gchar *address)
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
find_attendee_if_sentby (icalcomponent *ical_comp,
                         const gchar *address)
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
find_to_address (ItipView *view,
                 icalcomponent *ical_comp,
                 icalparameter_partstat *status)
{
	ESourceRegistry *registry;
	ESourceMailIdentity *extension;
	GList *list, *link;
	const gchar *extension_name;

	registry = view->priv->registry;
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;

	if (view->priv->to_address != NULL)
		return;

	if (view->priv->message != NULL && view->priv->folder != NULL) {
		ESource *source;

		source = em_utils_guess_mail_identity (
			registry, view->priv->message,
			view->priv->folder, view->priv->message_uid);

		if (source != NULL) {
			extension = e_source_get_extension (source, extension_name);

			view->priv->to_address = e_source_mail_identity_dup_address (extension);

			g_object_unref (source);
		}
	}

	if (view->priv->to_address != NULL)
		return;

	/* Look through the list of attendees to find the user's address */
	list = e_source_registry_list_enabled (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		icalproperty *prop = NULL;
		icalparameter *param;
		gchar *address;
		gchar *text;

		extension = e_source_get_extension (source, extension_name);
		address = e_source_mail_identity_dup_address (extension);

		prop = find_attendee (ical_comp, address);
		if (!prop) {
			GHashTable *aliases;

			aliases = e_source_mail_identity_get_aliases_as_hash_table (extension);
			if (aliases) {
				GHashTableIter iter;
				gpointer key = NULL;

				g_hash_table_iter_init (&iter, aliases);
				while (g_hash_table_iter_next (&iter, &key, NULL)) {
					const gchar *alias_address = key;

					if (alias_address && *alias_address) {
						prop = find_attendee (ical_comp, alias_address);
						if (prop) {
							g_free (address);
							address = g_strdup (alias_address);
							break;
						}
					}
				}

				g_hash_table_destroy (aliases);
			}
		}

		if (!prop) {
			g_free (address);
			continue;
		}

		param = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
		if (param != NULL)
			view->priv->to_name = g_strdup (icalparameter_get_cn (param));

		text = icalproperty_get_value_as_string_r (prop);

		view->priv->to_address = g_strdup (itip_strip_mailto (text));
		g_free (text);
		g_strstrip (view->priv->to_address);

		view->priv->my_address = address;

		param = icalproperty_get_first_parameter (prop, ICAL_RSVP_PARAMETER);
		if (param != NULL &&
		    icalparameter_get_rsvp (param) == ICAL_RSVP_FALSE)
			view->priv->no_reply_wanted = TRUE;

		if (status) {
			param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
			*status = param ? icalparameter_get_partstat (param) : ICAL_PARTSTAT_NEEDSACTION;
		}

		break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (view->priv->to_address != NULL)
		return;

	/* If the user's address was not found in the attendee's list,
	 * then the user might be responding on behalf of his/her delegator.
	 * In this case, we would want to go through the SENT-BY fields of
	 * the attendees to find the user's address.
 	 *
 *
	 * Note: This functionality could have been (easily) implemented
	 * in the previous loop, but it would hurt the performance for all
	 * providers in general. Hence, we choose to iterate through the
	 * accounts list again.
 	 */

	list = e_source_registry_list_enabled (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		icalproperty *prop = NULL;
		icalparameter *param;
		gchar *address;
		gchar *text;

		extension = e_source_get_extension (source, extension_name);
		address = e_source_mail_identity_dup_address (extension);

		prop = find_attendee_if_sentby (ical_comp, address);
		if (!prop) {
			GHashTable *aliases;

			aliases = e_source_mail_identity_get_aliases_as_hash_table (extension);
			if (aliases) {
				GHashTableIter iter;
				gpointer key = NULL;

				g_hash_table_iter_init (&iter, aliases);
				while (g_hash_table_iter_next (&iter, &key, NULL)) {
					const gchar *alias_address = key;

					if (alias_address && *alias_address) {
						prop = find_attendee_if_sentby (ical_comp, alias_address);
						if (prop) {
							g_free (address);
							address = g_strdup (alias_address);
							break;
						}
					}
				}

				g_hash_table_destroy (aliases);
			}
		}

		if (!prop) {
			g_free (address);
			continue;
		}

		param = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
		if (param != NULL)
			view->priv->to_name = g_strdup (icalparameter_get_cn (param));

		text = icalproperty_get_value_as_string_r (prop);

		view->priv->to_address = g_strdup (itip_strip_mailto (text));
		g_free (text);
		g_strstrip (view->priv->to_address);

		view->priv->my_address = address;

		param = icalproperty_get_first_parameter (prop, ICAL_RSVP_PARAMETER);
		if (param != NULL &&
		    ICAL_RSVP_FALSE == icalparameter_get_rsvp (param))
			view->priv->no_reply_wanted = TRUE;

		if (status) {
			param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
			*status = param ? icalparameter_get_partstat (param) : ICAL_PARTSTAT_NEEDSACTION;
		}

		break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
find_from_address (ItipView *view,
                   icalcomponent *ical_comp)
{
	ESourceRegistry *registry;
	GList *list, *link;
	icalproperty *prop;
	gchar *organizer;
	icalparameter *param;
	const gchar *extension_name;
	const gchar *organizer_sentby;
	gchar *organizer_clean = NULL;
	gchar *organizer_sentby_clean = NULL;

	registry = view->priv->registry;

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

	view->priv->from_address = g_strdup (organizer_clean);

	param = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
	if (param)
		view->priv->from_name = g_strdup (icalparameter_get_cn (param));

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	list = e_source_registry_list_enabled (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceMailIdentity *extension;
		GHashTable *aliases;
		const gchar *address;

		extension = e_source_get_extension (source, extension_name);
		address = e_source_mail_identity_get_address (extension);

		if (address) {
			if ((organizer_clean && !g_ascii_strcasecmp (organizer_clean, address))
			    || (organizer_sentby_clean && !g_ascii_strcasecmp (organizer_sentby_clean, address))) {
				view->priv->my_address = g_strdup (address);

				break;
			}
		}

		aliases = e_source_mail_identity_get_aliases_as_hash_table (extension);
		if (aliases) {
			GHashTableIter iter;
			gpointer key = NULL;
			gboolean found = FALSE;

			g_hash_table_iter_init (&iter, aliases);
			while (g_hash_table_iter_next (&iter, &key, NULL)) {
				const gchar *alias_address = key;

				if (alias_address && *alias_address) {
					if ((organizer_clean && !g_ascii_strcasecmp (organizer_clean, alias_address))
					    || (organizer_sentby_clean && !g_ascii_strcasecmp (organizer_sentby_clean, alias_address))) {
						view->priv->my_address = g_strdup (alias_address);
						found = TRUE;
						break;
					}
				}
			}

			g_hash_table_destroy (aliases);

			if (found)
				break;
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	g_free (organizer_sentby_clean);
	g_free (organizer_clean);
}

static ECalComponent *
get_real_item (ItipView *view)
{
	ECalComponent *comp = NULL;
	ESource *source;

	source = e_client_get_source (E_CLIENT (view->priv->current_client));
	if (source)
		comp = g_hash_table_lookup (view->priv->real_comps, e_source_get_uid (source));

	if (!comp) {
		return NULL;
	}

	return e_cal_component_clone (comp);
}

static void
adjust_item (ItipView *view,
             ECalComponent *comp)
{
	ECalComponent *real_comp;

	real_comp = get_real_item (view);
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

static gboolean
same_attendee_status (ItipView *view,
                      ECalComponent *received_comp)
{
	ECalComponent *saved_comp;
	GSList *received_attendees = NULL, *saved_attendees = NULL, *riter, *siter;
	gboolean same = FALSE;

	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	saved_comp = get_real_item (view);
	if (!saved_comp)
		return FALSE;

	e_cal_component_get_attendee_list (received_comp, &received_attendees);
	e_cal_component_get_attendee_list (saved_comp, &saved_attendees);

	same = received_attendees && saved_attendees;

	for (riter = received_attendees; same && riter; riter = g_slist_next (riter)) {
		const ECalComponentAttendee *rattendee = riter->data;

		if (!rattendee) {
			same = FALSE;
			continue;
		}

		/* no need to create a hash table for quicker searches, there might
		 * be one attendee in the received component only */
		for (siter = saved_attendees; siter; siter = g_slist_next (siter)) {
			const ECalComponentAttendee *sattendee = siter->data;

			if (!sattendee)
				continue;

			if (rattendee->value && sattendee->value &&
			    g_ascii_strcasecmp (rattendee->value, sattendee->value) == 0) {
				same = rattendee->status == sattendee->status;
				break;
			}
		}

		/* received attendee was not found in the saved attendees */
		if (!siter)
			same = FALSE;
	}

	e_cal_component_free_attendee_list (received_attendees);
	e_cal_component_free_attendee_list (saved_attendees);
	g_object_unref (saved_comp);

	return same;
}

static void
set_buttons_sensitive (ItipView *view)
{
	gboolean enabled = view->priv->current_client != NULL;

	if (enabled && view->priv->current_client)
		enabled = !e_client_is_readonly (E_CLIENT (view->priv->current_client));

	itip_view_set_buttons_sensitive (view, enabled);

	if (enabled && itip_view_get_mode (view) == ITIP_VIEW_MODE_REPLY &&
	    view->priv->comp && same_attendee_status (view, view->priv->comp)) {
		itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Attendee status updated"));

		enable_button (view, BUTTON_UPDATE_ATTENDEE_STATUS, FALSE);
	}
}

static void
add_failed_to_load_msg (ItipView *view,
                        const GError *error)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (error != NULL);

	itip_view_add_lower_info_item (
		view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING, error->message);
}

static void
itip_view_cal_opened_cb (GObject *source_object,
                         GAsyncResult *result,
                         gpointer user_data)
{
	ItipView *view;
	EClient *client;
	GError *error = NULL;

	view = ITIP_VIEW (user_data);

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		goto exit;

	} else if (error != NULL) {
		add_failed_to_load_msg (view, error);
		g_error_free (error);
		goto exit;
	}

	if (e_cal_client_check_recurrences_no_master (E_CAL_CLIENT (client))) {
		icalcomponent *icalcomp;
		gboolean show_recur_check;

		icalcomp = e_cal_component_get_icalcomponent (view->priv->comp);

		show_recur_check = check_is_instance (icalcomp);
		itip_view_set_show_recur_check (view, show_recur_check);
	}

	if (view->priv->type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS) {
		gboolean needs_decline;

		needs_decline = e_client_check_capability (
			client,
			CAL_STATIC_CAPABILITY_HAS_UNACCEPTED_MEETING);
		itip_view_set_needs_decline (view, needs_decline);
		itip_view_set_mode (view, ITIP_VIEW_MODE_PUBLISH);
	}

	view->priv->current_client = g_object_ref (client);

	set_buttons_sensitive (view);

exit:
	g_clear_object (&client);
	g_clear_object (&view);
}

static void
start_calendar_server (ItipView *view,
                       ESource *source,
                       ECalClientSourceType type,
                       GAsyncReadyCallback func,
                       gpointer data)
{
	EClientCache *client_cache;
	const gchar *extension_name = NULL;

	g_return_if_fail (source != NULL);

	switch (type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			break;
		default:
			g_return_if_reached ();
	}

	client_cache = itip_view_get_client_cache (view);

	e_client_cache_get_client (
		client_cache, source, extension_name, 30,
		view->priv->cancellable, func, data);
}

static void
start_calendar_server_by_uid (ItipView *view,
                              const gchar *uid,
                              ECalClientSourceType type)
{
	ESource *source;

	itip_view_set_buttons_sensitive (view, FALSE);

	source = e_source_registry_ref_source (view->priv->registry, uid);

	if (source != NULL) {
		start_calendar_server (
			view, source, type,
			itip_view_cal_opened_cb,
			g_object_ref (view));
		g_object_unref (source);
	}
}

static void
source_selected_cb (ItipView *view,
                    ESource *source,
                    gpointer user_data)
{
	g_return_if_fail (ITIP_IS_VIEW (view));
	g_return_if_fail (E_IS_SOURCE (source));

	itip_view_set_buttons_sensitive (view, FALSE);

	start_calendar_server (
		view, source, view->priv->type,
		itip_view_cal_opened_cb,
		g_object_ref (view));
}

static gboolean
itip_comp_older_than_stored (ItipView *view,
			     ECalComponent *real_comp)
{
	gboolean is_older = FALSE;
	gint *psequence = NULL;
	ECalComponentId *mail_id, *real_id;

	if (!real_comp || !view->priv->comp ||
	    e_cal_component_get_vtype (view->priv->comp) != E_CAL_COMPONENT_EVENT)
		return FALSE;

	e_cal_component_get_sequence (view->priv->comp, &psequence);
	if (!psequence)
		return FALSE;

	mail_id = e_cal_component_get_id (view->priv->comp);
	if (!mail_id) {
		e_cal_component_free_sequence (psequence);
		return FALSE;
	}

	real_id = e_cal_component_get_id (real_comp);
	if (real_id && g_strcmp0 (real_id->uid, mail_id->uid) == 0 && g_strcmp0 (real_id->rid, mail_id->rid) == 0) {
		gint *pint = NULL;

		e_cal_component_get_sequence (real_comp, &pint);
		if (pint) {
			is_older = *psequence < *pint;

			e_cal_component_free_sequence (pint);
		}
	}

	if (real_id)
		e_cal_component_free_id (real_id);

	e_cal_component_free_sequence (psequence);
	e_cal_component_free_id (mail_id);

	return is_older;
}

static void
find_cal_update_ui (FormatItipFindData *fd,
                    ECalClient *cal_client)
{
	ItipView *view;
	ESource *source;

	g_return_if_fail (fd != NULL);

	view = fd->view;

	/* UI part gone */
	if (g_cancellable_is_cancelled (fd->cancellable))
		return;

	source = cal_client ? e_client_get_source (E_CLIENT (cal_client)) : NULL;

	if (cal_client && g_hash_table_lookup (fd->conflicts, cal_client)) {
		GSList *icalcomps = g_hash_table_lookup (fd->conflicts, cal_client);
		guint ncomps;

		ncomps = g_slist_length (icalcomps);
		if (ncomps == 1 && icalcomps->data) {
			icalcomponent *icalcomp = icalcomps->data;

			itip_view_add_upper_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
				_("An appointment “%s” in the calendar “%s” conflicts with this meeting"),
				icalcomponent_get_summary (icalcomp),
				e_source_get_display_name (source));
		} else {
			itip_view_add_upper_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
				ngettext ("The calendar “%s” contains an appointment which conflicts with this meeting",
					  "The calendar “%s” contains %d appointments which conflict with this meeting",
					  ncomps),
				e_source_get_display_name (source),
				ncomps);
		}
	}

	/* search for a master object if the detached object doesn't exist in the calendar */
	if (view->priv->current_client && view->priv->current_client == cal_client) {
		const gchar *extension_name;
		gboolean rsvp_enabled = FALSE;

		itip_view_set_show_keep_alarm_check (view, fd->keep_alarm_check);

		view->priv->current_client = cal_client;

		/* Provide extra info, since its not in the component */
		/* FIXME Check sequence number of meeting? */
		/* FIXME Do we need to adjust elsewhere for the delegated calendar item? */
		/* FIXME Need to update the fields in the view now */
		if (view->priv->method == ICAL_METHOD_REPLY || view->priv->method == ICAL_METHOD_REFRESH)
			adjust_item (view, view->priv->comp);

		/* We clear everything because we don't really care
		 * about any other info/warnings now we found an
		 * existing versions */
		itip_view_clear_lower_info_items (view);
		view->priv->progress_info_id = 0;

		/* FIXME Check read only state of calendar? */
		itip_view_add_lower_info_item_printf (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Found the appointment in the calendar “%s”"), e_source_get_display_name (source));

		g_cancellable_cancel (fd->cancellable);

		if (view->priv->method == ICAL_METHOD_REQUEST &&
		    itip_comp_older_than_stored (view, g_hash_table_lookup (view->priv->real_comps, e_source_get_uid (source)))) {
			itip_view_set_mode (view, ITIP_VIEW_MODE_HIDE_ALL);
			itip_view_add_lower_info_item (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("This meeting invitation is obsolete. It had been updated."));
			itip_view_set_rsvp (view, FALSE);
			itip_view_set_show_free_time_check (view, FALSE);
			itip_view_set_show_inherit_alarm_check (view, FALSE);
			itip_view_set_show_keep_alarm_check (view, FALSE);
			itip_view_set_show_recur_check (view, FALSE);
			itip_view_set_show_rsvp_check (view, FALSE);
			itip_view_set_show_update_check (view, FALSE);
			set_buttons_sensitive (view);
		} else {
			/*
			 * Only allow replies if backend doesn't do that automatically.
			 * Only enable it for forwarded invitiations (PUBLISH) or direct
			 * invitiations (REQUEST), but not replies (REPLY).
			 * Replies only make sense for events with an organizer.
			 */
			if ((!view->priv->current_client || !e_cal_client_check_save_schedules (view->priv->current_client)) &&
			    (view->priv->method == ICAL_METHOD_PUBLISH || view->priv->method == ICAL_METHOD_REQUEST) &&
			    view->priv->has_organizer) {
				rsvp_enabled = TRUE;
			}
			itip_view_set_show_rsvp_check (view, rsvp_enabled);

			/* default is chosen in extract_itip_data() based on content of the VEVENT */
			itip_view_set_rsvp (view, !view->priv->no_reply_wanted);

			set_buttons_sensitive (view);

			switch (view->priv->type) {
				case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
					extension_name = E_SOURCE_EXTENSION_CALENDAR;
					break;
				case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
					extension_name = E_SOURCE_EXTENSION_TASK_LIST;
					break;
				case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
					extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
					break;
				default:
					g_return_if_reached ();
			}

			itip_view_set_extension_name (view, extension_name);

			g_signal_connect (
				view, "source_selected",
				G_CALLBACK (source_selected_cb), NULL);

			itip_view_set_source (view, source);
		}
	} else if (!view->priv->current_client)
		itip_view_set_show_keep_alarm_check (view, FALSE);

	if (view->priv->current_client && view->priv->current_client == cal_client &&
	    itip_view_get_mode (view) != ITIP_VIEW_MODE_HIDE_ALL) {
		if (e_cal_client_check_recurrences_no_master (view->priv->current_client)) {
			icalcomponent *icalcomp = e_cal_component_get_icalcomponent (view->priv->comp);

			if (check_is_instance (icalcomp))
				itip_view_set_show_recur_check (view, TRUE);
			else
				itip_view_set_show_recur_check (view, FALSE);
		}

		if (view->priv->type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS) {
			/* TODO The static capability should be made generic to convey that the calendar contains unaccepted items */
			if (e_client_check_capability (E_CLIENT (view->priv->current_client), CAL_STATIC_CAPABILITY_HAS_UNACCEPTED_MEETING))
				itip_view_set_needs_decline (view, TRUE);
			else
				itip_view_set_needs_decline (view, FALSE);

			itip_view_set_mode (view, ITIP_VIEW_MODE_PUBLISH);
		}
	}
}

static void
decrease_find_data (FormatItipFindData *fd)
{
	g_return_if_fail (fd != NULL);

	fd->count--;
	d (printf ("Decreasing itip formatter search count to %d\n", fd->count));

	if (fd->count == 0 && !g_cancellable_is_cancelled (fd->cancellable)) {
		gboolean rsvp_enabled = FALSE;
		ItipView *view = fd->view;

		itip_view_remove_lower_info_item (view, view->priv->progress_info_id);
		view->priv->progress_info_id = 0;

		/*
		 * Only allow replies if backend doesn't do that automatically.
		 * Only enable it for forwarded invitiations (PUBLISH) or direct
		 * invitiations (REQUEST), but not replies (REPLY).
		 * Replies only make sense for events with an organizer.
		 */
		if ((!view->priv->current_client || !e_cal_client_check_save_schedules (view->priv->current_client)) &&
		    (view->priv->method == ICAL_METHOD_PUBLISH || view->priv->method == ICAL_METHOD_REQUEST) &&
		    view->priv->has_organizer) {
			rsvp_enabled = TRUE;
		}
		itip_view_set_show_rsvp_check (view, rsvp_enabled);

		/* default is chosen in extract_itip_data() based on content of the VEVENT */
		itip_view_set_rsvp (view, !view->priv->no_reply_wanted);

		if ((view->priv->method == ICAL_METHOD_PUBLISH || view->priv->method == ICAL_METHOD_REQUEST)
		    && !view->priv->current_client) {
			/* Reuse already declared one or rename? */
			ESource *source = NULL;
			const gchar *extension_name;

			switch (view->priv->type) {
				case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
					extension_name = E_SOURCE_EXTENSION_CALENDAR;
					break;
				case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
					extension_name = E_SOURCE_EXTENSION_TASK_LIST;
					break;
				case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
					extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
					break;
				default:
					g_return_if_reached ();
			}

			source = e_source_registry_ref_default_for_extension_name (
				view->priv->registry, extension_name);

			itip_view_set_extension_name (view, extension_name);

			g_signal_connect (
				view, "source_selected",
				G_CALLBACK (source_selected_cb), NULL);

			if (source != NULL) {
				itip_view_set_source (view, source);
				g_object_unref (source);

 				/* FIXME Shouldn't the buttons be sensitized here? */
			} else {
				itip_view_add_lower_info_item (view, ITIP_VIEW_INFO_ITEM_TYPE_ERROR, _("Unable to find any calendars"));
				itip_view_set_buttons_sensitive (view, FALSE);
			}
		} else if (!view->priv->current_client) {
			switch (view->priv->type) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				itip_view_add_lower_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
					_("Unable to find this meeting in any calendar"));
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				itip_view_add_lower_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
					_("Unable to find this task in any task list"));
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				itip_view_add_lower_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
					_("Unable to find this memo in any memo list"));
				break;
			default:
				g_warn_if_reached ();
				break;
			}
		}
	}

	if (fd->count == 0) {
		g_hash_table_destroy (fd->conflicts);
		g_cancellable_disconnect (fd->itip_cancellable, fd->cancelled_id);
		g_object_unref (fd->cancellable);
		g_object_unref (fd->itip_cancellable);
		g_object_unref (fd->view);
		g_free (fd->uid);
		g_free (fd->rid);
		if (fd->sexp)
			g_free (fd->sexp);
		g_free (fd);
	}
}

static void
get_object_without_rid_ready_cb (GObject *source_object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	ECalClient *cal_client = E_CAL_CLIENT (source_object);
	FormatItipFindData *fd = user_data;
	icalcomponent *icalcomp = NULL;
	GError *error = NULL;

	e_cal_client_get_object_finish (cal_client, result, &icalcomp, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
	    g_cancellable_is_cancelled (fd->cancellable)) {
		g_clear_error (&error);
		find_cal_update_ui (fd, cal_client);
		decrease_find_data (fd);
		return;
	}

	g_clear_error (&error);

	if (icalcomp) {
		ECalComponent *comp;

		fd->view->priv->current_client = cal_client;
		fd->keep_alarm_check = (fd->view->priv->method == ICAL_METHOD_PUBLISH || fd->view->priv->method == ICAL_METHOD_REQUEST) &&
			(icalcomponent_get_first_component (icalcomp, ICAL_VALARM_COMPONENT) ||
			icalcomponent_get_first_component (icalcomp, ICAL_XAUDIOALARM_COMPONENT) ||
			icalcomponent_get_first_component (icalcomp, ICAL_XDISPLAYALARM_COMPONENT) ||
			icalcomponent_get_first_component (icalcomp, ICAL_XPROCEDUREALARM_COMPONENT) ||
			icalcomponent_get_first_component (icalcomp, ICAL_XEMAILALARM_COMPONENT));

		comp = e_cal_component_new_from_icalcomponent (icalcomp);
		if (comp) {
			ESource *source = e_client_get_source (E_CLIENT (cal_client));

			g_hash_table_insert (fd->view->priv->real_comps, g_strdup (e_source_get_uid (source)), comp);
		}

		find_cal_update_ui (fd, cal_client);
		decrease_find_data (fd);
		return;
	}

	find_cal_update_ui (fd, cal_client);
	decrease_find_data (fd);
}

static void
get_object_with_rid_ready_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	ECalClient *cal_client = E_CAL_CLIENT (source_object);
	FormatItipFindData *fd = user_data;
	icalcomponent *icalcomp = NULL;
	GError *error = NULL;

	e_cal_client_get_object_finish (cal_client, result, &icalcomp, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
	    g_cancellable_is_cancelled (fd->cancellable)) {
		g_clear_error (&error);
		find_cal_update_ui (fd, cal_client);
		decrease_find_data (fd);
		return;
	}

	g_clear_error (&error);

	if (icalcomp) {
		ECalComponent *comp;

		fd->view->priv->current_client = cal_client;
		fd->keep_alarm_check = (fd->view->priv->method == ICAL_METHOD_PUBLISH || fd->view->priv->method == ICAL_METHOD_REQUEST) &&
			(icalcomponent_get_first_component (icalcomp, ICAL_VALARM_COMPONENT) ||
			icalcomponent_get_first_component (icalcomp, ICAL_XAUDIOALARM_COMPONENT) ||
			icalcomponent_get_first_component (icalcomp, ICAL_XDISPLAYALARM_COMPONENT) ||
			icalcomponent_get_first_component (icalcomp, ICAL_XPROCEDUREALARM_COMPONENT) ||
			icalcomponent_get_first_component (icalcomp, ICAL_XEMAILALARM_COMPONENT));

		comp = e_cal_component_new_from_icalcomponent (icalcomp);
		if (comp) {
			ESource *source = e_client_get_source (E_CLIENT (cal_client));

			g_hash_table_insert (fd->view->priv->real_comps, g_strdup (e_source_get_uid (source)), comp);
		}

		find_cal_update_ui (fd, cal_client);
		decrease_find_data (fd);
		return;
	}

	if (fd->rid && *fd->rid) {
		e_cal_client_get_object (cal_client, fd->uid, NULL, fd->cancellable, get_object_without_rid_ready_cb, fd);
		return;
	}

	find_cal_update_ui (fd, cal_client);
	decrease_find_data (fd);
}

static void
get_object_list_ready_cb (GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	ECalClient *cal_client = E_CAL_CLIENT (source_object);
	FormatItipFindData *fd = user_data;
	GSList *objects = NULL;
	GError *error = NULL;

	e_cal_client_get_object_list_finish (
		cal_client, result, &objects, &error);

	if (g_cancellable_is_cancelled (fd->cancellable)) {
		g_clear_error (&error);
		decrease_find_data (fd);
		return;
	}

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		decrease_find_data (fd);
		return;

	} else if (error != NULL) {
		g_error_free (error);

	} else {
		g_hash_table_insert (fd->conflicts, cal_client, objects);
	}

	e_cal_client_get_object (
		cal_client, fd->uid, fd->rid, fd->cancellable,
		get_object_with_rid_ready_cb, fd);
}

static void
find_cal_opened_cb (GObject *source_object,
                    GAsyncResult *result,
                    gpointer user_data)
{
	FormatItipFindData *fd = user_data;
	ItipView *view = fd->view;
	EClient *client;
	ESource *source;
	ECalClient *cal_client;
	gboolean search_for_conflicts = FALSE;
	const gchar *extension_name;
	GError *error = NULL;

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		decrease_find_data (fd);
		g_error_free (error);
		return;
	}

	if (g_cancellable_is_cancelled (fd->cancellable)) {
		g_clear_error (&error);
		decrease_find_data (fd);
		return;
	}

	if (error != NULL) {
		/* FIXME Do we really want to warn here?  If we fail
		 * to find the item, this won't be cleared but the
		 * selector might be shown */
		add_failed_to_load_msg (view, error);
		decrease_find_data (fd);
		g_error_free (error);
		return;
	}

	cal_client = E_CAL_CLIENT (client);

	source = e_client_get_source (client);

	extension_name = E_SOURCE_EXTENSION_CONFLICT_SEARCH;
	if (e_source_has_extension (source, extension_name)) {
		ESourceConflictSearch *extension;

		extension = e_source_get_extension (source, extension_name);
		search_for_conflicts =
			(view->priv->type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) &&
			e_source_conflict_search_get_include_me (extension);
	}

	/* Do not process read-only calendars */
	if (e_client_is_readonly (E_CLIENT (cal_client))) {
		g_object_unref (cal_client);
		decrease_find_data (fd);
		return;
	}

 	/* Check for conflicts */
 	/* If the query fails, we'll just ignore it */
 	/* FIXME What happens for recurring conflicts? */
	if (search_for_conflicts) {
		e_cal_client_get_object_list (
			cal_client, fd->sexp,
			fd->cancellable,
			get_object_list_ready_cb, fd);
		return;
	}

	if (!view->priv->current_client) {
		e_cal_client_get_object (
			cal_client, fd->uid, fd->rid,
			fd->cancellable,
			get_object_with_rid_ready_cb, fd);
		return;
	}

	decrease_find_data (fd);
	g_clear_object (&cal_client);
}

static void
itip_cancellable_cancelled (GCancellable *itip_cancellable,
                            GCancellable *fd_cancellable)
{
	g_cancellable_cancel (fd_cancellable);
}

static void
find_server (ItipView *view,
             ECalComponent *comp)
{
	FormatItipFindData *fd = NULL;
	const gchar *uid;
	gchar *rid = NULL;
	CamelStore *parent_store;
	ESource *current_source = NULL;
	GList *list, *link;
	GList *conflict_list = NULL;
	const gchar *extension_name;
	const gchar *store_uid;

	g_return_if_fail (ITIP_IS_VIEW (view));
	g_return_if_fail (view->priv->folder != NULL);

	switch (view->priv->type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			break;
		default:
			g_return_if_reached ();
	}

	list = e_source_registry_list_enabled (
		view->priv->registry, extension_name);

	e_cal_component_get_uid (comp, &uid);
	rid = e_cal_component_get_recurid_as_string (comp);

	/* XXX Not sure what this was trying to do,
	 *     but it propbably doesn't work anymore.
	 *     Some comments would have been helpful. */
	parent_store = camel_folder_get_parent_store (view->priv->folder);

	store_uid = camel_service_get_uid (CAMEL_SERVICE (parent_store));

	itip_view_set_buttons_sensitive (view, FALSE);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		gboolean search_for_conflicts = FALSE;
		const gchar *source_uid;

		extension_name = E_SOURCE_EXTENSION_CONFLICT_SEARCH;
		if (e_source_has_extension (source, extension_name)) {
			ESourceConflictSearch *extension;

			extension =
				e_source_get_extension (source, extension_name);
			search_for_conflicts =
				e_source_conflict_search_get_include_me (extension);
		}

		if (search_for_conflicts)
			conflict_list = g_list_prepend (
				conflict_list, g_object_ref (source));

		if (current_source != NULL)
			continue;

		source_uid = e_source_get_uid (source);
		if (g_strcmp0 (source_uid, store_uid) == 0) {
			current_source = source;
			conflict_list = g_list_prepend (
				conflict_list, g_object_ref (source));

			continue;
		}
	}

	if (current_source) {
		link = conflict_list;

		view->priv->progress_info_id = itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS,
			_("Opening the calendar. Please wait..."));
	} else {
		link = list;
		view->priv->progress_info_id = itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS,
			_("Searching for an existing version of this appointment"));
	}

	for (; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);

		if (!fd) {
			gchar *start = NULL, *end = NULL;

			fd = g_new0 (FormatItipFindData, 1);
			fd->view = g_object_ref (view);
			fd->itip_cancellable = g_object_ref (view->priv->cancellable);
			fd->cancellable = g_cancellable_new ();
			fd->cancelled_id = g_cancellable_connect (
				fd->itip_cancellable,
				G_CALLBACK (itip_cancellable_cancelled), fd->cancellable, NULL);
			fd->conflicts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) e_cal_client_free_icalcomp_slist);
			fd->uid = g_strdup (uid);
			fd->rid = rid;
			/* avoid free this at the end */
			rid = NULL;

			if (view->priv->start_time && view->priv->end_time) {
				start = isodate_from_time_t (view->priv->start_time);
				end = isodate_from_time_t (view->priv->end_time);

				fd->sexp = g_strdup_printf (
					"(and (occur-in-time-range? "
					"(make-time \"%s\") "
					"(make-time \"%s\")) "
					"(not (uid? \"%s\")))",
					start, end,
					icalcomponent_get_uid (view->priv->ical_comp));
			}

			g_free (start);
			g_free (end);
		}
		fd->count++;
		d (printf ("Increasing itip formatter search count to %d\n", fd->count));

		start_calendar_server (
			view, source, view->priv->type,
			find_cal_opened_cb, fd);
	}

	g_list_free_full (conflict_list, (GDestroyNotify) g_object_unref);
	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	g_free (rid);
}

static gboolean
change_status (ESourceRegistry *registry,
               icalcomponent *ical_comp,
               const gchar *address,
               icalparameter_partstat status)
{
	icalproperty *prop;

	prop = find_attendee (ical_comp, address);
	if (prop) {
		icalparameter *param;

		icalproperty_remove_parameter_by_kind (prop, ICAL_PARTSTAT_PARAMETER);
		param = icalparameter_new_partstat (status);
		icalproperty_add_parameter (prop, param);
	} else {
		icalparameter *param;

		if (address && *address) {
			gchar *mailto_uri;

			mailto_uri = g_strconcat ("mailto:", itip_strip_mailto (address), NULL);
			prop = icalproperty_new_attendee (mailto_uri);
			icalcomponent_add_property (ical_comp, prop);
			g_free (mailto_uri);

			param = icalparameter_new_role (ICAL_ROLE_OPTPARTICIPANT);
			icalproperty_add_parameter (prop, param);

			param = icalparameter_new_partstat (status);
			icalproperty_add_parameter (prop, param);
		} else {
			gchar *default_name = NULL;
			gchar *default_address = NULL;
			gchar *mailto_uri;

			itip_get_default_name_and_address (
				registry, &default_name, &default_address);

			mailto_uri = g_strconcat ("mailto:", itip_strip_mailto (default_address), NULL);
			prop = icalproperty_new_attendee (mailto_uri);
			icalcomponent_add_property (ical_comp, prop);
			g_free (mailto_uri);

			if (default_name && *default_name && g_strcmp0 (default_name, default_address) != 0) {
				param = icalparameter_new_cn (default_name);
				icalproperty_add_parameter (prop, param);
			}

			param = icalparameter_new_role (ICAL_ROLE_REQPARTICIPANT);
			icalproperty_add_parameter (prop, param);

			param = icalparameter_new_partstat (status);
			icalproperty_add_parameter (prop, param);

			g_free (default_name);
			g_free (default_address);
		}
	}

	return TRUE;
}

static void
message_foreach_part (CamelMimePart *part,
                      GSList **part_list)
{
	CamelDataWrapper *containee;
	gint parts, i;
	gint go = TRUE;

	if (!part)
		return;

	*part_list = g_slist_append (*part_list, part);

	containee = camel_medium_get_content (CAMEL_MEDIUM (part));

	if (containee == NULL)
		return;

	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART (containee)) {
		parts = camel_multipart_get_number (CAMEL_MULTIPART (containee));
		for (i = 0; go && i < parts; i++) {
			/* Reuse already declared *parts? */
			CamelMimePart *subpart = camel_multipart_get_part (CAMEL_MULTIPART (containee), i);

			message_foreach_part (subpart, part_list);
		}
	} else if (CAMEL_IS_MIME_MESSAGE (containee)) {
		message_foreach_part ((CamelMimePart *) containee, part_list);
	}
}

static void
attachment_load_finished (EAttachment *attachment,
                          GAsyncResult *result,
                          gpointer user_data)
{
	struct {
		GFile *file;
		gboolean done;
	} *status = user_data;

	/* Should be no need to check for error here. */
	e_attachment_load_finish (attachment, result, NULL);

	status->done = TRUE;
}

static void
attachment_save_finished (EAttachment *attachment,
                          GAsyncResult *result,
                          gpointer user_data)
{
	GError *error = NULL;

	struct {
		GFile *file;
		gboolean done;
	} *status = user_data;

	status->file = e_attachment_save_finish (attachment, result, &error);
	status->done = TRUE;

	/* XXX Error handling needs improvement. */
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static gchar *
get_uri_for_part (CamelMimePart *mime_part)
{
	EAttachment *attachment;
	GFile *temp_directory;
	gchar *template;
	gchar *path;

	struct {
		GFile *file;
		gboolean done;
	} status;

	/* XXX Error handling leaves much to be desired. */

	template = g_strdup_printf (PACKAGE "-%s-XXXXXX", g_get_user_name ());
	path = e_mkdtemp (template);
	g_free (template);

	if (path == NULL)
		return NULL;

	temp_directory = g_file_new_for_path (path);
	g_free (path);

	attachment = e_attachment_new ();
	e_attachment_set_mime_part (attachment, mime_part);

	status.done = FALSE;

	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		attachment_load_finished, &status);

	/* Loading should be instantaneous since we already have
	 * the full content, but we still have to crank the main
	 * loop until the callback gets triggered. */
	/* coverity[loop_condition] */
	while (!status.done)
		gtk_main_iteration ();

	status.file = NULL;
	status.done = FALSE;

	e_attachment_save_async (
		attachment, temp_directory, (GAsyncReadyCallback)
		attachment_save_finished, &status);

	/* We can't return until we have results, so crank
	 * the main loop until the callback gets triggered. */
	/* coverity[loop_condition] */
	while (!status.done)
		gtk_main_iteration ();

	if (status.file != NULL) {
		path = g_file_get_path (status.file);
		g_object_unref (status.file);
	} else
		path = NULL;

	g_object_unref (attachment);
	g_object_unref (temp_directory);

	return path;
}

static void
update_item_progress_info (ItipView *view,
                           const gchar *message)
{
	if (view->priv->update_item_progress_info_id) {
		itip_view_remove_lower_info_item (view, view->priv->update_item_progress_info_id);
		view->priv->update_item_progress_info_id = 0;

		if (!message)
			itip_view_set_buttons_sensitive (view, TRUE);
	}

	if (view->priv->update_item_error_info_id) {
		itip_view_remove_lower_info_item (view, view->priv->update_item_error_info_id);
		view->priv->update_item_error_info_id = 0;
	}

	if (message) {
		itip_view_set_buttons_sensitive (view, FALSE);
		view->priv->update_item_progress_info_id =
			itip_view_add_lower_info_item (
				view,
				ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS,
				message);
	}
}

static gboolean
itip_view_get_delete_message (void)
{
	GSettings *settings;
	gboolean delete_message;

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.itip");
	delete_message = g_settings_get_boolean (settings, "delete-processed");
	g_clear_object (&settings);

	return delete_message;
}

static void
finish_message_delete_with_rsvp (ItipView *view,
                                 ECalClient *client)
{
	if (itip_view_get_delete_message () && view->priv->folder)
		camel_folder_delete_message (view->priv->folder, view->priv->message_uid);

	if (itip_view_get_rsvp (view)) {
		ECalComponent *comp = NULL;
		icalcomponent *ical_comp;
		icalproperty *prop;
		icalvalue *value;
		const gchar *attendee;
		gchar *comment;
		GSList *l, *list = NULL;
		gboolean found;

		comp = e_cal_component_clone (view->priv->comp);
		if (comp == NULL)
			return;

		if (view->priv->to_address == NULL)
			find_to_address (view, view->priv->ical_comp, NULL);
		g_return_if_fail (view->priv->to_address != NULL);

		ical_comp = e_cal_component_get_icalcomponent (comp);

		/* Remove all attendees except the one we are responding as */
		found = FALSE;
		for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
		     prop != NULL;
		     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY)) {
			gchar *text;

			value = icalproperty_get_value (prop);
			if (!value)
				continue;

			attendee = icalvalue_get_string (value);

			text = g_strdup (itip_strip_mailto (attendee));
			text = g_strstrip (text);

			/* We do this to ensure there is at most one
			 * attendee in the response */
			if (found || g_ascii_strcasecmp (view->priv->to_address, text))
				list = g_slist_prepend (list, prop);
			else if (!g_ascii_strcasecmp (view->priv->to_address, text))
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
		comment = itip_view_get_rsvp_comment (view);
		if (comment) {
			GSList comments;
			ECalComponentText text;

			text.value = comment;
			text.altrep = NULL;

			comments.data = &text;
			comments.next = NULL;

			e_cal_component_set_comment_list (comp, &comments);

			g_free (comment);
		}

		e_cal_component_rescan (comp);

		if (itip_send_comp_sync (
				view->priv->registry,
				E_CAL_COMPONENT_METHOD_REPLY,
				comp, view->priv->current_client,
				view->priv->top_level, NULL, NULL, TRUE, FALSE, NULL, NULL) &&
				view->priv->folder) {
			camel_folder_set_message_flags (
				view->priv->folder, view->priv->message_uid,
				CAMEL_MESSAGE_ANSWERED,
				CAMEL_MESSAGE_ANSWERED);
		}

		g_object_unref (comp);
	}

	update_item_progress_info (view, NULL);
}

static void
receive_objects_ready_cb (GObject *ecalclient,
                          GAsyncResult *result,
                          gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (ecalclient);
	ESource *source = e_client_get_source (E_CLIENT (client));
	ItipView *view = user_data;
	GError *error = NULL;

	e_cal_client_receive_objects_finish (client, result, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return;

	} else if (error != NULL) {
		update_item_progress_info (view, NULL);
		view->priv->update_item_error_info_id =
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Unable to send item to calendar “%s”.  %s"),
				e_source_get_display_name (source),
				error->message);
		g_error_free (error);
		return;
	}

	itip_view_set_extension_name (view, NULL);

	itip_view_clear_lower_info_items (view);

	switch (view->priv->update_item_response) {
	case ITIP_VIEW_RESPONSE_ACCEPT:
		itip_view_add_lower_info_item_printf (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Sent to calendar “%s” as accepted"), e_source_get_display_name (source));
		break;
	case ITIP_VIEW_RESPONSE_TENTATIVE:
		itip_view_add_lower_info_item_printf (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Sent to calendar “%s” as tentative"), e_source_get_display_name (source));
		break;
	case ITIP_VIEW_RESPONSE_DECLINE:
		/* FIXME some calendars just might not save it at all, is this accurate? */
		itip_view_add_lower_info_item_printf (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Sent to calendar “%s” as declined"), e_source_get_display_name (source));
		break;
	case ITIP_VIEW_RESPONSE_CANCEL:
		/* FIXME some calendars just might not save it at all, is this accurate? */
		itip_view_add_lower_info_item_printf (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Sent to calendar “%s” as cancelled"), e_source_get_display_name (source));
		break;
	default:
		g_warn_if_reached ();
		break;
	}

	finish_message_delete_with_rsvp (view, client);
}

static void
remove_alarms_in_component (icalcomponent *clone)
{
	icalcomponent *alarm_comp;
	icalcompiter alarm_iter;

	alarm_iter = icalcomponent_begin_component (clone, ICAL_VALARM_COMPONENT);
	while ((alarm_comp = icalcompiter_deref (&alarm_iter)) != NULL) {
		icalcompiter_next (&alarm_iter);

		icalcomponent_remove_component (clone, alarm_comp);
		icalcomponent_free (alarm_comp);
	}
}

static void
update_item (ItipView *view,
             ItipViewResponse response)
{
	struct icaltimetype stamp;
	icalproperty *prop;
	icalcomponent *toplevel_clone, *clone;
	gboolean remove_alarms;
	ECalComponent *clone_comp;
	gchar *str;

	update_item_progress_info (view, _("Saving changes to the calendar. Please wait..."));

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
	icalcomponent_add_property (view->priv->ical_comp, prop);

	toplevel_clone = icalcomponent_new_clone (view->priv->top_level);
	clone = icalcomponent_new_clone (view->priv->ical_comp);
	icalcomponent_add_component (toplevel_clone, clone);
	icalcomponent_set_method (toplevel_clone, view->priv->method);

	remove_alarms = !itip_view_get_inherit_alarm_check_state (view);

	if (remove_alarms)
		remove_alarms_in_component (clone);

	if (view->priv->with_detached_instances) {
		icalcomponent *icomp;
		icalcomponent_kind use_kind = icalcomponent_isa (view->priv->ical_comp);

		for (icomp = icalcomponent_get_first_component (view->priv->main_comp, use_kind);
		     icomp;
		     icomp = icalcomponent_get_next_component (view->priv->main_comp, use_kind)) {
			if (icomp != view->priv->ical_comp) {
				icalcomponent *di_clone = icalcomponent_new_clone (icomp);

				if (remove_alarms)
					remove_alarms_in_component (di_clone);

				icalcomponent_add_component (toplevel_clone, di_clone);
			}
		}
	}

	clone_comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (clone_comp, clone)) {
		update_item_progress_info (view, NULL);
		view->priv->update_item_error_info_id =
			itip_view_add_lower_info_item (
				view, ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
				_("Unable to parse item"));
		goto cleanup;
	}

	if (itip_view_get_keep_alarm_check_state (view)) {
		ECalComponent *real_comp;
		GList *alarms, *l;
		ECalComponentAlarm *alarm;

		real_comp = get_real_item (view);
		if (real_comp != NULL) {
			alarms = e_cal_component_get_alarm_uids (real_comp);

			for (l = alarms; l; l = l->next) {
				alarm = e_cal_component_get_alarm (
					real_comp, (const gchar *) l->data);

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
		CamelMimeMessage *msg = view->priv->message;

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
					if (part == (CamelMimePart *) msg || part == view->priv->itip_mime_part)
						continue;

					new_uri = get_uri_for_part (part);
					if (new_uri != NULL)
						new_attachments = g_slist_append (new_attachments, new_uri);
				}

				g_slist_free (parts);

			} else if (!g_ascii_strncasecmp (uri, "cid:", 4)) {
				part = camel_mime_message_get_part_by_content_id (msg, uri + 4);
				if (part) {
					new_uri = get_uri_for_part (part);
					if (new_uri != NULL)
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

	view->priv->update_item_response = response;

	e_cal_client_receive_objects (
		view->priv->current_client,
		toplevel_clone,
		view->priv->cancellable,
		receive_objects_ready_cb,
		view);

 cleanup:
	g_object_unref (clone_comp);
	icalcomponent_free (toplevel_clone);
}

/* TODO These operations should be available in e-cal-component.c */
static void
set_attendee (ECalComponent *comp,
              const gchar *address)
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
send_comp_to_attendee (ESourceRegistry *registry,
                       ECalComponentItipMethod method,
                       ECalComponent *comp,
                       const gchar *user,
                       ECalClient *client,
                       const gchar *comment)
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
	status = itip_send_comp_sync (
		registry, method, send_comp,
		client, NULL, NULL, NULL, TRUE, FALSE, NULL, NULL);

	g_object_unref (send_comp);

	return status;
}

static void
remove_delegate (ItipView *view,
                 const gchar *delegate,
                 const gchar *delegator,
                 ECalComponent *comp)
{
	gboolean status;
	gchar *comment;

	comment = g_strdup_printf (
		_("Organizer has removed the delegate %s "),
		itip_strip_mailto (delegate));

	/* send cancellation notice to delegate */
	status = send_comp_to_attendee (
		view->priv->registry,
		E_CAL_COMPONENT_METHOD_CANCEL, view->priv->comp,
		delegate, view->priv->current_client, comment);
	if (status != 0) {
		send_comp_to_attendee (
			view->priv->registry,
			E_CAL_COMPONENT_METHOD_REQUEST, view->priv->comp,
			delegator, view->priv->current_client, comment);
	}
	if (status != 0) {
		itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Sent a cancelation notice to the delegate"));
	} else {
		itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Could not send the cancelation notice to the delegate"));
	}

	g_free (comment);

}

static void
update_x (ECalComponent *view_comp,
          ECalComponent *comp)
{
	icalcomponent *itip_icalcomp = e_cal_component_get_icalcomponent (view_comp);
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
modify_object_cb (GObject *ecalclient,
                  GAsyncResult *result,
                  gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (ecalclient);
	ItipView *view = user_data;
	GError *error = NULL;

	e_cal_client_modify_object_finish (client, result, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);

	} else if (error != NULL) {
		update_item_progress_info (view, NULL);
		view->priv->update_item_error_info_id =
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
				_("Unable to update attendee. %s"),
				error->message);
		g_error_free (error);

	} else {
		update_item_progress_info (view, NULL);
		itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Attendee status updated"));

		enable_button (view, BUTTON_UPDATE_ATTENDEE_STATUS, FALSE);

		if (itip_view_get_delete_message () && view->priv->folder)
			camel_folder_delete_message (view->priv->folder, view->priv->message_uid);
	}
}

static void
update_attendee_status_icalcomp (ItipView *view,
                                 icalcomponent *icalcomp)
{
	ECalComponent *comp;
	const gchar *uid = NULL;
	gchar *rid;
	GSList *attendees;

	e_cal_component_get_uid (view->priv->comp, &uid);
	rid = e_cal_component_get_recurid_as_string (view->priv->comp);

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		icalcomponent_free (icalcomp);

		itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
			_("The meeting is invalid and cannot be updated"));
	} else {
		icalcomponent *org_icalcomp;
		const gchar *delegate;

		org_icalcomp = e_cal_component_get_icalcomponent (view->priv->comp);

		e_cal_component_get_attendee_list (view->priv->comp, &attendees);
		if (attendees != NULL) {
			ECalComponentAttendee *a = attendees->data;
			icalproperty *prop, *del_prop;
			EShell *shell = e_shell_get_default ();

			prop = find_attendee (icalcomp, itip_strip_mailto (a->value));
			if ((a->status == ICAL_PARTSTAT_DELEGATED) && (del_prop = find_attendee (org_icalcomp, itip_strip_mailto (a->delto))) && !(find_attendee (icalcomp, itip_strip_mailto (a->delto)))) {
				gint response;
				delegate = icalproperty_get_attendee (del_prop);
				response = e_alert_run_dialog_for_args (
					e_shell_get_active_window (shell),
					"org.gnome.itip-formatter:add-delegate",
					itip_strip_mailto (a->value),
					itip_strip_mailto (delegate), NULL);
				if (response == GTK_RESPONSE_YES) {
					icalcomponent_add_property (icalcomp, icalproperty_new_clone (del_prop));
					e_cal_component_rescan (comp);
				} else if (response == GTK_RESPONSE_NO) {
					remove_delegate (view, delegate, itip_strip_mailto (a->value), comp);
					goto cleanup;
				} else {
					goto cleanup;
				}
			}

			if (prop == NULL) {
				gint response;

				if (a->delfrom && *a->delfrom) {
					response = e_alert_run_dialog_for_args (
						e_shell_get_active_window (shell),
						"org.gnome.itip-formatter:add-delegate",
						itip_strip_mailto (a->delfrom),
						itip_strip_mailto (a->value), NULL);
					if (response == GTK_RESPONSE_YES) {
						/* Already declared in this function */
						icalproperty *prop = find_attendee (icalcomp, itip_strip_mailto (a->value));
						icalcomponent_add_property (icalcomp,icalproperty_new_clone (prop));
						e_cal_component_rescan (comp);
					} else if (response == GTK_RESPONSE_NO) {
						remove_delegate (
							view,
							itip_strip_mailto (a->value),
							itip_strip_mailto (a->delfrom),
							comp);
						goto cleanup;
					} else {
						goto cleanup;
					}
				}

				response = e_alert_run_dialog_for_args (
					e_shell_get_active_window (shell),
					"org.gnome.itip-formatter:add-unknown-attendee", NULL);

				if (response == GTK_RESPONSE_YES) {
					change_status (
						view->priv->registry, icalcomp,
						itip_strip_mailto (a->value),
						a->status);
					e_cal_component_rescan (comp);
				} else {
					goto cleanup;
				}
			} else if (a->status == ICAL_PARTSTAT_NONE || a->status == ICAL_PARTSTAT_X) {
				itip_view_add_lower_info_item (
					view, ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
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
				} else {
					change_status (
						view->priv->registry,icalcomp,
						itip_strip_mailto (a->value),
						a->status);
				}

				e_cal_component_rescan (comp);
			}
		}
	}

	update_x (view->priv->comp, comp);

	if (itip_view_get_update (view)) {
		e_cal_component_commit_sequence (comp);
		itip_send_comp_sync (
			view->priv->registry,
			E_CAL_COMPONENT_METHOD_REQUEST,
			comp, view->priv->current_client,
			NULL, NULL, NULL, TRUE, FALSE, NULL, NULL);
	}

	update_item_progress_info (view, _("Saving changes to the calendar. Please wait..."));

	e_cal_client_modify_object (
		view->priv->current_client,
		icalcomp, rid ? E_CAL_OBJ_MOD_THIS : E_CAL_OBJ_MOD_ALL,
		view->priv->cancellable,
		modify_object_cb,
		view);

 cleanup:
	g_object_unref (comp);
}

static void
update_attendee_status_get_object_without_rid_cb (GObject *ecalclient,
                                                  GAsyncResult *result,
                                                  gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (ecalclient);
	ItipView *view = user_data;
	icalcomponent *icalcomp = NULL;
	GError *error = NULL;

	e_cal_client_get_object_finish (client, result, &icalcomp, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);

	} else if (error != NULL) {
		g_error_free (error);

		update_item_progress_info (view, NULL);
		view->priv->update_item_error_info_id =
			itip_view_add_lower_info_item (
				view,
				ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
				_("Attendee status can not be updated "
				"because the item no longer exists"));

	} else {
		update_attendee_status_icalcomp (view, icalcomp);
	}
}

static void
update_attendee_status_get_object_with_rid_cb (GObject *ecalclient,
                                               GAsyncResult *result,
                                               gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (ecalclient);
	ItipView *view = user_data;
	icalcomponent *icalcomp = NULL;
	GError *error = NULL;

	e_cal_client_get_object_finish (client, result, &icalcomp, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);

	} else if (error != NULL) {
		const gchar *uid;
		gchar *rid;

		g_error_free (error);

		e_cal_component_get_uid (view->priv->comp, &uid);
		rid = e_cal_component_get_recurid_as_string (view->priv->comp);

		if (rid == NULL || *rid == '\0') {
			update_item_progress_info (view, NULL);
			view->priv->update_item_error_info_id =
				itip_view_add_lower_info_item (
					view,
					ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
					_("Attendee status can not be updated "
					"because the item no longer exists"));
		} else {
			e_cal_client_get_object (
				view->priv->current_client,
				uid,
				NULL,
				view->priv->cancellable,
				update_attendee_status_get_object_without_rid_cb,
				view);
		}

		g_free (rid);

	} else {
		update_attendee_status_icalcomp (view, icalcomp);
	}
}

static void
update_attendee_status (ItipView *view)
{
	const gchar *uid = NULL;
	gchar *rid;

	/* Obtain our version */
	e_cal_component_get_uid (view->priv->comp, &uid);
	rid = e_cal_component_get_recurid_as_string (view->priv->comp);

	update_item_progress_info (view, _("Saving changes to the calendar. Please wait..."));

	/* search for a master object if the detached object doesn't exist in the calendar */
	e_cal_client_get_object (
		view->priv->current_client,
		uid, rid,
		view->priv->cancellable,
		update_attendee_status_get_object_with_rid_cb,
		view);

	g_free (rid);
}

static void
send_item (ItipView *view)
{
	ECalComponent *comp;

	comp = get_real_item (view);

	if (comp != NULL) {
		itip_send_comp_sync (
			view->priv->registry,
			E_CAL_COMPONENT_METHOD_REQUEST,
			comp, view->priv->current_client,
			NULL, NULL, NULL, TRUE, FALSE, NULL, NULL);
		g_object_unref (comp);

		switch (view->priv->type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			itip_view_add_lower_info_item (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Meeting information sent"));
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			itip_view_add_lower_info_item (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Task information sent"));
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			itip_view_add_lower_info_item (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Memo information sent"));
			break;
		default:
			g_warn_if_reached ();
			break;
		}
	} else {
		switch (view->priv->type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			itip_view_add_lower_info_item (
				view, ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
				_("Unable to send meeting information, the meeting does not exist"));
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			itip_view_add_lower_info_item (
				view, ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
				_("Unable to send task information, the task does not exist"));
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			itip_view_add_lower_info_item (
				view, ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
				_("Unable to send memo information, the memo does not exist"));
			break;
		default:
			g_warn_if_reached ();
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
attachment_load_finish (EAttachment *attachment,
                        GAsyncResult *result,
                        GFile *file)
{
	EShell *shell;
	GtkWindow *parent;

	/* XXX Theoretically, this should never fail. */
	e_attachment_load_finish (attachment, result, NULL);

	shell = e_shell_get_default ();
	parent = e_shell_get_active_window (shell);

	e_attachment_save_async (
		attachment, file, (GAsyncReadyCallback)
		e_attachment_save_handle_error, parent);

	g_object_unref (file);
}

static void
save_vcalendar_cb (ItipView *view)
{
	EAttachment *attachment;
	EShell *shell;
	GFile *file;
	const gchar *suggestion;

	g_return_if_fail (ITIP_IS_VIEW (view));
	g_return_if_fail (view->priv->vcalendar != NULL);
	g_return_if_fail (view->priv->itip_mime_part != NULL);

	suggestion = camel_mime_part_get_filename (view->priv->itip_mime_part);
	if (suggestion == NULL) {
		/* Translators: This is a default filename for a calendar. */
		suggestion = _("calendar.ics");
	}

	shell = e_shell_get_default ();
	file = e_shell_run_save_dialog (
		shell, _("Save Calendar"), suggestion, "*.ics:text/calendar", NULL, NULL);
	if (file == NULL)
		return;

	attachment = e_attachment_new ();
	e_attachment_set_mime_part (attachment, view->priv->itip_mime_part);

	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		attachment_load_finish, file);
}

static void
set_itip_error (ItipView *view,
                const gchar *primary,
                const gchar *secondary,
                gboolean save_btn)
{
	gchar *error;

	error = g_strdup_printf (
		"<div class=\"error\">"
		"<p><b>%s</b></p>"
		"<p>%s</p>",
		primary, secondary);

	itip_view_set_error (view, error, save_btn);

	g_free (error);
}

static gboolean
extract_itip_data (ItipView *view,
                   gboolean *have_alarms)
{
	GSettings *settings;
	icalproperty *prop;
	icalcomponent_kind kind = ICAL_NO_COMPONENT;
	icalcomponent *tz_comp;
	icalcompiter tz_iter;
	icalcomponent *alarm_comp;
	icalcompiter alarm_iter;
	ECalComponent *comp;
	gboolean use_default_reminder;
	gint total;

	if (!view->priv->vcalendar) {
		set_itip_error (
			view,
			_("The calendar attached is not valid"),
			_("The message claims to contain a calendar, but the calendar is not a valid iCalendar."),
			FALSE);

		return FALSE;
	}

	view->priv->top_level = e_cal_util_new_top_level ();

	view->priv->main_comp = icalparser_parse_string (view->priv->vcalendar);
	if (view->priv->main_comp == NULL || !is_icalcomp_valid (view->priv->main_comp)) {
		set_itip_error (
			view,
			_("The calendar attached is not valid"),
			_("The message claims to contain a calendar, but the calendar is not a valid iCalendar."),
			FALSE);

		if (view->priv->main_comp) {
			icalcomponent_free (view->priv->main_comp);
			view->priv->main_comp = NULL;
		}

		return FALSE;
	}

	prop = icalcomponent_get_first_property (view->priv->main_comp, ICAL_METHOD_PROPERTY);
	if (prop == NULL) {
		view->priv->method = ICAL_METHOD_PUBLISH;
	} else {
		view->priv->method = icalproperty_get_method (prop);
	}

	tz_iter = icalcomponent_begin_component (view->priv->main_comp, ICAL_VTIMEZONE_COMPONENT);
	while ((tz_comp = icalcompiter_deref (&tz_iter)) != NULL) {
		icalcomponent *clone;

		clone = icalcomponent_new_clone (tz_comp);
		icalcomponent_add_component (view->priv->top_level, clone);

		icalcompiter_next (&tz_iter);
	}

	view->priv->iter = icalcomponent_begin_component (view->priv->main_comp, ICAL_ANY_COMPONENT);
	view->priv->ical_comp = icalcompiter_deref (&view->priv->iter);
	if (view->priv->ical_comp != NULL) {
		kind = icalcomponent_isa (view->priv->ical_comp);
		if (kind != ICAL_VEVENT_COMPONENT
		    && kind != ICAL_VTODO_COMPONENT
		    && kind != ICAL_VFREEBUSY_COMPONENT
		    && kind != ICAL_VJOURNAL_COMPONENT)
			view->priv->ical_comp = get_next (&view->priv->iter);
	}

	if (view->priv->ical_comp == NULL) {
		set_itip_error (
			view,
			_("The item in the calendar is not valid"),
			_("The message does contain a calendar, but the calendar contains no events, tasks or free/busy information"),
			FALSE);

		return FALSE;
	}

	view->priv->with_detached_instances = FALSE;

	total = icalcomponent_count_components (view->priv->main_comp, ICAL_VEVENT_COMPONENT);
	total += icalcomponent_count_components (view->priv->main_comp, ICAL_VTODO_COMPONENT);
	total += icalcomponent_count_components (view->priv->main_comp, ICAL_VFREEBUSY_COMPONENT);
	total += icalcomponent_count_components (view->priv->main_comp, ICAL_VJOURNAL_COMPONENT);

	if (total > 1) {
		icalcomponent *icomp, *master_comp = NULL;
		gint orig_total = total;
		const gchar *expected_uid = NULL;

		for (icomp = icalcomponent_get_first_component (view->priv->main_comp, ICAL_ANY_COMPONENT);
		     icomp;
		     icomp = icalcomponent_get_next_component (view->priv->main_comp, ICAL_ANY_COMPONENT)) {
			icalcomponent_kind icomp_kind;
			const gchar *uid;

			icomp_kind = icalcomponent_isa (icomp);

			if (icomp_kind != ICAL_VEVENT_COMPONENT &&
			    icomp_kind != ICAL_VJOURNAL_COMPONENT &&
			    icomp_kind != ICAL_VTODO_COMPONENT)
				continue;

			uid = icalcomponent_get_uid (icomp);

			if (!master_comp) {
				struct icaltimetype rid;

				rid = icalcomponent_get_recurrenceid (icomp);
				if (!icaltime_is_valid_time (rid) || icaltime_is_null_time (rid))
					master_comp = icomp;
			}

			/* Maybe it's an event with detached instances */
			if (!expected_uid) {
				expected_uid = uid;
			} else if (g_strcmp0 (uid, expected_uid) == 0) {
				total--;
			} else {
				total = orig_total;
				break;
			}
		}

		view->priv->with_detached_instances = orig_total != total;
		if (view->priv->with_detached_instances && master_comp && master_comp != view->priv->ical_comp)
			view->priv->ical_comp = master_comp;
	}

	switch (icalcomponent_isa (view->priv->ical_comp)) {
	case ICAL_VEVENT_COMPONENT:
		view->priv->type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
		view->priv->has_organizer = icalcomponent_get_first_property (view->priv->ical_comp, ICAL_ORGANIZER_PROPERTY) != NULL;
		if (icalcomponent_get_first_property (view->priv->ical_comp, ICAL_ATTENDEE_PROPERTY) == NULL) {
			/* no attendees: assume that that this is not a meeting and organizer doesn't want a reply */
			view->priv->no_reply_wanted = TRUE;
		} else {
			/*
			 * if we have attendees, then find_to_address() will check for our RSVP
			 * and set no_reply_wanted=TRUE if RSVP=FALSE for the current user
			 */
		}
		break;
	case ICAL_VTODO_COMPONENT:
		view->priv->type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
		break;
	case ICAL_VJOURNAL_COMPONENT:
		view->priv->type = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
		break;
	default:
		set_itip_error (
			view,
			_("The item in the calendar is not valid"),
			_("The message does contain a calendar, but the calendar contains no events, tasks or free/busy information"),
			FALSE);

		return FALSE;
	}

	if (total > 1) {
		set_itip_error (
			view,
			_("The calendar attached contains multiple items"),
			_("To process all of these items, the file should be saved and the calendar imported"),
			TRUE);
	}

	if (total > 0) {
		view->priv->current = 1;
	} else {
		view->priv->current = 0;
	}

	if (icalcomponent_isa (view->priv->ical_comp) != ICAL_VJOURNAL_COMPONENT) {
		gchar *my_address;

		prop = NULL;
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (view->priv->ical_comp));
		my_address = itip_get_comp_attendee (
			view->priv->registry, comp, NULL);
		g_object_unref (comp);
		comp = NULL;

		if (!prop)
			prop = find_attendee (view->priv->ical_comp, my_address);
		if (!prop)
			prop = find_attendee_if_sentby (view->priv->ical_comp, my_address);
		if (prop) {
			icalparameter *param;
			const gchar * delfrom;

			if ((param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDFROM_PARAMETER))) {
				delfrom = icalparameter_get_delegatedfrom (param);

				view->priv->delegator_address = g_strdup (itip_strip_mailto (delfrom));
			}
		}
		g_free (my_address);
		prop = NULL;

		/* Determine any delegate sections */
		prop = icalcomponent_get_first_property (view->priv->ical_comp, ICAL_X_PROPERTY);
		while (prop) {
			const gchar *x_name, *x_val;

			x_name = icalproperty_get_x_name (prop);
			x_val = icalproperty_get_x (prop);

			if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-CALENDAR-UID"))
				view->priv->calendar_uid = g_strdup (x_val);
			else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-CALENDAR-URI"))
				g_warning (G_STRLOC ": X-EVOLUTION-DELEGATOR-CALENDAR-URI used");
			else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-ADDRESS"))
				view->priv->delegator_address = g_strdup (x_val);
			else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-NAME"))
				view->priv->delegator_name = g_strdup (x_val);

			prop = icalcomponent_get_next_property (view->priv->ical_comp, ICAL_X_PROPERTY);
		}

		/* Strip out procedural alarms for security purposes */
		alarm_iter = icalcomponent_begin_component (view->priv->ical_comp, ICAL_VALARM_COMPONENT);
		while ((alarm_comp = icalcompiter_deref (&alarm_iter)) != NULL) {
			icalproperty *p;

			icalcompiter_next (&alarm_iter);

			p = icalcomponent_get_first_property (alarm_comp, ICAL_ACTION_PROPERTY);
			if (!p || icalproperty_get_action (p) == ICAL_ACTION_PROCEDURE)
				icalcomponent_remove_component (view->priv->ical_comp, alarm_comp);

			icalcomponent_free (alarm_comp);
		}

		if (have_alarms) {
			alarm_iter = icalcomponent_begin_component (view->priv->ical_comp, ICAL_VALARM_COMPONENT);
			*have_alarms = icalcompiter_deref (&alarm_iter) != NULL;
		}
	}

	view->priv->comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (view->priv->comp, view->priv->ical_comp)) {
		g_object_unref (view->priv->comp);
		view->priv->comp = NULL;

		set_itip_error (
			view,
			_("The item in the calendar is not valid"),
			_("The message does contain a calendar, but the calendar contains no events, tasks or free/busy information"),
			FALSE);

		return FALSE;
	};

	/* Add default reminder if the config says so */

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	use_default_reminder =
		g_settings_get_boolean (settings, "use-default-reminder");

	if (use_default_reminder) {
		ECalComponentAlarm *acomp;
		gint interval;
		EDurationType units;
		ECalComponentAlarmTrigger trigger;

		interval = g_settings_get_int (
			settings, "default-reminder-interval");
		units = g_settings_get_enum (
			settings, "default-reminder-units");

		acomp = e_cal_component_alarm_new ();

		e_cal_component_alarm_set_action (acomp, E_CAL_COMPONENT_ALARM_DISPLAY);

		trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;
		memset (&trigger.u.rel_duration, 0, sizeof (trigger.u.rel_duration));

		trigger.u.rel_duration.is_neg = TRUE;

		switch (units) {
			case E_DURATION_MINUTES:
				trigger.u.rel_duration.minutes = interval;
				break;
			case E_DURATION_HOURS:
				trigger.u.rel_duration.hours = interval;
				break;
			case E_DURATION_DAYS:
				trigger.u.rel_duration.days = interval;
				break;
			default:
				g_warn_if_reached ();
				break;
		}

		e_cal_component_alarm_set_trigger (acomp, trigger);
		e_cal_component_add_alarm (view->priv->comp, acomp);

		e_cal_component_alarm_free (acomp);
	}

	g_object_unref (settings);

	find_from_address (view, view->priv->ical_comp);
	find_to_address (view, view->priv->ical_comp, NULL);

	return TRUE;
}

static gboolean
idle_open_cb (gpointer user_data)
{
	ItipView *view = user_data;
	EShell *shell;
	const gchar *uris[2];
	gchar *start, *end, *shell_uri;

	start = isodate_from_time_t (view->priv->start_time ? view->priv->start_time : time (NULL));
	end = isodate_from_time_t (view->priv->end_time ? view->priv->end_time : time (NULL));
	shell_uri = g_strdup_printf ("calendar:///?startdate=%s&enddate=%s", start, end);

	uris[0] = shell_uri;
	uris[1] = NULL;

	shell = e_shell_get_default ();
	e_shell_handle_uris (shell, uris, FALSE);

	g_free (shell_uri);
	g_free (start);
	g_free (end);

	return FALSE;
}

static void
view_response_cb (ItipView *view,
                  ItipViewResponse response,
                  gpointer user_data)
{
	gboolean status = FALSE;
	icalproperty *prop;
	ECalComponentTransparency trans;

	if (response == ITIP_VIEW_RESPONSE_SAVE) {
		save_vcalendar_cb (view);
		return;
	}

	if (view->priv->method == ICAL_METHOD_PUBLISH || view->priv->method == ICAL_METHOD_REQUEST) {
		if (itip_view_get_free_time_check_state (view))
			e_cal_component_set_transparency (view->priv->comp, E_CAL_COMPONENT_TRANSP_TRANSPARENT);
		else
			e_cal_component_set_transparency (view->priv->comp, E_CAL_COMPONENT_TRANSP_OPAQUE);
	} else {
		e_cal_component_get_transparency (view->priv->comp, &trans);

		if (trans == E_CAL_COMPONENT_TRANSP_NONE)
			e_cal_component_set_transparency (view->priv->comp, E_CAL_COMPONENT_TRANSP_OPAQUE);
	}

	if (!view->priv->to_address && view->priv->current_client != NULL) {
		e_client_get_backend_property_sync (E_CLIENT (view->priv->current_client), CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &view->priv->to_address, NULL, NULL);

		if (view->priv->to_address && !*view->priv->to_address) {
			g_free (view->priv->to_address);
			view->priv->to_address = NULL;
		}
	}

	/* check if it is a  recur instance (no master object) and
	 * add a property */
	if (itip_view_get_recur_check_state (view)) {
		prop = icalproperty_new_x ("All");
		icalproperty_set_x_name (prop, "X-GW-RECUR-INSTANCES-MOD-TYPE");
		icalcomponent_add_property (view->priv->ical_comp, prop);
	}

	switch (response) {
		case ITIP_VIEW_RESPONSE_ACCEPT:
			if (view->priv->type != E_CAL_CLIENT_SOURCE_TYPE_MEMOS)
				status = change_status (
					view->priv->registry,
					view->priv->ical_comp,
					view->priv->to_address,
					ICAL_PARTSTAT_ACCEPTED);
			else
				status = TRUE;
			if (status) {
				e_cal_component_rescan (view->priv->comp);
				update_item (view, response);
			}
			break;
		case ITIP_VIEW_RESPONSE_TENTATIVE:
			status = change_status (
					view->priv->registry,
					view->priv->ical_comp,
					view->priv->to_address,
					ICAL_PARTSTAT_TENTATIVE);
			if (status) {
				e_cal_component_rescan (view->priv->comp);
				update_item (view, response);
			}
			break;
		case ITIP_VIEW_RESPONSE_DECLINE:
			if (view->priv->type != E_CAL_CLIENT_SOURCE_TYPE_MEMOS)
				status = change_status (
					view->priv->registry,
					view->priv->ical_comp,
					view->priv->to_address,
					ICAL_PARTSTAT_DECLINED);
			else {
				prop = icalproperty_new_x ("1");
				icalproperty_set_x_name (prop, "X-GW-DECLINED");
				icalcomponent_add_property (view->priv->ical_comp, prop);
				status = TRUE;
			}

			if (status) {
				e_cal_component_rescan (view->priv->comp);
				update_item (view, response);
			}
			break;
		case ITIP_VIEW_RESPONSE_UPDATE:
			update_attendee_status (view);
			break;
		case ITIP_VIEW_RESPONSE_CANCEL:
			update_item (view, response);
			break;
		case ITIP_VIEW_RESPONSE_REFRESH:
			send_item (view);
			break;
		case ITIP_VIEW_RESPONSE_OPEN:
			/* Prioritize ahead of GTK+ redraws. */
			g_idle_add_full (
				G_PRIORITY_HIGH_IDLE,
				idle_open_cb, g_object_ref (view), g_object_unref);
			return;
		default:
			break;
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
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	MailFolderCache *folder_cache;
	ESourceRegistry *registry;
	CamelStore *store;
	const gchar *folder_name;
	gboolean res = TRUE;
	CamelFolderInfoFlags flags = 0;
	gboolean have_flags;

	if (folder == NULL)
		return FALSE;

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	folder_cache = e_mail_session_get_folder_cache (session);

	store = camel_folder_get_parent_store (folder);
	folder_name = camel_folder_get_full_name (folder);

	have_flags = mail_folder_cache_get_folder_info_flags (
		folder_cache, store, folder_name, &flags);

	if (have_flags) {
		/* it should be neither trash nor junk folder, */
		res = ((flags & CAMEL_FOLDER_TYPE_MASK) != CAMEL_FOLDER_TYPE_TRASH &&
		       (flags & CAMEL_FOLDER_TYPE_MASK) != CAMEL_FOLDER_TYPE_JUNK &&
			  /* it can be Inbox */
			((flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX ||
			  /* or any other virtual folder */
			  CAMEL_IS_VEE_FOLDER (folder) ||
			  /* or anything else except of sent, outbox or drafts folder */
			  (!em_utils_folder_is_sent (registry, folder) &&
			   !em_utils_folder_is_outbox (registry, folder) &&
			   !em_utils_folder_is_drafts (registry, folder))
			));
	} else {
		/* cannot check for Inbox folder here */
		res = (camel_folder_get_flags (folder) & (CAMEL_FOLDER_IS_TRASH | CAMEL_FOLDER_IS_JUNK)) == 0 && (
		      (CAMEL_IS_VEE_FOLDER (folder)) || (
		      !em_utils_folder_is_sent (registry, folder) &&
		      !em_utils_folder_is_outbox (registry, folder) &&
		      !em_utils_folder_is_drafts (registry, folder)));
	}

	return res;
}

static icaltimezone *
itip_view_guess_timezone (const gchar *tzid)
{
	icaltimezone *zone;

	if (!tzid || !*tzid)
		return NULL;

	zone = icaltimezone_get_builtin_timezone (tzid);
	if (zone)
		return zone;

	zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	if (zone)
		return zone;

	tzid = e_cal_match_tzid (tzid);
	if (tzid)
		zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);

	return zone;
}

void
itip_view_init_view (ItipView *view)
{
	ECalComponentText text;
	ECalComponentOrganizer organizer;
	ECalComponentDateTime datetime;
	icaltimezone *from_zone;
	icaltimezone *to_zone = NULL;
	GSettings *settings;
	GString *gstring = NULL;
	GSList *list, *l;
	icalcomponent *icalcomp;
	const gchar *string, *org;
	gboolean response_enabled;
	gboolean have_alarms = FALSE;

	g_return_if_fail (ITIP_IS_VIEW (view));

        /* Reset current client before initializing view */
	view->priv->current_client = NULL;

        /* FIXME Handle multiple VEVENTS with the same UID, ie detached instances */
	if (!extract_itip_data (view, &have_alarms))
		return;

	response_enabled = in_proper_folder (view->priv->folder);

	if (!response_enabled) {
		itip_view_set_mode (view, ITIP_VIEW_MODE_HIDE_ALL);
	} else {
		itip_view_set_show_inherit_alarm_check (
			view,
			have_alarms && (view->priv->method == ICAL_METHOD_PUBLISH || view->priv->method == ICAL_METHOD_REQUEST));

		switch (view->priv->method) {
			case ICAL_METHOD_PUBLISH:
			case ICAL_METHOD_REQUEST:
                                /*
                                 * Treat meeting request (sent by organizer directly) and
                                 * published evend (forwarded by organizer or attendee) alike:
                                 * if the event has an organizer, then it can be replied to and
                                 * we show the "accept/tentative/decline" choice.
                                 * Otherwise only show "accept".
                                 */
				itip_view_set_mode (
					view,
					view->priv->has_organizer ?
					ITIP_VIEW_MODE_REQUEST :
					ITIP_VIEW_MODE_PUBLISH);
				break;
			case ICAL_METHOD_REPLY:
				itip_view_set_mode (view, ITIP_VIEW_MODE_REPLY);
				break;
			case ICAL_METHOD_ADD:
				itip_view_set_mode (view, ITIP_VIEW_MODE_ADD);
				break;
			case ICAL_METHOD_CANCEL:
				itip_view_set_mode (view, ITIP_VIEW_MODE_CANCEL);
				break;
			case ICAL_METHOD_REFRESH:
				itip_view_set_mode (view, ITIP_VIEW_MODE_REFRESH);
				break;
			case ICAL_METHOD_COUNTER:
				itip_view_set_mode (view, ITIP_VIEW_MODE_COUNTER);
				break;
			case ICAL_METHOD_DECLINECOUNTER:
				itip_view_set_mode (view, ITIP_VIEW_MODE_DECLINECOUNTER);
				break;
			case ICAL_METHOD_X :
                                /* Handle appointment requests from Microsoft Live. This is
                                 * a best-at-hand-now handling. Must be revisited when we have
                                 * better access to the source of such meetings */
				view->priv->method = ICAL_METHOD_REQUEST;
				itip_view_set_mode (view, ITIP_VIEW_MODE_REQUEST);
				break;
			default:
				return;
		}
	}

	itip_view_set_item_type (view, view->priv->type);

	if (response_enabled) {
		switch (view->priv->method) {
			case ICAL_METHOD_REQUEST:
                                /* FIXME What about the name? */
				itip_view_set_delegator (view, view->priv->delegator_name ? view->priv->delegator_name : view->priv->delegator_address);
				/* coverity[fallthrough] */
			case ICAL_METHOD_PUBLISH:
			case ICAL_METHOD_ADD:
			case ICAL_METHOD_CANCEL:
			case ICAL_METHOD_DECLINECOUNTER:
				itip_view_set_show_update_check (view, FALSE);

                                /* An organizer sent this */
				e_cal_component_get_organizer (view->priv->comp, &organizer);
				org = organizer.cn ? organizer.cn : itip_strip_mailto (organizer.value);

				itip_view_set_organizer (view, org);
				if (organizer.sentby)
					itip_view_set_organizer_sentby (
						view, itip_strip_mailto (organizer.sentby));

				if (view->priv->my_address) {
					if (!(organizer.value && !g_ascii_strcasecmp (itip_strip_mailto (organizer.value), view->priv->my_address))
						&& !(organizer.sentby && !g_ascii_strcasecmp (itip_strip_mailto (organizer.sentby), view->priv->my_address))
						&& (view->priv->to_address && g_ascii_strcasecmp (view->priv->to_address, view->priv->my_address)))
						itip_view_set_proxy (view, view->priv->to_name ? view->priv->to_name : view->priv->to_address);
				}
				break;
			case ICAL_METHOD_REPLY:
			case ICAL_METHOD_REFRESH:
			case ICAL_METHOD_COUNTER:
				itip_view_set_show_update_check (view, TRUE);

                                /* An attendee sent this */
				e_cal_component_get_attendee_list (view->priv->comp, &list);
				if (list != NULL) {
					ECalComponentAttendee *attendee;

					attendee = list->data;

					itip_view_set_attendee (view, attendee->cn ? attendee->cn : itip_strip_mailto (attendee->value));

					if (attendee->sentby)
						itip_view_set_attendee_sentby (view, itip_strip_mailto (attendee->sentby));

					if (view->priv->my_address) {
						if (!(attendee->value && !g_ascii_strcasecmp (itip_strip_mailto (attendee->value), view->priv->my_address))
							&& !(attendee->sentby && !g_ascii_strcasecmp (itip_strip_mailto (attendee->sentby), view->priv->my_address))
							&& (view->priv->from_address && g_ascii_strcasecmp (view->priv->from_address, view->priv->my_address)))
							itip_view_set_proxy (view, view->priv->from_name ? view->priv->from_name : view->priv->from_address);
					}

					e_cal_component_free_attendee_list (list);
				}
				break;
			default:
				g_warn_if_reached ();
				break;
		}
	}

	e_cal_component_get_summary (view->priv->comp, &text);
	itip_view_set_summary (view, text.value ? text.value : C_("cal-itip", "None"));

	e_cal_component_get_location (view->priv->comp, &string);
	itip_view_set_location (view, string);

        /* Status really only applies for REPLY */
	if (response_enabled && view->priv->method == ICAL_METHOD_REPLY) {
		e_cal_component_get_attendee_list (view->priv->comp, &list);
		if (list != NULL) {
			ECalComponentAttendee *a = list->data;

			switch (a->status) {
				case ICAL_PARTSTAT_ACCEPTED:
					itip_view_set_status (view, _("Accepted"));
					break;
				case ICAL_PARTSTAT_TENTATIVE:
					itip_view_set_status (view, _("Tentatively Accepted"));
					break;
				case ICAL_PARTSTAT_DECLINED:
					itip_view_set_status (view, _("Declined"));
					break;
				case ICAL_PARTSTAT_DELEGATED:
					itip_view_set_status (view, _("Delegated"));
					break;
				default:
					itip_view_set_status (view, _("Unknown"));
			}
		}
		e_cal_component_free_attendee_list (list);
	}

	if (view->priv->method == ICAL_METHOD_REPLY
		|| view->priv->method == ICAL_METHOD_COUNTER
		|| view->priv->method == ICAL_METHOD_DECLINECOUNTER) {
                /* FIXME Check spec to see if multiple comments are actually valid */
                /* Comments for iTIP are limited to one per object */
		e_cal_component_get_comment_list (view->priv->comp, &list);
		if (list) {
			ECalComponentText *text = list->data;

			if (text->value) {
				gchar *html;

				html = camel_text_to_html (
					text->value,
					CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES,
					0);

				itip_view_set_comment (view, html);

				g_free (html);
			}
		}
		e_cal_component_free_text_list (list);
	}

	itip_view_extract_attendee_info (view);

	e_cal_component_get_description_list (view->priv->comp, &list);
	for (l = list; l; l = l->next) {
		ECalComponentText *text = l->data;

		if (!gstring && text->value)
			gstring = g_string_new (text->value);
		else if (text->value)
			g_string_append_printf (gstring, "\n\n%s", text->value);
	}

	e_cal_component_free_text_list (list);

	if (gstring) {
		gchar *html;

		html = camel_text_to_html (
			gstring->str,
			CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
			CAMEL_MIME_FILTER_TOHTML_MARK_CITATION |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES,
			0);

		itip_view_set_description (view, html);
		g_string_free (gstring, TRUE);
		g_free (html);
	}

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone"))
		to_zone = e_cal_util_get_system_timezone ();
	else {
		gchar *location;

		location = g_settings_get_string (settings, "timezone");
		if (location != NULL) {
			to_zone = icaltimezone_get_builtin_timezone (location);
			g_free (location);
		}
	}

	if (to_zone == NULL)
		to_zone = icaltimezone_get_utc_timezone ();

	g_object_unref (settings);

	e_cal_component_get_dtstart (view->priv->comp, &datetime);
	view->priv->start_time = 0;
	if (datetime.value) {
		struct tm start_tm;

                /* If the timezone is not in the component, guess the local time */
                /* Should we guess if the timezone is an olsen name somehow? */
		if (icaltime_is_utc (*datetime.value))
			from_zone = icaltimezone_get_utc_timezone ();
		else if (!icaltime_is_utc (*datetime.value) && datetime.tzid) {
			from_zone = icalcomponent_get_timezone (view->priv->top_level, datetime.tzid);

			if (!from_zone)
				from_zone = itip_view_guess_timezone (datetime.tzid);
		} else
			from_zone = NULL;

		start_tm = icaltimetype_to_tm_with_zone (datetime.value, from_zone, to_zone);

		itip_view_set_start (view, &start_tm, datetime.value->is_date);
		view->priv->start_time = icaltime_as_timet_with_zone (*datetime.value, from_zone);
	}

	icalcomp = e_cal_component_get_icalcomponent (view->priv->comp);

        /* Set the recurrence id */
	if (check_is_instance (icalcomp) && datetime.value) {
		ECalComponentRange *recur_id;
		struct icaltimetype icaltime = icaltime_convert_to_zone (*datetime.value, to_zone);

		recur_id = g_new0 (ECalComponentRange, 1);
		recur_id->type = E_CAL_COMPONENT_RANGE_SINGLE;
		recur_id->datetime.value = &icaltime;
		recur_id->datetime.tzid = icaltimezone_get_tzid (to_zone);
		e_cal_component_set_recurid (view->priv->comp, recur_id);
		g_free (recur_id); /* it's ok to call g_free here */
	}
	e_cal_component_free_datetime (&datetime);

	e_cal_component_get_dtend (view->priv->comp, &datetime);
	view->priv->end_time = 0;
	if (datetime.value) {
		struct tm end_tm;

                /* If the timezone is not in the component, guess the local time */
                /* Should we guess if the timezone is an olsen name somehow? */
		if (icaltime_is_utc (*datetime.value))
			from_zone = icaltimezone_get_utc_timezone ();
		else if (!icaltime_is_utc (*datetime.value) && datetime.tzid) {
			from_zone = icalcomponent_get_timezone (view->priv->top_level, datetime.tzid);

			if (!from_zone)
				from_zone = itip_view_guess_timezone (datetime.tzid);
		} else
			from_zone = NULL;

		if (datetime.value->is_date) {
                        /* RFC says the DTEND is not inclusive, thus subtract one day
                         * if we have a date */

			icaltime_adjust (datetime.value, -1, 0, 0, 0);
		}

		end_tm = icaltimetype_to_tm_with_zone (datetime.value, from_zone, to_zone);

		itip_view_set_end (view, &end_tm, datetime.value->is_date);
		view->priv->end_time = icaltime_as_timet_with_zone (*datetime.value, from_zone);
	}
	e_cal_component_free_datetime (&datetime);

        /* Recurrence info */
        /* FIXME Better recurring description */
	if (e_cal_component_has_recurrences (view->priv->comp)) {
                /* FIXME Tell the user we don't support recurring tasks */
		switch (view->priv->type) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				itip_view_add_upper_info_item (view, ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("This meeting recurs"));
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				itip_view_add_upper_info_item (view, ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("This task recurs"));
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				itip_view_add_upper_info_item (view, ITIP_VIEW_INFO_ITEM_TYPE_INFO, _("This memo recurs"));
				break;
			default:
				g_warn_if_reached ();
				break;
		}
	}

	g_signal_connect (
		view, "response",
		G_CALLBACK (view_response_cb), NULL);

	if (response_enabled) {
		itip_view_set_show_free_time_check (view, view->priv->type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS && (view->priv->method == ICAL_METHOD_PUBLISH || view->priv->method == ICAL_METHOD_REQUEST));

		if (view->priv->calendar_uid) {
			start_calendar_server_by_uid (view, view->priv->calendar_uid, view->priv->type);
		} else {
			find_server (view, view->priv->comp);
			set_buttons_sensitive (view);
		}
	} else if (view->priv->web_extension) {
		/* The Open Calendar button can be shown, thus enable it */
		enable_button (view, BUTTON_OPEN_CALENDAR, TRUE);
	}
}

void
itip_view_set_web_view (ItipView *view,
			EWebView *web_view)
{
	g_return_if_fail (ITIP_IS_VIEW (view));
	if (web_view)
		g_return_if_fail (E_IS_WEB_VIEW (web_view));

	g_weak_ref_set (view->priv->web_view_weakref, web_view);

	itip_view_register_clicked_listener (view);
}

EWebView *
itip_view_ref_web_view (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return g_weak_ref_get (view->priv->web_view_weakref);
}
