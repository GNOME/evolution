/* Evolution calendar utilities and types
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef CAL_UTIL_H
#define CAL_UTIL_H

#include <libgnome/gnome-defs.h>
#include <time.h>
#include <glib.h>
#include <cal-util/calobj.h>

BEGIN_GNOME_DECLS



/* Instance of a calendar object.  This can be an actual occurrence, a
 * recurrence, or an alarm trigger of a `real' calendar object.
 */
typedef struct {
	char *uid;			/* UID of the object */
	time_t start;			/* Start time of instance */
	time_t end;			/* End time of instance */
} CalObjInstance;

void cal_obj_instance_list_free (GList *list);

/* Used for multiple UID queries */
typedef enum {
	CALOBJ_TYPE_EVENT   = 1 << 0,
	CALOBJ_TYPE_TODO    = 1 << 1,
	CALOBJ_TYPE_JOURNAL = 1 << 2,
	CALOBJ_TYPE_OTHER   = 1 << 3,
	CALOBJ_TYPE_ANY     = 0x0f
} CalObjType;

void cal_obj_uid_list_free (GList *list);

END_GNOME_DECLS

#endif
