/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: JP Rosevear <jpr@novell.com>
 */

#ifndef _E_MEETING_TYPES_H_
#define _E_MEETING_TYPES_H_

#include <glib.h>

/* Extended free/busy (XFB) vfreebusy properties */
#define E_MEETING_FREE_BUSY_XPROP_SUMMARY  "X-SUMMARY"
#define E_MEETING_FREE_BUSY_XPROP_LOCATION "X-LOCATION"
/* Maximum string length displayed in the XFB tooltip */
#define E_MEETING_FREE_BUSY_XPROP_MAXLEN   200

G_BEGIN_DECLS

typedef struct _EMeetingTime               EMeetingTime;
typedef struct _EMeetingFreeBusyPeriod     EMeetingFreeBusyPeriod;
typedef struct _EMeetingXfbData            EMeetingXfbData;

/* These are used to specify whether an attendee is free or busy at a
 * particular time. We'll probably replace this with a global calendar type.
 * These should be ordered in increasing order of preference. Higher precedence
 * busy periods will be painted over lower precedence ones. These are also
 * used as for loop counters, so they should start at 0 and be ordered. */
typedef enum
{
	E_MEETING_FREE_BUSY_TENTATIVE = 0,
	E_MEETING_FREE_BUSY_OUT_OF_OFFICE = 1,
	E_MEETING_FREE_BUSY_BUSY = 2,
	E_MEETING_FREE_BUSY_FREE = 3,

	E_MEETING_FREE_BUSY_LAST = 4
} EMeetingFreeBusyType;

/* This is our representation of a time. We use a GDate to store the day,
 * and guint8s for the hours and minutes. */
struct _EMeetingTime
{
	GDate	date;
	guint8	hour;
	guint8	minute;
};

/* This represents extended free/busy data (XFB) associated
 * with a busy period (optional). Groupware servers like Kolab
 * may send it as X-SUMMARY and X-LOCATION properties of vfreebusy
 * calendar objects.
 * See http://wiki.kolab.org/Free_Busy#Kolab_Object_Storage_Format
 * for a reference. If we find that a vfreebusy object carries
 * such information, we extract it and display it as a tooltip
 * for the busy period in the meeting time selector scheduling page.
 */
struct _EMeetingXfbData
{
	/* if adding more items, adapt e_meeting_xfb_data_clear() */
	gchar *summary;
	gchar *location;
};

/* This represents a busy period. */
struct _EMeetingFreeBusyPeriod
{
	EMeetingTime start;
	EMeetingTime end;
	EMeetingFreeBusyType busy_type;
	EMeetingXfbData xfb;
};

G_END_DECLS

#endif /* _E_MEETING_TYPES_H_ */
