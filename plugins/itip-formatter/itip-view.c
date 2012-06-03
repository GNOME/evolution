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
#include <glib/gi18n.h>
#include <libedataserverui/libedataserverui.h>

#include <mail/em-format-hook.h>
#include <mail/em-format-html.h>
#include <e-util/e-util.h>
#include <e-util/e-unicode.h>
#include <calendar/gui/itip-utils.h>
#include <webkit/webkitdom.h>

#include "itip-view.h"

#define d(x)

#define MEETING_ICON "stock_new-meeting"

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
	ESourceRegistry *registry;
	gulong source_added_id;
	gulong source_removed_id;
	gchar *extension_name;

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
	gint start_tm_is_date : 1;
	gchar *start_label;
	const gchar *start_header;

	struct tm *end_tm;
	gint end_tm_is_date : 1;
	gchar *end_label;
	const gchar *end_header;

	GSList *upper_info_items;
	GSList *lower_info_items;

	guint next_info_item_id;

	gchar *description;

	gint buttons_sensitive : 1;

	gboolean is_recur_set;

	gint needs_decline : 1;

	WebKitDOMDocument *dom_document;
	ItipPURI *puri;

	gchar *error;
};

#define TEXT_ROW_SENDER "text_row_sender"
#define TABLE_ROW_SUMMARY "table_row_summary"
#define TABLE_ROW_LOCATION "table_row_location"
#define TABLE_ROW_START_DATE "table_row_start_time"
#define TABLE_ROW_END_DATE "table_row_end_time"
#define TABLE_ROW_STATUS "table_row_status"
#define TABLE_ROW_COMMENT "table_row_comment"
#define TABLE_ROW_DESCRIPTION "table_row_description"
#define TABLE_ROW_RSVP_COMMENT "table_row_rsvp_comment"
#define TABLE_ROW_ESCB "table_row_escb"
#define TABLE_ROW_BUTTONS "table_row_buttons"
#define TABLE_ROW_ESCB_LABEL "table_row_escb_label"

#define TABLE_BUTTONS "table_buttons"

#define SELECT_ESOURCE "select_esource"
#define TEXTAREA_RSVP_COMMENT "textarea_rsvp_comment"

#define CHECKBOX_RSVP "checkbox_rsvp"
#define CHECKBOX_RECUR "checkbox_recur"
#define CHECKBOX_UPDATE "checkbox_update"
#define CHECKBOX_FREE_TIME "checkbox_free_time"
#define CHECKBOX_KEEP_ALARM "checkbox_keep_alarm"
#define CHECKBOX_INHERIT_ALARM "checkbox_inherit_alarm"

#define BUTTON_OPEN_CALENDAR "button_open_calendar"
#define BUTTON_DECLINE "button_decline"
#define BUTTON_DECLINE_ALL "button_decline_all"
#define BUTTON_ACCEPT "button_accept"
#define BUTTON_ACCEPT_ALL "button_accept_all"
#define BUTTON_TENTATIVE "button_tentative"
#define BUTTON_TENTATIVE_ALL "button_tentative_all"
#define BUTTON_SEND_INFORMATION "button_send_information"
#define BUTTON_UPDATE "button_update"
#define BUTTON_UPDATE_ATTENDEE_STATUS "button_update_attendee_status"
#define BUTTON_SAVE "button_save"

#define TABLE_UPPER_ITIP_INFO "table_upper_itip_info"
#define TABLE_LOWER_ITIP_INFO "table_lower_itip_info"

#define DIV_ITIP_CONTENT "div_itip_content"
#define DIV_ITIP_ERROR "div_itip_error"

enum {
	PROP_0,
	PROP_EXTENSION_NAME,
	PROP_REGISTRY
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
			sender = dupe_first_bold (_("%s through %s has canceled the following meeting:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has canceled the following meeting:"), organizer, NULL);
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

	if (sender && on_behalf_of)
		sender = g_strjoin (NULL, sender, "\n", on_behalf_of, NULL);

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
			sender = dupe_first_bold (_("%s through %s has canceled the following assigned task:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has canceled the following assigned task:"), organizer, NULL);
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

	if (sender && on_behalf_of)
		sender = g_strjoin (NULL, sender, "\n", on_behalf_of, NULL);

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
			sender = dupe_first_bold (_("%s through %s has canceled the following shared memo:"), organizer, priv->organizer_sentby);
		else
			sender = dupe_first_bold (_("%s has canceled the following shared memo:"), organizer, NULL);
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
set_sender_text (ItipView *view)
{
	ItipViewPrivate *priv;
	priv = view->priv;

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

	if (priv->sender && priv->dom_document) {
		WebKitDOMElement *div;

		div = webkit_dom_document_get_element_by_id (
			priv->dom_document, TEXT_ROW_SENDER);
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (div), priv->sender, NULL);
	}
}

static void
update_start_end_times (ItipView *view)
{
	ItipViewPrivate *priv;
	WebKitDOMElement *row, *col;
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

	if (priv->dom_document) {
		row = webkit_dom_document_get_element_by_id (
			priv->dom_document, TABLE_ROW_START_DATE);
		if (priv->start_header && priv->start_label) {
			webkit_dom_html_element_set_hidden (
				WEBKIT_DOM_HTML_ELEMENT (row), FALSE);

			col = webkit_dom_element_get_first_element_child (row);
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (col), priv->start_header, NULL);

			col = webkit_dom_element_get_last_element_child (row);
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (col), priv->start_label, NULL);
		} else {
			webkit_dom_html_element_set_hidden (
				WEBKIT_DOM_HTML_ELEMENT (row), TRUE);
		}

		row = webkit_dom_document_get_element_by_id (
			priv->dom_document, TABLE_ROW_END_DATE);
		if (priv->end_header && priv->end_label) {
			webkit_dom_html_element_set_hidden (
				WEBKIT_DOM_HTML_ELEMENT (row), FALSE);

			col = webkit_dom_element_get_first_element_child (row);
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (col), priv->end_header, NULL);

			col = webkit_dom_element_get_last_element_child (row);
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (col), priv->end_label, NULL);
		} else {
			webkit_dom_html_element_set_hidden (
				WEBKIT_DOM_HTML_ELEMENT (row), TRUE);
		}
	}
}

static void
button_clicked_cb (WebKitDOMElement *element,
                   WebKitDOMEvent *event,
                   gpointer data)
{
	ItipViewResponse response;
	gchar *responseStr;

	responseStr = webkit_dom_html_button_element_get_value (
		WEBKIT_DOM_HTML_BUTTON_ELEMENT (element));

	response = atoi (responseStr);

	//d(printf("Clicked btton %d\n", response));
	g_signal_emit (G_OBJECT (data), signals[RESPONSE], 0, response);
}

static void
rsvp_toggled_cb (WebKitDOMHTMLInputElement *input,
                 WebKitDOMEvent *event,
                 gpointer data)
{
	WebKitDOMElement *el;

	ItipView *view = data;
	gboolean rsvp;

	rsvp = webkit_dom_html_input_element_get_checked (input);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TEXTAREA_RSVP_COMMENT);
	webkit_dom_html_text_area_element_set_disabled (
		WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (el), !rsvp);
}

static void
recur_toggled_cb (WebKitDOMHTMLInputElement *input,
                  WebKitDOMEvent *event,
                  gpointer data)
{
	ItipView *view = data;

	itip_view_set_mode (view, view->priv->mode);
}

/*
  alarm_check_toggled_cb
  check1 was changed, so make the second available based on state of the first check.
*/
static void
alarm_check_toggled_cb (WebKitDOMHTMLInputElement *check1,
                        WebKitDOMEvent *event,
                        ItipView *view)
{
	WebKitDOMElement *check2;
	gchar *id = webkit_dom_html_element_get_id (WEBKIT_DOM_HTML_ELEMENT (check1));

	if (g_strcmp0 (id, CHECKBOX_INHERIT_ALARM)) {
		check2 = webkit_dom_document_get_element_by_id (
			view->priv->dom_document, CHECKBOX_KEEP_ALARM);
	} else {
		check2 = webkit_dom_document_get_element_by_id (
			view->priv->dom_document, CHECKBOX_INHERIT_ALARM);
	}

	g_free (id);

	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (check2),
		(webkit_dom_html_element_get_hidden (
				WEBKIT_DOM_HTML_ELEMENT (check1)) &&
			webkit_dom_html_input_element_get_checked (check1)));
}

static void
source_changed_cb (WebKitDOMElement *select,
                   WebKitDOMEvent *event,
                   ItipView *view)
{
	ESource *source;

	source = itip_view_ref_source (view);

	d(printf("Source changed to '%s'\n", e_source_get_display_name (source)));
	g_signal_emit (view, signals[SOURCE_SELECTED], 0, source);

	g_object_unref (source);
}

static gchar *
parse_html_mnemonics (const gchar *label,
                      gchar **access_key)
{
	const gchar *pos = NULL;
	gchar ak = 0;
	GString *html_label = NULL;

	pos = strstr (label, "_");
	if (pos != NULL) {
		ak = pos[1];

		/* Convert to uppercase */
		if (ak >= 'a')
			ak = ak - 32;

		html_label = g_string_new ("");
		g_string_append_len (html_label, label, pos - label);
		g_string_append_printf (html_label, "<u>%c</u>", pos[1]);
		g_string_append (html_label, &pos[2]);

		if (access_key) {
			if (ak) {
				*access_key = g_strdup_printf ("%c", ak);
			} else {
				*access_key = NULL;
			}
		}

	} else {
		html_label = g_string_new (label);

		if (access_key) {
			*access_key = NULL;
		}
	}

	return g_string_free (html_label, FALSE);
}

static void
append_checkbox_table_row (GString *buffer,
                           const gchar *name,
                           const gchar *label)
{
	gchar *access_key, *html_label;

	html_label = parse_html_mnemonics (label, &access_key);

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

		g_string_append_printf (buffer,
			"<tr id=\"%s\" %s><th>%s</th><td>%s</td></tr>\n",
			id, (value && *value) ? "" : "hidden=\"\"", label, value);

	} else {

		g_string_append_printf (
			buffer,
			"<tr id=\"%s\" hidden=\"\"><td colspan=\"2\"></td></tr>\n",
			id);

	}
}

static void
append_info_item_row (ItipView *view,
                      const gchar *table_id,
                      ItipViewInfoItem *item)
{
	WebKitDOMElement *table;
	WebKitDOMHTMLElement *row, *cell;
	const gchar *icon_name;
	gchar *id;

	table = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, table_id);
	row = webkit_dom_html_table_element_insert_row (
		WEBKIT_DOM_HTML_TABLE_ELEMENT (table), -1, NULL);

	id = g_strdup_printf ("%s_row_%d", table_id, item->id);
	webkit_dom_html_element_set_id (row, id);
	g_free (id);

	switch (item->type) {
		case ITIP_VIEW_INFO_ITEM_TYPE_INFO:
			icon_name = GTK_STOCK_DIALOG_INFO;
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_WARNING:
			icon_name = GTK_STOCK_DIALOG_WARNING;
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_ERROR:
			icon_name = GTK_STOCK_DIALOG_ERROR;
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS:
			icon_name = GTK_STOCK_FIND;
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_NONE:
			default:
			icon_name = NULL;
	}

	cell = webkit_dom_html_table_row_element_insert_cell (
		(WebKitDOMHTMLTableRowElement *) row, -1, NULL);

	if (icon_name) {
		WebKitDOMElement *image;
		gchar *icon_uri;

		image = webkit_dom_document_create_element (
			view->priv->dom_document, "IMG", NULL);

		icon_uri = g_strdup_printf ("gtk-stock://%s", icon_name);
		webkit_dom_html_image_element_set_src (
			WEBKIT_DOM_HTML_IMAGE_ELEMENT (image), icon_uri);
		g_free (icon_uri);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (cell),
			WEBKIT_DOM_NODE (image),
			NULL);
	}

	cell = webkit_dom_html_table_row_element_insert_cell (
		(WebKitDOMHTMLTableRowElement *) row, -1, NULL);

	webkit_dom_html_element_set_inner_html (cell, item->message, NULL);

	d(printf("Added row %s_row_%d ('%s')\n", table_id, item->id, item->message));
}

static void
remove_info_item_row (ItipView *view,
                      const gchar *table_id,
                      guint id)
{
	WebKitDOMElement *row;
	gchar *row_id;

	row_id = g_strdup_printf ("%s_row_%d", table_id, id);
	row = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, row_id);
	g_free (row_id);

	webkit_dom_node_remove_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (row)),
		WEBKIT_DOM_NODE (row),
		NULL);

	d(printf("Removed row %s_row_%d\n", table_id, id));
}

static void
buttons_table_write_button (GString *buffer,
                            const gchar *name,
                            const gchar *label,
                            const gchar *icon,
                            ItipViewResponse response)
{
	gchar *access_key, *html_label;

	html_label = parse_html_mnemonics (label, &access_key);

	g_string_append_printf (
		buffer,
		"<td><button type=\"button\" name=\"%s\" value=\"%d\" id=\"%s\" accesskey=\"%s\" hidden>"
		"<div><img src=\"gtk-stock://%s?size=%d\"> <span>%s</span></div>"
		"</button></td>\n",
		name, response, name, access_key ? access_key : "" , icon,
		GTK_ICON_SIZE_BUTTON, html_label);

	g_free (html_label);

	if (access_key)
		g_free (access_key);
}

static void
append_buttons_table (GString *buffer)
{
	g_string_append (buffer,
			 "<table class=\"itip buttons\" border=\"0\" "
				"id=\"" TABLE_BUTTONS "\" cellspacing=\"6\" "
				"cellpadding=\"0\" >"
			 "<tr id=\"" TABLE_ROW_BUTTONS "\">");

        /* Everything gets the open button */
	buttons_table_write_button (
		buffer, BUTTON_OPEN_CALENDAR, _("_Open Calendar"),
		GTK_STOCK_JUMP_TO, ITIP_VIEW_RESPONSE_OPEN);
	buttons_table_write_button (
		buffer, BUTTON_DECLINE_ALL, _("_Decline all"),
		GTK_STOCK_CANCEL, ITIP_VIEW_RESPONSE_DECLINE);
	buttons_table_write_button (
		buffer, BUTTON_DECLINE, _("_Decline"),
		GTK_STOCK_CANCEL, ITIP_VIEW_RESPONSE_DECLINE);
	buttons_table_write_button (
		buffer, BUTTON_TENTATIVE_ALL, _("_Tentative all"),
		GTK_STOCK_DIALOG_QUESTION, ITIP_VIEW_RESPONSE_TENTATIVE);
	buttons_table_write_button (
		buffer, BUTTON_TENTATIVE, _("_Tentative"),
		GTK_STOCK_DIALOG_QUESTION, ITIP_VIEW_RESPONSE_TENTATIVE);
	buttons_table_write_button (
		buffer, BUTTON_ACCEPT_ALL, _("A_ccept all"),
		GTK_STOCK_APPLY, ITIP_VIEW_RESPONSE_ACCEPT);
	buttons_table_write_button (
		buffer, BUTTON_ACCEPT, _("A_ccept"),
		GTK_STOCK_APPLY, ITIP_VIEW_RESPONSE_ACCEPT);
	buttons_table_write_button (
		buffer, BUTTON_SEND_INFORMATION, _("_Send Information"),
		GTK_STOCK_REFRESH, ITIP_VIEW_RESPONSE_REFRESH);
	buttons_table_write_button (
		buffer, BUTTON_UPDATE_ATTENDEE_STATUS, _("_Update Attendee Status"),
		GTK_STOCK_REFRESH, ITIP_VIEW_RESPONSE_UPDATE);
	buttons_table_write_button (
		buffer, BUTTON_UPDATE,  _("_Update"),
		GTK_STOCK_REFRESH, ITIP_VIEW_RESPONSE_CANCEL);

	g_string_append (buffer, "</tr></table>");
}

static void
itip_view_rebuild_source_list (ItipView *view)
{
	ESourceRegistry *registry;
	WebKitDOMElement *select;
	GList *list, *link;
	const gchar *extension_name;

	d(printf("Assigning a new source list!\n"));

	if (!view->priv->dom_document)
		return;

	registry = itip_view_get_registry (view);
	extension_name = itip_view_get_extension_name (view);

	select = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, SELECT_ESOURCE);

	while (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (select))) {
		webkit_dom_node_remove_child (
			WEBKIT_DOM_NODE (select),
			webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (select)),
			NULL);
	}

	if (extension_name == NULL)
		return;

	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		WebKitDOMElement *option;

		option = webkit_dom_document_create_element (
			view->priv->dom_document, "OPTION", NULL);
		webkit_dom_html_option_element_set_value (
			WEBKIT_DOM_HTML_OPTION_ELEMENT (option),
			e_source_get_uid (source));
		webkit_dom_html_option_element_set_label (
			WEBKIT_DOM_HTML_OPTION_ELEMENT (option),
			e_source_get_display_name (source));
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (option),
			e_source_get_display_name (source), NULL);
		webkit_dom_html_element_set_class_name (
			WEBKIT_DOM_HTML_ELEMENT (option), "calendar");

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (select),
			WEBKIT_DOM_NODE (option),
			NULL);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	source_changed_cb (select, NULL, view);
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
itip_view_set_registry (ItipView *view,
                        ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (view->priv->registry == NULL);

	view->priv->registry = g_object_ref (registry);
}

static void
itip_view_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXTENSION_NAME:
			itip_view_set_extension_name (
				ITIP_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_REGISTRY:
			itip_view_set_registry (
				ITIP_VIEW (object),
				g_value_get_object (value));
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
		case PROP_EXTENSION_NAME:
			g_value_set_string (
				value, itip_view_get_extension_name (
				ITIP_VIEW (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value, itip_view_get_registry (
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

	if (priv->registry != NULL) {
		g_signal_handler_disconnect (
			priv->registry, priv->source_added_id);
		g_signal_handler_disconnect (
			priv->registry, priv->source_removed_id);
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (itip_view_parent_class)->dispose (object);
}

static void
itip_view_finalize (GObject *object)
{
	ItipViewPrivate *priv;
	GSList *iter;

	priv = ITIP_VIEW_GET_PRIVATE (object);

	d(printf("Itip view finalized!\n"));

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

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (itip_view_parent_class)->finalize (object);
}

static void
itip_view_constructed (GObject *object)
{
	ItipView *view;
	ESourceRegistry *registry;

	view = ITIP_VIEW (object);
	registry = itip_view_get_registry (view);

	view->priv->source_added_id = g_signal_connect (
		registry, "source-added",
		G_CALLBACK (itip_view_source_added_cb), view);

	view->priv->source_removed_id = g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (itip_view_source_removed_cb), view);

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
		PROP_REGISTRY,
		g_param_spec_string (
			"extension-name",
			"Extension Name",
			"Show only data sources with this extension",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

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

static void
itip_view_init (ItipView *view)
{
	view->priv = ITIP_VIEW_GET_PRIVATE (view);
}

ItipView *
itip_view_new (ItipPURI *puri,
               ESourceRegistry *registry)
{
	ItipView *view;

	view = g_object_new (ITIP_TYPE_VIEW, "registry", registry, NULL);
	view->priv->puri = puri;

	return view;
}

void
itip_view_write (GString *buffer)
{
	g_string_append (buffer,
		"<html>\n"
		"<head>\n"
		"<title>ITIP</title>\n"
		"<link type=\"text/css\" rel=\"stylesheet\" href=\"evo-file://" EVOLUTION_PRIVDATADIR "/theme/webview.css\" />\n"
		"</head>\n"
		"<body>\n");

	g_string_append_printf (buffer,
		"<img src=\"gtk-stock://%s?size=%d\" class=\"itip icon\" />\n",
			MEETING_ICON, GTK_ICON_SIZE_BUTTON);

	g_string_append (buffer,
		"<div class=\"itip content\" id=\"" DIV_ITIP_CONTENT "\">\n");

        /* The first section listing the sender */
        /* FIXME What to do if the send and organizer do not match */
	g_string_append (buffer,
		"<div id=\"" TEXT_ROW_SENDER "\" class=\"itip sender\"></div>\n");

	g_string_append (buffer, "<hr>\n");

        /* Elementary event information */
	g_string_append (buffer,
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
	g_string_append (buffer,
		"<table class=\"itip info\" id=\"" TABLE_UPPER_ITIP_INFO "\" border=\"0\" "
		       "cellspacing=\"5\" cellpadding=\"0\">");

        /* Description */
	g_string_append (buffer,
		"<div id=\"" TABLE_ROW_DESCRIPTION "\" class=\"itip description\" hidden=\"\"></div>\n");

	g_string_append (buffer, "<hr>\n");

	/* Lower Info items */
	g_string_append (buffer,
		"<table class=\"itip info\" id=\"" TABLE_LOWER_ITIP_INFO "\" border=\"0\" "
		       "cellspacing=\"5\" cellpadding=\"0\">");

	g_string_append (buffer,
		"<table class=\"itip table\" border=\"0\" "
		       "cellspacing=\"5\" cellpadding=\"0\">\n");

	g_string_append (buffer,
		"<tr id=\"" TABLE_ROW_ESCB "\" hidden=\"\""">"
		"<th><label id=\"" TABLE_ROW_ESCB_LABEL "\" for=\"" SELECT_ESOURCE "\"></label></th>"
		"<td><select name=\"" SELECT_ESOURCE "\" id=\"" SELECT_ESOURCE "\"></select></td>"
		"</tr>\n");

	/* RSVP area */
	append_checkbox_table_row (buffer, CHECKBOX_RSVP, _("Send reply to sender"));

        /* Comments */
	g_string_append_printf (buffer,
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
	append_buttons_table (buffer);

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

	g_string_append (buffer,
		"<div class=\"itip print_content\" id=\"" DIV_ITIP_CONTENT "\">\n");

        /* The first section listing the sender */
        /* FIXME What to do if the send and organizer do not match */
	g_string_append_printf (buffer,
		"<div id=\"" TEXT_ROW_SENDER "\" class=\"itip sender\">%s</div>\n",
		view->priv->sender ? view->priv->sender : "");

	g_string_append (buffer, "<hr>\n");

        /* Elementary event information */
	g_string_append (buffer,
		"<table class=\"itip table\" border=\"0\" "
		       "cellspacing=\"5\" cellpadding=\"0\">\n");

	append_text_table_row (
		buffer, TABLE_ROW_SUMMARY,
		NULL, view->priv->summary);
	append_text_table_row (
		buffer, TABLE_ROW_LOCATION,
		_("Location:"), view->priv->location);
	append_text_table_row (
		buffer, TABLE_ROW_START_DATE,
		view->priv->start_header, view->priv->start_label);
	append_text_table_row (
		buffer, TABLE_ROW_END_DATE,
		view->priv->end_header, view->priv->end_label);
	append_text_table_row (
		buffer, TABLE_ROW_STATUS,
		_("Status:"), view->priv->status);
	append_text_table_row (
		buffer, TABLE_ROW_COMMENT,
		_("Comment:"), view->priv->comment);

	g_string_append (buffer, "</table>\n");

        /* Description */
	g_string_append_printf (
		buffer,
		"<div id=\"" TABLE_ROW_DESCRIPTION "\" "
		     "class=\"itip description\" %s>%s</div>\n",
		view->priv->description ? "" : "hidden=\"\"", view->priv->description);

	g_string_append (buffer, "</div>");
}

void
itip_view_create_dom_bindings (ItipView *view,
                               WebKitDOMElement *element)
{
	WebKitDOMElement *el;
	WebKitDOMDocument *doc;

	doc = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));
	view->priv->dom_document = doc;

	el = webkit_dom_document_get_element_by_id (doc, CHECKBOX_RECUR);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (recur_toggled_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, CHECKBOX_RSVP);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (rsvp_toggled_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, CHECKBOX_INHERIT_ALARM);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (alarm_check_toggled_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, CHECKBOX_KEEP_ALARM);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (alarm_check_toggled_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, BUTTON_OPEN_CALENDAR);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, BUTTON_ACCEPT);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, BUTTON_ACCEPT_ALL);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, BUTTON_TENTATIVE);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, BUTTON_TENTATIVE_ALL);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, BUTTON_DECLINE);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, BUTTON_DECLINE_ALL);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, BUTTON_UPDATE);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, BUTTON_UPDATE_ATTENDEE_STATUS);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, BUTTON_SEND_INFORMATION);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}

	el = webkit_dom_document_get_element_by_id (doc, SELECT_ESOURCE);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "change",
			G_CALLBACK (source_changed_cb), FALSE, view);
	}
}

ItipPURI *
itip_view_get_puri (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->puri;
}

ESourceRegistry *
itip_view_get_registry (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->registry;
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

static void
show_button (ItipView *view,
             const gchar *id)
{
	WebKitDOMElement *button;

	button = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, id);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (button), FALSE);
}

void
itip_view_set_mode (ItipView *view,
                    ItipViewMode mode)
{
	WebKitDOMElement *row, *cell;
	WebKitDOMElement *button;

	g_return_if_fail (ITIP_IS_VIEW (view));

	view->priv->mode = mode;

	set_sender_text (view);

	if (!view->priv->dom_document)
		return;

	row = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TABLE_ROW_BUTTONS);
	cell = webkit_dom_element_get_first_element_child (row);
	do {
		button = webkit_dom_element_get_first_element_child (cell);
		webkit_dom_html_element_set_hidden (
			WEBKIT_DOM_HTML_ELEMENT (button), TRUE);
	} while ((cell = webkit_dom_element_get_next_element_sibling (cell)) != NULL);

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
	WebKitDOMElement *label;
	const gchar *header;
	gchar *access_key, *html_label;

	g_return_if_fail (ITIP_IS_VIEW (view));

	view->priv->type = type;

	if (!view->priv->dom_document)
		return;

	label = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TABLE_ROW_ESCB_LABEL);

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

	html_label = parse_html_mnemonics (header, &access_key);

	webkit_dom_html_element_set_access_key (
		WEBKIT_DOM_HTML_ELEMENT (label), access_key);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (label), html_label, NULL);

	g_free (html_label);

	if (access_key)
		g_free (access_key);

	set_sender_text (view);
}

ECalClientSourceType
itip_view_get_item_type (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), ITIP_VIEW_MODE_NONE);

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
	WebKitDOMElement *row, *col;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->summary)
		g_free (view->priv->summary);

	view->priv->summary = summary ? g_strstrip (e_utf8_ensure_valid (summary)) : NULL;

	if (!view->priv->dom_document)
		return;

	row = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TABLE_ROW_SUMMARY);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (row), (view->priv->summary == NULL));

	col = webkit_dom_element_get_last_element_child (row);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (col),
		view->priv->summary ? view->priv->summary : "",
		NULL);
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
	WebKitDOMElement *row, *col;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->location)
		g_free (view->priv->location);

	view->priv->location = location ? g_strstrip (e_utf8_ensure_valid (location)) : NULL;

	if (!view->priv->dom_document)
		return;

	row = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TABLE_ROW_LOCATION);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (row), (view->priv->location == NULL));

	col = webkit_dom_element_get_last_element_child (row);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (col),
		view->priv->location ? view->priv->location : "",
		NULL);
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
	WebKitDOMElement *row, *col;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->status)
		g_free (view->priv->status);

	view->priv->status = status ? g_strstrip (e_utf8_ensure_valid (status)) : NULL;

	if (!view->priv->dom_document)
		return;

	row = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TABLE_ROW_STATUS);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (row), (view->priv->status == NULL));

	col = webkit_dom_element_get_last_element_child (row);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (col),
		view->priv->status ? view->priv->status : "",
		NULL);
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
	WebKitDOMElement *row, *col;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->comment)
		g_free (view->priv->comment);

	view->priv->comment = comment ? g_strstrip (e_utf8_ensure_valid (comment)) : NULL;

	if (!view->priv->dom_document)
		return;

	row = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TABLE_ROW_COMMENT);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (row), (view->priv->comment == NULL));

	col = webkit_dom_element_get_last_element_child (row);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (col),
		view->priv->comment ? view->priv->comment : "",
		NULL);
}

const gchar *
itip_view_get_comment (ItipView *view)
{
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	return view->priv->comment;
}

void
itip_view_set_description (ItipView *view,
                           const gchar *description)
{
	WebKitDOMElement *div;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (view->priv->description)
		g_free (view->priv->description);

	view->priv->description = description ? g_strstrip (e_utf8_ensure_valid (description)) : NULL;

	if (!view->priv->dom_document)
		return;

	div = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TABLE_ROW_DESCRIPTION);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (div), (view->priv->description == NULL));

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (div),
		view->priv->description ? view->priv->description : "",
		NULL);
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

	if (!view->priv->dom_document)
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

			if (!view->priv->dom_document)
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

		if (view->priv->dom_document)
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

	if (!view->priv->dom_document)
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

			if (view->priv->dom_document)
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

		if (view->priv->dom_document)
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
	WebKitDOMElement *select;
	WebKitDOMElement *row;
	ESource *selected_source;
	gulong i, len;

	g_return_if_fail (ITIP_IS_VIEW (view));

	d(printf("Settings default source '%s'\n", e_source_get_display_name (source)));

	if (!view->priv->dom_document)
		return;

	row = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TABLE_ROW_ESCB);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (row), (source == NULL));
	if (source == NULL)
		return;

	select = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, SELECT_ESOURCE);

	/* <select> does not emit 'change' event when already selected
	 * <option> is re-selected, but we need to notify itip formatter,
	 * so that it would make all the buttons sensitive. */
	selected_source = itip_view_ref_source (view);
	if (source == selected_source)
		source_changed_cb (select, NULL, view);
	if (selected_source != NULL)
		g_object_unref (selected_source);

	if (webkit_dom_html_select_element_get_disabled (
			WEBKIT_DOM_HTML_SELECT_ELEMENT (select))) {
		webkit_dom_html_select_element_set_disabled (
			WEBKIT_DOM_HTML_SELECT_ELEMENT (select), FALSE);
	}

	len = webkit_dom_html_select_element_get_length (
		WEBKIT_DOM_HTML_SELECT_ELEMENT (select));
	for (i = 0; i < len; i++) {

		WebKitDOMNode *node;
		WebKitDOMHTMLOptionElement *option;
		gchar *value;

		node = webkit_dom_html_select_element_item (
			WEBKIT_DOM_HTML_SELECT_ELEMENT (select), i);
		option = WEBKIT_DOM_HTML_OPTION_ELEMENT (node);

		value = webkit_dom_html_option_element_get_value (option);
		if (g_strcmp0 (value, e_source_get_uid (source)) == 0) {
			webkit_dom_html_option_element_set_selected (
				option, TRUE);

			g_free (value);
			break;
		}

		g_free (value);
	}
}

ESource *
itip_view_ref_source (ItipView *view)
{
	ESourceRegistry *registry;
	WebKitDOMElement *select;
	gchar *uid;
	ESource *source;
	gboolean disable = FALSE;

	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	if (!view->priv->dom_document)
		return NULL;

	select = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, SELECT_ESOURCE);
	if (webkit_dom_html_select_element_get_disabled (
			WEBKIT_DOM_HTML_SELECT_ELEMENT (select))) {
		webkit_dom_html_select_element_set_disabled (
		WEBKIT_DOM_HTML_SELECT_ELEMENT (select), FALSE);
		disable = TRUE;
	}

	uid = webkit_dom_html_select_element_get_value (
		WEBKIT_DOM_HTML_SELECT_ELEMENT (select));

	registry = itip_view_get_registry (view);
	source = e_source_registry_ref_source (registry, uid);

	g_free (uid);

	if (disable) {
		webkit_dom_html_select_element_set_disabled (
			WEBKIT_DOM_HTML_SELECT_ELEMENT (select), TRUE);
	}

	return source;
}

void
itip_view_set_rsvp (ItipView *view,
                    gboolean rsvp)
{
	WebKitDOMElement *el;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->dom_document)
		return;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_RSVP);
	webkit_dom_html_input_element_set_checked (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), rsvp);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TEXTAREA_RSVP_COMMENT);
	webkit_dom_html_text_area_element_set_disabled (
		WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (el), !rsvp);
}

gboolean
itip_view_get_rsvp (ItipView *view)
{
	WebKitDOMElement *el;

	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	if (!view->priv->dom_document)
		return FALSE;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_UPDATE);
	return webkit_dom_html_input_element_get_checked (WEBKIT_DOM_HTML_INPUT_ELEMENT (el));
}

void
itip_view_set_show_rsvp_check (ItipView *view,
                               gboolean show)
{
	WebKitDOMElement *label;
	WebKitDOMElement *el;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->dom_document)
		return;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, "table_row_" CHECKBOX_RSVP);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (el), !show);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_RSVP);
	label = webkit_dom_element_get_next_element_sibling (el);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (label), !show);

	if (!show) {
		webkit_dom_html_input_element_set_checked (
			WEBKIT_DOM_HTML_INPUT_ELEMENT (el), FALSE);
	}

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TABLE_ROW_RSVP_COMMENT);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (el), !show);
}

gboolean
itip_view_get_show_rsvp_check (ItipView *view)
{
	WebKitDOMElement *el;

	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	if (!view->priv->dom_document)
		return FALSE;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_RSVP);
	return !webkit_dom_html_element_get_hidden (WEBKIT_DOM_HTML_ELEMENT (el));
}

void
itip_view_set_update (ItipView *view,
                      gboolean update)
{
	WebKitDOMElement *el;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->dom_document)
		return;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_UPDATE);

	webkit_dom_html_input_element_set_checked (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), update);
}

gboolean
itip_view_get_update (ItipView *view)
{
	WebKitDOMElement *el;

	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	if (!view->priv->dom_document)
		return FALSE;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_UPDATE);
	return webkit_dom_html_input_element_get_checked (WEBKIT_DOM_HTML_INPUT_ELEMENT (el));
}

void
itip_view_set_show_update_check (ItipView *view,
                                 gboolean show)
{
	WebKitDOMElement *label;
	WebKitDOMElement *el;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->dom_document)
		return;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, "table_row_" CHECKBOX_UPDATE);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (el), !show);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_UPDATE);
	label = webkit_dom_element_get_next_element_sibling (el);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (label), !show);

	if (!show) {
		webkit_dom_html_input_element_set_checked (
			WEBKIT_DOM_HTML_INPUT_ELEMENT (el), FALSE);
	}
}

gboolean
itip_view_get_show_update_check (ItipView *view)
{
	WebKitDOMElement *el;

	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	if (!view->priv->dom_document)
		return FALSE;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_UPDATE);
	return !webkit_dom_html_element_get_hidden (WEBKIT_DOM_HTML_ELEMENT (el));
}

void
itip_view_set_rsvp_comment (ItipView *view,
                            const gchar *comment)
{
	WebKitDOMElement *el;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->dom_document)
		return;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TEXTAREA_RSVP_COMMENT);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (el), (comment == NULL));

	if (comment) {
		webkit_dom_html_text_area_element_set_value (
			WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (el), comment);
	}
}

gchar *
itip_view_get_rsvp_comment (ItipView *view)
{
	WebKitDOMElement *el;

	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);

	if (!view->priv->dom_document)
		return NULL;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TEXTAREA_RSVP_COMMENT);

	if (webkit_dom_html_element_get_hidden (WEBKIT_DOM_HTML_ELEMENT (el))) {
		return NULL;
	}

	return webkit_dom_html_text_area_element_get_value (
		WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (el));
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
	WebKitDOMElement *el, *cell;

	g_return_if_fail (ITIP_IS_VIEW (view));

	d(printf("Settings buttons %s\n", sensitive ? "sensitive" : "insensitive"));

	view->priv->buttons_sensitive = sensitive;

	if (!view->priv->dom_document)
		return;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_UPDATE);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_RECUR);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_FREE_TIME);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_KEEP_ALARM);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_INHERIT_ALARM);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_RSVP);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TEXTAREA_RSVP_COMMENT);
	webkit_dom_html_text_area_element_set_disabled (
		WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, SELECT_ESOURCE);
	webkit_dom_html_select_element_set_disabled (
		WEBKIT_DOM_HTML_SELECT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, TABLE_ROW_BUTTONS);
	cell = webkit_dom_element_get_first_element_child (el);
	do {
		WebKitDOMElement *btn;
		btn = webkit_dom_element_get_first_element_child (cell);
		if (!webkit_dom_html_element_get_hidden (
			WEBKIT_DOM_HTML_ELEMENT (btn))) {
			webkit_dom_html_button_element_set_disabled (
				WEBKIT_DOM_HTML_BUTTON_ELEMENT (btn), !sensitive);
		}
	} while ((cell = webkit_dom_element_get_next_element_sibling (cell)) != NULL);
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
	WebKitDOMElement *el;

	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	if (!view->priv->dom_document)
		return FALSE;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_RECUR);
	return webkit_dom_html_input_element_get_checked (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el));
}

void
itip_view_set_show_recur_check (ItipView *view,
                                gboolean show)
{
	WebKitDOMElement *label;
	WebKitDOMElement *el;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->dom_document)
		return;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, "table_row_" CHECKBOX_RECUR);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (el), !show);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_RECUR);
	label = webkit_dom_element_get_next_element_sibling (el);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (label), !show);

	if (!show) {
		webkit_dom_html_input_element_set_checked (
			WEBKIT_DOM_HTML_INPUT_ELEMENT (el), FALSE);
	}

        /* and update state of the second check */
	alarm_check_toggled_cb (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el),
		NULL, view);
}

void
itip_view_set_show_free_time_check (ItipView *view,
                                    gboolean show)
{
	WebKitDOMElement *label;
	WebKitDOMElement *el;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->dom_document)
		return;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, "table_row_" CHECKBOX_FREE_TIME);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (el), !show);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_FREE_TIME);
	label = webkit_dom_element_get_next_element_sibling (el);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (label), !show);

	if (!show) {
		webkit_dom_html_input_element_set_checked (
			WEBKIT_DOM_HTML_INPUT_ELEMENT (el), FALSE);
	}

        /* and update state of the second check */
	alarm_check_toggled_cb (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el),
		NULL, view);
}

gboolean
itip_view_get_free_time_check_state (ItipView *view)
{
	WebKitDOMElement *el;

	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	if (!view->priv->dom_document)
		return FALSE;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_FREE_TIME);
	return webkit_dom_html_input_element_get_checked (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el));
}

void
itip_view_set_show_keep_alarm_check (ItipView *view,
                                     gboolean show)
{
	WebKitDOMElement *label;
	WebKitDOMElement *el;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->dom_document)
		return;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, "table_row_" CHECKBOX_KEEP_ALARM);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (el), !show);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_KEEP_ALARM);
	label = webkit_dom_element_get_next_element_sibling (el);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (label), !show);

	if (!show) {
		webkit_dom_html_input_element_set_checked (
			WEBKIT_DOM_HTML_INPUT_ELEMENT (el), FALSE);
	}

        /* and update state of the second check */
	alarm_check_toggled_cb (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el),
		NULL, view);
}

gboolean
itip_view_get_keep_alarm_check_state (ItipView *view)
{
	WebKitDOMElement *el;

	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	if (!view->priv->dom_document)
		return FALSE;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_KEEP_ALARM);
	return webkit_dom_html_input_element_get_checked (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el));
}

void
itip_view_set_show_inherit_alarm_check (ItipView *view,
                                        gboolean show)
{
	WebKitDOMElement *label;
	WebKitDOMElement *el;

	g_return_if_fail (ITIP_IS_VIEW (view));

	if (!view->priv->dom_document)
		return;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, "table_row_" CHECKBOX_INHERIT_ALARM);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (el), !show);

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_INHERIT_ALARM);
	label = webkit_dom_element_get_next_element_sibling (el);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (label), !show);

	if (!show) {
		webkit_dom_html_input_element_set_checked (
			WEBKIT_DOM_HTML_INPUT_ELEMENT (el), FALSE);
	}

	/* and update state of the second check */
	alarm_check_toggled_cb (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el),
		NULL, view);
}

gboolean
itip_view_get_inherit_alarm_check_state (ItipView *view)
{
	WebKitDOMElement *el;

	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);

	if (!view->priv->dom_document)
		return FALSE;

	el = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, CHECKBOX_INHERIT_ALARM);
	return webkit_dom_html_input_element_get_checked (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el));
}

void
itip_view_set_error (ItipView *view,
                     const gchar *error_html,
                     gboolean show_save_btn)
{
	WebKitDOMElement *content, *error;
	GString *str;

	g_return_if_fail (ITIP_IS_VIEW (view));
	g_return_if_fail (error_html);

	str = g_string_new (error_html);

	if (show_save_btn) {
		g_string_append (str,
			"<table border=\"0\" width=\"100%\">"
			"<tr width=\"100%\" id=\"" TABLE_ROW_BUTTONS "\">");

		buttons_table_write_button (
			str, BUTTON_SAVE, _("_Save"),
			GTK_STOCK_SAVE, ITIP_VIEW_RESPONSE_SAVE);

		g_string_append (str, "</tr></table>");
	}

	view->priv->error = str->str;
	g_string_free (str, FALSE);

	if (!view->priv->dom_document)
		return;

	content = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, DIV_ITIP_CONTENT);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (content), TRUE);

	error = webkit_dom_document_get_element_by_id (
		view->priv->dom_document, DIV_ITIP_ERROR);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (error), FALSE);

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (error), view->priv->error, NULL);

	if (show_save_btn) {
		WebKitDOMElement *el;

		show_button (view, BUTTON_SAVE);

		el = webkit_dom_document_get_element_by_id (
			view->priv->dom_document, BUTTON_SAVE);
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (button_clicked_cb), FALSE, view);
	}
}
