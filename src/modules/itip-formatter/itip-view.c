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

#include "calendar/gui/calendar-config.h"
#include "calendar/gui/comp-util.h"
#include "calendar/gui/itip-utils.h"

#include <mail/em-config.h>
#include <mail/em-utils.h>
#include <em-format/e-mail-formatter-utils.h>

#include "itip-view.h"
#include "e-mail-part-itip.h"

#include "itip-view-elements-defines.h"

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
	gchar *url;

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

	gchar *part_id;
	gchar *selected_source_uid;

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
	ICalComponent *main_comp;
	ICalComponent *ical_comp;
	ICalComponent *top_level;
	ICalPropertyMethod method;
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

	gchar *state_rsvp_comment;
	gboolean state_rsvp_check;
	gboolean state_update_check;
	gboolean state_recur_check;
	gboolean state_free_time_check;
	gboolean state_keep_alarm_check;
	gboolean state_inherit_alarm_check;
	gint state_response_id;
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
			gboolean *out_is_abbreviated_value,
                        gchar *buffer,
                        gint buffer_size)
{
	gchar *format;
	struct tm tomorrow_tm, week_tm;

	*out_is_abbreviated_value = TRUE;

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
		*out_is_abbreviated_value = FALSE;

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
		*out_is_abbreviated_value = FALSE;

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
contact_abbreviated_date (const gchar *buffer,
			  struct tm *tm_time,
			  gboolean tm_is_date,
			  gboolean is_abbreviated_value)
{
	gchar *res, *value;

	if (!*buffer || !is_abbreviated_value || !tm_time)
		return g_strdup (buffer);

	value = e_datetime_format_format_tm ("calendar", "table", DTFormatKindDate, tm_time);

	if (value && *value) {
		/* Translators: The first '%s' is replaced with an abbreviated date/time of an appointment start or end, like "Tomorrow" or "Tomorrow 10:30";
		   the second '%s' is replaced with the actual date, to know what the 'Tomorrow' means. What the date looks like depends on the user settings.
		   Example: 'Tomorrow 10:30 (20.2.2020)' */
		res = g_strdup_printf (C_("cal-itip", "%s (%s)"), buffer, value);
	} else {
		res = g_strdup (buffer);
	}

	g_free (value);

	return res;
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

static const gchar *
htmlize_text (const gchar *id,
	      const gchar *text,
	      gchar **out_tmp)
{
	if (text && *text) {
		if (g_strcmp0 (id, TABLE_ROW_LOCATION) == 0 ||
		    g_strcmp0 (id, TABLE_ROW_URL) == 0) {
			*out_tmp = camel_text_to_html (text, CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS | CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
		} else {
			*out_tmp = g_markup_escape_text (text, -1);
		}

		text = *out_tmp;
	}

	return text;
}

static void
enable_button (ItipView *view,
	       const gchar *button_id,
               gboolean enable)
{
	EWebView *web_view;

	web_view = itip_view_ref_web_view (view);
	if (web_view) {
		e_web_view_jsc_set_element_disabled (WEBKIT_WEB_VIEW (web_view), view->priv->part_id, button_id, !enable,
			e_web_view_get_cancellable (web_view));
		g_object_unref (web_view);
	}
}

static void
hide_element (ItipView *view,
	      const gchar *element_id,
              gboolean hide)
{
	EWebView *web_view;

	web_view = itip_view_ref_web_view (view);
	if (web_view) {
		e_web_view_jsc_set_element_hidden (WEBKIT_WEB_VIEW (web_view), view->priv->part_id, element_id, hide,
			e_web_view_get_cancellable (web_view));
		g_object_unref (web_view);
	}
}

#define show_button(_view, _id) hide_element(_view, _id, FALSE)

static void
set_inner_html (ItipView *view,
	        const gchar *element_id,
                const gchar *inner_html)
{
	EWebView *web_view;

	web_view = itip_view_ref_web_view (view);
	if (web_view) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
			"EvoItip.SetElementInnerHTML(%s, %s, %s);",
			view->priv->part_id, element_id, inner_html);
		g_object_unref (web_view);
	}
}

static void
input_set_checked (ItipView *view,
                   const gchar *input_id,
                   gboolean checked)
{
	EWebView *web_view;

	web_view = itip_view_ref_web_view (view);
	if (web_view) {
		e_web_view_jsc_set_element_checked (WEBKIT_WEB_VIEW (web_view), view->priv->part_id, input_id, checked,
			e_web_view_get_cancellable (web_view));
		g_object_unref (web_view);
	}
}

static void
show_checkbox (ItipView *view,
               const gchar *element_id,
               gboolean show,
	       gboolean update_second)
{
	EWebView *web_view;

	web_view = itip_view_ref_web_view (view);
	if (web_view) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
			"EvoItip.SetShowCheckbox(%s, %s, %x, %x);",
			view->priv->part_id, element_id, show, update_second);
		g_object_unref (web_view);
	}
}

static void
set_area_text (ItipView *view,
	       const gchar *id,
	       const gchar *text,
	       gboolean is_html)
{
	EWebView *web_view;

	web_view = itip_view_ref_web_view (view);
	if (web_view) {
		gchar *tmp = NULL;

		if (!is_html)
			text = htmlize_text (id, text, &tmp);

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
			"EvoItip.SetAreaText(%s, %s, %s);",
			view->priv->part_id, id, text);

		g_object_unref (web_view);
		g_free (tmp);
	}
}

static void
set_sender_text (ItipView *view)
{
	ItipViewPrivate *priv;
	priv = view->priv;

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

	if (priv->sender)
		set_inner_html (view, TEXT_ROW_SENDER, priv->sender);
}

static void
update_start_end_times (ItipView *view)
{
	ItipViewPrivate *priv;
	EWebView *web_view;
	gchar buffer[256];
	time_t now;
	struct tm *now_tm;
	gboolean is_abbreviated_value = FALSE;

	priv = view->priv;

	now = time (NULL);
	now_tm = localtime (&now);

	g_free (priv->start_label);
	g_free (priv->end_label);

	#define is_same(_member) (priv->start_tm->_member == priv->end_tm->_member)
	if (priv->start_tm && priv->end_tm && priv->start_tm_is_date && priv->end_tm_is_date
	    && is_same (tm_mday) && is_same (tm_mon) && is_same (tm_year)) {
		/* it's an all day event in one particular day */
		format_date_and_time_x (priv->start_tm, now_tm, FALSE, TRUE, FALSE, priv->start_tm_is_date, &is_abbreviated_value, buffer, 256);
		priv->start_label = contact_abbreviated_date (buffer, priv->start_tm, priv->start_tm_is_date, is_abbreviated_value);
		priv->start_header = _("All day:");
		priv->end_header = NULL;
		priv->end_label = NULL;
	} else {
		if (priv->start_tm) {
			format_date_and_time_x (priv->start_tm, now_tm, FALSE, TRUE, FALSE, priv->start_tm_is_date, &is_abbreviated_value, buffer, 256);
			priv->start_header = priv->start_tm_is_date ? _("Start day:") : _("Start time:");
			priv->start_label = contact_abbreviated_date (buffer, priv->start_tm, priv->start_tm_is_date, is_abbreviated_value);
		} else {
			priv->start_header = NULL;
			priv->start_label = NULL;
		}

		if (priv->end_tm) {
			format_date_and_time_x (priv->end_tm, now_tm, FALSE, TRUE, FALSE, priv->end_tm_is_date, &is_abbreviated_value, buffer, 256);
			priv->end_header = priv->end_tm_is_date ? _("End day:") : _("End time:");
			priv->end_label = contact_abbreviated_date (buffer, priv->end_tm, priv->end_tm_is_date, is_abbreviated_value);
		} else {
			priv->end_header = NULL;
			priv->end_label = NULL;
		}
	}
	#undef is_same

	web_view = itip_view_ref_web_view (view);

	if (!web_view)
		return;

	if (priv->start_header && priv->start_label) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
			"EvoItip.UpdateTimes(%s, %s, %s, %s);",
			view->priv->part_id,
			TABLE_ROW_START_DATE,
			priv->start_header,
			priv->start_label);
	} else {
		hide_element (view, TABLE_ROW_START_DATE, TRUE);
	}

	if (priv->end_header && priv->end_label) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
			"EvoItip.UpdateTimes(%s, %s, %s, %s);",
			view->priv->part_id,
			TABLE_ROW_END_DATE,
			priv->end_header,
			priv->end_label);
	} else {
		hide_element (view, TABLE_ROW_END_DATE, TRUE);
	}

	g_object_unref (web_view);
}

static void
itip_view_get_state_cb (GObject *source_object,
			GAsyncResult *result,
			gpointer user_data)
{
	ItipView *view;
	GWeakRef *wkrf = user_data;

	g_return_if_fail (E_IS_WEB_VIEW (source_object));
	g_return_if_fail (wkrf != NULL);

	view = g_weak_ref_get (wkrf);
	if (view) {
		WebKitJavascriptResult *js_result;
		GError *error = NULL;

		g_clear_pointer (&view->priv->state_rsvp_comment, g_free);

		js_result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (source_object), result, &error);

		if (error) {
			if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
			    (!g_error_matches (error, WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED) ||
			     /* WebKit can return empty error message, thus ignore those. */
			     (error->message && *(error->message))))
				g_warning ("Failed to call 'ItipView.GetState()' function: %s:%d: %s", g_quark_to_string (error->domain), error->code, error->message);
			g_clear_error (&error);
		}

		if (js_result) {
			JSCException *exception;
			JSCValue *value;

			value = webkit_javascript_result_get_js_value (js_result);
			exception = jsc_context_get_exception (jsc_value_get_context (value));

			if (exception) {
				g_warning ("Failed to call 'ItipView.GetState()': %s", jsc_exception_get_message (exception));
				jsc_context_clear_exception (jsc_value_get_context (value));
			}

			view->priv->state_rsvp_comment = e_web_view_jsc_get_object_property_string (value, "rsvp-comment", NULL);
			view->priv->state_rsvp_check = e_web_view_jsc_get_object_property_boolean (value, "rsvp-check", FALSE);
			view->priv->state_update_check = e_web_view_jsc_get_object_property_boolean (value, "update-check", FALSE);
			view->priv->state_recur_check = e_web_view_jsc_get_object_property_boolean (value, "recur-check", FALSE);
			view->priv->state_free_time_check = e_web_view_jsc_get_object_property_boolean (value, "free-time-check", FALSE);
			view->priv->state_keep_alarm_check = e_web_view_jsc_get_object_property_boolean (value, "keep-alarm-check", FALSE);
			view->priv->state_inherit_alarm_check = e_web_view_jsc_get_object_property_boolean (value, "inherit-alarm-check", FALSE);

			webkit_javascript_result_unref (js_result);

			g_signal_emit (view, signals[RESPONSE], 0, view->priv->state_response_id);
		}

		g_object_unref (view);
	}

	e_weak_ref_free (wkrf);
}

static void
itip_view_itip_button_clicked_cb (EWebView *web_view,
				  const gchar *iframe_id,
				  const gchar *element_id,
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
		gchar *script;

		view->priv->state_response_id = response;

		script = e_web_view_jsc_printf_script ("EvoItip.GetState(%s);", view->priv->part_id);

		webkit_web_view_run_javascript (WEBKIT_WEB_VIEW (web_view),
			script, e_web_view_get_cancellable (web_view), itip_view_get_state_cb, e_weak_ref_new (view));

		g_free (script);
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
itip_set_selected_source_uid (ItipView *view,
			      const gchar *uid)
{
	if (g_strcmp0 (view->priv->selected_source_uid, uid) != 0) {
		g_free (view->priv->selected_source_uid);
		view->priv->selected_source_uid = g_strdup (uid);
	}
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
append_checkbox_table_row (GString *buffer,
                           const gchar *name,
                           const gchar *label,
			   gboolean checked)
{
	gchar *access_key, *html_label;

	html_label = e_mail_formatter_parse_html_mnemonics (label, &access_key);

	g_string_append_printf (
		buffer,
		"<tr id=\"table_row_%s\" hidden=\"\"><td colspan=\"2\">"
		"<input type=\"checkbox\" name=\"%s\" id=\"%s\" value=\"%s\"%s>"
		"<label for=\"%s\" accesskey=\"%s\">%s</label>"
		"</td></tr>\n",
		name, name, name, name,
		checked ? " checked" : "",
		name, access_key ? access_key : "", html_label);

	g_free (html_label);

	g_free (access_key);
}

static void
append_text_table_row (GString *buffer,
                       const gchar *id,
                       const gchar *label,
                       const gchar *value)
{
	gchar *tmp = NULL;

	value = htmlize_text (id, value, &tmp);

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

	g_free (tmp);
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
	EWebView *web_view;
	const gchar *icon_name;
	gchar *row_id;

	web_view = itip_view_ref_web_view (view);

	if (!web_view)
		return;

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
			break;
	}

	row_id = g_strdup_printf ("%s_row_%d", table_id, item->id);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
		"EvoItip.AppendInfoRow(%s, %s, %s, %s, %s);",
		view->priv->part_id,
		table_id,
		row_id,
		icon_name,
		item->message);

	g_object_unref (web_view);
	g_free (row_id);

	d (printf ("Added row %s_row_%d ('%s')\n", table_id, item->id, item->message));
}

static void
remove_info_item_row (ItipView *view,
                      const gchar *table_id,
                      guint id)
{
	EWebView *web_view;
	gchar *row_id;

	web_view = itip_view_ref_web_view (view);

	if (!web_view)
		return;

	row_id = g_strdup_printf ("%s_row_%d", table_id, id);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
		"EvoItip.RemoveInfoRow(%s, %s);",
		view->priv->part_id,
		row_id);

	g_object_unref (web_view);
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
	EWebView *web_view;
	GList *list, *link;
	GString *script;
	const gchar *extension_name;

	d (printf ("Assigning a new source list!\n"));

	web_view = itip_view_ref_web_view (view);

	if (!web_view)
		return;

	registry = view->priv->registry;
	extension_name = itip_view_get_extension_name (view);

	if (!extension_name) {
		g_object_unref (web_view);
		return;
	}

	script = g_string_sized_new (1024);

	e_web_view_jsc_printf_script_gstring (script,
		"EvoItip.RemoveChildNodes(%s, %s);",
		view->priv->part_id,
		SELECT_ESOURCE);

	list = e_source_registry_list_enabled (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESource *parent;
		const gchar *uid;

		uid = e_source_get_parent (source);
		parent = uid ? e_source_registry_ref_source (registry, uid) : NULL;

		e_web_view_jsc_printf_script_gstring (script,
			"EvoItip.AddToSourceList(%s, %s, %s, %s, %s, %x);",
			view->priv->part_id,
			parent ? e_source_get_uid (parent) : "",
			parent ? e_source_get_display_name (parent) : "",
			e_source_get_uid (source),
			e_source_get_display_name (source),
			e_source_get_writable (source));

		g_clear_object (&parent);
	}

	e_web_view_jsc_run_script_take (WEBKIT_WEB_VIEW (web_view), g_string_free (script, FALSE),
		e_web_view_get_cancellable (web_view));

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
	g_object_unref (web_view);

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

	g_clear_object (&priv->client_cache);
	g_clear_object (&priv->registry);
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
	g_free (priv->url);
	g_free (priv->start_tm);
	g_free (priv->start_label);
	g_free (priv->end_tm);
	g_free (priv->end_label);
	g_free (priv->description);
	g_free (priv->error);
	g_free (priv->part_id);
	g_free (priv->selected_source_uid);

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
	g_free (priv->state_rsvp_comment);

	g_clear_object (&priv->folder);
	g_clear_object (&priv->message);
	g_clear_object (&priv->itip_mime_part);
	g_clear_object (&priv->top_level);
	g_clear_object (&priv->main_comp);

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
	append_text_table_row (buffer, TABLE_ROW_URL, _("URL:"), NULL);
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
	append_checkbox_table_row (buffer, CHECKBOX_RSVP, _("Send reply to sender"), TRUE);

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
	append_checkbox_table_row (buffer, CHECKBOX_UPDATE, _("Send _updates to attendees"), TRUE);

        /* The recurrence check button */
	append_checkbox_table_row (buffer, CHECKBOX_RECUR, _("_Apply to all instances"), FALSE);
	append_checkbox_table_row (buffer, CHECKBOX_FREE_TIME, _("Show time as _free"), FALSE);
	append_checkbox_table_row (buffer, CHECKBOX_KEEP_ALARM, _("_Preserve my reminder"), FALSE);
	append_checkbox_table_row (buffer, CHECKBOX_INHERIT_ALARM, _("_Inherit reminder"), TRUE);

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
		buffer, TABLE_ROW_URL,
		_("URL:"), view->priv->url);
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
itip_view_new (const gchar *part_id,
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
	view->priv->part_id = g_strdup (part_id);
	view->priv->itip_part_ptr = itip_part_ptr;
	view->priv->folder = folder ? g_object_ref (folder) : NULL;
	view->priv->message_uid = g_strdup (message_uid);
	view->priv->message = message ? g_object_ref (message) : NULL;
	view->priv->itip_mime_part = g_object_ref (itip_mime_part);
	view->priv->vcalendar = g_strdup (vcalendar);
	view->priv->cancellable = g_object_ref (cancellable);

	return view;
}

void
itip_view_set_mode (ItipView *view,
                    ItipViewMode mode)
{
	EWebView *web_view;
	g_return_if_fail (ITIP_IS_VIEW (view));

	view->priv->mode = mode;

	set_sender_text (view);

	web_view = itip_view_ref_web_view (view);

	if (!web_view)
		return;

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
		"EvoItip.HideButtons(%s, %s);",
		view->priv->part_id,
		TABLE_ROW_BUTTONS);

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

	g_object_unref (web_view);
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
	EWebView *web_view;
	const gchar *header;
	gchar *access_key, *html_label;

	g_return_if_fail (ITIP_IS_VIEW (view));

	view->priv->type = type;
	web_view = itip_view_ref_web_view (view);

	if (!web_view)
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
		g_object_unref (web_view);
		return;
	}

	html_label = e_mail_formatter_parse_html_mnemonics (header, &access_key);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
		"EvoItip.SetElementAccessKey(%s, %s, %s);",
		view->priv->part_id,
		TABLE_ROW_ESCB_LABEL,
		access_key);

	set_inner_html (view, TABLE_ROW_ESCB_LABEL, html_label);

	g_object_unref (web_view);
	g_free (html_label);
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

	g_free (view->priv->summary);

	view->priv->summary = summary ? g_strstrip (e_utf8_ensure_valid (summary)) : NULL;

	set_area_text (view, TABLE_ROW_SUMMARY, view->priv->summary, FALSE);
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

	g_free (view->priv->location);

	view->priv->location = location ? g_strstrip (e_utf8_ensure_valid (location)) : NULL;

	set_area_text (view, TABLE_ROW_LOCATION, view->priv->location, FALSE);
}

const gchar *
itip_view_get_location (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->location;
}

void
itip_view_set_url (ItipView *view,
		   const gchar *url)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->url == url)
		return;

	g_free (view->priv->url);

	view->priv->url = url ? g_strstrip (e_utf8_ensure_valid (url)) : NULL;

	set_area_text (view, TABLE_ROW_URL, view->priv->url, FALSE);
}

const gchar *
itip_view_get_url (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->url;
}

void
itip_view_set_status (ItipView *view,
                      const gchar *status)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	g_free (view->priv->status);

	view->priv->status = status ? g_strstrip (e_utf8_ensure_valid (status)) : NULL;

	set_area_text (view, TABLE_ROW_STATUS, view->priv->status, FALSE);
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

	g_free (view->priv->comment);

	view->priv->comment = comment ? g_strstrip (e_utf8_ensure_valid (comment)) : NULL;

	set_area_text (view, TABLE_ROW_COMMENT, view->priv->comment, TRUE);
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
	ICalProperty *prop;
	ICalComponent *icomp;
	gint num_attendees;
	const gchar *top_comment;
	GString *new_comment = NULL;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->comp)
		return;

	icomp = e_cal_component_get_icalcomponent (view->priv->comp);
	if (!icomp)
		return;

	num_attendees = i_cal_component_count_properties (icomp, I_CAL_ATTENDEE_PROPERTY);
	if (num_attendees <= 0)
		return;

	top_comment = i_cal_component_get_comment (icomp);

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
		gchar *guests_str = NULL;
		guint32 num_guests = 0;
		const gchar *value;
		gchar *prop_value;

		prop_value = cal_comp_util_dup_parameter_xvalue (prop, "X-NUM-GUESTS");
		if (prop_value && *prop_value)
			num_guests = atoi (prop_value);
		g_free (prop_value);

		prop_value = cal_comp_util_dup_parameter_xvalue (prop, "X-RESPONSE-COMMENT");
		value = prop_value;

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
			const gchar *email = i_cal_property_get_attendee (prop);
			const gchar *cn = NULL;
			ICalParameter *cnparam;

			cnparam = i_cal_property_get_first_parameter (prop, I_CAL_CN_PARAMETER);
			if (cnparam) {
				cn = i_cal_parameter_get_cn (cnparam);
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

			g_clear_object (&cnparam);
		}

		g_free (prop_value);
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
	EWebView *web_view;

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

	web_view = itip_view_ref_web_view (view);

	if (!web_view)
		return;

	e_web_view_jsc_set_element_disabled (WEBKIT_WEB_VIEW (web_view),
		view->priv->part_id, SELECT_ESOURCE, FALSE,
		e_web_view_get_cancellable (web_view));

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
		"EvoItip.SetSelectSelected(%s, %s, %s);",
		view->priv->part_id,
		SELECT_ESOURCE,
		e_source_get_uid (source));

	itip_set_selected_source_uid (view, e_source_get_uid (source));

	source_changed_cb (view);
	g_object_unref (web_view);
}

ESource *
itip_view_ref_source (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	if (!view->priv->selected_source_uid || !*view->priv->selected_source_uid)
		return NULL;

	return e_source_registry_ref_source (view->priv->registry, view->priv->selected_source_uid);
}

void
itip_view_set_rsvp (ItipView *view,
                    gboolean rsvp)
{
	EWebView *web_view;

	web_view = itip_view_ref_web_view (view);

	if (!web_view)
		return;

	input_set_checked (view, CHECKBOX_RSVP, rsvp);

	e_web_view_jsc_set_element_disabled (WEBKIT_WEB_VIEW (web_view),
		view->priv->part_id, TEXTAREA_RSVP_COMMENT, rsvp,
		e_web_view_get_cancellable (web_view));

	g_object_unref (web_view);
}

gboolean
itip_view_get_rsvp (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return view->priv->state_rsvp_check;
}

void
itip_view_set_show_rsvp_check (ItipView *view,
                               gboolean show)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	show_checkbox (view, CHECKBOX_RSVP, show, FALSE);
	hide_element (view, TABLE_ROW_RSVP_COMMENT, !show);
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

	return view->priv->state_update_check;
}

void
itip_view_set_show_update_check (ItipView *view,
                                 gboolean show)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	show_checkbox (view, CHECKBOX_UPDATE, show, FALSE);
}

void
itip_view_set_rsvp_comment (ItipView *view,
                            const gchar *comment)
{
	EWebView *web_view;

	web_view = itip_view_ref_web_view (view);

	if (!web_view)
		return;

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
		"EvoItip.SetAreaText(%s, %s, %s);",
		view->priv->part_id,
		TEXTAREA_RSVP_COMMENT,
		comment);

	g_object_unref (web_view);
}

const gchar *
itip_view_get_rsvp_comment (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->state_rsvp_comment;
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
	EWebView *web_view;

	g_return_if_fail (ITIP_IS_VIEW (view));

	d (printf ("Settings buttons %s\n", sensitive ? "sensitive" : "insensitive"));

	view->priv->buttons_sensitive = sensitive;

	web_view = itip_view_ref_web_view (view);

	if (web_view) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
			"EvoItip.SetButtonsDisabled(%s, %x);",
			view->priv->part_id,
			!sensitive);
		g_object_unref (web_view);
	}
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

	return view->priv->state_recur_check;
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

	return view->priv->state_free_time_check;
}

void
itip_view_set_show_keep_alarm_check (ItipView *view,
                                     gboolean show)
{
	g_return_if_fail (ITIP_IS_VIEW (view));

	show_checkbox (view, CHECKBOX_KEEP_ALARM, show, TRUE);

	if (show) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.plugin.itip");

		if (g_settings_get_boolean (settings, "preserve-reminder"))
			input_set_checked (view, CHECKBOX_KEEP_ALARM, TRUE);

		g_object_unref (settings);
	}
}

gboolean
itip_view_get_keep_alarm_check_state (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	return view->priv->state_keep_alarm_check;
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

	return view->priv->state_inherit_alarm_check;
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

	view->priv->error = g_string_free (str, FALSE);

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

static gboolean check_is_instance (ICalComponent *icomp);

static ICalProperty *
find_attendee_if_sentby (ICalComponent *icomp,
                         const gchar *address)
{
	ICalProperty *prop;

	if (!address)
		return NULL;

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
		ICalParameter *param;
		const gchar *attendee_sentby;
		gchar *text;

		param = i_cal_property_get_first_parameter (prop, I_CAL_SENTBY_PARAMETER);
		if (!param)
			continue;

		attendee_sentby = i_cal_parameter_get_sentby (param);

		if (!attendee_sentby) {
			g_object_unref (param);
			continue;
		}

		text = g_strdup (itip_strip_mailto (attendee_sentby));
		text = g_strstrip (text);
		if (text && !g_ascii_strcasecmp (address, text)) {
			g_object_unref (param);
			g_free (text);
			break;
		}
		g_object_unref (param);
		g_free (text);
	}

	return prop;
}

static void
find_to_address (ItipView *view,
		 ICalComponent *icomp,
		 ICalParameterPartstat *partstat)
{
	ESourceRegistry *registry;
	ESourceMailIdentity *extension;
	GList *list, *link;
	const gchar *extension_name;

	registry = view->priv->registry;
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;

	if (view->priv->to_address != NULL)
		return;

	/* Look through the list of attendees to find the user's address */
	list = e_source_registry_list_enabled (registry, extension_name);

	if (view->priv->message && view->priv->folder) {
		ESource *source;

		source = em_utils_guess_mail_identity (
			registry, view->priv->message,
			view->priv->folder, view->priv->message_uid);

		if (source) {
			if (g_list_find (list, source)) {
				list = g_list_remove (list, source);
				g_object_unref (source);
			}

			/* Try the account where the message is located first */
			list = g_list_prepend (list, source);
		}
	}

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ICalProperty *prop;
		ICalParameter *param;
		gchar *address;
		gchar *text;

		extension = e_source_get_extension (source, extension_name);
		address = e_source_mail_identity_dup_address (extension);

		prop = itip_utils_find_attendee_property (icomp, address);
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
						prop = itip_utils_find_attendee_property (icomp, alias_address);
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

		param = i_cal_property_get_first_parameter (prop, I_CAL_CN_PARAMETER);
		if (param)
			view->priv->to_name = g_strdup (i_cal_parameter_get_cn (param));
		g_clear_object (&param);

		text = i_cal_property_get_value_as_string (prop);

		view->priv->to_address = g_strdup (itip_strip_mailto (text));
		g_free (text);
		g_strstrip (view->priv->to_address);

		view->priv->my_address = address;

		param = i_cal_property_get_first_parameter (prop, I_CAL_RSVP_PARAMETER);
		if (param != NULL &&
		    i_cal_parameter_get_rsvp (param) == I_CAL_RSVP_FALSE)
			view->priv->no_reply_wanted = TRUE;
		g_clear_object (&param);

		if (partstat) {
			param = i_cal_property_get_first_parameter (prop, I_CAL_PARTSTAT_PARAMETER);
			*partstat = param ? i_cal_parameter_get_partstat (param) : I_CAL_PARTSTAT_NEEDSACTION;
			g_clear_object (&param);
		}

		g_object_unref (prop);
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
	 * Note: This functionality could have been (easily) implemented
	 * in the previous loop, but it would hurt the performance for all
	 * providers in general. Hence, we choose to iterate through the
	 * accounts list again.
 	 */

	list = e_source_registry_list_enabled (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ICalProperty *prop;
		ICalParameter *param;
		gchar *address;
		gchar *text;

		extension = e_source_get_extension (source, extension_name);
		address = e_source_mail_identity_dup_address (extension);

		prop = find_attendee_if_sentby (icomp, address);
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
						prop = find_attendee_if_sentby (icomp, alias_address);
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

		param = i_cal_property_get_first_parameter (prop, I_CAL_CN_PARAMETER);
		if (param)
			view->priv->to_name = g_strdup (i_cal_parameter_get_cn (param));
		g_clear_object (&param);

		text = i_cal_property_get_value_as_string (prop);

		view->priv->to_address = g_strdup (itip_strip_mailto (text));
		g_free (text);
		g_strstrip (view->priv->to_address);

		view->priv->my_address = address;

		param = i_cal_property_get_first_parameter (prop, I_CAL_RSVP_PARAMETER);
		if (param != NULL &&
		    i_cal_parameter_get_rsvp (param) == I_CAL_RSVP_FALSE)
			view->priv->no_reply_wanted = TRUE;
		g_clear_object (&param);

		if (partstat) {
			param = i_cal_property_get_first_parameter (prop, I_CAL_PARTSTAT_PARAMETER);
			*partstat = param ? i_cal_parameter_get_partstat (param) : I_CAL_PARTSTAT_NEEDSACTION;
			g_clear_object (&param);
		}

		g_object_unref (prop);
		break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (view->priv->to_address)
		return;

	/* Guess based on the message location only as the last resort, because
	   the attendee in the list of attendees is required. */
	if (view->priv->message && view->priv->folder) {
		ESource *source;

		source = em_utils_guess_mail_identity (
			registry, view->priv->message,
			view->priv->folder, view->priv->message_uid);

		if (source) {
			extension = e_source_get_extension (source, extension_name);

			view->priv->to_address = e_source_mail_identity_dup_address (extension);

			g_object_unref (source);
		}
	}
}

static void
find_from_address (ItipView *view,
		   ICalComponent *icomp)
{
	ESourceRegistry *registry;
	GList *list, *link;
	ICalProperty *prop;
	ICalParameter *param;
	gchar *organizer;
	const gchar *extension_name;
	const gchar *organizer_sentby;
	gchar *organizer_clean = NULL;
	gchar *organizer_sentby_clean = NULL;

	registry = view->priv->registry;

	prop = i_cal_component_get_first_property (icomp, I_CAL_ORGANIZER_PROPERTY);

	if (!prop)
		return;

	organizer = i_cal_property_get_value_as_string (prop);
	if (organizer) {
		organizer_clean = g_strdup (itip_strip_mailto (organizer));
		organizer_clean = g_strstrip (organizer_clean);
		g_free (organizer);
	}

	param = i_cal_property_get_first_parameter (prop, I_CAL_SENTBY_PARAMETER);
	if (param) {
		organizer_sentby = i_cal_parameter_get_sentby (param);
		if (organizer_sentby) {
			organizer_sentby_clean = g_strdup (itip_strip_mailto (organizer_sentby));
			organizer_sentby_clean = g_strstrip (organizer_sentby_clean);
		}
		g_clear_object (&param);
	}

	if (!(organizer_sentby_clean || organizer_clean)) {
		g_object_unref (prop);
		return;
	}

	view->priv->from_address = g_strdup (organizer_clean);

	param = i_cal_property_get_first_parameter (prop, I_CAL_CN_PARAMETER);
	if (param)
		view->priv->from_name = g_strdup (i_cal_parameter_get_cn (param));
	g_clear_object (&param);

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
	g_object_unref (prop);
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
		ECalComponentText *text;
		gchar *string;
		GSList *lst;

		text = e_cal_component_get_summary (real_comp);
		e_cal_component_set_summary (comp, text);
		e_cal_component_text_free (text);

		string = e_cal_component_get_location (real_comp);
		e_cal_component_set_location (comp, string);
		g_free (string);

		lst = e_cal_component_get_descriptions (real_comp);
		e_cal_component_set_descriptions (comp, lst);
		g_slist_free_full (lst, e_cal_component_text_free);

		g_object_unref (real_comp);
	} else {
		ECalComponentText *text;

		text = e_cal_component_text_new (_("Unknown"), NULL);
		e_cal_component_set_summary (comp, text);
		e_cal_component_text_free (text);
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

	received_attendees = e_cal_component_get_attendees (received_comp);
	saved_attendees = e_cal_component_get_attendees (saved_comp);

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

			if (e_cal_component_attendee_get_value (rattendee) &&
			    e_cal_component_attendee_get_value (sattendee) &&
			    g_ascii_strcasecmp (e_cal_component_attendee_get_value (rattendee), e_cal_component_attendee_get_value (sattendee)) == 0) {
				same = e_cal_component_attendee_get_partstat (rattendee) == e_cal_component_attendee_get_partstat (sattendee);
				break;
			}
		}

		/* received attendee was not found in the saved attendees */
		if (!siter)
			same = FALSE;
	}

	g_slist_free_full (received_attendees, e_cal_component_attendee_free);
	g_slist_free_full (saved_attendees, e_cal_component_attendee_free);
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
		ICalComponent *icomp;
		gboolean show_recur_check;

		icomp = e_cal_component_get_icalcomponent (view->priv->comp);

		show_recur_check = check_is_instance (icomp);
		itip_view_set_show_recur_check (view, show_recur_check);
	}

	if (view->priv->type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS) {
		gboolean needs_decline;

		needs_decline = e_client_check_capability (
			client,
			E_CAL_STATIC_CAPABILITY_HAS_UNACCEPTED_MEETING);
		itip_view_set_needs_decline (view, needs_decline);
		itip_view_set_mode (view, ITIP_VIEW_MODE_PUBLISH);
	}

	view->priv->current_client = E_CAL_CLIENT (g_object_ref (client));

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
	gint sequence;
	ECalComponentId *mail_id, *real_id;

	if (!real_comp || !view->priv->comp ||
	    e_cal_component_get_vtype (view->priv->comp) != E_CAL_COMPONENT_EVENT)
		return FALSE;

	sequence = e_cal_component_get_sequence (view->priv->comp);
	if (sequence < 0)
		return FALSE;

	mail_id = e_cal_component_get_id (view->priv->comp);
	if (!mail_id)
		return FALSE;

	real_id = e_cal_component_get_id (real_comp);
	if (real_id && e_cal_component_id_equal (real_id, mail_id)) {
		gint real_sequence;

		real_sequence = e_cal_component_get_sequence (real_comp);
		if (real_sequence >= 0)
			is_older = sequence < real_sequence;
	}

	e_cal_component_id_free (real_id);
	e_cal_component_id_free (mail_id);

	return is_older;
}

static gchar *
itip_view_dup_source_full_display_name (ItipView *view,
					ESource *source)
{
	ESourceRegistry *registry;
	gchar *display_name;

	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	if (!source)
		return NULL;

	registry = e_client_cache_ref_registry (view->priv->client_cache);
	display_name = e_util_get_source_full_name (registry, source);
	g_clear_object (&registry);

	return display_name;
}

static void
find_cal_update_ui (FormatItipFindData *fd,
                    ECalClient *cal_client)
{
	ItipView *view;
	ESource *source;
	gchar *source_display_name;

	g_return_if_fail (fd != NULL);

	view = fd->view;

	/* UI part gone */
	if (g_cancellable_is_cancelled (fd->cancellable))
		return;

	source = cal_client ? e_client_get_source (E_CLIENT (cal_client)) : NULL;
	source_display_name = itip_view_dup_source_full_display_name (view, source);

	if (cal_client && g_hash_table_lookup (fd->conflicts, cal_client)) {
		GSList *icomps = g_hash_table_lookup (fd->conflicts, cal_client);
		guint ncomps;

		ncomps = g_slist_length (icomps);
		if (ncomps == 1 && icomps->data) {
			ICalComponent *icomp = icomps->data;

			switch (e_cal_client_get_source_type (cal_client)) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			default:
				itip_view_add_upper_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
					_("An appointment “%s” in the calendar “%s” conflicts with this meeting"),
					i_cal_component_get_summary (icomp),
					source_display_name);
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				itip_view_add_upper_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
					_("A task “%s” in the task list “%s” conflicts with this task"),
					i_cal_component_get_summary (icomp),
					source_display_name);
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				itip_view_add_upper_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
					_("A memo “%s” in the memo list “%s” conflicts with this memo"),
					i_cal_component_get_summary (icomp),
					source_display_name);
				break;
			}
		} else {
			switch (e_cal_client_get_source_type (cal_client)) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			default:
				itip_view_add_upper_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
					ngettext ("The calendar “%s” contains an appointment which conflicts with this meeting",
						  "The calendar “%s” contains %d appointments which conflict with this meeting",
						  ncomps),
					source_display_name,
					ncomps);
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				itip_view_add_upper_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
					ngettext ("The task list “%s” contains a task which conflicts with this task",
						  "The task list “%s” contains %d tasks which conflict with this task",
						  ncomps),
					source_display_name,
					ncomps);
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				itip_view_add_upper_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
					ngettext ("The memo list “%s” contains a memo which conflicts with this memo",
						  "The memo list “%s” contains %d memos which conflict with this memo",
						  ncomps),
					source_display_name,
					ncomps);
				break;
			}
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
		if (view->priv->method == I_CAL_METHOD_REPLY || view->priv->method == I_CAL_METHOD_REFRESH)
			adjust_item (view, view->priv->comp);

		/* We clear everything because we don't really care
		 * about any other info/warnings now we found an
		 * existing versions */
		itip_view_clear_lower_info_items (view);
		view->priv->progress_info_id = 0;

		/* FIXME Check read only state of calendar? */
		switch (e_cal_client_get_source_type (cal_client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		default:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Found the appointment in the calendar “%s”"), source_display_name);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Found the task in the task list “%s”"), source_display_name);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Found the memo in the memo list “%s”"), source_display_name);
			break;
		}

		g_cancellable_cancel (fd->cancellable);

		if (view->priv->method == I_CAL_METHOD_REQUEST &&
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
			    (view->priv->method == I_CAL_METHOD_PUBLISH || view->priv->method == I_CAL_METHOD_REQUEST) &&
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
					g_clear_pointer (&source_display_name, g_free);
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
			ICalComponent *icomp = e_cal_component_get_icalcomponent (view->priv->comp);

			if (check_is_instance (icomp))
				itip_view_set_show_recur_check (view, TRUE);
			else
				itip_view_set_show_recur_check (view, FALSE);
		}

		if (view->priv->type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS) {
			/* TODO The static capability should be made generic to convey that the calendar contains unaccepted items */
			if (e_client_check_capability (E_CLIENT (view->priv->current_client), E_CAL_STATIC_CAPABILITY_HAS_UNACCEPTED_MEETING))
				itip_view_set_needs_decline (view, TRUE);
			else
				itip_view_set_needs_decline (view, FALSE);

			itip_view_set_mode (view, ITIP_VIEW_MODE_PUBLISH);
		}
	}

	g_free (source_display_name);
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
		    (view->priv->method == I_CAL_METHOD_PUBLISH || view->priv->method == I_CAL_METHOD_REQUEST) &&
		    view->priv->has_organizer) {
			rsvp_enabled = TRUE;
		}
		itip_view_set_show_rsvp_check (view, rsvp_enabled);

		/* default is chosen in extract_itip_data() based on content of the VEVENT */
		itip_view_set_rsvp (view, !view->priv->no_reply_wanted);

		if ((view->priv->method == I_CAL_METHOD_PUBLISH || view->priv->method == I_CAL_METHOD_REQUEST)
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
		g_free (fd->sexp);
		g_slice_free (FormatItipFindData, fd);
	}
}

static gboolean
comp_has_subcomponent (ICalComponent *icomp,
		       ICalComponentKind kind)
{
	ICalComponent *subcomp;

	subcomp = i_cal_component_get_first_component (icomp, kind);
	if (subcomp) {
		g_object_unref (subcomp);
		return TRUE;
	}

	return FALSE;
}

static void
get_object_without_rid_ready_cb (GObject *source_object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	ECalClient *cal_client = E_CAL_CLIENT (source_object);
	FormatItipFindData *fd = user_data;
	ICalComponent *icomp = NULL;
	GError *error = NULL;

	e_cal_client_get_object_finish (cal_client, result, &icomp, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
	    g_cancellable_is_cancelled (fd->cancellable)) {
		g_clear_error (&error);
		find_cal_update_ui (fd, cal_client);
		decrease_find_data (fd);
		return;
	}

	g_clear_error (&error);

	if (icomp) {
		ECalComponent *comp;

		fd->view->priv->current_client = cal_client;
		fd->keep_alarm_check = (fd->view->priv->method == I_CAL_METHOD_PUBLISH || fd->view->priv->method == I_CAL_METHOD_REQUEST) &&
			(comp_has_subcomponent (icomp, I_CAL_VALARM_COMPONENT) ||
			comp_has_subcomponent (icomp, I_CAL_XAUDIOALARM_COMPONENT) ||
			comp_has_subcomponent (icomp, I_CAL_XDISPLAYALARM_COMPONENT) ||
			comp_has_subcomponent (icomp, I_CAL_XPROCEDUREALARM_COMPONENT) ||
			comp_has_subcomponent (icomp, I_CAL_XEMAILALARM_COMPONENT));

		comp = e_cal_component_new_from_icalcomponent (icomp);
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
	ICalComponent *icomp = NULL;
	GError *error = NULL;

	e_cal_client_get_object_finish (cal_client, result, &icomp, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
	    g_cancellable_is_cancelled (fd->cancellable)) {
		g_clear_error (&error);
		find_cal_update_ui (fd, cal_client);
		decrease_find_data (fd);
		return;
	}

	g_clear_error (&error);

	if (icomp) {
		ECalComponent *comp;

		fd->view->priv->current_client = cal_client;
		fd->keep_alarm_check = (fd->view->priv->method == I_CAL_METHOD_PUBLISH || fd->view->priv->method == I_CAL_METHOD_REQUEST) &&
			(comp_has_subcomponent (icomp, I_CAL_VALARM_COMPONENT) ||
			comp_has_subcomponent (icomp, I_CAL_XAUDIOALARM_COMPONENT) ||
			comp_has_subcomponent (icomp, I_CAL_XDISPLAYALARM_COMPONENT) ||
			comp_has_subcomponent (icomp, I_CAL_XPROCEDUREALARM_COMPONENT) ||
			comp_has_subcomponent (icomp, I_CAL_XEMAILALARM_COMPONENT));

		comp = e_cal_component_new_from_icalcomponent (icomp);
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
		GSList *link = objects;

		while (link) {
			ICalComponent *icomp = link->data;
			ICalProperty *prop;

			link = g_slist_next (link);

			prop = icomp ? i_cal_component_get_first_property (icomp, I_CAL_TRANSP_PROPERTY) : NULL;

			/* Ignore non-opaque components in the conflict search */
			if (prop && i_cal_property_get_transp (prop) != I_CAL_TRANSP_OPAQUE && i_cal_property_get_transp (prop) != I_CAL_TRANSP_NONE) {
				objects = g_slist_remove (objects, icomp);
				g_object_unref (icomp);
			}

			g_clear_object (&prop);
		}

		if (objects)
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
	const gchar *searching_text = NULL;
	const gchar *extension_name;
	const gchar *store_uid;

	g_return_if_fail (ITIP_IS_VIEW (view));
	g_return_if_fail (view->priv->folder != NULL);

	switch (view->priv->type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			searching_text = _("Searching for an existing version of this appointment");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			searching_text = _("Searching for an existing version of this task");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			searching_text = _("Searching for an existing version of this memo");
			break;
		default:
			g_return_if_reached ();
	}

	list = e_source_registry_list_enabled (
		view->priv->registry, extension_name);

	uid = e_cal_component_get_uid (comp);
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
			_("Opening the calendar. Please wait…"));
	} else {
		link = list;
		view->priv->progress_info_id = itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS,
			searching_text);
	}

	for (; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);

		if (!fd) {
			gchar *start = NULL, *end = NULL;

			fd = g_slice_new0 (FormatItipFindData);
			fd->view = g_object_ref (view);
			fd->itip_cancellable = g_object_ref (view->priv->cancellable);
			fd->cancellable = g_cancellable_new ();
			fd->cancelled_id = g_cancellable_connect (
				fd->itip_cancellable,
				G_CALLBACK (itip_cancellable_cancelled), fd->cancellable, NULL);
			fd->conflicts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) e_util_free_nullable_object_slist);
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
					i_cal_component_get_uid (view->priv->ical_comp));
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
		ICalComponent *icomp;
		ICalProperty *prop;
		const gchar *attendee;
		const gchar *comment;
		GSList *l, *list = NULL;
		gboolean found;

		comp = e_cal_component_clone (view->priv->comp);
		if (comp == NULL)
			return;

		if (view->priv->to_address == NULL)
			find_to_address (view, view->priv->ical_comp, NULL);
		g_return_if_fail (view->priv->to_address != NULL);

		icomp = e_cal_component_get_icalcomponent (comp);

		/* Remove all attendees except the one we are responding as */
		found = FALSE;
		for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
		     prop;
		     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
			gchar *text;

			attendee = i_cal_property_get_attendee (prop);
			if (!attendee)
				continue;

			text = g_strdup (itip_strip_mailto (attendee));
			text = g_strstrip (text);

			/* We do this to ensure there is at most one
			 * attendee in the response */
			if (found || g_ascii_strcasecmp (view->priv->to_address, text))
				list = g_slist_prepend (list, g_object_ref (prop));
			else if (!g_ascii_strcasecmp (view->priv->to_address, text))
				found = TRUE;
			g_free (text);
		}

		for (l = list; l; l = l->next) {
			prop = l->data;
			i_cal_component_remove_property (icomp, prop);
		}
		g_slist_free_full (list, g_object_unref);

		/* Add a comment if there user set one */
		comment = itip_view_get_rsvp_comment (view);
		if (comment) {
			GSList comments;
			ECalComponentText *text;

			text = e_cal_component_text_new (comment, NULL);

			comments.data = text;
			comments.next = NULL;

			e_cal_component_set_comments (comp, &comments);

			e_cal_component_text_free (text);
		}

		if (itip_send_comp_sync (
				view->priv->registry,
				I_CAL_METHOD_REPLY,
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
	gchar *source_display_name;
	GError *error = NULL;

	e_cal_client_receive_objects_finish (client, result, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return;
	}

	source_display_name = itip_view_dup_source_full_display_name (view, source);

	if (error != NULL) {
		update_item_progress_info (view, NULL);
		switch (e_cal_client_get_source_type (client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		default:
			view->priv->update_item_error_info_id =
				itip_view_add_lower_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
					_("Unable to send item to calendar “%s”. %s"),
					source_display_name,
					error->message);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			view->priv->update_item_error_info_id =
				itip_view_add_lower_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
					_("Unable to send item to task list “%s”. %s"),
					source_display_name,
					error->message);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			view->priv->update_item_error_info_id =
				itip_view_add_lower_info_item_printf (
					view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
					_("Unable to send item to memo list “%s”. %s"),
					source_display_name,
					error->message);
			break;
		}
		g_error_free (error);
		g_free (source_display_name);
		return;
	}

	itip_view_set_extension_name (view, NULL);

	itip_view_clear_lower_info_items (view);

	switch (view->priv->update_item_response) {
	case ITIP_VIEW_RESPONSE_ACCEPT:
		switch (e_cal_client_get_source_type (client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		default:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to calendar “%s” as accepted"), source_display_name);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to task list “%s” as accepted"), source_display_name);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to memo list “%s” as accepted"), source_display_name);
			break;
		}
		break;
	case ITIP_VIEW_RESPONSE_TENTATIVE:
		switch (e_cal_client_get_source_type (client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		default:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to calendar “%s” as tentative"), source_display_name);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to task list “%s” as tentative"), source_display_name);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to memo list “%s” as tentative"), source_display_name);
			break;
		}
		break;
	case ITIP_VIEW_RESPONSE_DECLINE:
		switch (e_cal_client_get_source_type (client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		default:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to calendar “%s” as declined"), source_display_name);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to task list “%s” as declined"), source_display_name);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to memo list “%s” as declined"), source_display_name);
			break;
		}
		break;
	case ITIP_VIEW_RESPONSE_CANCEL:
		switch (e_cal_client_get_source_type (client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		default:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to calendar “%s” as cancelled"), source_display_name);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to task list “%s” as cancelled"), source_display_name);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			itip_view_add_lower_info_item_printf (
				view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
				_("Sent to memo list “%s” as cancelled"), source_display_name);
			break;
		}
		break;
	default:
		g_warn_if_reached ();
		break;
	}

	finish_message_delete_with_rsvp (view, client);
	g_free (source_display_name);
}

static void
claim_progress_saving_changes (ItipView *view)
{
	switch (e_cal_client_get_source_type (view->priv->current_client)) {
	case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
	default:
		update_item_progress_info (view, _("Saving changes to the calendar. Please wait…"));
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
		update_item_progress_info (view, _("Saving changes to the task list. Please wait…"));
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
		update_item_progress_info (view, _("Saving changes to the memo list. Please wait…"));
		break;
	}
}

static void
remove_alarms_in_component (ICalComponent *clone)
{
	ICalComponent *alarm_comp;
	ICalCompIter *iter;

	iter = i_cal_component_begin_component (clone, I_CAL_VALARM_COMPONENT);
	alarm_comp = i_cal_comp_iter_deref (iter);
	while (alarm_comp) {
		ICalComponent *next_subcomp;

		next_subcomp = i_cal_comp_iter_next (iter);

		i_cal_component_remove_component (clone, alarm_comp);
		g_object_unref (alarm_comp);
		alarm_comp = next_subcomp;
	}

	g_object_unref (iter);
}

static void
update_item (ItipView *view,
             ItipViewResponse response)
{
	ICalComponent *toplevel_clone, *clone;
	gboolean remove_alarms;
	ECalComponent *clone_comp;

	claim_progress_saving_changes (view);
	itip_utils_update_cdo_replytime	(view->priv->ical_comp);

	toplevel_clone = i_cal_component_clone (view->priv->top_level);
	clone = i_cal_component_clone (view->priv->ical_comp);
	i_cal_component_add_component (toplevel_clone, clone);
	i_cal_component_set_method (toplevel_clone, view->priv->method);

	remove_alarms = !itip_view_get_inherit_alarm_check_state (view);

	if (remove_alarms)
		remove_alarms_in_component (clone);

	if (view->priv->with_detached_instances) {
		ICalComponent *icomp;
		ICalComponentKind use_kind = i_cal_component_isa (view->priv->ical_comp);

		for (icomp = i_cal_component_get_first_component (view->priv->main_comp, use_kind);
		     icomp;
		     g_object_unref (icomp), icomp = i_cal_component_get_next_component (view->priv->main_comp, use_kind)) {
			if (i_cal_object_get_native (I_CAL_OBJECT (icomp)) != i_cal_object_get_native (I_CAL_OBJECT (view->priv->ical_comp))) {
				ICalComponent *di_clone = i_cal_component_clone (icomp);

				if (remove_alarms)
					remove_alarms_in_component (di_clone);

				i_cal_component_take_component (toplevel_clone, di_clone);
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

		real_comp = get_real_item (view);
		if (real_comp != NULL) {
			GSList *alarms, *link;

			alarms = e_cal_component_get_alarm_uids (real_comp);

			for (link = alarms; link; link = g_slist_next (link)) {
				ECalComponentAlarm *alarm;

				alarm = e_cal_component_get_alarm (real_comp, link->data);

				if (alarm) {
					ECalComponentAlarm *aclone = e_cal_component_alarm_copy (alarm);

					if (aclone) {
						e_cal_component_add_alarm (clone_comp, aclone);
						e_cal_component_alarm_free (aclone);
					}

					e_cal_component_alarm_free (alarm);
				}
			}

			g_slist_free_full (alarms, g_free);
			g_object_unref (real_comp);
		}
	}

	if (response != ITIP_VIEW_RESPONSE_CANCEL &&
	    response != ITIP_VIEW_RESPONSE_DECLINE) {
		GSList *attachments = NULL, *new_attachments = NULL, *link;
		CamelMimeMessage *msg = view->priv->message;

		attachments = e_cal_component_get_attachments (clone_comp);

		for (link = attachments; link; link = g_slist_next (link)) {
			GSList *parts = NULL, *m;
			const gchar *uri;
			gchar *new_uri;
			CamelMimePart *part;
			ICalAttach *attach = link->data;

			if (!attach)
				continue;

			if (!i_cal_attach_get_is_url (attach)) {
				/* Preserve existing non-URL attachments */
				new_attachments = g_slist_prepend (new_attachments, g_object_ref (attach));
				continue;
			}

			uri = i_cal_attach_get_url (attach);

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
						new_attachments = g_slist_prepend (new_attachments, i_cal_attach_new_from_url (new_uri));
					g_free (new_uri);
				}

				g_slist_free (parts);

			} else if (!g_ascii_strncasecmp (uri, "cid:", 4)) {
				part = camel_mime_message_get_part_by_content_id (msg, uri + 4);
				if (part) {
					new_uri = get_uri_for_part (part);
					if (new_uri != NULL)
						new_attachments = g_slist_prepend (new_attachments, i_cal_attach_new_from_url (new_uri));
					g_free (new_uri);
				}
			} else {
				/* Preserve existing non-cid ones */
				new_attachments = g_slist_prepend (new_attachments, g_object_ref (attach));
			}
		}

		g_slist_free_full (attachments, g_object_unref);

		e_cal_component_set_attachments (clone_comp, new_attachments);

		g_slist_free_full (new_attachments, g_object_unref);
	}

	view->priv->update_item_response = response;

	e_cal_client_receive_objects (
		view->priv->current_client,
		toplevel_clone,
		E_CAL_OPERATION_FLAG_NONE,
		view->priv->cancellable,
		receive_objects_ready_cb,
		view);

 cleanup:
	g_object_unref (clone_comp);
	g_object_unref (toplevel_clone);
}

/* TODO These operations should be available in e-cal-component.c */
static void
set_attendee (ECalComponent *comp,
              const gchar *address)
{
	ICalComponent *icomp;
	gboolean found = FALSE;

	icomp = e_cal_component_get_icalcomponent (comp);
	found = itip_utils_remove_all_but_attendee (icomp, address);

	if (!found) {
		ICalProperty *prop;
		ICalParameter *param;
		gchar *temp = g_strdup_printf ("mailto:%s", address);

		prop = i_cal_property_new_attendee ((const gchar *) temp);

		param = i_cal_parameter_new_partstat (I_CAL_PARTSTAT_NEEDSACTION);
		i_cal_property_take_parameter (prop, param);

		param = i_cal_parameter_new_role (I_CAL_ROLE_REQPARTICIPANT);
		i_cal_property_take_parameter (prop, param);

		param = i_cal_parameter_new_cutype (I_CAL_CUTYPE_INDIVIDUAL);
		i_cal_property_take_parameter (prop, param);

		param = i_cal_parameter_new_rsvp (I_CAL_RSVP_TRUE);
		i_cal_property_take_parameter (prop, param);

		i_cal_component_take_property (icomp, prop);

		g_free (temp);
	}

}

static gboolean
send_comp_to_attendee (ESourceRegistry *registry,
                       ICalPropertyMethod method,
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
		ECalComponentText *text;

		text = e_cal_component_text_new (comment, NULL);

		comments.data = text;
		comments.next = NULL;

		e_cal_component_set_comments (send_comp, &comments);
		e_cal_component_text_free (text);
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
		I_CAL_METHOD_CANCEL, view->priv->comp,
		delegate, view->priv->current_client, comment);
	if (status != 0) {
		send_comp_to_attendee (
			view->priv->registry,
			I_CAL_METHOD_REQUEST, view->priv->comp,
			delegator, view->priv->current_client, comment);
	}
	if (status != 0) {
		itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Sent a cancellation notice to the delegate"));
	} else {
		itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_INFO,
			_("Could not send the cancellation notice to the delegate"));
	}

	g_free (comment);
}

static void
update_x (ECalComponent *view_comp,
          ECalComponent *comp)
{
	ICalComponent *itip_icomp = e_cal_component_get_icalcomponent (view_comp);
	ICalComponent *icomp = e_cal_component_get_icalcomponent (comp);
	ICalProperty *prop;

	for (prop = i_cal_component_get_first_property (itip_icomp, I_CAL_X_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (itip_icomp, I_CAL_X_PROPERTY)) {
		const gchar *name = i_cal_property_get_x_name (prop);

		if (name && !g_ascii_strcasecmp (name, "X-EVOLUTION-IS-REPLY")) {
			ICalProperty *new_prop = i_cal_property_new_x (i_cal_property_get_x (prop));
			i_cal_property_set_x_name (new_prop, "X-EVOLUTION-IS-REPLY");
			i_cal_component_take_property (icomp, new_prop);
		}
	}
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
update_attendee_status_icomp (ItipView *view,
			      ICalComponent *icomp)
{
	ECalComponent *comp;
	GSList *attendees = NULL;
	gboolean is_instance;

	is_instance = e_cal_component_is_instance (view->priv->comp);

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icomp)) {
		g_object_unref (icomp);

		itip_view_add_lower_info_item (
			view, ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
			_("The meeting is invalid and cannot be updated"));
	} else {
		ICalComponent *org_icomp;
		const gchar *delegate;

		org_icomp = e_cal_component_get_icalcomponent (view->priv->comp);

		attendees = e_cal_component_get_attendees (view->priv->comp);
		if (attendees != NULL) {
			ECalComponentAttendee *a = attendees->data;
			ICalProperty *prop, *del_prop = NULL, *delto = NULL;
			EShell *shell = e_shell_get_default ();

			prop = itip_utils_find_attendee_property (icomp, itip_strip_mailto (e_cal_component_attendee_get_value (a)));
			if ((e_cal_component_attendee_get_partstat (a) == I_CAL_PARTSTAT_DELEGATED) &&
			    (del_prop = itip_utils_find_attendee_property (org_icomp, itip_strip_mailto (e_cal_component_attendee_get_delegatedto (a)))) &&
			    !(delto = itip_utils_find_attendee_property (icomp, itip_strip_mailto (e_cal_component_attendee_get_delegatedto (a))))) {
				gint response;
				delegate = i_cal_property_get_attendee (del_prop);
				response = e_alert_run_dialog_for_args (
					e_shell_get_active_window (shell),
					"org.gnome.itip-formatter:add-delegate",
					itip_strip_mailto (e_cal_component_attendee_get_value (a)),
					itip_strip_mailto (delegate), NULL);
				if (response == GTK_RESPONSE_YES) {
					i_cal_component_take_property (icomp, i_cal_property_clone (del_prop));
				} else if (response == GTK_RESPONSE_NO) {
					remove_delegate (view, delegate, itip_strip_mailto (e_cal_component_attendee_get_value (a)), comp);
					g_clear_object (&del_prop);
					g_clear_object (&delto);
					goto cleanup;
				} else {
					g_clear_object (&del_prop);
					g_clear_object (&delto);
					goto cleanup;
				}
			}

			g_clear_object (&del_prop);
			g_clear_object (&delto);

			if (prop == NULL) {
				const gchar *delfrom;
				gint response;

				delfrom = e_cal_component_attendee_get_delegatedfrom (a);

				if (delfrom && *delfrom) {
					response = e_alert_run_dialog_for_args (
						e_shell_get_active_window (shell),
						"org.gnome.itip-formatter:add-delegate",
						itip_strip_mailto (delfrom),
						itip_strip_mailto (e_cal_component_attendee_get_value (a)), NULL);
					if (response == GTK_RESPONSE_YES) {
						/* Already declared in this function */
						ICalProperty *prop = itip_utils_find_attendee_property (icomp, itip_strip_mailto (e_cal_component_attendee_get_value (a)));
						i_cal_component_take_property (icomp, i_cal_property_clone (prop));
					} else if (response == GTK_RESPONSE_NO) {
						remove_delegate (
							view,
							itip_strip_mailto (e_cal_component_attendee_get_value (a)),
							itip_strip_mailto (delfrom),
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
					itip_utils_prepare_attendee_response (
						view->priv->registry, icomp,
						itip_strip_mailto (e_cal_component_attendee_get_value (a)),
						e_cal_component_attendee_get_partstat (a));
				} else {
					goto cleanup;
				}
			} else if (e_cal_component_attendee_get_partstat (a) == I_CAL_PARTSTAT_NONE ||
				   e_cal_component_attendee_get_partstat (a) == I_CAL_PARTSTAT_X) {
				itip_view_add_lower_info_item (
					view, ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
					_("Attendee status could not be updated because the status is invalid"));
				g_clear_object (&prop);
				goto cleanup;
			} else {
				if (e_cal_component_attendee_get_partstat (a) == I_CAL_PARTSTAT_DELEGATED) {
					/* *prop already declared in this function */
					ICalProperty *subprop, *new_prop;

					subprop = itip_utils_find_attendee_property (icomp, itip_strip_mailto (e_cal_component_attendee_get_value (a)));
					i_cal_component_remove_property (icomp, subprop);
					g_clear_object (&subprop);

					new_prop = itip_utils_find_attendee_property (org_icomp, itip_strip_mailto (e_cal_component_attendee_get_value (a)));
					i_cal_component_take_property (icomp, i_cal_property_clone (new_prop));
					g_clear_object (&new_prop);
				} else {
					itip_utils_prepare_attendee_response (
						view->priv->registry, icomp,
						itip_strip_mailto (e_cal_component_attendee_get_value (a)),
						e_cal_component_attendee_get_partstat (a));
				}

				g_clear_object (&prop);
			}
		}
	}

	update_x (view->priv->comp, comp);

	if (itip_view_get_update (view)) {
		e_cal_component_commit_sequence (comp);
		itip_send_comp_sync (
			view->priv->registry,
			I_CAL_METHOD_REQUEST,
			comp, view->priv->current_client,
			NULL, NULL, NULL, TRUE, FALSE, NULL, NULL);
	}

	claim_progress_saving_changes (view);

	e_cal_client_modify_object (
		view->priv->current_client,
		icomp, is_instance ? E_CAL_OBJ_MOD_THIS : E_CAL_OBJ_MOD_ALL,
		E_CAL_OPERATION_FLAG_NONE,
		view->priv->cancellable,
		modify_object_cb,
		view);

 cleanup:
	g_slist_free_full (attendees, e_cal_component_attendee_free);
	g_object_unref (comp);
}

static void
update_attendee_status_get_object_without_rid_cb (GObject *ecalclient,
                                                  GAsyncResult *result,
                                                  gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (ecalclient);
	ItipView *view = user_data;
	ICalComponent *icomp = NULL;
	GError *error = NULL;

	e_cal_client_get_object_finish (client, result, &icomp, &error);

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
		update_attendee_status_icomp (view, icomp);
	}
}

static void
update_attendee_status_get_object_with_rid_cb (GObject *ecalclient,
                                               GAsyncResult *result,
                                               gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (ecalclient);
	ItipView *view = user_data;
	ICalComponent *icomp = NULL;
	GError *error = NULL;

	e_cal_client_get_object_finish (client, result, &icomp, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);

	} else if (error != NULL) {
		const gchar *uid;
		gchar *rid;

		g_error_free (error);

		uid = e_cal_component_get_uid (view->priv->comp);
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
		update_attendee_status_icomp (view, icomp);
	}
}

static void
update_attendee_status (ItipView *view)
{
	const gchar *uid;
	gchar *rid;

	/* Obtain our version */
	uid = e_cal_component_get_uid (view->priv->comp);
	rid = e_cal_component_get_recurid_as_string (view->priv->comp);

	claim_progress_saving_changes (view);

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
			I_CAL_METHOD_REQUEST,
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
	ICalProperty *prop;
	ICalComponentKind kind = I_CAL_NO_COMPONENT;
	ICalComponent *tz_comp;
	ICalComponent *alarm_comp;
	ICalCompIter *iter;
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

	view->priv->main_comp = i_cal_parser_parse_string (view->priv->vcalendar);
	if (view->priv->main_comp == NULL || !itip_is_component_valid (view->priv->main_comp)) {
		set_itip_error (
			view,
			_("The calendar attached is not valid"),
			_("The message claims to contain a calendar, but the calendar is not a valid iCalendar."),
			FALSE);

		g_clear_object (&view->priv->main_comp);

		return FALSE;
	}

	prop = i_cal_component_get_first_property (view->priv->main_comp, I_CAL_METHOD_PROPERTY);
	if (prop == NULL) {
		ICalComponent *subcomp;

		view->priv->method = I_CAL_METHOD_PUBLISH;

		/* Search in sub-components for the METHOD property when not found in the VCALENDAR */
		for (subcomp = i_cal_component_get_first_component (view->priv->main_comp, I_CAL_ANY_COMPONENT);
		     subcomp;
		     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (view->priv->main_comp, I_CAL_ANY_COMPONENT)) {
			kind = i_cal_component_isa (subcomp);

			if (kind == I_CAL_VEVENT_COMPONENT ||
			    kind == I_CAL_VTODO_COMPONENT ||
			    kind == I_CAL_VJOURNAL_COMPONENT ||
			    kind == I_CAL_VFREEBUSY_COMPONENT) {
				prop = i_cal_component_get_first_property (subcomp, I_CAL_METHOD_PROPERTY);
				if (prop) {
					view->priv->method = i_cal_property_get_method (prop);
					g_object_unref (subcomp);
					g_clear_object (&prop);
					break;
				}
			}
		}
	} else {
		view->priv->method = i_cal_property_get_method (prop);
		g_clear_object (&prop);
	}

	iter = i_cal_component_begin_component (view->priv->main_comp, I_CAL_VTIMEZONE_COMPONENT);
	tz_comp = i_cal_comp_iter_deref (iter);
	while (tz_comp) {
		ICalComponent *next_subcomp;
		ICalComponent *clone;

		next_subcomp = i_cal_comp_iter_next (iter);

		clone = i_cal_component_clone (tz_comp);
		i_cal_component_take_component (view->priv->top_level, clone);

		g_object_unref (tz_comp);
		tz_comp = next_subcomp;
	}

	g_clear_object (&iter);

	iter = i_cal_component_begin_component (view->priv->main_comp, I_CAL_ANY_COMPONENT);
	view->priv->ical_comp = i_cal_comp_iter_deref (iter);
	if (view->priv->ical_comp != NULL) {
		kind = i_cal_component_isa (view->priv->ical_comp);
		if (kind != I_CAL_VEVENT_COMPONENT &&
		    kind != I_CAL_VTODO_COMPONENT &&
		    kind != I_CAL_VFREEBUSY_COMPONENT &&
		    kind != I_CAL_VJOURNAL_COMPONENT) {
			do {
				g_clear_object (&view->priv->ical_comp);
				view->priv->ical_comp = i_cal_comp_iter_next (iter);
				if (!view->priv->ical_comp)
					break;
				kind = i_cal_component_isa (view->priv->ical_comp);
			} while (view->priv->ical_comp != NULL &&
				 kind != I_CAL_VEVENT_COMPONENT &&
				 kind != I_CAL_VTODO_COMPONENT &&
				 kind != I_CAL_VFREEBUSY_COMPONENT &&
				 kind != I_CAL_VJOURNAL_COMPONENT);
		}
	}

	g_clear_object (&iter);

	if (view->priv->ical_comp == NULL) {
		set_itip_error (
			view,
			_("The item in the calendar is not valid"),
			_("The message does contain a calendar, but the calendar contains no events, tasks or free/busy information"),
			FALSE);

		return FALSE;
	}

	view->priv->with_detached_instances = FALSE;

	total = i_cal_component_count_components (view->priv->main_comp, I_CAL_VEVENT_COMPONENT);
	total += i_cal_component_count_components (view->priv->main_comp, I_CAL_VTODO_COMPONENT);
	total += i_cal_component_count_components (view->priv->main_comp, I_CAL_VFREEBUSY_COMPONENT);
	total += i_cal_component_count_components (view->priv->main_comp, I_CAL_VJOURNAL_COMPONENT);

	if (total > 1) {
		ICalComponent *icomp, *master_comp = NULL;
		gint orig_total = total;
		const gchar *expected_uid = NULL;

		for (icomp = i_cal_component_get_first_component (view->priv->main_comp, I_CAL_ANY_COMPONENT);
		     icomp;
		     g_object_unref (icomp), icomp = i_cal_component_get_next_component (view->priv->main_comp, I_CAL_ANY_COMPONENT)) {
			ICalComponentKind icomp_kind;
			const gchar *uid;

			icomp_kind = i_cal_component_isa (icomp);

			if (icomp_kind != I_CAL_VEVENT_COMPONENT &&
			    icomp_kind != I_CAL_VJOURNAL_COMPONENT &&
			    icomp_kind != I_CAL_VTODO_COMPONENT)
				continue;

			uid = i_cal_component_get_uid (icomp);

			if (!master_comp &&
			    !e_cal_util_component_has_property (icomp, I_CAL_RECURRENCEID_PROPERTY)) {
				master_comp = g_object_ref (icomp);
			}

			/* Maybe it's an event with detached instances */
			if (!expected_uid) {
				expected_uid = uid;
			} else if (g_strcmp0 (uid, expected_uid) == 0) {
				total--;
			} else {
				total = orig_total;
				g_object_unref (icomp);
				break;
			}
		}

		view->priv->with_detached_instances = orig_total != total;
		if (view->priv->with_detached_instances && master_comp &&
		    i_cal_object_get_native (I_CAL_OBJECT (master_comp)) != i_cal_object_get_native (I_CAL_OBJECT (view->priv->ical_comp))) {
			g_clear_object (&view->priv->ical_comp);
			view->priv->ical_comp = g_object_ref (master_comp);
		}

		g_clear_object (&master_comp);
	}

	switch (i_cal_component_isa (view->priv->ical_comp)) {
	case I_CAL_VEVENT_COMPONENT:
		view->priv->type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
		view->priv->has_organizer = e_cal_util_component_has_property (view->priv->ical_comp, I_CAL_ORGANIZER_PROPERTY);
		if (!e_cal_util_component_has_property (view->priv->ical_comp, I_CAL_ATTENDEE_PROPERTY)) {
			/* no attendees: assume that this is not a meeting and organizer doesn't want a reply */
			view->priv->no_reply_wanted = TRUE;
		} else {
			/*
			 * if we have attendees, then find_to_address() will check for our RSVP
			 * and set no_reply_wanted=TRUE if RSVP=FALSE for the current user
			 */
		}
		break;
	case I_CAL_VTODO_COMPONENT:
		view->priv->type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
		break;
	case I_CAL_VJOURNAL_COMPONENT:
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

	if (i_cal_component_isa (view->priv->ical_comp) != I_CAL_VJOURNAL_COMPONENT) {
		gchar *my_address;

		prop = NULL;
		comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (view->priv->ical_comp));
		my_address = itip_get_comp_attendee (
			view->priv->registry, comp, NULL);
		g_clear_object (&comp);

		prop = itip_utils_find_attendee_property (view->priv->ical_comp, my_address);
		if (!prop)
			prop = find_attendee_if_sentby (view->priv->ical_comp, my_address);
		if (prop) {
			ICalParameter *param;
			const gchar *delfrom;

			if ((param = i_cal_property_get_first_parameter (prop, I_CAL_DELEGATEDFROM_PARAMETER))) {
				delfrom = i_cal_parameter_get_delegatedfrom (param);

				view->priv->delegator_address = g_strdup (itip_strip_mailto (delfrom));

				g_object_unref (param);
			}
		}
		g_free (my_address);
		g_clear_object (&prop);

		/* Determine any delegate sections */
		for (prop = i_cal_component_get_first_property (view->priv->ical_comp, I_CAL_X_PROPERTY);
		     prop;
		     g_object_unref (prop), prop = i_cal_component_get_next_property (view->priv->ical_comp, I_CAL_X_PROPERTY)) {
			const gchar *x_name, *x_val;

			x_name = i_cal_property_get_x_name (prop);
			x_val = i_cal_property_get_x (prop);

			if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-CALENDAR-UID"))
				view->priv->calendar_uid = g_strdup (x_val);
			else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-CALENDAR-URI"))
				g_warning (G_STRLOC ": X-EVOLUTION-DELEGATOR-CALENDAR-URI used");
			else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-ADDRESS"))
				view->priv->delegator_address = g_strdup (x_val);
			else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-NAME"))
				view->priv->delegator_name = g_strdup (x_val);
		}

		/* Strip out procedural alarms for security purposes */
		iter = i_cal_component_begin_component (view->priv->ical_comp, I_CAL_VALARM_COMPONENT);
		alarm_comp = i_cal_comp_iter_deref (iter);
		while (alarm_comp) {
			ICalComponent *next_subcomp;
			ICalProperty *pp;

			next_subcomp = i_cal_comp_iter_next (iter);

			pp = i_cal_component_get_first_property (alarm_comp, I_CAL_ACTION_PROPERTY);
			if (!pp || i_cal_property_get_action (pp) == I_CAL_ACTION_PROCEDURE)
				i_cal_component_remove_component (view->priv->ical_comp, alarm_comp);

			g_clear_object (&pp);

			g_object_unref (alarm_comp);
			alarm_comp = next_subcomp;
		}

		g_clear_object (&iter);

		if (have_alarms) {
			iter = i_cal_component_begin_component (view->priv->ical_comp, I_CAL_VALARM_COMPONENT);
			alarm_comp = i_cal_comp_iter_deref (iter);
			*have_alarms = alarm_comp != NULL;
			g_clear_object (&alarm_comp);
			g_clear_object (&iter);
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
		ICalDuration *duration;
		ECalComponentAlarmTrigger *trigger;

		interval = g_settings_get_int (
			settings, "default-reminder-interval");
		units = g_settings_get_enum (
			settings, "default-reminder-units");

		acomp = e_cal_component_alarm_new ();

		e_cal_component_alarm_set_action (acomp, E_CAL_COMPONENT_ALARM_DISPLAY);

		duration = i_cal_duration_new_null_duration ();
		i_cal_duration_set_is_neg (duration, TRUE);

		switch (units) {
			case E_DURATION_MINUTES:
				i_cal_duration_set_minutes (duration, interval);
				break;
			case E_DURATION_HOURS:
				i_cal_duration_set_hours (duration, interval);
				break;
			case E_DURATION_DAYS:
				i_cal_duration_set_days (duration, interval);
				break;
			default:
				g_warn_if_reached ();
				break;
		}

		trigger = e_cal_component_alarm_trigger_new_relative (E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, duration);

		e_cal_component_alarm_take_trigger (acomp, trigger);
		e_cal_component_add_alarm (view->priv->comp, acomp);

		e_cal_component_alarm_free (acomp);
		g_clear_object (&duration);
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
	ICalProperty *prop;
	ECalComponentTransparency trans;

	if (response == ITIP_VIEW_RESPONSE_SAVE) {
		save_vcalendar_cb (view);
		return;
	}

	if (view->priv->method == I_CAL_METHOD_PUBLISH || view->priv->method == I_CAL_METHOD_REQUEST) {
		if (itip_view_get_free_time_check_state (view))
			e_cal_component_set_transparency (view->priv->comp, E_CAL_COMPONENT_TRANSP_TRANSPARENT);
		else
			e_cal_component_set_transparency (view->priv->comp, E_CAL_COMPONENT_TRANSP_OPAQUE);
	} else {
		trans = e_cal_component_get_transparency (view->priv->comp);

		if (trans == E_CAL_COMPONENT_TRANSP_NONE)
			e_cal_component_set_transparency (view->priv->comp, E_CAL_COMPONENT_TRANSP_OPAQUE);
	}

	if (!view->priv->to_address && view->priv->current_client != NULL) {
		e_client_get_backend_property_sync (E_CLIENT (view->priv->current_client), E_CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &view->priv->to_address, NULL, NULL);

		if (view->priv->to_address && !*view->priv->to_address) {
			g_free (view->priv->to_address);
			view->priv->to_address = NULL;
		}
	}

	/* check if it is a recur instance (no master object) and
	 * add a property */
	if (itip_view_get_recur_check_state (view)) {
		prop = i_cal_property_new_x ("All");
		i_cal_property_set_x_name (prop, "X-GW-RECUR-INSTANCES-MOD-TYPE");
		i_cal_component_take_property (view->priv->ical_comp, prop);
	}

	switch (response) {
		case ITIP_VIEW_RESPONSE_ACCEPT:
			if (view->priv->type != E_CAL_CLIENT_SOURCE_TYPE_MEMOS)
				itip_utils_prepare_attendee_response (
					view->priv->registry,
					view->priv->ical_comp,
					view->priv->to_address,
					I_CAL_PARTSTAT_ACCEPTED);
			update_item (view, response);
			break;
		case ITIP_VIEW_RESPONSE_TENTATIVE:
			itip_utils_prepare_attendee_response (
					view->priv->registry,
					view->priv->ical_comp,
					view->priv->to_address,
					I_CAL_PARTSTAT_TENTATIVE);
			update_item (view, response);
			break;
		case ITIP_VIEW_RESPONSE_DECLINE:
			if (view->priv->type != E_CAL_CLIENT_SOURCE_TYPE_MEMOS) {
				itip_utils_prepare_attendee_response (
					view->priv->registry,
					view->priv->ical_comp,
					view->priv->to_address,
					I_CAL_PARTSTAT_DECLINED);
			} else {
				prop = i_cal_property_new_x ("1");
				i_cal_property_set_x_name (prop, "X-GW-DECLINED");
				i_cal_component_take_property (view->priv->ical_comp, prop);
			}

			update_item (view, response);
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
check_is_instance (ICalComponent *icomp)
{
	ICalProperty *prop;

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_X_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_X_PROPERTY)) {
		const gchar *x_name;

		x_name = i_cal_property_get_x_name (prop);
		if (!g_strcmp0 (x_name, "X-GW-RECURRENCE-KEY")) {
			g_object_unref (prop);
			return TRUE;
		}
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

static ICalTimezone *
itip_view_guess_timezone (const gchar *tzid)
{
	ICalTimezone *zone;

	if (!tzid || !*tzid)
		return NULL;

	zone = i_cal_timezone_get_builtin_timezone (tzid);
	if (zone)
		return zone;

	zone = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);
	if (zone)
		return zone;

	tzid = e_cal_match_tzid (tzid);
	if (tzid)
		zone = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);

	return zone;
}

static void
itip_view_add_recurring_info (ItipView *view)
{
	gchar *description;

	description = e_cal_recur_describe_recurrence_ex (e_cal_component_get_icalcomponent (view->priv->comp),
		calendar_config_get_week_start_day (),
		E_CAL_RECUR_DESCRIBE_RECURRENCE_FLAG_PREFIXED | E_CAL_RECUR_DESCRIBE_RECURRENCE_FLAG_FALLBACK,
		cal_comp_util_format_itt);

	if (description) {
		itip_view_add_upper_info_item (view, ITIP_VIEW_INFO_ITEM_TYPE_INFO, description);
		g_free (description);
	}
}

void
itip_view_init_view (ItipView *view)
{
	ECalComponentText *text;
	ECalComponentOrganizer *organizer;
	ECalComponentDateTime *datetime;
	ICalTimezone *from_zone;
	ICalTimezone *to_zone = NULL;
	GSettings *settings;
	GString *gstring = NULL;
	GSList *list, *l;
	ICalComponent *icomp;
	ICalProperty *prop;
	const gchar *org;
	gchar *string;
	gboolean response_enabled;
	gboolean have_alarms = FALSE;
	gboolean description_is_html = FALSE;

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
			have_alarms && (view->priv->method == I_CAL_METHOD_PUBLISH || view->priv->method == I_CAL_METHOD_REQUEST));

		switch (view->priv->method) {
			case I_CAL_METHOD_PUBLISH:
			case I_CAL_METHOD_REQUEST:
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
			case I_CAL_METHOD_REPLY:
				itip_view_set_mode (view, ITIP_VIEW_MODE_REPLY);
				break;
			case I_CAL_METHOD_ADD:
				itip_view_set_mode (view, ITIP_VIEW_MODE_ADD);
				break;
			case I_CAL_METHOD_CANCEL:
				itip_view_set_mode (view, ITIP_VIEW_MODE_CANCEL);
				break;
			case I_CAL_METHOD_REFRESH:
				itip_view_set_mode (view, ITIP_VIEW_MODE_REFRESH);
				break;
			case I_CAL_METHOD_COUNTER:
				itip_view_set_mode (view, ITIP_VIEW_MODE_COUNTER);
				break;
			case I_CAL_METHOD_DECLINECOUNTER:
				itip_view_set_mode (view, ITIP_VIEW_MODE_DECLINECOUNTER);
				break;
			case I_CAL_METHOD_X :
                                /* Handle appointment requests from Microsoft Live. This is
                                 * a best-at-hand-now handling. Must be revisited when we have
                                 * better access to the source of such meetings */
				view->priv->method = I_CAL_METHOD_REQUEST;
				itip_view_set_mode (view, ITIP_VIEW_MODE_REQUEST);
				break;
			default:
				return;
		}
	}

	itip_view_set_item_type (view, view->priv->type);

	if (response_enabled) {
		switch (view->priv->method) {
			case I_CAL_METHOD_REQUEST:
                                /* FIXME What about the name? */
				itip_view_set_delegator (view, view->priv->delegator_name ? view->priv->delegator_name : view->priv->delegator_address);
				/* coverity[fallthrough] */
				/* falls through */
			case I_CAL_METHOD_PUBLISH:
			case I_CAL_METHOD_ADD:
			case I_CAL_METHOD_CANCEL:
			case I_CAL_METHOD_DECLINECOUNTER:
				itip_view_set_show_update_check (view, FALSE);

                                /* An organizer sent this */
				organizer = e_cal_component_get_organizer (view->priv->comp);
				if (!organizer)
					break;

				org = e_cal_component_organizer_get_cn (organizer) ? e_cal_component_organizer_get_cn (organizer) :
					itip_strip_mailto (e_cal_component_organizer_get_value (organizer));

				itip_view_set_organizer (view, org);
				if (e_cal_component_organizer_get_sentby (organizer)) {
					const gchar *sentby = itip_strip_mailto (e_cal_component_organizer_get_sentby (organizer));

					if (sentby && *sentby) {
						gchar *tmp = NULL;

						if (view->priv->message) {
							const gchar *sender = camel_medium_get_header (CAMEL_MEDIUM (view->priv->message), "Sender");

							if (sender && *sender) {
								CamelInternetAddress *addr;
								const gchar *name = NULL, *email = NULL;

								addr = camel_internet_address_new ();
								if (camel_address_decode (CAMEL_ADDRESS (addr), sender) == 1 &&
								    camel_internet_address_get (addr, 0, &name, &email) &&
								    name && *name && email && *email &&
								    g_ascii_strcasecmp (sentby, email) == 0) {
									tmp = camel_internet_address_format_address (name, sentby);
									sentby = tmp;
								}

								g_object_unref (addr);
							}
						}

						itip_view_set_organizer_sentby (view, sentby);

						g_free (tmp);
					}
				}

				if (view->priv->my_address) {
					if (!(e_cal_component_organizer_get_value (organizer) &&
					      !g_ascii_strcasecmp (itip_strip_mailto (e_cal_component_organizer_get_value (organizer)), view->priv->my_address)) &&
					    !(e_cal_component_organizer_get_sentby (organizer) &&
					      !g_ascii_strcasecmp (itip_strip_mailto (e_cal_component_organizer_get_sentby (organizer)), view->priv->my_address)) &&
					    (view->priv->to_address && g_ascii_strcasecmp (view->priv->to_address, view->priv->my_address)))
						itip_view_set_proxy (view, view->priv->to_name ? view->priv->to_name : view->priv->to_address);
				}

				e_cal_component_organizer_free (organizer);
				break;
			case I_CAL_METHOD_REPLY:
			case I_CAL_METHOD_REFRESH:
			case I_CAL_METHOD_COUNTER:
				itip_view_set_show_update_check (view, TRUE);

                                /* An attendee sent this */
				list = e_cal_component_get_attendees (view->priv->comp);
				if (list != NULL) {
					ECalComponentAttendee *attendee;

					attendee = list->data;

					itip_view_set_attendee (view, e_cal_component_attendee_get_cn (attendee) ?
						e_cal_component_attendee_get_cn (attendee) : itip_strip_mailto (e_cal_component_attendee_get_value (attendee)));

					if (e_cal_component_attendee_get_sentby (attendee))
						itip_view_set_attendee_sentby (view, itip_strip_mailto (e_cal_component_attendee_get_sentby (attendee)));

					if (view->priv->my_address) {
						if (!(e_cal_component_attendee_get_value (attendee) &&
						      !g_ascii_strcasecmp (itip_strip_mailto (e_cal_component_attendee_get_value (attendee)), view->priv->my_address)) &&
						    !(e_cal_component_attendee_get_sentby (attendee) &&
						      !g_ascii_strcasecmp (itip_strip_mailto (e_cal_component_attendee_get_sentby (attendee)), view->priv->my_address)) &&
						    (view->priv->from_address && g_ascii_strcasecmp (view->priv->from_address, view->priv->my_address)))
							itip_view_set_proxy (view, view->priv->from_name ? view->priv->from_name : view->priv->from_address);
					}

					g_slist_free_full (list, e_cal_component_attendee_free);
				}
				break;
			default:
				g_warn_if_reached ();
				break;
		}
	}

	text = e_cal_component_get_summary (view->priv->comp);
	itip_view_set_summary (view, text && e_cal_component_text_get_value (text) ? e_cal_component_text_get_value (text) : C_("cal-itip", "None"));
	e_cal_component_text_free (text);

	string = e_cal_component_get_location (view->priv->comp);
	itip_view_set_location (view, string);
	g_free (string);

	string = e_cal_component_get_url (view->priv->comp);
	itip_view_set_url (view, string);
	g_free (string);

        /* Status really only applies for REPLY */
	if (response_enabled && view->priv->method == I_CAL_METHOD_REPLY) {
		list = e_cal_component_get_attendees (view->priv->comp);
		if (list != NULL) {
			ECalComponentAttendee *a = list->data;

			switch (e_cal_component_attendee_get_partstat (a)) {
				case I_CAL_PARTSTAT_ACCEPTED:
					itip_view_set_status (view, _("Accepted"));
					break;
				case I_CAL_PARTSTAT_TENTATIVE:
					itip_view_set_status (view, _("Tentatively Accepted"));
					break;
				case I_CAL_PARTSTAT_DECLINED:
					itip_view_set_status (view, _("Declined"));
					break;
				case I_CAL_PARTSTAT_DELEGATED:
					itip_view_set_status (view, _("Delegated"));
					break;
				default:
					itip_view_set_status (view, _("Unknown"));
			}
		}
		g_slist_free_full (list, e_cal_component_attendee_free);
	}

	if (view->priv->method == I_CAL_METHOD_REPLY ||
	    view->priv->method == I_CAL_METHOD_COUNTER ||
	    view->priv->method == I_CAL_METHOD_DECLINECOUNTER) {
                /* FIXME Check spec to see if multiple comments are actually valid */
                /* Comments for iTIP are limited to one per object */
		list = e_cal_component_get_comments (view->priv->comp);
		if (list) {
			text = list->data;

			if (text && e_cal_component_text_get_value (text)) {
				gchar *html;

				html = camel_text_to_html (
					e_cal_component_text_get_value (text),
					CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES,
					0);

				itip_view_set_comment (view, html);

				g_free (html);
			}
		}

		g_slist_free_full (list, e_cal_component_text_free);
	}

	itip_view_extract_attendee_info (view);

	icomp = e_cal_component_get_icalcomponent (view->priv->comp);
	prop = e_cal_util_component_find_x_property (icomp, "X-ALT-DESC");

	if (prop) {
		ICalParameter *param;

		param = i_cal_property_get_first_parameter (prop, I_CAL_FMTTYPE_PARAMETER);

		if (param && i_cal_parameter_get_fmttype (param) &&
		    g_ascii_strcasecmp (i_cal_parameter_get_fmttype (param), "text/html") == 0) {
			ICalValue *value;
			const gchar *str = NULL;

			value = i_cal_property_get_value (prop);

			if (value)
				str = i_cal_value_get_x (value);

			if (str && *str) {
				gstring = g_string_new (str);
				description_is_html = TRUE;
			}

			g_clear_object (&value);
		}

		g_clear_object (&param);
		g_object_unref (prop);
	}

	if (!gstring) {
		list = e_cal_component_get_descriptions (view->priv->comp);
		for (l = list; l; l = l->next) {
			text = l->data;

			if (!text)
				continue;

			if (!gstring && e_cal_component_text_get_value (text))
				gstring = g_string_new (e_cal_component_text_get_value (text));
			else if (e_cal_component_text_get_value (text))
				g_string_append_printf (gstring, "\n\n%s", e_cal_component_text_get_value (text));
		}

		g_slist_free_full (list, e_cal_component_text_free);
	}

	if (gstring) {
		gchar *html = NULL;

		if (description_is_html) {
			/* Do nothing, trust the text provider */

		/* Google encodes HTML into the description, without giving a clue about it,
		   but try to guess whether it can be an HTML blob or not. */
		} else if (camel_strstrcase (gstring->str, "<html>") ||
			   camel_strstrcase (gstring->str, "<body>") ||
			   camel_strstrcase (gstring->str, "<br>") ||
			   camel_strstrcase (gstring->str, "<span>") ||
			   camel_strstrcase (gstring->str, "<b>") ||
			   camel_strstrcase (gstring->str, "<i>") ||
			   camel_strstrcase (gstring->str, "<u>") ||
			   camel_strstrcase (gstring->str, "&nbsp;") ||
			   camel_strstrcase (gstring->str, "<ul>") ||
			   camel_strstrcase (gstring->str, "<li>") ||
			   camel_strstrcase (gstring->str, "</a>")) {
			gchar *ptr = gstring->str;
			/* To make things easier, Google mixes HTML '<br>' with plain text '\n'... */
			while (ptr = strchr (ptr, '\n'), ptr) {
				gssize pos = ptr - gstring->str;

				g_string_insert (gstring, pos, "<br>");

				ptr = gstring->str + pos + 4 + 1;
			}
		} else {
			html = camel_text_to_html (
				gstring->str,
				CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
				CAMEL_MIME_FILTER_TOHTML_MARK_CITATION |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES,
				0);
		}

		itip_view_set_description (view, html ? html : gstring->str);
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
			to_zone = i_cal_timezone_get_builtin_timezone (location);
			g_free (location);
		}
	}

	if (to_zone == NULL)
		to_zone = i_cal_timezone_get_utc_timezone ();

	g_object_unref (settings);

	datetime = e_cal_component_get_dtstart (view->priv->comp);
	view->priv->start_time = 0;
	if (datetime && e_cal_component_datetime_get_value (datetime)) {
		ICalTime *itt = e_cal_component_datetime_get_value (datetime);
		struct tm start_tm;

                /* If the timezone is not in the component, guess the local time */
                /* Should we guess if the timezone is an olsen name somehow? */
		if (i_cal_time_is_utc (itt))
			from_zone = g_object_ref (i_cal_timezone_get_utc_timezone ());
		else if (e_cal_component_datetime_get_tzid (datetime)) {
			from_zone = i_cal_component_get_timezone (view->priv->top_level, e_cal_component_datetime_get_tzid (datetime));

			if (!from_zone) {
				from_zone = itip_view_guess_timezone (e_cal_component_datetime_get_tzid (datetime));
				if (from_zone)
					g_object_ref (from_zone);
			}
		} else
			from_zone = NULL;

		start_tm = e_cal_util_icaltime_to_tm_with_zone (itt, from_zone, to_zone);

		itip_view_set_start (view, &start_tm, i_cal_time_is_date (itt));
		view->priv->start_time = i_cal_time_as_timet_with_zone (itt, from_zone);

		g_clear_object (&from_zone);
	}

        /* Set the recurrence id */
	if (check_is_instance (icomp) && datetime && e_cal_component_datetime_get_value (datetime)) {
		ECalComponentRange *recur_id;
		ICalTime *itt = i_cal_time_convert_to_zone (e_cal_component_datetime_get_value (datetime), to_zone);

		recur_id = e_cal_component_range_new_take (E_CAL_COMPONENT_RANGE_SINGLE,
			e_cal_component_datetime_new_take (itt, g_strdup (i_cal_timezone_get_tzid (to_zone))));
		e_cal_component_set_recurid (view->priv->comp, recur_id);
		e_cal_component_range_free (recur_id);
	}
	e_cal_component_datetime_free (datetime);

	datetime = e_cal_component_get_dtend (view->priv->comp);
	view->priv->end_time = 0;
	if (datetime && e_cal_component_datetime_get_value (datetime)) {
		ICalTime *itt = e_cal_component_datetime_get_value (datetime);
		struct tm end_tm;

                /* If the timezone is not in the component, guess the local time */
                /* Should we guess if the timezone is an olsen name somehow? */
		if (i_cal_time_is_utc (itt))
			from_zone = g_object_ref (i_cal_timezone_get_utc_timezone ());
		else if (e_cal_component_datetime_get_tzid (datetime)) {
			from_zone = i_cal_component_get_timezone (view->priv->top_level, e_cal_component_datetime_get_tzid (datetime));

			if (!from_zone) {
				from_zone = itip_view_guess_timezone (e_cal_component_datetime_get_tzid (datetime));
				if (from_zone)
					g_object_ref (from_zone);
			}
		} else
			from_zone = NULL;

		if (i_cal_time_is_date (itt)) {
                        /* RFC says the DTEND is not inclusive, thus subtract one day
                         * if we have a date */

			i_cal_time_adjust (itt, -1, 0, 0, 0);
		}

		end_tm = e_cal_util_icaltime_to_tm_with_zone (itt, from_zone, to_zone);

		itip_view_set_end (view, &end_tm, i_cal_time_is_date (itt));
		view->priv->end_time = i_cal_time_as_timet_with_zone (itt, from_zone);
		g_clear_object (&from_zone);
	}
	e_cal_component_datetime_free (datetime);

        /* Recurrence info */
	itip_view_add_recurring_info (view);

	g_signal_connect (
		view, "response",
		G_CALLBACK (view_response_cb), NULL);

	if (response_enabled) {
		itip_view_set_show_free_time_check (view, view->priv->type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS &&
			(view->priv->method == I_CAL_METHOD_PUBLISH || view->priv->method == I_CAL_METHOD_REQUEST));

		if (view->priv->calendar_uid) {
			start_calendar_server_by_uid (view, view->priv->calendar_uid, view->priv->type);
		} else {
			find_server (view, view->priv->comp);
			set_buttons_sensitive (view);
		}
	} else {
		/* The Open Calendar button can be shown, thus enable it */
		enable_button (view, BUTTON_OPEN_CALENDAR, TRUE);
	}
}

static void
itip_source_changed_cb (WebKitUserContentManager *manager,
			WebKitJavascriptResult *js_result,
			gpointer user_data)
{
	ItipView *view = user_data;
	JSCValue *jsc_value;
	gchar *iframe_id, *source_uid;

	g_return_if_fail (view != NULL);
	g_return_if_fail (js_result != NULL);

	jsc_value = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_object (jsc_value));

	iframe_id = e_web_view_jsc_get_object_property_string (jsc_value, "iframe-id", NULL);
	source_uid = e_web_view_jsc_get_object_property_string (jsc_value, "source-uid", NULL);

	if (g_strcmp0 (iframe_id, view->priv->part_id) == 0) {
		itip_set_selected_source_uid (view, source_uid);
		source_changed_cb (view);
	}

	g_free (iframe_id);
}

static void
itip_recur_toggled_cb (WebKitUserContentManager *manager,
		       WebKitJavascriptResult *js_result,
		       gpointer user_data)
{
	ItipView *view = user_data;
	JSCValue *jsc_value;
	gchar *iframe_id;

	g_return_if_fail (view != NULL);
	g_return_if_fail (js_result != NULL);

	jsc_value = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_string (jsc_value));

	iframe_id = jsc_value_to_string (jsc_value);

	if (g_strcmp0 (iframe_id, view->priv->part_id) == 0)
		itip_view_set_mode (view, view->priv->mode);

	g_free (iframe_id);
}

void
itip_view_set_web_view (ItipView *view,
			EWebView *web_view)
{
	g_return_if_fail (ITIP_IS_VIEW (view));
	if (web_view)
		g_return_if_fail (E_IS_WEB_VIEW (web_view));

	g_weak_ref_set (view->priv->web_view_weakref, web_view);

	if (web_view) {
		WebKitUserContentManager *manager;

		manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (web_view));

		g_signal_connect_object (manager, "script-message-received::itipSourceChanged",
			G_CALLBACK (itip_source_changed_cb), view, 0);

		g_signal_connect_object (manager, "script-message-received::itipRecurToggled",
			G_CALLBACK (itip_recur_toggled_cb), view, 0);

		webkit_user_content_manager_register_script_message_handler (manager, "itipSourceChanged");
		webkit_user_content_manager_register_script_message_handler (manager, "itipRecurToggled");

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
			"EvoItip.Initialize(%s);",
			view->priv->part_id);

		itip_view_init_view (view);
	}

	itip_view_register_clicked_listener (view);
}

EWebView *
itip_view_ref_web_view (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return g_weak_ref_get (view->priv->web_view_weakref);
}
