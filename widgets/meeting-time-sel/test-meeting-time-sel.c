/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@gtk.org>
 *
 * Copyright 1999, Helix Code, Inc.
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

#include <config.h>
#include <gnome.h>
#include "e-meeting-time-sel.h"

void add_random_attendee_test_data (EMeetingTimeSelector *mts);
void add_simple_attendee_test_data (EMeetingTimeSelector *mts);
gint get_random_int (gint max);

int
main (int argc, char *argv[])
{
	GtkWidget *window, *mts;
	gint i;

	gnome_init ("test-meeting-time-selector", "0.1", argc, argv);

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());

	window = gnome_dialog_new ("Plan a Meeting", "Make Meeting",
				   GNOME_STOCK_BUTTON_CLOSE, NULL);
	gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);

	mts = e_meeting_time_selector_new ();
	gtk_container_add (GTK_CONTAINER (GNOME_DIALOG (window)->vbox), mts);
	gtk_window_add_accel_group (GTK_WINDOW (window),
				    E_MEETING_TIME_SELECTOR (mts)->accel_group);
	gtk_widget_show (mts);

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	i = 0;
#if 1
	for (i = 0; i < 20; i++) {
		add_random_attendee_test_data (E_MEETING_TIME_SELECTOR (mts));
	}
#else
	for (i = 0; i < 1; i++) {
		add_simple_attendee_test_data (E_MEETING_TIME_SELECTOR (mts));
	}
#endif

#if 0
	e_meeting_time_selector_dump (E_MEETING_TIME_SELECTOR (mts));
#endif

	gnome_dialog_run (GNOME_DIALOG (window));

	gtk_main ();
	return 0;
}


/* Adds an attendee and a lot of random busy periods. The periods start 60
   days before the current date and extend over 365 days, to match the range
   that EMeetingTimeSelector currently displays. We generate a time_t and an
   interval and then convert them into a struct tm which provides everything
   we need. */
void
add_random_attendee_test_data (EMeetingTimeSelector *mts)
{
	gchar buffer[128], *name;
	gint row, num_periods, busy_period, random_num, duration;
	EMeetingTimeSelectorAttendeeType type;
	EMeetingTimeSelectorBusyType busy_type;
	time_t range_start;
	time_t period_start;
	time_t period_end;
	struct tm *tmp_tm;
	struct tm tm1;
	struct tm tm2;

	/* Determine the type of attendee. */
	random_num = get_random_int (10);
	if (random_num < 4) {
		type = E_MEETING_TIME_SELECTOR_REQUIRED_PERSON;
		name = "Req. Attendee";
	} else if (random_num < 7) {
		type = E_MEETING_TIME_SELECTOR_OPTIONAL_PERSON;
		name = "Opt. Attendee";
	} else {
		type = E_MEETING_TIME_SELECTOR_RESOURCE;
		name = "Resource";
	}

	sprintf (buffer, "%s %i", name, mts->attendees->len + 1);
	row = e_meeting_time_selector_attendee_add (mts, buffer, NULL);
	e_meeting_time_selector_attendee_set_type (mts, row, type);

	/* Don't send the meeting request to some attendees. */
	if (get_random_int (10) <= 2)
		e_meeting_time_selector_attendee_set_send_meeting_to (mts, row,
								      FALSE);

	/* Some attendees have no calendar information. */
	if (get_random_int (10) == 2)
		return;

	range_start = time (NULL) - 61 * 24 * 60 * 60;
	num_periods = get_random_int (1000);
#if 0
	g_print ("num_periods: %i\n", num_periods);
#endif
	for (busy_period = 0; busy_period < num_periods; busy_period++) {

		period_start = range_start + get_random_int (365 * 24 * 60 * 60);

		/* Make busy periods mainly 30 mins to a few hours, with a
		   couple of week/fortnight periods as well. */
		random_num = get_random_int (10000);
		if (random_num < 2000)
			duration = 30;
		else if (random_num < 5000)
			duration = 60;
		else if (random_num < 7500)
			duration = 90;
		else if (random_num < 9995)
			duration = 120;
		else if (random_num < 9998)
			duration = 60 * 24 * 7;
		else
			duration = 60 * 24 * 14;
#if 0
		g_print ("random_num: %i, duration: %i\n",
			 random_num, duration);
#endif
		period_end = period_start + duration * 60;

		tmp_tm = localtime (&period_start);
		tm1 = *tmp_tm;
		tmp_tm = localtime (&period_end);
		tm2 = *tmp_tm;

		/* A hack to avoid daylight-saving time problems. */
		if (tm2.tm_hour == tm1.tm_hour && tm2.tm_min < tm1.tm_min)
			tm2.tm_hour++;

		busy_type = get_random_int (E_MEETING_TIME_SELECTOR_BUSY_LAST);

		if (!e_meeting_time_selector_attendee_add_busy_period (mts, row, tm1.tm_year + 1900, tm1.tm_mon + 1, tm1.tm_mday, tm1.tm_hour, tm1.tm_min, tm2.tm_year + 1900, tm2.tm_mon + 1, tm2.tm_mday, tm2.tm_hour, tm2.tm_min, busy_type))
			{
				g_print ("Invalid busy period %i/%i/%i %i:%i to %i/%i/%i %i:%i\n", tm1.tm_year + 1900, tm1.tm_mon + 1, tm1.tm_mday, tm1.tm_hour, tm1.tm_min, tm2.tm_year + 1900, tm2.tm_mon + 1, tm2.tm_mday, tm2.tm_hour, tm2.tm_min);
				g_print ("random_num: %i, duration: %i\n",
					 random_num, duration);
			}
	}
}


/* Returns a random integer between 0 and max - 1. */
gint
get_random_int (gint max)
{
	gint random_num;

	random_num = (int) (max * (rand () / (RAND_MAX + 1.0)));
#if 0
	g_print ("Random num (%i): %i\n", max, random_num);
#endif
	return random_num;
}


void
add_simple_attendee_test_data (EMeetingTimeSelector *mts)
{
	gint row;

	row = e_meeting_time_selector_attendee_add (mts, "John Smith", NULL);
	if (!e_meeting_time_selector_attendee_add_busy_period (mts, row,
							       1999, 11, 7, 14, 30,
							       1999, 11, 7, 16, 30,
							       E_MEETING_TIME_SELECTOR_BUSY_BUSY))
			g_warning ("Invalid busy period");

	e_meeting_time_selector_attendee_add_busy_period (mts, row,
							  1999, 11, 7, 10, 30,
							  1999, 11, 7, 11, 30,
							  E_MEETING_TIME_SELECTOR_BUSY_OUT_OF_OFFICE);
	e_meeting_time_selector_attendee_add_busy_period (mts, row,
							  1999, 11, 4, 10, 30,
							  1999, 11, 7, 11, 30,
							  E_MEETING_TIME_SELECTOR_BUSY_BUSY);
	row = e_meeting_time_selector_attendee_add (mts, "Dave Jones", NULL);
	e_meeting_time_selector_attendee_add_busy_period (mts, row,
							  1999, 11, 7, 15, 30,
							  1999, 11, 7, 18, 30,
							  E_MEETING_TIME_SELECTOR_BUSY_TENTATIVE);
	e_meeting_time_selector_attendee_add_busy_period (mts, row,
							  1999, 11, 7, 11, 00,
							  1999, 11, 7, 12, 00,
							  E_MEETING_TIME_SELECTOR_BUSY_BUSY);

	row = e_meeting_time_selector_attendee_add (mts, "Andrew Carlisle", NULL);
	e_meeting_time_selector_attendee_set_send_meeting_to (mts, row, FALSE);

	row = e_meeting_time_selector_attendee_add (mts, "Michael Cain", NULL);
	e_meeting_time_selector_attendee_add_busy_period (mts, row,
							  1999, 11, 7, 15, 30,
							  1999, 11, 7, 18, 30,
							  E_MEETING_TIME_SELECTOR_BUSY_TENTATIVE);
	e_meeting_time_selector_attendee_add_busy_period (mts, row,
							  1999, 11, 7, 12, 30,
							  1999, 11, 7, 13, 30,
							  E_MEETING_TIME_SELECTOR_BUSY_OUT_OF_OFFICE);
	e_meeting_time_selector_attendee_add_busy_period (mts, row,
							  1999, 11, 7, 11, 00,
							  1999, 11, 7, 12, 00,
							  E_MEETING_TIME_SELECTOR_BUSY_TENTATIVE);
}
