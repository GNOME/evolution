/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*======================================================================
 FILE: icaltimezone.h
 CREATOR: Damon Chaplin 15 March 2001


 $Id$
 $Locker$

 (C) COPYRIGHT 2001, Damon Chaplin

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


======================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "icalarray.h"
#include "icalerror.h"
#include "icalparser.h"
#include "icaltimezone.h"


/* This is the toplevel directory where the timezone data is installed in. */
#define ZONEINFO_DIRECTORY	PACKAGE_DATA_DIR "/zoneinfo"

/* This is the filename of the file containing the city names and coordinates
   of all the builtin timezones. */
#define ZONES_TAB_FILENAME	"zones.tab"

/* This is the number of years of extra coverage we do when expanding the
   timezone changes. */
#define ICALTIMEZONE_EXTRA_COVERAGE	5

/* This is the maximum year we will expand to. time_t values only go up to
   somewhere around 2037. */
#define ICALTIMEZONE_MAX_YEAR		2032


struct _icaltimezone {
    /* The unique ID of this timezone,
       e.g. "/softwarestudio.org/Olson_20010601_1/Africa/Banjul".
       This should only be used to identify a VTIMEZONE. It is not meant to
       be displayed to the user in any form. */
    char		*tzid;

    /* The location for the timezone, e.g. "Africa/Accra" for the Olson
       database. We look for this in the "LOCATION" or "X-LIC-LOCATION"
       properties of the VTIMEZONE component. It isn't a standard property
       yet. This will be NULL if no location is found in the VTIMEZONE. */
    char		*location;

    /* This will be set to a combination of the TZNAME properties from the last
       STANDARD and DAYLIGHT components in the VTIMEZONE, e.g. "EST/EDT".
       If they both use the same TZNAME, or only one type of component is
       found, then only one TZNAME will appear, e.g. "AZOT". If no TZNAME
       is found this will be NULL. */
    char		*tznames;

    /* The coordinates of the city, in degrees. */
    double		 latitude;
    double		 longitude;

    /* The toplevel VTIMEZONE component loaded from the .ics file for this
       timezone. If we need to regenerate the changes data we need this. */
    icalcomponent	*component;

    /* If this is not NULL it points to the builtin icaltimezone that the
       above TZID refers to. This icaltimezone should be used instead when
       accessing the timezone changes data, so that the expanded timezone
       changes data is shared between calendar components. */
    icaltimezone	*builtin_timezone;

    /* This is the last year for which we have expanded the data to.
       If we need to calculate a date past this we need to expand the
       timezone component data from scratch. */
    int			 end_year;

    /* A dynamically-allocated array of time zone changes, sorted by the
       time of the change in local time. So we can do fast binary-searches
       to convert from local time to UTC. */
    icalarray		*changes;
};


typedef struct _icaltimezonechange	icaltimezonechange;

struct _icaltimezonechange {
    /* The offset to add to UTC to get local time, in seconds. */
    int		 utc_offset;

    /* The offset to add to UTC, before this change, in seconds. */
    int		 prev_utc_offset;

    /* The time that the change came into effect, in UTC.
       Note that the prev_utc_offset applies to this local time,
       since we haven't changed to the new offset yet. */
    int		 year;		/* Actual year, e.g. 2001. */
    char	 month;		/* 1 (Jan) to 12 (Dec). */
    char	 day;
    char	 hour;
    char	 minute;
    char	 second;

    /* Whether this is STANDARD or DAYLIGHT time. */
    char	 is_daylight;
};


/* An array of icaltimezones for the builtin timezones. */
icalarray *builtin_timezones = NULL;

/* This is the special UTC timezone, which isn't in builtin_timezones. */
icaltimezone utc_timezone = { 0 };



static void  icaltimezone_expand_changes	(icaltimezone	*zone,
						 int		 end_year);
static void  icaltimezone_expand_vtimezone	(icalcomponent	*comp,
						 int		 end_year,
						 icalarray	*changes);
static int   icaltimezone_compare_change_fn	(const void	*elem1,
						 const void	*elem2);

static int   icaltimezone_find_nearby_change	(icaltimezone	*zone,
						 icaltimezonechange *change);

static void  icaltimezone_adjust_change		(icaltimezonechange *tt,
						 int		 days,
						 int		 hours,
						 int		 minutes,
						 int		 seconds);

static void  icaltimezone_init			(icaltimezone	*zone);

/* Initializes an icaltimezone from the given VTIMEZONE component. It gets
   the TZID from the VTIMEZONE component. It returns 1 on success, or 0 if
   the TZID can't be found. */
static int   icaltimezone_init_from_vtimezone	(icaltimezone	*zone,
						 icalcomponent	*component);


static void  icaltimezone_load			(icaltimezone	*zone);

static void  icaltimezone_ensure_coverage	(icaltimezone	*zone,
						 int		 end_year);


static void  icaltimezone_init_builtin_timezones(void);

static void  icaltimezone_parse_zone_tab	(void);

static char* icaltimezone_load_get_line_fn	(char		*s,
						 size_t		 size,
						 void		*data);

static void  format_utc_offset			(int		 utc_offset,
						 char		*buffer);



/* Initializes an icaltimezone with the given TZID. */
static void
icaltimezone_init			(icaltimezone	*zone)
{
    zone->tzid = NULL;
    zone->location = NULL;
    zone->tznames = NULL;
    zone->latitude = 0.0;
    zone->longitude = 0.0;
    zone->component = NULL;
    zone->builtin_timezone = NULL;
    zone->end_year = 0;
    zone->changes = NULL;
}


/* Initializes an icaltimezone from the given VTIMEZONE component. It gets
   the TZID from the VTIMEZONE component. It returns 1 on success, or 0 if
   the TZID can't be found. */
static int
icaltimezone_init_from_vtimezone	(icaltimezone	*zone,
					 icalcomponent	*component)
{
    icalproperty *prop;
    const char *tzid;

    prop = icalcomponent_get_first_property (component, ICAL_TZID_PROPERTY);
    if (!prop)
	return 0;

    tzid = icalproperty_get_tzid (prop);
    if (!tzid)
	return 0;

    icaltimezone_init (zone);
    zone->tzid = strdup (tzid);
    zone->component = component;

    return 1;
}


static void
icaltimezone_ensure_coverage		(icaltimezone	*zone,
					 int		 end_year)
{
    /* When we expand timezone changes we always expand at least up to this
       year, plus ICALTIMEZONE_EXTRA_COVERAGE. */
    static int icaltimezone_minimum_expansion_year = -1;

    int changes_end_year;

    if (!zone->component)
	icaltimezone_load (zone);

    if (icaltimezone_minimum_expansion_year == -1) {
	struct tm *tmp_tm;
	time_t t;

	t = time (NULL);
	tmp_tm = localtime (&t);
	icaltimezone_minimum_expansion_year = tmp_tm->tm_year + 1900;
    }

    changes_end_year = end_year;
    if (changes_end_year < icaltimezone_minimum_expansion_year)
	changes_end_year = icaltimezone_minimum_expansion_year;

    changes_end_year += ICALTIMEZONE_EXTRA_COVERAGE;

    if (changes_end_year > ICALTIMEZONE_MAX_YEAR)
	changes_end_year = ICALTIMEZONE_MAX_YEAR;

    if (!zone->changes || zone->end_year < end_year)
	icaltimezone_expand_changes (zone, changes_end_year);
}


static void
icaltimezone_expand_changes		(icaltimezone	*zone,
					 int		 end_year)
{
    icalarray *changes;
    icalcomponent *comp;

#if 0
    printf ("\nExpanding changes for: %s to year: %i\n", zone->tzid, end_year);
#endif

    changes = icalarray_new (sizeof (icaltimezonechange), 32);
    if (!changes)
	return;

    /* Scan the STANDARD and DAYLIGHT subcomponents. */
    comp = icalcomponent_get_first_component (zone->component,
					      ICAL_ANY_COMPONENT);
    while (comp) {
	icaltimezone_expand_vtimezone (comp, end_year, changes);
	comp = icalcomponent_get_next_component (zone->component,
						 ICAL_ANY_COMPONENT);
    }

    /* Sort the changes. We may have duplicates but I don't think it will
       matter. */
    icalarray_sort (changes, icaltimezone_compare_change_fn);

    if (zone->changes)
	icalarray_free (zone->changes);

    zone->changes = changes;
    zone->end_year = end_year;
}


static void
icaltimezone_expand_vtimezone		(icalcomponent	*comp,
					 int		 end_year,
					 icalarray	*changes)
{
    icaltimezonechange change;
    icalproperty *prop;
    struct icaltimetype dtstart, occ;
    struct icalrecurrencetype rrule;
    icalrecur_iterator* rrule_iterator;
    struct icaldatetimeperiodtype rdate;
    int found_dtstart = 0, found_tzoffsetto = 0, found_tzoffsetfrom = 0;
    int has_recurrence = 0;

    /* First we check if it is a STANDARD or DAYLIGHT component, and
       just return if it isn't. */
    if (icalcomponent_isa (comp) == ICAL_XSTANDARD_COMPONENT)
	change.is_daylight = 0;
    else if (icalcomponent_isa (comp) == ICAL_XDAYLIGHT_COMPONENT)
	change.is_daylight = 1;
    else 
	return;

    /* Step through each of the properties to find the DTSTART,
       TZOFFSETFROM and TZOFFSETTO. We can't expand recurrences here
       since we need these properties before we can do that. */
    prop = icalcomponent_get_first_property (comp, ICAL_ANY_PROPERTY);
    while (prop) {
	switch (icalproperty_isa (prop)) {
	case ICAL_DTSTART_PROPERTY:
	    dtstart = icalproperty_get_dtstart (prop);
	    found_dtstart = 1;
	    break;
	case ICAL_TZOFFSETTO_PROPERTY:
	    change.utc_offset = icalproperty_get_tzoffsetto (prop);
	    /*printf ("Found TZOFFSETTO: %i\n", change.utc_offset);*/
	    found_tzoffsetto = 1;
	    break;
	case ICAL_TZOFFSETFROM_PROPERTY:
	    change.prev_utc_offset = icalproperty_get_tzoffsetfrom (prop);
	    /*printf ("Found TZOFFSETFROM: %i\n", change.prev_utc_offset);*/
	    found_tzoffsetfrom = 1;
	    break;
	case ICAL_RDATE_PROPERTY:
	case ICAL_RRULE_PROPERTY:
	    has_recurrence = 1;
	    break;
	default:
	    /* Just ignore any other properties. */
	    break;
	}

	prop = icalcomponent_get_next_property (comp, ICAL_ANY_PROPERTY);
    }

    /* If we didn't find a DTSTART, TZOFFSETTO and TZOFFSETFROM we have to
       ignore the component. FIXME: Add an error property? */
    if (!found_dtstart || !found_tzoffsetto || !found_tzoffsetfrom)
	return;

#if 0
    printf ("\n Expanding component DTSTART (Y/M/D): %i/%i/%i %i:%02i:%02i\n",
	    dtstart.year, dtstart.month, dtstart.day,
	    dtstart.hour, dtstart.minute, dtstart.second);
#endif

    /* If the STANDARD/DAYLIGHT component has no recurrence data, we just add
       a single change for the DTSTART. */
    if (!has_recurrence) {
	change.year   = dtstart.year;
	change.month  = dtstart.month;
	change.day    = dtstart.day;
	change.hour   = dtstart.hour;
	change.minute = dtstart.minute;
	change.second = dtstart.second;

	/* Convert to UTC. */
	icaltimezone_adjust_change (&change, 0, 0, 0, -change.prev_utc_offset);

#if 0
	printf ("  Appending single DTSTART (Y/M/D): %i/%02i/%02i %i:%02i:%02i\n",
		change.year, change.month, change.day,
		change.hour, change.minute, change.second);
#endif

	/* Add the change to the array. */
	icalarray_append (changes, &change);
	return;
    }

    /* The component has recurrence data, so we expand that now. */
    prop = icalcomponent_get_first_property (comp, ICAL_ANY_PROPERTY);
    while (prop) {
#if 0
	printf ("Expanding property...\n");
#endif
	switch (icalproperty_isa (prop)) {
	case ICAL_RDATE_PROPERTY:
	    rdate = icalproperty_get_rdate (prop);
	    change.year   = rdate.time.year;
	    change.month  = rdate.time.month;
	    change.day    = rdate.time.day;
	    /* RDATEs with a DATE value inherit the time from
	       the DTSTART. */
	    if (rdate.time.is_date) {
		change.hour   = dtstart.hour;
		change.minute = dtstart.minute;
		change.second = dtstart.second;
	    } else {
		change.hour   = rdate.time.hour;
		change.minute = rdate.time.minute;
		change.second = rdate.time.second;

		/* The spec was a bit vague about whether RDATEs were in local
		   time or UTC so we support both to be safe. So if it is in
		   UTC we have to add the UTC offset to get a local time. */
		if (!rdate.time.is_utc)
		    icaltimezone_adjust_change (&change, 0, 0, 0,
						-change.prev_utc_offset);
	    }

#if 0
	    printf ("  Appending RDATE element (Y/M/D): %i/%02i/%02i %i:%02i:%02i\n",
		    change.year, change.month, change.day,
		    change.hour, change.minute, change.second);
#endif

	    icalarray_append (changes, &change);
	    break;
	case ICAL_RRULE_PROPERTY:
	    rrule = icalproperty_get_rrule (prop);

	    /* If the rrule UNTIL value is set and is in UTC, we convert it to
	       a local time, since the recurrence code has no way to convert
	       it itself. */
	    if (!icaltime_is_null_time (rrule.until) && rrule.until.is_utc) {
#if 0
		printf ("  Found RRULE UNTIL in UTC.\n");
#endif

		/* To convert from UTC to a local time, we use the TZOFFSETFROM
		   since that is the offset from UTC that will be in effect
		   when each of the RRULE occurrences happens. */
		icaltime_adjust (&rrule.until, 0, 0, 0,
				 change.prev_utc_offset);
		rrule.until.is_utc = 0;
	    }

	    rrule_iterator = icalrecur_iterator_new (rrule, dtstart);
	    for (;;) {
		occ = icalrecur_iterator_next (rrule_iterator);
		if (occ.year > end_year || icaltime_is_null_time (occ))
		    break;

		change.year   = occ.year;
		change.month  = occ.month;
		change.day    = occ.day;
		change.hour   = occ.hour;
		change.minute = occ.minute;
		change.second = occ.second;

#if 0
		printf ("  Appending RRULE element (Y/M/D): %i/%02i/%02i %i:%02i:%02i\n",
			change.year, change.month, change.day,
			change.hour, change.minute, change.second);
#endif

		icaltimezone_adjust_change (&change, 0, 0, 0,
					    -change.prev_utc_offset);

		icalarray_append (changes, &change);
	    }

	    icalrecur_iterator_free (rrule_iterator);
	    break;
	default:
	    break;
	}

	prop = icalcomponent_get_next_property (comp, ICAL_ANY_PROPERTY);
    }
}


/* A function to compare 2 icaltimezonechange elements, used for qsort(). */
static int
icaltimezone_compare_change_fn		(const void	*elem1,
					 const void	*elem2)
{
    const icaltimezonechange *change1, *change2;
    int retval;

    change1 = elem1;
    change2 = elem2;

    if (change1->year < change2->year)
	retval = -1;
    else if (change1->year > change2->year)
	retval = 1;

    else if (change1->month < change2->month)
	retval = -1;
    else if (change1->month > change2->month)
	retval = 1;

    else if (change1->day < change2->day)
	retval = -1;
    else if (change1->day > change2->day)
	retval = 1;

    else if (change1->hour < change2->hour)
	retval = -1;
    else if (change1->hour > change2->hour)
	retval = 1;

    else if (change1->minute < change2->minute)
	retval = -1;
    else if (change1->minute > change2->minute)
	retval = 1;

    else if (change1->second < change2->second)
	retval = -1;
    else if (change1->second > change2->second)
	retval = 1;

    else
	retval = 0;

    return retval;
}



void
icaltimezone_convert_time		(struct icaltimetype *tt,
					 icaltimezone	*from_zone,
					 icaltimezone	*to_zone)
{
    int utc_offset, is_daylight;

    /* First we convert the time to UTC by getting the UTC offset and
       subtracting it. */       
    utc_offset = icaltimezone_get_utc_offset (from_zone, tt, NULL);
    icaltime_adjust (tt, 0, 0, 0, -utc_offset);

    /* Now we convert the time to the new timezone by getting the UTC offset
       of our UTC time and adding it. */       
    utc_offset = icaltimezone_get_utc_offset_of_utc_time (to_zone, tt,
							  &is_daylight);
    tt->is_daylight = is_daylight;
    icaltime_adjust (tt, 0, 0, 0, utc_offset);
}





/* Calculates the UTC offset of a given local time in the given timezone.
   It is the number of seconds to add to UTC to get local time.
   The is_daylight flag is set to 1 if the time is in daylight-savings time. */
int
icaltimezone_get_utc_offset		(icaltimezone	*zone,
					 struct icaltimetype	*tt,
					 int		*is_daylight)
{
    icaltimezonechange *zone_change, *prev_zone_change, tt_change, tmp_change;
    int change_num, step, utc_offset_change, cmp;
    int change_num_to_use;
    char want_daylight;

    if (is_daylight)
	*is_daylight = 0;

    /* For local times and UTC return 0. */
    if (zone == NULL || zone == &utc_timezone)
	return 0;

    /* Use the builtin icaltimezone if possible. */
    if (zone->builtin_timezone)
	zone = zone->builtin_timezone;

    /* Make sure the changes array is expanded up to the given time. */
    icaltimezone_ensure_coverage (zone, tt->year);

    if (!zone->changes || zone->changes->num_elements == 0)
	return 0;

    /* Copy the time parts of the icaltimetype to an icaltimezonechange so we
       can use our comparison function on it. */
    tt_change.year   = tt->year;
    tt_change.month  = tt->month;
    tt_change.day    = tt->day;
    tt_change.hour   = tt->hour;
    tt_change.minute = tt->minute;
    tt_change.second = tt->second;

    /* This should find a change close to the time, either the change before
       it or the change after it. */
    change_num = icaltimezone_find_nearby_change (zone, &tt_change);

    /* Sanity check. */
    icalerror_assert (change_num >= 0,
		      "Negative timezone change index");
    icalerror_assert (change_num < zone->changes->num_elements,
		      "Timezone change index out of bounds");

    /* Now move backwards or forwards to find the timezone change that applies
       to tt. It should only have to do 1 or 2 steps. */
    zone_change = icalarray_element_at (zone->changes, change_num);
    step = 1;
    change_num_to_use = -1;
    for (;;) {
	/* Copy the change, so we can adjust it. */
	tmp_change = *zone_change;

	/* If the clock is going backward, check if it is in the region of time
	   that is used twice. If it is, use the change with the daylight
	   setting which matches tt, or use standard if we don't know. */
	if (tmp_change.utc_offset < tmp_change.prev_utc_offset) {
	    /* If the time change is at 2:00AM local time and the clock is
	       going back to 1:00AM we adjust the change to 1:00AM. We may
	       have the wrong change but we'll figure that out later. */
	    icaltimezone_adjust_change (&tmp_change, 0, 0, 0,
					tmp_change.utc_offset);
	} else {
	    icaltimezone_adjust_change (&tmp_change, 0, 0, 0,
					tmp_change.prev_utc_offset);
	}

	cmp = icaltimezone_compare_change_fn (&tt_change, &tmp_change);

	/* If the given time is on or after this change, then this change may
	   apply, but we continue as a later change may be the right one.
	   If the given time is before this change, then if we have already
	   found a change which applies we can use that, else we need to step
	   backwards. */
	if (cmp >= 0)
	    change_num_to_use = change_num;
	else
	    step = -1;

	/* If we are stepping backwards through the changes and we have found
	   a change that applies, then we know this is the change to use so
	   we exit the loop. */
	if (step == -1 && change_num_to_use != -1)
	    break;

	change_num += step;

	/* If we go past the start of the changes array, then we have no data
	   for this time so we return a UTC offset of 0. */
	if (change_num < 0)
	    return 0;

	if (change_num >= zone->changes->num_elements)
	    break;

	zone_change = icalarray_element_at (zone->changes, change_num);
    }

    /* If we didn't find a change to use, then we have a bug! */
    icalerror_assert (change_num_to_use != -1,
		      "No applicable timezone change found");

    /* Now we just need to check if the time is in the overlapped region of
       time when clocks go back. */
    zone_change = icalarray_element_at (zone->changes, change_num_to_use);

    utc_offset_change = zone_change->utc_offset - zone_change->prev_utc_offset;
    if (utc_offset_change < 0 && change_num_to_use > 0) {
	tmp_change = *zone_change;
	icaltimezone_adjust_change (&tmp_change, 0, 0, 0,
				    tmp_change.prev_utc_offset);

	if (icaltimezone_compare_change_fn (&tt_change, &tmp_change) < 0) {
	    /* The time is in the overlapped region, so we may need to use
	       either the current zone_change or the previous one. If the
	       time has the is_daylight field set we use the matching change,
	       else we use the change with standard time. */
	    prev_zone_change = icalarray_element_at (zone->changes,
						     change_num_to_use - 1);

	    /* I was going to add an is_daylight flag to struct icaltimetype,
	       but iCalendar doesn't let us distinguish between standard and
	       daylight time anyway, so there's no point. So we just use the
	       standard time instead. */
	    want_daylight = (tt->is_daylight == 1) ? 1 : 0;

	    if (zone_change->is_daylight == prev_zone_change->is_daylight)
		printf (" **** Same is_daylight setting\n");

	    if (zone_change->is_daylight != want_daylight
		&& prev_zone_change->is_daylight == want_daylight)
		zone_change = prev_zone_change;
	}
    }

    /* Now we know exactly which timezone change applies to the time, so
       we can return the UTC offset and whether it is a daylight time. */
    if (is_daylight)
	*is_daylight = zone_change->is_daylight;
    return zone_change->utc_offset;
}


/* Calculates the UTC offset of a given UTC time in the given timezone.
   It is the number of seconds to add to UTC to get local time.
   The is_daylight flag is set to 1 if the time is in daylight-savings time. */
int
icaltimezone_get_utc_offset_of_utc_time	(icaltimezone	*zone,
					 struct icaltimetype	*tt,
					 int		*is_daylight)
{
    icaltimezonechange *zone_change, tt_change, tmp_change;
    int change_num, step, change_num_to_use;
    int debug = 0;

    if (is_daylight)
	*is_daylight = 0;

#if 0
    if (tt->day == 30 && tt->month == 3 && tt->year == 2000
	&& tt->hour == 22 && tt->minute == 0 &&  tt->second == 0) {
	printf ("Getting UTC offset of %i/%i/%i %i:%02i:%02i\n",
		tt->day, tt->month, tt->year,
		tt->hour, tt->minute, tt->second);
	debug = 1;
    }
#endif

    /* For local times and UTC return 0. */
    if (zone == NULL || zone == &utc_timezone)
	return 0;

    /* Use the builtin icaltimezone if possible. */
    if (zone->builtin_timezone)
	zone = zone->builtin_timezone;

    /* Make sure the changes array is expanded up to the given time. */
    icaltimezone_ensure_coverage (zone, tt->year);

    if (!zone->changes || zone->changes->num_elements == 0)
	return 0;

    /* Copy the time parts of the icaltimetype to an icaltimezonechange so we
       can use our comparison function on it. */
    tt_change.year   = tt->year;
    tt_change.month  = tt->month;
    tt_change.day    = tt->day;
    tt_change.hour   = tt->hour;
    tt_change.minute = tt->minute;
    tt_change.second = tt->second;

    /* This should find a change close to the time, either the change before
       it or the change after it. */
    change_num = icaltimezone_find_nearby_change (zone, &tt_change);

    /* Sanity check. */
    icalerror_assert (change_num >= 0,
		      "Negative timezone change index");
    icalerror_assert (change_num < zone->changes->num_elements,
		      "Timezone change index out of bounds");

    /* Now move backwards or forwards to find the timezone change that applies
       to tt. It should only have to do 1 or 2 steps. */
    zone_change = icalarray_element_at (zone->changes, change_num);
    step = 1;
    change_num_to_use = -1;
    for (;;) {
	/* Copy the change and adjust it to UTC. */
	tmp_change = *zone_change;

	if (debug) {
	    printf ("  Change: %i/%i/%i %i:%02i:%02i\n",
		    zone_change->day, zone_change->month, zone_change->year,
		    zone_change->hour, zone_change->minute, zone_change->second);
	}


	/* If the given time is on or after this change, then this change may
	   apply, but we continue as a later change may be the right one.
	   If the given time is before this change, then if we have already
	   found a change which applies we can use that, else we need to step
	   backwards. */
	if (icaltimezone_compare_change_fn (&tt_change, &tmp_change) >= 0)
	    change_num_to_use = change_num;
	else
	    step = -1;

	/* If we are stepping backwards through the changes and we have found
	   a change that applies, then we know this is the change to use so
	   we exit the loop. */
	if (step == -1 && change_num_to_use != -1)
	    break;

	change_num += step;

	/* If we go past the start of the changes array, then we have no data
	   for this time so we return a UTC offset of 0. */
	if (change_num < 0)
	    return 0;

	if (change_num >= zone->changes->num_elements)
	    break;

	zone_change = icalarray_element_at (zone->changes, change_num);
    }

    /* If we didn't find a change to use, then we have a bug! */
    icalerror_assert (change_num_to_use != -1,
		      "No applicable timezone change found");

    /* Now we know exactly which timezone change applies to the time, so
       we can return the UTC offset and whether it is a daylight time. */
    zone_change = icalarray_element_at (zone->changes, change_num_to_use);
    if (is_daylight)
	*is_daylight = zone_change->is_daylight;

    if (debug) {
	printf ("  Change: %i/%i/%i %i:%02i:%02i\n",
		zone_change->day, zone_change->month, zone_change->year,
		zone_change->hour, zone_change->minute, zone_change->second);
	printf ("  -> %i\n", zone_change->utc_offset);
    }

    return zone_change->utc_offset;
}


/* Returns the index of a timezone change which is close to the time given
   in change. */
static int
icaltimezone_find_nearby_change		(icaltimezone		*zone,
					 icaltimezonechange	*change)
{
    icaltimezonechange *zone_change;
    int lower, upper, middle, cmp;
					 
    /* Do a simple binary search. */
    lower = middle = 0;
    upper = zone->changes->num_elements;

    while (lower < upper) {
	middle = (lower + upper) >> 1;
	zone_change = icalarray_element_at (zone->changes, middle);
	cmp = icaltimezone_compare_change_fn (change, zone_change);
	if (cmp == 0)
	    break;
	else if (cmp < 0)
	    upper = middle;
	else
	    lower = middle + 1;
    }

    return middle;
}




/* Adds (or subtracts) a time from a icaltimezonechange.
   NOTE: This function is exactly the same as icaltime_adjust()
   except for the type of the first parameter. */
static void
icaltimezone_adjust_change		(icaltimezonechange *tt,
					 int		 days,
					 int		 hours,
					 int		 minutes,
					 int		 seconds)
{
    int second, minute, hour, day;
    int minutes_overflow, hours_overflow, days_overflow;
    int days_in_month;

    /* Add on the seconds. */
    second = tt->second + seconds;
    tt->second = second % 60;
    minutes_overflow = second / 60;
    if (tt->second < 0) {
	tt->second += 60;
	minutes_overflow--;
    }

    /* Add on the minutes. */
    minute = tt->minute + minutes + minutes_overflow;
    tt->minute = minute % 60;
    hours_overflow = minute / 60;
    if (tt->minute < 0) {
	tt->minute += 60;
	hours_overflow--;
    }

    /* Add on the hours. */
    hour = tt->hour + hours + hours_overflow;
    tt->hour = hour % 24;
    days_overflow = hour / 24;
    if (tt->hour < 0) {
	tt->hour += 24;
	days_overflow--;
    }

    /* Add on the days. */
    day = tt->day + days + days_overflow;
    if (day > 0) {
	for (;;) {
	    days_in_month = icaltime_days_in_month (tt->month, tt->year);
	    if (day <= days_in_month)
		break;

	    tt->month++;
	    if (tt->month >= 13) {
		tt->year++;
		tt->month = 1;
	    }

	    day -= days_in_month;
	}
    } else {
	while (day <= 0) {
	    if (tt->month == 1) {
		tt->year--;
		tt->month = 12;
	    } else {
		tt->month--;
	    }

	    day += icaltime_days_in_month (tt->month, tt->year);
	}
    }
    tt->day = day;
}



/* Compares 2 VTIMEZONE components to see if they match, ignoring their TZIDs.
   It returns 1 if they match, 0 if they don't, or -1 on error. */
int
icaltimezone_compare_vtimezone		(icalcomponent	*vtimezone1,
					 icalcomponent	*vtimezone2)
{
    icalproperty *prop1, *prop2;
    const char *tzid1, *tzid2;
    char *tzid2_copy, *string1, *string2;
    int cmp;

    /* Get the TZID property of the first VTIMEZONE. */
    prop1 = icalcomponent_get_first_property (vtimezone1, ICAL_TZID_PROPERTY);
    if (!prop1)
	return -1;

    tzid1 = icalproperty_get_tzid (prop1);
    if (!tzid1)
	return -1;

    /* Get the TZID property of the second VTIMEZONE. */
    prop2 = icalcomponent_get_first_property (vtimezone1, ICAL_TZID_PROPERTY);
    if (!prop2)
	return -1;

    tzid2 = icalproperty_get_tzid (prop2);
    if (!tzid2)
	return -1;

    /* Copy the second TZID, and set the property to the same as the first
       TZID, since we don't care if these match of not. */
    tzid2_copy = strdup (tzid2);
    icalproperty_set_tzid (prop2, tzid1);

    /* Now convert both VTIMEZONEs to strings and compare them. */
    string1 = icalcomponent_as_ical_string (vtimezone1);
    if (!string1) {
	free (tzid2_copy);
	return -1;
    }

    string2 = icalcomponent_as_ical_string (vtimezone2);
    if (!string2) {
	free (string1);
	free (tzid2_copy);
	return -1;
    }

    cmp = strcmp (string1, string2);

    free (string1);
    free (string2);

    /* Now reset the second TZID. */
    icalproperty_set_tzid (prop2, tzid2_copy);
    free (tzid2_copy);

    return (cmp == 0) ? 1 : 0;
}



char*
icaltimezone_get_tzid			(icaltimezone	*zone)
{
    return zone->tzid;
}


char*
icaltimezone_get_location		(icaltimezone	*zone)
{
    return zone->location;
}


/* Returns the latitude of a builtin timezone. */
double
icaltimezone_get_latitude		(icaltimezone	*zone)
{
    return zone->latitude;
}


/* Returns the longitude of a builtin timezone. */
double
icaltimezone_get_longitude		(icaltimezone	*zone)
{
    return zone->longitude;
}


/* Returns the VTIMEZONE component of a timezone. */
icalcomponent*
icaltimezone_get_component		(icaltimezone	*zone)
{
    return zone->component;

}


icalarray*
icaltimezone_array_new			(void)
{
    return icalarray_new (sizeof (icaltimezone), 16);
}


void
icaltimezone_array_append_from_vtimezone (icalarray	*timezones,
					  icalcomponent	*child)
{
    icaltimezone zone;

    if (icaltimezone_init_from_vtimezone (&zone, child))
	icalarray_append (timezones, &zone);
}



/*
 * BUILTIN TIMEZONE HANDLING
 */


/* Returns an icalarray of icaltimezone structs, one for each builtin timezone.
   This will load and parse the zones.tab file to get the timezone names and
   their coordinates. It will not load the VTIMEZONE data for any timezones. */
icalarray*
icaltimezone_get_builtin_timezones	(void)
{
    if (!builtin_timezones)
	icaltimezone_init_builtin_timezones ();

    return builtin_timezones;
}


/* Returns an icaltimezone corresponding to the given location. */
icaltimezone*
icaltimezone_get_utc_timezone		(void)
{
    return &utc_timezone;
}



/* This initializes the builtin timezone data, i.e. the builtin_timezones
   array and the special UTC timezone. It should be called before any
   code that uses the timezone functions. */
static void
icaltimezone_init_builtin_timezones	(void)
{
    /* Initialize the special UTC timezone. */
    utc_timezone.tzid = "UTC";

    icaltimezone_parse_zone_tab ();
}


/* This parses the zones.tab file containing the names and locations of the
   builtin timezones. It creates the builtin_timezones array which is an
   icalarray of icaltimezone structs. It only fills in the location, latitude
   and longtude fields; the rest are left blank. The VTIMEZONE component is
   loaded later if it is needed. The timezones in the zones.tab file are
   sorted by their name, which is useful for binary searches. */
static void
icaltimezone_parse_zone_tab		(void)
{
    char *filename;
    FILE *fp;
    char buf[1024];  /* Used to store each line of zones.tab as it is read. */
    char location[1024]; /* Stores the city name when parsing buf. */
    int filename_len;
    int latitude_degrees, latitude_minutes, latitude_seconds;
    int longitude_degrees, longitude_minutes, longitude_seconds;
    icaltimezone zone;

    icalerror_assert (builtin_timezones == NULL,
		      "Parsing zones.tab file multiple times");

    filename_len = strlen (ZONEINFO_DIRECTORY) + strlen (ZONES_TAB_FILENAME)
	+ 2;

    filename = (char*) malloc (filename_len);
    if (!filename) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return;
    }

    snprintf (filename, filename_len, "%s/%s", ZONEINFO_DIRECTORY,
	      ZONES_TAB_FILENAME);

    fp = fopen (filename, "r");
    free (filename);
    if (!fp) {
	icalerror_set_errno(ICAL_FILE_ERROR);
	return;
    }

    builtin_timezones = icalarray_new (sizeof (icaltimezone), 32);

    while (fgets (buf, sizeof(buf), fp)) {
	if (*buf == '#') continue;

	/* The format of each line is: "latitude longitude location". */
	if (sscanf (buf, "%4d%2d%2d %4d%2d%2d %s",
		    &latitude_degrees, &latitude_minutes,
		    &latitude_seconds,
		    &longitude_degrees, &longitude_minutes,
		    &longitude_seconds,
		    &location) != 7) {
	    fprintf (stderr, "Invalid timezone description line: %s\n", buf);
	    continue;
	}

	icaltimezone_init (&zone);
	zone.location = strdup (location);

	if (latitude_degrees >= 0)
	    zone.latitude = (double) latitude_degrees
		+ (double) latitude_minutes / 60
		+ (double) latitude_seconds / 3600;
	else
	    zone.latitude = (double) latitude_degrees
		- (double) latitude_minutes / 60
		- (double) latitude_seconds / 3600;

	if (longitude_degrees >= 0)
	    zone.longitude = (double) longitude_degrees
		+ (double) longitude_minutes / 60
		+ (double) longitude_seconds / 3600;
	else
	    zone.longitude = (double) longitude_degrees
		- (double) longitude_minutes / 60
		- (double) longitude_seconds / 3600;

	icalarray_append (builtin_timezones, &zone);

#if 0
	printf ("Found zone: %s %f %f\n",
		location, zone.latitude, zone.longitude);
#endif
    }

    fclose (fp);
}


/* Loads the builtin VTIMEZONE data for the given timezone. */
static void
icaltimezone_load			(icaltimezone	*zone)
{
    char *filename;
    int filename_len;
    FILE *fp;
    icalparser *parser;
    icalcomponent *comp;

    filename_len = strlen (ZONEINFO_DIRECTORY) + strlen (zone->location) + 6;

    filename = (char*) malloc (filename_len);
    if (!filename) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return;
    }

    snprintf (filename, filename_len, "%s/%s.ics", ZONEINFO_DIRECTORY,
	      zone->location);

    fp = fopen (filename, "r");
    free (filename);
    if (!fp) {
	icalerror_set_errno(ICAL_FILE_ERROR);
	return;
    }

    parser = icalparser_new ();
    icalparser_set_gen_data (parser, fp);
    comp = icalparser_parse (parser, icaltimezone_load_get_line_fn);
    icalparser_free (parser);
    fclose (fp);

    /* Find the VTIMEZONE component inside the VCALENDAR. There should be 1. */
    zone->component = icalcomponent_get_first_component (comp, ICAL_VTIMEZONE_COMPONENT);
}


/* Callback used from icalparser_parse() */
static char *
icaltimezone_load_get_line_fn		(char		*s,
					 size_t		 size,
					 void		*data)
{
    return fgets (s, size, (FILE*) data);
}




/*
 * DEBUGGING
 */

/*
 * This outputs a list of timezone changes for the given timezone to the
 * given file, up to the maximum year given. We compare this output with the
 * output from 'vzic --dump-changes' to make sure that we are consistent.
 * (vzic is the Olson timezone database to VTIMEZONE converter.)
 * 
 * The output format is:
 *
 *	Zone-Name [tab] Date [tab] Time [tab] UTC-Offset
 *
 * The Date and Time fields specify the time change in UTC.
 *
 * The UTC Offset is for local (wall-clock) time. It is the amount of time
 * to add to UTC to get local time.
 */
int
icaltimezone_dump_changes		(icaltimezone	*zone,
					 int		 max_year,
					 FILE		*fp)
{
    static char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
			      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    icaltimezonechange *zone_change;
    int change_num;
    char buffer[8];

    /* Make sure the changes array is expanded up to the given time. */
    icaltimezone_ensure_coverage (zone, max_year);

#if 0
    printf ("Num changes: %i\n", zone->changes->num_elements);
#endif

    change_num = 0;
    for (change_num = 0; change_num < zone->changes->num_elements; change_num++) {
	zone_change = icalarray_element_at (zone->changes, change_num);

	if (zone_change->year > max_year)
	    break;

	fprintf (fp, "%s\t%2i %s %04i\t%2i:%02i:%02i",
		zone->location,
		zone_change->day, months[zone_change->month - 1],
		zone_change->year,
		zone_change->hour, zone_change->minute, zone_change->second);

	/* Wall Clock Time offset from UTC. */
	format_utc_offset (zone_change->utc_offset, buffer);
	fprintf (fp, "\t%s", buffer);

	fprintf (fp, "\n");
    }
}


/* This formats a UTC offset as "+HHMM" or "+HHMMSS".
   buffer should have space for 8 characters. */
static void
format_utc_offset			(int		 utc_offset,
					 char		*buffer)
{
  char *sign = "+";
  int hours, minutes, seconds;

  if (utc_offset < 0) {
    utc_offset = -utc_offset;
    sign = "-";
  }

  hours = utc_offset / 3600;
  minutes = (utc_offset % 3600) / 60;
  seconds = utc_offset % 60;

  /* Sanity check. Standard timezone offsets shouldn't be much more than 12
     hours, and daylight saving shouldn't change it by more than a few hours.
     (The maximum offset is 15 hours 56 minutes at present.) */
  if (hours < 0 || hours >= 24 || minutes < 0 || minutes >= 60
      || seconds < 0 || seconds >= 60) {
    fprintf (stderr, "Warning: Strange timezone offset: H:%i M:%i S:%i\n",
	     hours, minutes, seconds);
  }

  if (seconds == 0)
    sprintf (buffer, "%s%02i%02i", sign, hours, minutes);
  else
    sprintf (buffer, "%s%02i%02i%02i", sign, hours, minutes, seconds);
}
