/* Evolution calendar - utility functions
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/gnome-i18n.h>
#include <e-util/e-time-utils.h>
#include <libecal/e-cal-time-util.h>
#include "config-data.h"
#include "util.h"

/* Converts a time_t to a string, relative to the specified timezone */
char *
timet_to_str_with_zone (time_t t, icaltimezone *zone)
{
	struct icaltimetype itt;
	struct tm tm;
	char buf[256];

	if (t == -1)
		return g_strdup (_("invalid time"));

	itt = icaltime_from_timet_with_zone (t, FALSE, zone);
	tm = icaltimetype_to_tm (&itt);

	e_time_format_date_and_time (&tm, config_data_get_24_hour_format (),
				     FALSE, FALSE, buf, sizeof (buf));
	return g_strdup (buf);
}
