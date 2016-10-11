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
 *		Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-util.h"

GType
gal_a11y_type_register_static_with_private (GType parent_type,
                                            const gchar *type_name,
                                            GTypeInfo *info,
                                            GTypeFlags flags,
                                            gint priv_size,
                                            gint *priv_offset)
{
	GTypeQuery query;

	g_type_query (parent_type, &query);

	info->class_size = query.class_size;
	info->instance_size = query.instance_size + priv_size;

	if (priv_offset)
		*priv_offset = query.instance_size;

	return g_type_register_static (parent_type, type_name, info, flags);
}
