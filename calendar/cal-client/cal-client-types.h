/* Evolution calendar utilities and types
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          JP Rosevear <jpr@ximian.com>
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

#ifndef CAL_CLIENT_TYPES_H
#define CAL_CLIENT_TYPES_H

#include <cal-util/cal-component.h>

G_BEGIN_DECLS



typedef enum {
	CAL_CLIENT_CHANGE_ADDED = 1 << 0,
	CAL_CLIENT_CHANGE_MODIFIED = 1 << 1,
	CAL_CLIENT_CHANGE_DELETED = 1 << 2
} CalClientChangeType;

typedef struct 
{
	CalComponent *comp;
	CalClientChangeType type;
} CalClientChange;

void cal_client_change_list_free (GList *list);

G_END_DECLS

#endif

