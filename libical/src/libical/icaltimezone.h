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


#ifndef ICALTIMEZONE_H
#define ICALTIMEZONE_H

#include <stdio.h> /* For FILE* */
#include "icaltime.h"


/* An opaque struct representing a timezone. */
typedef struct _icaltimezone		icaltimezone;



/*
 * Accessing timezones.
 */

/* Returns the array of builtin icaltimezones. */
icalarray* icaltimezone_get_builtin_timezones	(void);

/* Returns a single builtin timezone, given its Olson city name. */
icaltimezone* icaltimezone_get_builtin_timezone	(const char *location);

/* Returns the UTC timezone. */
icaltimezone* icaltimezone_get_utc_timezone	(void);

/* Returns the TZID of a timezone. */
char*	icaltimezone_get_tzid			(icaltimezone	*zone);

/* Returns the city name of a timezone. */
char*	icaltimezone_get_location		(icaltimezone	*zone);

/* Returns the latitude of a builtin timezone. */
double	icaltimezone_get_latitude		(icaltimezone	*zone);

/* Returns the longitude of a builtin timezone. */
double	icaltimezone_get_longitude		(icaltimezone	*zone);

/* Returns the VTIMEZONE component of a timezone. */
icalcomponent*	icaltimezone_get_component	(icaltimezone	*zone);


/*
 * Converting times between timezones.
 */

void	icaltimezone_convert_time		(struct icaltimetype *tt,
						 icaltimezone	*from_zone,
						 icaltimezone	*to_zone);


/*
 * Getting offsets from UTC.
 */

/* Calculates the UTC offset of a given local time in the given timezone.
   It is the number of seconds to add to UTC to get local time.
   The is_daylight flag is set to 1 if the time is in daylight-savings time. */
int	icaltimezone_get_utc_offset		(icaltimezone	*zone,
						 struct icaltimetype *tt,
						 int		*is_daylight);

/* Calculates the UTC offset of a given UTC time in the given timezone.
   It is the number of seconds to add to UTC to get local time.
   The is_daylight flag is set to 1 if the time is in daylight-savings time. */
int	icaltimezone_get_utc_offset_of_utc_time	(icaltimezone	*zone,
						 struct icaltimetype *tt,
						 int		*is_daylight);




/*
 * Comparing VTIMEZONE components.
 */

/* Compares 2 VTIMEZONE components to see if they match, ignoring their TZIDs.
   It returns 1 if they match, 0 if they don't, or -1 on error. */
int	icaltimezone_compare_vtimezone		(icalcomponent	*vtimezone1,
						 icalcomponent	*vtimezone2);




/*
 * Handling arrays of timezones.
 */
icalarray*  icaltimezone_array_new		(void);

void	    icaltimezone_array_append_from_vtimezone (icalarray	    *timezones,
						      icalcomponent *child);



/*
 * Debugging Output.
 */

/* Dumps information about changes in the timezone up to and including
   max_year. */
int	icaltimezone_dump_changes		(icaltimezone	*zone,
						 int		 max_year,
						 FILE		*fp);

#endif /* ICALTIMEZONE_H */
