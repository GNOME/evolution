/* Evolution calendar utilities and types
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          JP Rosevear <jpr@ximian.com>
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
#include "cal-client-types.h"



/**
 * cal_client_change_list_free:
 * @list: List of #CalClientChange structures.
 * 
 * Frees a list of #CalClientChange structures.
 **/
void
cal_client_change_list_free (GList *list)
{
	CalClientChange *c;
	GList *l;

	for (l = list; l; l = l->next) {
		c = l->data;

		g_assert (c != NULL);
		g_assert (c->comp != NULL);

		gtk_object_unref (GTK_OBJECT (c->comp));
		g_free (c);
	}

	g_list_free (list);
}
