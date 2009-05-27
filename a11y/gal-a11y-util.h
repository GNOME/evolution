/*
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
 *		Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __GAL_A11Y_UTIL_H__
#define __GAL_A11Y_UTIL_H__

#include <glib-object.h>

GType  gal_a11y_type_register_static_with_private  (GType        parent_type,
						    const gchar *type_name,
						    GTypeInfo   *info,
						    GTypeFlags   flags,
						    gint          priv_size,
						    gint        *priv_offset);

#endif /* ! __GAL_A11Y_UTIL_H__ */
