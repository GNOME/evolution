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

#include <config.h>
#include <stdlib.h>
#include "cal-util.h"
#include "libversit/vcc.h"



/**
 * cal_obj_instance_list_free:
 * @list: List of CalObjInstance structures.
 * 
 * Frees a list of CalObjInstance structures.
 **/
void
cal_obj_instance_list_free (GList *list)
{
	CalObjInstance *i;
	GList *l;

	for (l = list; l; l = l->next) {
		i = l->data;

		g_assert (i != NULL);
		g_assert (i->uid != NULL);
		g_assert (i->calobj != NULL);

		g_free (i->uid);
		g_free (i->calobj);
		g_free (i);
	}

	g_list_free (list);
}

/**
 * cal_obj_uid_list_free:
 * @list: List of strings with unique identifiers.
 *
 * Frees a list of unique identifiers for calendar objects.
 **/
void
cal_obj_uid_list_free (GList *list)
{
	GList *l;

	for (l = list; l; l = l->next) {
		char *uid;

		uid = l->data;
		g_free (uid);
	}

	g_list_free (list);
}


#warning FIXME -- do we need a complete calendar here?  should we use libical instead of libversit?  can this be the same as string_from_ical_object in cal-backend.c?
char *ical_object_to_string (iCalObject *ico)
{
	VObject *vobj;
	char *buf;

	vobj = ical_object_to_vobject (ico);
	buf = writeMemVObject (NULL, NULL, vobj);
	cleanStrTbl ();
	return buf;
}

iCalObject *string_to_ical_object (char *buffer)
{
	/* FIX ME */
#if 0
	/* something */
	VObject *vcal;
	vcal = Parse_MIME (buffer, strlen (buffer));
#endif /* 0 */
	return NULL;
}


#if 0
this is the one from calendar.c:

/*
 * calendar_string_from_object:
 *
 * Returns the iCalObject @object armored around a vCalendar
 * object as a string.
 */
char *
calendar_string_from_object (iCalObject *object)
{
	Calendar *cal;
	char *str;

	g_return_val_if_fail (object != NULL, NULL);
	
	cal = calendar_new ("Temporal",CALENDAR_INIT_NIL);
	calendar_add_object (cal, object);
	str = calendar_get_as_vcal_string (cal);
	calendar_remove_object (cal, object);

	calendar_destroy (cal);
	
	return str;
}

char *
calendar_get_as_vcal_string (Calendar *cal)
{
	VObject *vcal;
	char *result;
	
	g_return_val_if_fail (cal != NULL, NULL);

	vcal = vcalendar_create_from_calendar (cal);
	result = writeMemVObject (NULL, 0, vcal);

	cleanVObject (vcal);
	cleanStrTbl ();

	return result;
}

#endif /* 0 */

