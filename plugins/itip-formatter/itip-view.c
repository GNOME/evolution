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
#include <libedataserver/e-time-utils.h>
#include <libedataserverui/e-source-option-menu.h>
#include <libecal/e-cal.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <mail/em-format-hook.h>
#include <mail/em-format-html.h>
#include <e-util/e-account-list.h>
#include <e-util/e-icon-factory.h>
#include <e-util/e-time-utils.h>
#include <e-util/e-gtk-utils.h>
#include <calendar/gui/itip-utils.h>
#include "itip-view.h"

#define MEETING_ICON "stock_new-meeting"

G_DEFINE_TYPE (ItipView, itip_view, GTK_TYPE_HBOX);

typedef struct  {
	ItipViewInfoItemType type;
	char *message;

	guint id;
} ItipViewInfoItem;

struct _ItipViewPrivate {
	ItipViewMode mode;
	ECalSourceType type;
	
	GtkWidget *sender_label;
	char *organizer;
	char *sentby;
	char *delegator;
	char *attendee;

	GtkWidget *summary_label;
	char *summary;
	
	GtkWidget *location_header;
	GtkWidget *location_label;
	char *location;

	GtkWidget *status_header;
	GtkWidget *status_label;
	char *status;

	GtkWidget *comment_header;
	GtkWidget *comment_label;
	char *comment;

	GtkWidget *start_header;
	GtkWidget *start_label;
	struct tm *start_tm;
	
	GtkWidget *end_header;
	GtkWidget *end_label;
	struct tm *end_tm;

	GtkWidget *upper_info_box;
	GSList *upper_info_items;

	GtkWidget *lower_info_box;
	GSList *lower_info_items;

	guint next_info_item_id;

	GtkWidget *description_label;
	char *description;

	GtkWidget *selector_box;
	GtkWidget *esom;
	GtkWidget *esom_header;
	ESourceList *source_list;
	
	GtkWidget *rsvp_box;
	GtkWidget *rsvp_check;
	GtkWidget *rsvp_comment_header;
	GtkWidget *rsvp_comment_entry;
	gboolean rsvp_show;

	GtkWidget *button_box;
	gboolean buttons_sensitive;
};

/* Signal IDs */
enum {
	SOURCE_SELECTED,
	RESPONSE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
format_date_and_time_x		(struct tm	*date_tm,
				 struct tm      *current_tm,
				 gboolean	 use_24_hour_format,
				 gboolean	 show_midnight,
				 gboolean	 show_zero_seconds,
				 char		*buffer,
				 int		 buffer_size)
{
	char *format;
	struct tm tomorrow_tm, week_tm;
	
	/* Calculate a normalized "tomorrow" */
	tomorrow_tm = *current_tm;
	if (tomorrow_tm.tm_mday == time_days_in_month (date_tm->tm_year + 1900, date_tm->tm_mon)) {
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
	if (week_tm.tm_mday + 6 > time_days_in_month (date_tm->tm_year + 1900, date_tm->tm_mon)) {
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
		if (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0) {
			/* strftime format of a weekday and a date. */
			format = _("Today");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a time, 
				   in 24-hour format, without seconds. */
				format = _("Today %H:%M");
			else
				/* strftime format of a time, 
				   in 24-hour format. */
				format = _("Today %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a time,
				   in 12-hour format, without seconds. */
				format = _("Today %l:%M %p");
			else
				/* strftime format of a time, 
				   in 12-hour format. */
				format = _("Today %l:%M:%S %p");
		}

	/* Tomorrow */		
	} else if (date_tm->tm_mday == tomorrow_tm.tm_mday &&
		   date_tm->tm_mon == tomorrow_tm.tm_mon &&
		   date_tm->tm_year == tomorrow_tm.tm_year) {
		if (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0) {
			/* strftime format of a weekday and a date. */
			format = _("Tomorrow");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a time, 
				   in 24-hour format, without seconds. */
				format = _("Tomorrow %H:%M");
			else
				/* strftime format of a time, 
				   in 24-hour format. */
				format = _("Tomorrow %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a time,
				   in 12-hour format, without seconds. */
				format = _("Tomorrow %l:%M %p");
			else
				/* strftime format of a time, 
				   in 12-hour format. */
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
		if (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0) {
			/* strftime format of a weekday. */
			format = _("%A");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday and a
				   time, in 24-hour format, without seconds. */
				format = _("%A %H:%M");
			else
				/* strftime format of a weekday and a
				   time, in 24-hour format. */
				format = _("%A %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday and a
				   time, in 12-hour format, without seconds. */
				format = _("%A %l:%M %p");
			else
				/* strftime format of a weekday and a
				   time, in 12-hour format. */
				format = _("%A %l:%M:%S %p");
		}

	/* This Year */		
	} else if (date_tm->tm_year == current_tm->tm_year) {
		if (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0) {
			/* strftime format of a weekday and a date
			   without a year. */
			format = _("%A, %B %e");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date 
				   without a year and a time, 
				   in 24-hour format, without seconds. */
				format = _("%A, %B %e %H:%M");
			else
				/* strftime format of a weekday, a date without a year
				   and a time, in 24-hour format. */
				format = _("%A, %B %e %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date without a year 
				   and a time, in 12-hour format, without seconds. */
				format = _("%A, %B %e %l:%M %p");
			else
				/* strftime format of a weekday, a date without a year
				   and a time, in 12-hour format. */
				format = _("%A, %B %e %l:%M:%S %p");
		}
	} else {
		if (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0) {
			/* strftime format of a weekday and a date. */
			format = _("%A, %B %e, %Y");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				   time, in 24-hour format, without seconds. */
				format = _("%A, %B %e, %Y %H:%M");
			else
				/* strftime format of a weekday, a date and a
				   time, in 24-hour format. */
				format = _("%A, %B %e, %Y %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format, without seconds. */
				format = _("%A, %B %e, %Y %l:%M %p");
			else
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format. */
				format = _("%A, %B %e, %Y %l:%M:%S %p");
		}
	}
	
	/* strftime returns 0 if the string doesn't fit, and leaves the buffer
	   undefined, so we set it to the empty string in that case. */
	if (e_utf8_strftime (buffer, buffer_size, format, date_tm) == 0)
		buffer[0] = '\0';
}

static void
set_calendar_sender_text (ItipView *view)
{
	ItipViewPrivate *priv;
	const char *organizer, *attendee;
	char *sender = NULL;

	priv = view->priv;

	organizer = priv->organizer ? priv->organizer : _("An unknown person");
	attendee = priv->attendee ? priv->attendee : _("An unknown person");
	
	switch (priv->mode) {
	case ITIP_VIEW_MODE_PUBLISH:
		if (priv->sentby)
			sender = g_strdup_printf (_("<b>%s</b> through %s has published the following meeting information:"), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> has published the following meeting information:"), organizer);
		break;
	case ITIP_VIEW_MODE_REQUEST:
		/* FIXME is the delegator stuff handled correctly here? */
		if (priv->delegator) {
			sender = g_strdup_printf (_("<b>%s</b> requests the presence of %s at the following meeting:"), organizer, priv->delegator);
		} else {
			if (priv->sentby)
				sender = g_strdup_printf (_("<b>%s</b> through %s requests your presence at the following meeting:"), organizer, priv->sentby);
			else
				sender = g_strdup_printf (_("<b>%s</b> requests your presence at the following meeting:"), organizer);
		}
		break;
	case ITIP_VIEW_MODE_ADD:
		/* FIXME What text for this? */
		if (priv->sentby)
			sender = g_strdup_printf (_("<b>%s</b> through %s wishes to add to an existing meeting:"), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> wishes to add to an existing meeting:"), organizer);
		break;
	case ITIP_VIEW_MODE_REFRESH:
		sender = g_strdup_printf (_("<b>%s</b> wishes to receive the latest information for the following meeting:"), attendee);
		break;
	case ITIP_VIEW_MODE_REPLY:
		sender = g_strdup_printf (_("<b>%s</b> has sent back the following meeting response:"), attendee);
		break;
	case ITIP_VIEW_MODE_CANCEL:
		if (priv->sentby)
			sender = g_strdup_printf (_("<b>%s</b> through %s has cancelled the following meeting:"), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> has cancelled the following meeting."), organizer);
		break;
	case ITIP_VIEW_MODE_COUNTER:
		sender = g_strdup_printf (_("<b>%s</b> has proposed the following meeting changes."), attendee);
		break;
	case ITIP_VIEW_MODE_DECLINECOUNTER:
		if (priv->sentby)
			sender = g_strdup_printf (_("<b>%s</b> through %s has declined the following meeting changes:"), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> has declined the following meeting changes."), organizer);
		break;
	default:
		break;
	}

	gtk_label_set_text (GTK_LABEL (priv->sender_label), sender);
	gtk_label_set_use_markup (GTK_LABEL (priv->sender_label), TRUE);

	g_free (sender);
}

static void
set_tasklist_sender_text (ItipView *view)
{
	ItipViewPrivate *priv;
	const char *organizer, *attendee;
	char *sender = NULL;

	priv = view->priv;

	organizer = priv->organizer ? priv->organizer : _("An unknown person");
	attendee = priv->attendee ? priv->attendee : _("An unknown person");
	
	switch (priv->mode) {
	case ITIP_VIEW_MODE_PUBLISH:
		if (priv->sentby)
			sender = g_strdup_printf (_("<b>%s</b> through %s has published the following task:"), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> has published the following task:"), organizer);
		break;
	case ITIP_VIEW_MODE_REQUEST:
		/* FIXME is the delegator stuff handled correctly here? */
		if (priv->delegator) {
			sender = g_strdup_printf (_("<b>%s</b> requests the assignment of %s to the following task:"), organizer, priv->delegator);
		} else {
			if (priv->sentby)
				sender = g_strdup_printf (_("<b>%s</b> through %s has assigned you a task:"), organizer, priv->sentby);
			else
				sender = g_strdup_printf (_("<b>%s</b> has assigned you a task:"), organizer);
		}
		break;
	case ITIP_VIEW_MODE_ADD:
		/* FIXME What text for this? */
		if (priv->sentby)
			sender = g_strdup_printf (_("<b>%s</b> through %s wishes to add to an existing task:"), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> wishes to add to an existing task:"), organizer);
		break;
	case ITIP_VIEW_MODE_REFRESH:
		sender = g_strdup_printf (_("<b>%s</b> wishes to receive the latest information for the following assigned task:"), attendee);
		break;
	case ITIP_VIEW_MODE_REPLY:
		sender = g_strdup_printf (_("<b>%s</b> has sent back the following assigned task response:"), attendee);
		break;
	case ITIP_VIEW_MODE_CANCEL:
		if (priv->sentby)
			sender = g_strdup_printf (_("<b>%s</b> through %s has cancelled the following assigned task:"), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> has cancelled the following assigned task:"), organizer);
		break;
	case ITIP_VIEW_MODE_COUNTER:
		sender = g_strdup_printf (_("<b>%s</b> has proposed the following task assignment changes:"), attendee);
		break;
	case ITIP_VIEW_MODE_DECLINECOUNTER:
		if (priv->sentby)
			sender = g_strdup_printf (_("<b>%s</b> through %s has declined the following assigned task:"), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> has declined the following assigned task:"), organizer);
		break;
	default:
		break;
	}

	gtk_label_set_text (GTK_LABEL (priv->sender_label), sender);
	gtk_label_set_use_markup (GTK_LABEL (priv->sender_label), TRUE);

	g_free (sender);
}

static void
set_sender_text (ItipView *view)
{
	ItipViewPrivate *priv;

	priv = view->priv;
	
	switch (priv->type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		set_calendar_sender_text (view);
		break;
	case E_CAL_SOURCE_TYPE_TODO:
		set_tasklist_sender_text (view);
		break;
	default:
		break;
	}
}

static void
set_summary_text (ItipView *view)
{
	ItipViewPrivate *priv;
	char *summary = NULL;

	priv = view->priv;

	summary = g_strdup_printf ("<b>%s</b>", priv->summary);

	gtk_label_set_text (GTK_LABEL (priv->summary_label), summary);
	gtk_label_set_use_markup (GTK_LABEL (priv->summary_label), TRUE);

	g_free (summary);
}

static void
set_location_text (ItipView *view)
{
	ItipViewPrivate *priv;

	priv = view->priv;

	gtk_label_set_text (GTK_LABEL (priv->location_label), priv->location);

	priv->location ? gtk_widget_show (priv->location_header) : gtk_widget_hide (priv->location_header);
	priv->location ? gtk_widget_show (priv->location_label) : gtk_widget_hide (priv->location_label);
}

static void
set_status_text (ItipView *view)
{
	ItipViewPrivate *priv;

	priv = view->priv;

	gtk_label_set_text (GTK_LABEL (priv->status_label), priv->status);

	priv->status ? gtk_widget_show (priv->status_header) : gtk_widget_hide (priv->status_header);
	priv->status ? gtk_widget_show (priv->status_label) : gtk_widget_hide (priv->status_label);
}

static void
set_comment_text (ItipView *view)
{
	ItipViewPrivate *priv;

	priv = view->priv;

	gtk_label_set_text (GTK_LABEL (priv->comment_label), priv->comment);

	priv->comment ? gtk_widget_show (priv->comment_header) : gtk_widget_hide (priv->comment_header);
	priv->comment ? gtk_widget_show (priv->comment_label) : gtk_widget_hide (priv->comment_label);
}

static void
set_description_text (ItipView *view)
{
	ItipViewPrivate *priv;

	priv = view->priv;

	gtk_label_set_text (GTK_LABEL (priv->description_label), priv->description);

	priv->description ? gtk_widget_show (priv->description_label) : gtk_widget_hide (priv->description_label);
}

static void
set_start_text (ItipView *view)
{
	ItipViewPrivate *priv;
	char buffer[256];
	time_t now;
	struct tm *now_tm;
	
	priv = view->priv;

	now = time (NULL);
	now_tm = localtime (&now);
	
	if (priv->start_tm) {
		format_date_and_time_x (priv->start_tm, now_tm, FALSE, TRUE, FALSE, buffer, 256);
		gtk_label_set_text (GTK_LABEL (priv->start_label), buffer);
	} else {
		gtk_label_set_text (GTK_LABEL (priv->start_label), NULL);
	}

	priv->start_tm ? gtk_widget_show (priv->start_header) : gtk_widget_hide (priv->start_header);
	priv->start_tm ? gtk_widget_show (priv->start_label) : gtk_widget_hide (priv->start_label);
}

static void
set_end_text (ItipView *view)
{
	ItipViewPrivate *priv;
	char buffer[256];
	time_t now;
	struct tm *now_tm;
	
	priv = view->priv;

	now = time (NULL);
	now_tm = localtime (&now);

	if (priv->end_tm) {
		format_date_and_time_x (priv->end_tm, now_tm, FALSE, TRUE, FALSE, buffer, 256);
		gtk_label_set_text (GTK_LABEL (priv->end_label), buffer);
	} else {
		gtk_label_set_text (GTK_LABEL (priv->end_label), NULL);
	}

	priv->end_tm ? gtk_widget_show (priv->end_header) : gtk_widget_hide (priv->end_header);
	priv->end_tm ? gtk_widget_show (priv->end_label) : gtk_widget_hide (priv->end_label);
}

static void
set_info_items (GtkWidget *info_box, GSList *info_items) 
{
	GSList *l;
	
	gtk_container_foreach (GTK_CONTAINER (info_box), (GtkCallback) gtk_widget_destroy, NULL);
	
	for (l = info_items; l; l = l->next) {
		ItipViewInfoItem *item = l->data;	
		GtkWidget *hbox, *image, *label;
		
		hbox = gtk_hbox_new (FALSE, 0);

		switch (item->type) {
		case ITIP_VIEW_INFO_ITEM_TYPE_INFO:
			image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_SMALL_TOOLBAR);
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_WARNING:			
			image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_SMALL_TOOLBAR);
			break;			
		case ITIP_VIEW_INFO_ITEM_TYPE_ERROR:
			image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_SMALL_TOOLBAR);
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS:
			image = e_icon_factory_get_image ("stock_animation", E_ICON_SIZE_BUTTON);
			break;
		case ITIP_VIEW_INFO_ITEM_TYPE_NONE:
		default:
			image = NULL;
		}
		
		if (image) {
			gtk_widget_show (image);
			gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 6);
		}
		
		label = gtk_label_new (item->message);
		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

		gtk_widget_show (hbox);
		gtk_box_pack_start (GTK_BOX (info_box), hbox, FALSE, FALSE, 6);
	}	
}

static void
set_upper_info_items (ItipView *view)
{
	ItipViewPrivate *priv;
	
	priv = view->priv;

	set_info_items (priv->upper_info_box, priv->upper_info_items);
}

static void
set_lower_info_items (ItipView *view)
{
	ItipViewPrivate *priv;
	
	priv = view->priv;

	set_info_items (priv->lower_info_box, priv->lower_info_items);
}

#define DATA_RESPONSE_KEY "ItipView::button_response"

static void
button_clicked_cb (GtkWidget *widget, gpointer data) 
{
	ItipViewResponse response;
	
	response = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), DATA_RESPONSE_KEY));

	g_message ("Response %d", response);
	g_signal_emit (G_OBJECT (data), signals[RESPONSE], 0, response);
}

static void
set_one_button (ItipView *view, char *label, char *stock_id, ItipViewResponse response) 
{
	ItipViewPrivate *priv;
	GtkWidget *button;
	
	priv = view->priv;

	button = e_gtk_button_new_with_icon (label, stock_id);
	g_object_set_data (G_OBJECT (button), DATA_RESPONSE_KEY, GINT_TO_POINTER (response));
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (priv->button_box), button);

	g_signal_connect (button, "clicked", G_CALLBACK (button_clicked_cb), view);
}

static void
set_buttons (ItipView *view) 
{
	ItipViewPrivate *priv;
	
	priv = view->priv;

	gtk_container_foreach (GTK_CONTAINER (priv->button_box), (GtkCallback) gtk_widget_destroy, NULL);

	/* Everything gets the open button */
	set_one_button (view, "_Open Calendar", GTK_STOCK_JUMP_TO, ITIP_VIEW_RESPONSE_OPEN);
	
	switch (priv->mode) {
	case ITIP_VIEW_MODE_PUBLISH:
		/* FIXME Is this really the right button? */
		set_one_button (view, "_Accept", GTK_STOCK_APPLY, ITIP_VIEW_RESPONSE_ACCEPT);
		break;
	case ITIP_VIEW_MODE_REQUEST:
		set_one_button (view, "_Decline", GTK_STOCK_CANCEL, ITIP_VIEW_RESPONSE_DECLINE);
		set_one_button (view, "_Tentative", GTK_STOCK_DIALOG_QUESTION, ITIP_VIEW_RESPONSE_TENTATIVE);
		set_one_button (view, "_Accept", GTK_STOCK_APPLY, ITIP_VIEW_RESPONSE_ACCEPT);
		break;
	case ITIP_VIEW_MODE_ADD:
		set_one_button (view, "_Decline", GTK_STOCK_CANCEL, ITIP_VIEW_RESPONSE_DECLINE);
		set_one_button (view, "_Tentative", GTK_STOCK_DIALOG_QUESTION, ITIP_VIEW_RESPONSE_TENTATIVE);
		set_one_button (view, "_Accept", GTK_STOCK_APPLY, ITIP_VIEW_RESPONSE_ACCEPT);
		break;
	case ITIP_VIEW_MODE_REFRESH:
		/* FIXME Is this really the right button? */
		set_one_button (view, "_Send Information", GTK_STOCK_REFRESH, ITIP_VIEW_RESPONSE_REFRESH);
		break;
	case ITIP_VIEW_MODE_REPLY:
		/* FIXME Is this really the right button? */
		set_one_button (view, "_Update Attendee Status", GTK_STOCK_REFRESH, ITIP_VIEW_RESPONSE_UPDATE);
		break;
	case ITIP_VIEW_MODE_CANCEL:
		set_one_button (view, "_Update", GTK_STOCK_REFRESH, ITIP_VIEW_RESPONSE_CANCEL);
		break;
	case ITIP_VIEW_MODE_COUNTER:
		set_one_button (view, "_Decline", GTK_STOCK_CANCEL, ITIP_VIEW_RESPONSE_DECLINE);
		set_one_button (view, "_Tentative", GTK_STOCK_DIALOG_QUESTION, ITIP_VIEW_RESPONSE_TENTATIVE);
		set_one_button (view, "_Accept", GTK_STOCK_APPLY, ITIP_VIEW_RESPONSE_ACCEPT);
		break;
	case ITIP_VIEW_MODE_DECLINECOUNTER:
		set_one_button (view, "_Decline", GTK_STOCK_CANCEL, ITIP_VIEW_RESPONSE_DECLINE);
		set_one_button (view, "_Tentative", GTK_STOCK_DIALOG_QUESTION, ITIP_VIEW_RESPONSE_TENTATIVE);
		set_one_button (view, "_Accept", GTK_STOCK_APPLY, ITIP_VIEW_RESPONSE_ACCEPT);
		break;
	default:
		break;
	}
}

static void
itip_view_destroy (GtkObject *object) 
{
	ItipView *view = ITIP_VIEW (object);
	ItipViewPrivate *priv = view->priv;
	
	if (priv) {		
		g_free (priv->organizer);
		g_free (priv->sentby);
		g_free (priv->delegator);
		g_free (priv->attendee);
		g_free (priv->location);
		g_free (priv->status);
		g_free (priv->comment);
		g_free (priv->start_tm);
		g_free (priv->end_tm);

		itip_view_clear_upper_info_items (view);
		itip_view_clear_lower_info_items (view);
		
		g_free (priv);
		view->priv = NULL;
	}

	GTK_OBJECT_CLASS (itip_view_parent_class)->destroy (object);
}

static void
itip_view_class_init (ItipViewClass *klass)
{
	GObjectClass *object_class;
	GtkObjectClass *gtkobject_class;
	
	object_class = G_OBJECT_CLASS (klass);
	gtkobject_class = GTK_OBJECT_CLASS (klass);
	
	gtkobject_class->destroy = itip_view_destroy;

	signals[SOURCE_SELECTED] =
		g_signal_new ("source_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ItipViewClass, source_selected),
			      NULL, NULL,
			      gtk_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[RESPONSE] =
		g_signal_new ("response",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ItipViewClass, response),
			      NULL, NULL,
			      gtk_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
rsvp_toggled_cb (GtkWidget *widget, gpointer data) 
{
	ItipView *view = data;
	ItipViewPrivate *priv;
	gboolean rsvp;
	
	priv = view->priv;
	
	rsvp = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->rsvp_check));

	gtk_widget_set_sensitive (priv->rsvp_comment_header, rsvp);
	gtk_widget_set_sensitive (priv->rsvp_comment_entry, rsvp);
}

static void
itip_view_init (ItipView *view)
{
	ItipViewPrivate *priv;
	GtkWidget *icon, *vbox, *hbox, *separator, *table, *label;

	priv = g_new0 (ItipViewPrivate, 1);	
	view->priv = priv;

	priv->mode = ITIP_VIEW_MODE_NONE;
	
	gtk_box_set_spacing (GTK_BOX (view), 12);

	/* The meeting icon */
	icon = e_icon_factory_get_image (MEETING_ICON, E_ICON_SIZE_LARGE_TOOLBAR);
	gtk_misc_set_alignment (GTK_MISC (icon), 0.5, 0);
	gtk_widget_show (icon);

	gtk_box_pack_start (GTK_BOX (view), icon, FALSE, FALSE, 0);

	/* The RHS */
	vbox = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (view), vbox, FALSE, FALSE, 0);

	/* The first section listing the sender */
	/* FIXME What to do if the send and organizer do not match */
	priv->sender_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->sender_label), 0, 0.5);
	gtk_widget_show (priv->sender_label);
	gtk_box_pack_start (GTK_BOX (vbox), priv->sender_label, FALSE, FALSE, 0);

	separator = gtk_hseparator_new ();
	gtk_widget_show (separator);
	gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);

	/* A table with information on the meeting and any extra info/warnings */
	table = gtk_table_new (4, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

	/* Summary */
	priv->summary_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->summary_label), 0, 0.5);
	gtk_widget_show (priv->summary_label);
	gtk_table_attach (GTK_TABLE (table), priv->summary_label, 0, 2, 0, 1, GTK_FILL, 0, 0, 0);

	/* Location */
	priv->location_header = gtk_label_new (_("Location:"));
	priv->location_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->location_header), 0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (priv->location_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), priv->location_header, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->location_label, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
	
	/* Start time */
	priv->start_header = gtk_label_new (_("Start time:"));
	priv->start_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->start_header), 0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (priv->start_label), 0, 0.5);
	gtk_widget_show (priv->start_header);
	gtk_table_attach (GTK_TABLE (table), priv->start_header, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->start_label, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);

	/* End time */
	priv->end_header = gtk_label_new (_("End time:"));
	priv->end_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->end_header), 0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (priv->end_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), priv->end_header, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->end_label, 1, 2, 3, 4, GTK_FILL, 0, 0, 0);

	/* Status */
	priv->status_header = gtk_label_new (_("Status:"));
	priv->status_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->status_header), 0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (priv->status_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), priv->status_header, 0, 1, 4, 5, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->status_label, 1, 2, 4, 5, GTK_FILL, 0, 0, 0);

	/* Comment */
	priv->comment_header = gtk_label_new (_("Comment:"));
	priv->comment_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->comment_header), 0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (priv->comment_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), priv->comment_header, 0, 1, 5, 6, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->comment_label, 1, 2, 5, 6, GTK_FILL, 0, 0, 0);

	/* Upper Info items */
	priv->upper_info_box = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (priv->upper_info_box);
	gtk_box_pack_start (GTK_BOX (vbox), priv->upper_info_box, FALSE, FALSE, 0);

	/* Description */
	priv->description_label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (priv->description_label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (priv->description_label), 0, 0.5);
//	gtk_box_pack_start (GTK_BOX (vbox), priv->description_label, FALSE, FALSE, 0);

	separator = gtk_hseparator_new ();
	gtk_widget_show (separator);
	gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);

	/* Lower Info items */
	priv->lower_info_box = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (priv->lower_info_box);
	gtk_box_pack_start (GTK_BOX (vbox), priv->lower_info_box, FALSE, FALSE, 0);

	/* Selector area */
	priv->selector_box = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (priv->selector_box);
	gtk_box_pack_start (GTK_BOX (vbox), priv->selector_box, FALSE, FALSE, 0);
	
	/* RSVP area */
	priv->rsvp_box = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), priv->rsvp_box, FALSE, FALSE, 0);

	priv->rsvp_check = gtk_check_button_new_with_mnemonic ("Send _reply to sender");
	gtk_widget_show (priv->rsvp_check);
	gtk_box_pack_start (GTK_BOX (priv->rsvp_box), priv->rsvp_check, FALSE, FALSE, 0);

	g_signal_connect (priv->rsvp_check, "toggled", G_CALLBACK (rsvp_toggled_cb), view);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (priv->rsvp_box), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (NULL);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	priv->rsvp_comment_header = gtk_label_new (_("Comment:"));
	gtk_widget_set_sensitive (priv->rsvp_comment_header, FALSE);
	gtk_widget_show (priv->rsvp_comment_header);
	gtk_box_pack_start (GTK_BOX (hbox), priv->rsvp_comment_header, FALSE, FALSE, 0);
	
	priv->rsvp_comment_entry = gtk_entry_new ();
	gtk_widget_set_sensitive (priv->rsvp_comment_entry, FALSE);
	gtk_widget_show (priv->rsvp_comment_entry);
	gtk_box_pack_start (GTK_BOX (hbox), priv->rsvp_comment_entry, FALSE, TRUE, 0);

	/* The buttons for actions */
	priv->button_box = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (priv->button_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (priv->button_box), 12);
	gtk_widget_show (priv->button_box);
	gtk_box_pack_start (GTK_BOX (vbox), priv->button_box, FALSE, FALSE, 0);

	priv->buttons_sensitive = TRUE;
}

GtkWidget *
itip_view_new (void)
{
	 ItipView *itip_view = g_object_new (ITIP_TYPE_VIEW, "homogeneous", FALSE, "spacing", 6, NULL);
	
	 return GTK_WIDGET (itip_view);
}

void
itip_view_set_mode (ItipView *view, ItipViewMode mode)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	priv->mode = mode;

	set_sender_text (view);
	set_buttons (view);
}

ItipViewMode
itip_view_get_mode (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, ITIP_VIEW_MODE_NONE);
	g_return_val_if_fail (ITIP_IS_VIEW (view), ITIP_VIEW_MODE_NONE);
	
	priv = view->priv;
	
	return priv->mode;
}

void
itip_view_set_item_type (ItipView *view, ECalSourceType type)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	priv->type = type;

	set_sender_text (view);
}

ECalSourceType
itip_view_get_item_type (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, ITIP_VIEW_MODE_NONE);
	g_return_val_if_fail (ITIP_IS_VIEW (view), ITIP_VIEW_MODE_NONE);
	
	priv = view->priv;
	
	return priv->type;
}


void
itip_view_set_organizer (ItipView *view, const char *organizer)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	if (priv->organizer)
		g_free (priv->organizer);

	priv->organizer = g_strdup (organizer);

	set_sender_text (view);
}

const char *
itip_view_get_organizer (ItipView *view)
{	
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->organizer;
}

void
itip_view_set_sentby (ItipView *view, const char *sentby)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	if (priv->sentby)
		g_free (priv->sentby);

	priv->sentby = g_strdup (sentby);

	set_sender_text (view);
}

const char *
itip_view_get_sentby (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->sentby;
}

void
itip_view_set_attendee (ItipView *view, const char *attendee)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	if (priv->attendee)
		g_free (priv->attendee);

	priv->attendee = g_strdup (attendee);

	set_sender_text (view);	
}

const char *
itip_view_get_attendee (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->attendee;
}

void
itip_view_set_delegator (ItipView *view, const char *delegator)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	if (priv->delegator)
		g_free (priv->delegator);

	priv->delegator = g_strdup (delegator);

	set_sender_text (view);	
}

const char *
itip_view_get_delegator (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->delegator;
}

void
itip_view_set_summary (ItipView *view, const char *summary)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	if (priv->summary)
		g_free (priv->summary);

	priv->summary = summary ? g_strstrip (g_strdup (summary)) : NULL;

	set_summary_text (view);
}

const char *
itip_view_get_summary (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->summary;
}

void
itip_view_set_location (ItipView *view, const char *location)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	if (priv->location)
		g_free (priv->location);

	priv->location = location ? g_strstrip (g_strdup (location)) : NULL;

	set_location_text (view);
}

const char *
itip_view_get_location (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->location;
}

void
itip_view_set_status (ItipView *view, const char *status)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	if (priv->status)
		g_free (priv->status);

	priv->status = status ? g_strstrip (g_strdup (status)) : NULL;

	set_status_text (view);
}

const char *
itip_view_get_status (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->status;
}

void
itip_view_set_comment (ItipView *view, const char *comment)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	if (priv->comment)
		g_free (priv->comment);

	priv->comment = comment ? g_strstrip (g_strdup (comment)) : NULL;

	set_comment_text (view);
}

const char *
itip_view_get_comment (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->comment;
}


void
itip_view_set_description (ItipView *view, const char *description)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	if (priv->description)
		g_free (priv->description);

	priv->description = description ? g_strstrip (g_strdup (description)) : NULL;

	set_description_text (view);
}

const char *
itip_view_get_description (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->description;
}


void
itip_view_set_start (ItipView *view, struct tm *start)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
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
	
	set_start_text (view);
}

const struct tm *
itip_view_get_start (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->start_tm;
}

void
itip_view_set_end (ItipView *view, struct tm *end)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
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
	
	set_end_text (view);
}

const struct tm *
itip_view_get_end (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (ITIP_IS_VIEW (view), NULL);
	
	priv = view->priv;
	
	return priv->end_tm;
}

guint
itip_view_add_upper_info_item (ItipView *view, ItipViewInfoItemType type, const char *message)
{
	ItipViewPrivate *priv;
	ItipViewInfoItem *item;

	g_return_val_if_fail (view != NULL, 0);
	g_return_val_if_fail (ITIP_IS_VIEW (view), 0);
	
	priv = view->priv;

	item = g_new0 (ItipViewInfoItem, 1);

	item->type = type;
	item->message = g_strdup (message);
	item->id = priv->next_info_item_id++;
	
	priv->upper_info_items = g_slist_append (priv->upper_info_items, item);

	set_upper_info_items (view);

	return item->id;
}

guint
itip_view_add_upper_info_item_printf (ItipView *view, ItipViewInfoItemType type, const char *format, ...)
{
	ItipViewPrivate *priv;
	va_list args;
	char *message;
	guint id;
	
	g_return_val_if_fail (view != NULL, 0);
	g_return_val_if_fail (ITIP_IS_VIEW (view), 0);	
	
	priv = view->priv;

	va_start (args, format);
	message = g_strdup_vprintf (format, args);
	va_end (args);

	id = itip_view_add_upper_info_item (view, type, message);
	g_free (message);
	
	return id;
}

void
itip_view_remove_upper_info_item (ItipView *view, guint id)
{
	ItipViewPrivate *priv;
	GSList *l;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;

	for (l = priv->upper_info_items; l; l = l->next) {
		ItipViewInfoItem *item = l->data;

		if (item->id == id) {
			priv->upper_info_items = g_slist_remove (priv->upper_info_items, item);

			g_free (item->message);
			g_free (item);

			set_upper_info_items (view);

			return;
		}
	}
}

void
itip_view_clear_upper_info_items (ItipView *view)
{
	ItipViewPrivate *priv;
	GSList *l;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;

	gtk_container_foreach (GTK_CONTAINER (priv->upper_info_box), (GtkCallback) gtk_widget_destroy, NULL);

	for (l = priv->upper_info_items; l; l = l->next) {
		ItipViewInfoItem *item = l->data;

		g_free (item->message);
		g_free (item);
	}

	g_slist_free (priv->upper_info_items);
	priv->upper_info_items = NULL;
}

guint
itip_view_add_lower_info_item (ItipView *view, ItipViewInfoItemType type, const char *message)
{
	ItipViewPrivate *priv;
	ItipViewInfoItem *item;

	g_return_val_if_fail (view != NULL, 0);
	g_return_val_if_fail (ITIP_IS_VIEW (view), 0);	
	
	priv = view->priv;

	item = g_new0 (ItipViewInfoItem, 1);

	item->type = type;
	item->message = g_strdup (message);
	item->id = priv->next_info_item_id++;
	
	priv->lower_info_items = g_slist_append (priv->lower_info_items, item);

	set_lower_info_items (view);

	return item->id;
}

guint
itip_view_add_lower_info_item_printf (ItipView *view, ItipViewInfoItemType type, const char *format, ...)
{
	ItipViewPrivate *priv;
	va_list args;
	char *message;
	guint id;
	
	g_return_val_if_fail (view != NULL, 0);
	g_return_val_if_fail (ITIP_IS_VIEW (view), 0);	
	
	priv = view->priv;

	va_start (args, format);
	message = g_strdup_vprintf (format, args);
	va_end (args);

	id = itip_view_add_lower_info_item (view, type, message);
	g_free (message);
	
	return id;
}

void
itip_view_remove_lower_info_item (ItipView *view, guint id)
{
	ItipViewPrivate *priv;
	GSList *l;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;

	for (l = priv->lower_info_items; l; l = l->next) {
		ItipViewInfoItem *item = l->data;

		if (item->id == id) {
			priv->lower_info_items = g_slist_remove (priv->lower_info_items, item);

			g_free (item->message);
			g_free (item);

			set_lower_info_items (view);

			return;
		}
	}
}

void
itip_view_clear_lower_info_items (ItipView *view)
{
	ItipViewPrivate *priv;
	GSList *l;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;

	gtk_container_foreach (GTK_CONTAINER (priv->lower_info_box), (GtkCallback) gtk_widget_destroy, NULL);

	for (l = priv->lower_info_items; l; l = l->next) {
		ItipViewInfoItem *item = l->data;

		g_free (item->message);
		g_free (item);
	}

	g_slist_free (priv->lower_info_items);
	priv->lower_info_items = NULL;
}

static void
source_selected_cb (ESourceOptionMenu *esom, ESource *source, gpointer data)
{
	ItipView *view = data;
	
	g_signal_emit (view, signals[SOURCE_SELECTED], 0, source);
}

void
itip_view_set_source_list (ItipView *view, ESourceList *source_list)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;

	if (priv->source_list)
		g_object_unref (priv->source_list);

	if (priv->esom)
		gtk_widget_destroy (priv->esom);
		
	if (!source_list) {
		if (priv->esom_header)
			gtk_widget_destroy (priv->esom_header);

		priv->source_list = NULL;
		priv->esom = NULL;
		priv->esom_header = NULL;
		
		return;
	}

	priv->source_list = g_object_ref (source_list);
	
	priv->esom = e_source_option_menu_new (source_list);
	gtk_widget_show (priv->esom);
	g_signal_connect (priv->esom, "source_selected", G_CALLBACK (source_selected_cb), view);

	if (!priv->esom_header) {
		priv->esom_header = gtk_label_new_with_mnemonic (_("_Calendar:"));
		gtk_label_set_mnemonic_widget (GTK_LABEL (priv->esom_header), priv->esom);
		gtk_widget_show (priv->esom_header);
	}
	
	gtk_box_pack_start (GTK_BOX (priv->selector_box), priv->esom_header, FALSE, TRUE, 6);
	gtk_box_pack_start (GTK_BOX (priv->selector_box), priv->esom, FALSE, TRUE, 0);
}

ESourceList *
itip_view_get_source_list (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);
	
	priv = view->priv;
	
	return priv->source_list;
}

void
itip_view_set_source (ItipView *view, ESource *source)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;

	if (!priv->esom)
		return;

	e_source_option_menu_select (E_SOURCE_OPTION_MENU (priv->esom), source);
}

ESource *
itip_view_get_source (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);
	
	priv = view->priv;
	
	if (!priv->esom)
		return NULL;

	return e_source_option_menu_peek_selected (E_SOURCE_OPTION_MENU (priv->esom));
}

void
itip_view_set_rsvp (ItipView *view, gboolean rsvp)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->rsvp_check), rsvp);

	gtk_widget_set_sensitive (priv->rsvp_comment_header, rsvp);
	gtk_widget_set_sensitive (priv->rsvp_comment_entry, rsvp);
}

gboolean
itip_view_get_rsvp (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);
	
	priv = view->priv;
	
	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->rsvp_check));
}

void
itip_view_set_show_rsvp (ItipView *view, gboolean rsvp)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	priv->rsvp_show = rsvp;

	priv->rsvp_show ? gtk_widget_show (priv->rsvp_box) : gtk_widget_hide (priv->rsvp_box);
}

gboolean
itip_view_get_show_rsvp (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);
	
	priv = view->priv;
	
	return priv->rsvp_show;
}

void
itip_view_set_rsvp_comment (ItipView *view, const char *comment)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	gtk_entry_set_text (GTK_ENTRY (priv->rsvp_comment_entry), comment);
}

const char *
itip_view_get_rsvp_comment (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);
	
	priv = view->priv;
	
	return gtk_entry_get_text (GTK_ENTRY (priv->rsvp_comment_entry));
}

void
itip_view_set_buttons_sensitive (ItipView *view, gboolean sensitive)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	priv->buttons_sensitive = sensitive;

	gtk_widget_set_sensitive (priv->button_box, priv->buttons_sensitive);
}

gboolean
itip_view_get_buttons_sensitive (ItipView *view)
{
	ItipViewPrivate *priv;

	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (ITIP_IS_VIEW (view), FALSE);
	
	priv = view->priv;
	
	return priv->buttons_sensitive;
}

	
