/*
 *
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

#include "e-meeting-utils.h"

gint
e_meeting_time_compare_times (EMeetingTime*time1,
			      EMeetingTime*time2)
{
	gint day_comparison;

	day_comparison = g_date_compare (&time1->date,
					 &time2->date);
	if (day_comparison != 0)
		return day_comparison;

	if (time1->hour < time2->hour)
		return -1;
	if (time1->hour > time2->hour)
		return 1;

	if (time1->minute < time2->minute)
		return -1;
	if (time1->minute > time2->minute)
		return 1;

	/* The start times are exactly the same. */
	return 0;
}
