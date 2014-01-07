/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_MEETING_UTILS_H_
#define _E_MEETING_UTILS_H_

#include "e-meeting-types.h"

G_BEGIN_DECLS



gint e_meeting_time_compare_times (EMeetingTime *time1,
				   EMeetingTime *time2);

/* Extended free/busy (XFB) helpers */

void e_meeting_xfb_data_init (EMeetingXfbData *xfb);

void e_meeting_xfb_data_set (EMeetingXfbData *xfb,
                             const gchar *summary,
                             const gchar *location);

void e_meeting_xfb_data_clear (EMeetingXfbData *xfb);

gchar * e_meeting_xfb_utf8_string_new_from_ical (const gchar *icalstring,
                                                 gsize max_len);

G_END_DECLS

#endif /* _E_MEETING_UTILS_H_ */
