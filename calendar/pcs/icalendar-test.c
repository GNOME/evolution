/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

#include <config.h>
#include <gtk/gtksignal.h>
#include <cal-util/calobj.h>
#include "libversit/vcc.h"

#include "icalendar-save.h"
#include "icalendar.h"


static icalcomponent* 
icalendar_parse_file (char* fname)
{
	FILE* fp;
	icalcomponent* comp = NULL;
	gchar* str;
	struct stat st;
	int n;

	fp = fopen (fname, "r");
	if (!fp) {
		g_warning ("Cannot open open calendar file.");
		return NULL;
	}
	
	stat (fname, &st);
	
	str = g_malloc (st.st_size + 2);
	
	n = fread ((gchar*) str, 1, st.st_size, fp);
	if (n != st.st_size) {
		g_warning ("Read error.");
	}
	str[n] = '\0';

	fclose (fp);
	
	comp = icalparser_parse_string (str);
	g_free (str);

	return comp;
}


static GList *
icalendar_calendar_load (GList *icals, char *fname)
{
	icalcomponent *comp;
	icalcomponent *subcomp;
	iCalObject    *ical;

	comp = icalendar_parse_file (fname);
	subcomp = icalcomponent_get_first_component (comp,
						     ICAL_ANY_COMPONENT);
	while (subcomp) {
		ical = ical_object_create_from_icalcomponent (subcomp);
		if (ical->type != ICAL_EVENT && 
		    ical->type != ICAL_TODO  &&
		    ical->type != ICAL_JOURNAL) {
			g_warning ("Skipping unsupported iCalendar component");
		} else {
			printf ("prepending %p\n", ical);
			icals = g_list_prepend (icals, ical);
		}
		subcomp = icalcomponent_get_next_component (comp,
							   ICAL_ANY_COMPONENT);
	}

	return icals;
}




static void
icalendar_calendar_save (GList *icals, char *fname)
{
	GList *cur;
	icalcomponent *top = icalcomponent_new (ICAL_VCALENDAR_COMPONENT);
	char *out_cal_string;

	for (cur=icals; cur; cur=cur->next) {
		iCalObject *ical = (iCalObject *) cur->data;
		icalcomponent *comp;
		comp = icalcomponent_create_from_ical_object (ical);
		icalcomponent_add_component (top, comp);
	}

	out_cal_string = icalcomponent_as_ical_string (top);

	printf ("---------------------------------------------------------\n");
	printf ("%s", out_cal_string);
}



int main (int argc, char *argv[])
{
	GList *icals = NULL;
	int i;
	long int n0, n1;
        struct icaldurationtype dt;


	/* test icaldurationtype_from_timet */
	srandom (time (0));

	for (i=0; i<10; i++) {
		n0 = random () % ((60 * 60 * 24 * 7) * 4);
		dt = icaldurationtype_from_timet (n0);
		n1 = icaldurationtype_as_timet (dt);

		printf ("%ld -> (%d %d %d %d %d) -> %ld\n",
			n0,
			dt.weeks, dt.days, dt.hours, dt.minutes, dt.seconds,
			n1);
		if (n0 != n1) abort ();
	}

	/*****************/
	/* test conversion of icalcomponents to and from iCalObjects */
	/*****************/

	/* load an ical file */

	if (argc < 2) {
		printf ("give ical file as argument.\n");
		return 1;
	}

	icals = icalendar_calendar_load (icals, argv[ 1 ]);

	printf ("loaded %d ical components\n", g_list_length (icals));


	/* save it back out */

	icalendar_calendar_save (icals, "out.ical");

	return 0;
}
