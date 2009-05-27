/*
 * Evolution Conduits - Pilot Map routines
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
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <libxml/parser.h>
#include <pi-util.h>

#include "e-pilot-util.h"

gchar *
e_pilot_utf8_to_pchar (const gchar *string)
{
	gchar *pstring = NULL;
	gint res;

	if (!string)
		return NULL;

	res = convert_ToPilotChar ("UTF-8", string, strlen (string), &pstring);

	if (res != 0)
		pstring = strdup (string);

	return pstring;
}

gchar *
e_pilot_utf8_from_pchar (const gchar *string)
{
	gchar *ustring = NULL;
	gint res;

	if (!string)
		return NULL;

	res = convert_FromPilotChar ("UTF-8", string, strlen (string), &ustring);

	if (res != 0)
		ustring = strdup (string);

	return ustring;
}

ESource *
e_pilot_get_sync_source (ESourceList *source_list)
{
	GSList *g;

	g_return_val_if_fail (source_list != NULL, NULL);
	g_return_val_if_fail (E_IS_SOURCE_LIST (source_list), NULL);

	for (g = e_source_list_peek_groups (source_list); g; g = g->next) {
		ESourceGroup *group = E_SOURCE_GROUP (g->data);
		GSList *s;

		for (s = e_source_group_peek_sources (group); s; s = s->next) {
			ESource *source = E_SOURCE (s->data);

			if (e_source_get_property (source, "pilot-sync"))
				return source;
		}
	}

	return NULL;
}

void
e_pilot_set_sync_source (ESourceList *source_list, ESource *source)
{
	GSList *g;

	g_return_if_fail (source_list != NULL);
	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	for (g = e_source_list_peek_groups (source_list); g; g = g->next) {
		GSList *s;
		for (s = e_source_group_peek_sources (E_SOURCE_GROUP (g->data));
		     s; s = s->next) {
			e_source_set_property (E_SOURCE (s->data), "pilot-sync", NULL);
		}
	}

	if (source)
		e_source_set_property (source, "pilot-sync", "true");
	e_source_list_sync (source_list, NULL);
}
