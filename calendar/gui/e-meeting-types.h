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

#ifndef _E_MEETING_TYPES_H_
#define _E_MEETING_TYPES_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include "e-meeting-types.h"

G_BEGIN_DECLS



typedef struct _EMeetingTime               EMeetingTime;
typedef struct _EMeetingFreeBusyPeriod     EMeetingFreeBusyPeriod;

/* These are used to specify whether an attendee is free or busy at a
   particular time. We'll probably replace this with a global calendar type.
   These should be ordered in increasing order of preference. Higher precedence
   busy periods will be painted over lower precedence ones. These are also
   used as for loop counters, so they should start at 0 and be ordered. */
typedef enum
{
	E_MEETING_FREE_BUSY_TENTATIVE		= 0,
	E_MEETING_FREE_BUSY_OUT_OF_OFFICE	= 1,
	E_MEETING_FREE_BUSY_BUSY		= 2,
	E_MEETING_FREE_BUSY_FREE		= 3,

	E_MEETING_FREE_BUSY_LAST		= 4
} EMeetingFreeBusyType;

/* This is our representation of a time. We use a GDate to store the day,
   and guint8s for the hours and minutes. */
struct _EMeetingTime
{
	GDate	date;
	guint8	hour;
	guint8	minute;
};

/* This represents a busy period. */
struct _EMeetingFreeBusyPeriod
{
	EMeetingTime start;
	EMeetingTime end;
	EMeetingFreeBusyType busy_type;
};

G_END_DECLS

#endif /* _E_MEETING_TYPES_H_ */
