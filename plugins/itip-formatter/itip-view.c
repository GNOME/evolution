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
#include <gtkhtml/gtkhtml-embedded.h>
#include <mail/em-format-hook.h>
#include <mail/em-format-html.h>
#include <e-util/e-account-list.h>
#include <e-util/e-icon-factory.h>
#include <e-util/e-time-utils.h>
#include <calendar/gui/itip-utils.h>
#include "itip-view.h"

#define MEETING_ICON "stock_new-meeting"

G_DEFINE_TYPE (ItipView, itip_view, GTK_TYPE_HBOX);

struct _ItipViewPrivate {
	ItipViewMode mode;
	
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

	GtkWidget *start_header;
	GtkWidget *start_label;
	struct tm *start_tm;
	
	GtkWidget *end_header;
	GtkWidget *end_label;
	struct tm *end_tm;
};

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
				/* strftime format of a weekday, a date and a
				   time, in 24-hour format, without seconds. */
				format = _("Today %H:%M");
			else
				/* strftime format of a weekday, a date and a
				   time, in 24-hour format. */
				format = _("Today %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format, without seconds. */
				format = _("Today %I:%M %p");
			else
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format. */
				format = _("Today %I:%M:%S %p");
		}

	/* Tomorrow */		
	} else if (date_tm->tm_year == current_tm->tm_year) {
		if (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0) {
			/* strftime format of a weekday and a date. */
			format = _("%A, %B %e");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				   time, in 24-hour format, without seconds. */
				format = _("%A, %B %e %H:%M");
			else
				/* strftime format of a weekday, a date and a
				   time, in 24-hour format. */
				format = _("%A, %B %e %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format, without seconds. */
				format = _("%A, %B %e %I:%M %p");
			else
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format. */
				format = _("%A, %B %e %I:%M:%S %p");
		}

	/* Within 7 days */		
	} else if (date_tm->tm_year == current_tm->tm_year) {
		if (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0) {
			/* strftime format of a weekday and a date. */
			format = _("%A");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				   time, in 24-hour format, without seconds. */
				format = _("%A %H:%M");
			else
				/* strftime format of a weekday, a date and a
				   time, in 24-hour format. */
				format = _("%A %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format, without seconds. */
				format = _("%A %I:%M %p");
			else
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format. */
				format = _("%A %I:%M:%S %p");
		}

	/* This Year */		
	} else if (date_tm->tm_year == current_tm->tm_year) {
		if (!show_midnight && date_tm->tm_hour == 0
		    && date_tm->tm_min == 0 && date_tm->tm_sec == 0) {
			/* strftime format of a weekday and a date. */
			format = _("%A, %B %e");
		} else if (use_24_hour_format) {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				   time, in 24-hour format, without seconds. */
				format = _("%A, %B %e %H:%M");
			else
				/* strftime format of a weekday, a date and a
				   time, in 24-hour format. */
				format = _("%A, %B %e %H:%M:%S");
		} else {
			if (!show_zero_seconds && date_tm->tm_sec == 0)
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format, without seconds. */
				format = _("%A, %B %e %I:%M %p");
			else
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format. */
				format = _("%A, %B %e %I:%M:%S %p");
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
				format = _("%A, %B %e, %Y %I:%M %p");
			else
				/* strftime format of a weekday, a date and a
				   time, in 12-hour format. */
				format = _("%A, %B %e, %Y %I:%M:%S %p");
		}
	}
	
	/* strftime returns 0 if the string doesn't fit, and leaves the buffer
	   undefined, so we set it to the empty string in that case. */
	if (e_utf8_strftime (buffer, buffer_size, format, date_tm) == 0)
		buffer[0] = '\0';
}

static void
set_sender_text (ItipView *view)
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
			sender = g_strdup_printf (_("<b>%s</b> through %s has published meeting information."), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> has published meeting information."), organizer);
		break;
	case ITIP_VIEW_MODE_REQUEST:
		/* FIXME is the delegator stuff handled correctly here? */
		if (priv->delegator) {
			sender = g_strdup_printf (_("<b>%s</b> requests the presence of %s at a meeting."), organizer, priv->delegator);
		} else {
			if (priv->sentby)
				sender = g_strdup_printf (_("<b>%s</b> through %s requests your presence at a meeting."), organizer, priv->sentby);
			else
				sender = g_strdup_printf (_("<b>%s</b> requests your presence at a meeting."), organizer);
		}
		break;
	case ITIP_VIEW_MODE_ADD:
		if (priv->sentby)
			sender = g_strdup_printf (_("<b>%s</b> through %s wishes to add to an existing meeting."), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> wishes to add to an existing meeting."), organizer);
		break;
	case ITIP_VIEW_MODE_REFRESH:
		sender = g_strdup_printf (_("<b>%s</b> wishes to receive the latest meeting information."), attendee);
		break;
	case ITIP_VIEW_MODE_REPLY:
		sender = g_strdup_printf (_("<b>%s</b> has replied to a meeting invitation."), attendee);
		break;
	case ITIP_VIEW_MODE_CANCEL:
		if (priv->sentby)
			sender = g_strdup_printf (_("<b>%s</b> through %s has cancelled a meeting."), organizer, priv->sentby);
		else
			sender = g_strdup_printf (_("<b>%s</b> has cancelled a meeting."), organizer);
		break;
	default:
		break;
	}

	gtk_label_set_text (GTK_LABEL (priv->sender_label), sender);
	gtk_label_set_use_markup (GTK_LABEL (priv->sender_label), TRUE);

	g_free (sender);
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
		g_free (priv->start_tm);
		g_free (priv->end_tm);
		
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
}

static void
itip_view_init (ItipView *view)
{
	ItipViewPrivate *priv;
	GtkWidget *icon, *vbox, *separator, *table;

	priv = g_new0 (ItipViewPrivate, 1);	
	view->priv = priv;

	priv->mode = ITIP_VIEW_MODE_NONE;
	
	/* The icon on the LHS */
	icon = e_icon_factory_get_image (MEETING_ICON, E_ICON_SIZE_LARGE_TOOLBAR);
	gtk_misc_set_alignment (GTK_MISC (icon), 0, 0);
	gtk_widget_show (icon);

	gtk_box_pack_start (GTK_BOX (view), icon, FALSE, FALSE, 6);

	/* The RHS */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (view), vbox, FALSE, FALSE, 6);

	/* The first section listing the sender */
	/* FIXME What to do if the send and organizer do not match */
	priv->sender_label = gtk_label_new (NULL);
	gtk_widget_show (priv->sender_label);
	gtk_box_pack_start (GTK_BOX (vbox), priv->sender_label, FALSE, FALSE, 6);

	separator = gtk_hseparator_new ();
	gtk_widget_show (separator);
	gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 6);

	/* A table with information on the meeting and any extra info/warnings */
	table = gtk_table_new (3, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 6);

	/* Summary */
	priv->summary_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->summary_label), 0, 0.5);
	gtk_widget_show (priv->summary_label);
	gtk_table_attach (GTK_TABLE (table), priv->summary_label, 0, 2, 0, 1, GTK_FILL, 0, 0, 0);

	/* Location */
	priv->location_header = gtk_label_new (_("Location:"));
	priv->location_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->location_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), priv->location_header, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->location_label, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
	
	priv->start_header = gtk_label_new (_("Starts:"));
	priv->start_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->start_header), 0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (priv->start_label), 0, 0.5);
	gtk_widget_show (priv->start_header);
	gtk_table_attach (GTK_TABLE (table), priv->start_header, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->start_label, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);

	priv->end_header = gtk_label_new (_("Ends:"));
	priv->end_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->end_header), 0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (priv->end_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), priv->end_header, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->end_label, 1, 2, 3, 4, GTK_FILL, 0, 0, 0);
	
	/* The buttons for actions */
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
itip_view_set_summary (ItipView *view, const char *summary)
{
	ItipViewPrivate *priv;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (ITIP_IS_VIEW (view));	
	
	priv = view->priv;
	
	if (priv->summary)
		g_free (priv->summary);

	priv->summary = g_strdup (summary);

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

	priv->location = g_strdup (location);

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
