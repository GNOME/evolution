/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
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
#include <gtk/gtk.h>
#include <cal-util/cal-recur.h>


/* Since events can recur infinitely, we set a limit to the number of
   occurrences we output. */
#define MAX_OCCURRENCES	1000

static void usage			(void);
static icalcomponent* scan_ics_file	(char		*filename);
static char* get_line			(char		*s,
					 size_t		 size,
					 void		*data);
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

	icalcomp = scan_ics_file (filename);
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


static icalcomponent*
scan_ics_file		(char		*filename)
{
	FILE *fp;
	icalcomponent *icalcomp;
	icalparser *parser;

	g_print ("Opening file: %s\n", filename);
	fp = fopen (filename, "r");

	if (!fp) {
		g_print ("Can't open file: %s\n", filename);
		return NULL;
	}

	parser = icalparser_new ();
	icalparser_set_gen_data (parser, fp);

	icalcomp = icalparser_parse (parser, get_line);
	icalparser_free (parser);

	return icalcomp;
}


/* Callback used from icalparser_parse() */
static char *
get_line		(char		*s,
			 size_t		 size,
			 void		*data)
{
	return fgets (s, size, (FILE*) data);
}


static void
generate_occurrences	(icalcomponent	*icalcomp)
{
	icalcompiter iter;

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

		occurrences = 0;
#if 0
		cal_recur_generate_instances (comp, 972950400, 976492800,
					      occurrence_cb, &occurrences);
#else
		cal_recur_generate_instances (comp, -1, -1,
					      occurrence_cb, &occurrences);
#endif

		g_print ("%s\n\n", icalcomponent_as_ical_string (tmp_icalcomp));
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
