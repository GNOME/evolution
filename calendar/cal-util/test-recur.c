/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

/*
 * This tests the recurrence rule expansion functions.
 *
 * NOTE: currently it starts from the event start date and continues
 * until all recurrence rules/dates end or we reach MAX_OCCURRENCES
 * occurrences. So it does not test generating occurrences for a specific
 * interval. A nice addition might be to do this automatically and compare
 * the results from the complete set to ensure they match.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtkmain.h>
#include <cal-util/cal-recur.h>
#include <cal-util/cal-util.h>


/* Since events can recur infinitely, we set a limit to the number of
   occurrences we output. */
#define MAX_OCCURRENCES	1000

static void usage			(void);
static void generate_occurrences	(icalcomponent	*comp);
static gboolean occurrence_cb		(CalComponent	*comp,
					 time_t		 instance_start,
					 time_t		 instance_end,
					 gpointer	 data);


int
main			(int		 argc,
			 char		*argv[])
{
	gchar *filename;
	icalcomponent *icalcomp;

	gtk_init (&argc, &argv);

	if (argc != 2)
		usage ();

	filename = argv[1];

	icalcomp = cal_util_parse_ics_file (filename);
	if (icalcomp)
		generate_occurrences	(icalcomp);

	return 0;
}


static void
usage			(void)
{
	g_print ("Usage: test-recur <filename>\n");
	exit (1);
}


/* This resolves any TZIDs in the components. The VTIMEZONEs must be in the
   file we are reading. */
static icaltimezone*
resolve_tzid_cb		(const char	*tzid,
			 gpointer	 user_data)
{
	icalcomponent *vcalendar_comp = user_data;

        if (!tzid || !tzid[0])
                return NULL;
        else if (!strcmp (tzid, "UTC"))
                return icaltimezone_get_utc_timezone ();

	return icalcomponent_get_timezone (vcalendar_comp, tzid);
}


static void
generate_occurrences	(icalcomponent	*icalcomp)
{
	icalcompiter iter;
	icaltimezone *default_timezone;

	/* This is the timezone we will use for DATE and floating values. */
	default_timezone = icaltimezone_get_utc_timezone ();

	for (iter = icalcomponent_begin_component (icalcomp, ICAL_ANY_COMPONENT);
	     icalcompiter_deref (&iter) != NULL;
	     icalcompiter_next (&iter)) {
		icalcomponent *tmp_icalcomp;
		CalComponent *comp;
		icalcomponent_kind kind;
		gint occurrences;

		tmp_icalcomp = icalcompiter_deref (&iter);
		kind = icalcomponent_isa (tmp_icalcomp);

		if (!(kind == ICAL_VEVENT_COMPONENT
		      || kind == ICAL_VTODO_COMPONENT
		      || kind == ICAL_VJOURNAL_COMPONENT))
			continue;

		comp = cal_component_new ();

		if (!cal_component_set_icalcomponent (comp, tmp_icalcomp))
			continue;

		g_print ("#############################################################################\n");
		g_print ("%s\n\n", icalcomponent_as_ical_string (tmp_icalcomp));
		g_print ("Instances:\n");

		occurrences = 0;
		/* I use specific times when I am trying to pin down a bug seen
		   in one of the calendar views. */
#if 0
		cal_recur_generate_instances (comp, 982022400, 982108800,
					      occurrence_cb, &occurrences,
					      resolve_tzid_cb, icalcomp,
					      default_timezone);
#else
		cal_recur_generate_instances (comp, -1, -1,
					      occurrence_cb, &occurrences,
					      resolve_tzid_cb, icalcomp,
					      default_timezone);
#endif

		/* Print the component again so we can see the
		   X-EVOLUTION-ENDDATE parameter (only set if COUNT is used).
		*/
		g_print ("#############################################################################\n");
#if 0
		g_print ("%s\n\n", icalcomponent_as_ical_string (tmp_icalcomp));
#endif
	}
}


static gboolean
occurrence_cb		(CalComponent	*comp,
			 time_t		 instance_start,
			 time_t		 instance_end,
			 gpointer	 data)
{
	char start[32], finish[32];
	gint *occurrences;

	occurrences = (gint*) data;

	strcpy (start, ctime (&instance_start));
	start[24] = '\0';
	strcpy (finish, ctime (&instance_end));
	finish[24] = '\0';

	g_print ("%s - %s\n", start, finish);

	(*occurrences)++;
	return (*occurrences == MAX_OCCURRENCES) ? FALSE : TRUE;
}
