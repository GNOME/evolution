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



/**
 * cal_obj_instance_list_free:
 * @list: List of #CalObjInstance structures.
 * 
 * Frees a list of #CalObjInstance structures.
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

		g_free (i->uid);
		g_free (i);
	}

	g_list_free (list);
}

/**
 * cal_alarm_instance_list_free:
 * @list: List of #CalAlarmInstance structures.
 * 
 * Frees a list of #CalAlarmInstance structures.
 **/
void
cal_alarm_instance_list_free (GList *list)
{
	CalAlarmInstance *i;
	GList *l;

	for (l = list; l; l = l->next) {
		i = l->data;

		g_assert (i != NULL);
		g_assert (i->uid != NULL);

		g_free (i->uid);
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

		g_assert (uid != NULL);
		g_free (uid);
	}

	g_list_free (list);
}
