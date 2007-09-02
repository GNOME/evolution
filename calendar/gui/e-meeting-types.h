/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* itip-attendee.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: JP Rosevear
 */

#ifndef _E_MEETING_TYPES_H_
#define _E_MEETING_TYPES_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include "e-meeting-types.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */



typedef struct _EMeetingTime               EMeetingTime;
typedef struct _EMeetingFreeBusyPeriod     EMeetingFreeBusyPeriod;

/* These are used to specify whether an attendee is free or busy at a
   particular time. We'll probably replace this with a global calendar type.
   These should be ordered in increasing order of preference. Higher precedence
   busy periods will be painted over lower precedence ones. These are also
   used as for loop counters, so they should start at 0 and be ordered. */
typedef enum
{
	E_MEETING_FREE_BUSY_TENTATIVE       = 0,
	E_MEETING_FREE_BUSY_OUT_OF_OFFICE	= 1,
	E_MEETING_FREE_BUSY_BUSY		= 2,

	E_MEETING_FREE_BUSY_LAST		= 3
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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_MEETING_TYPES_H_ */
