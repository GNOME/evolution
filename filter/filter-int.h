/*
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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _FILTER_INT_H
#define _FILTER_INT_H

#include "filter-element.h"

#define FILTER_TYPE_INT            (filter_int_get_type ())
#define FILTER_INT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_INT, FilterInt))
#define FILTER_INT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_INT, FilterIntClass))
#define IS_FILTER_INT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_INT))
#define IS_FILTER_INT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_INT))
#define FILTER_INT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_INT, FilterIntClass))

typedef struct _FilterInt FilterInt;
typedef struct _FilterIntClass FilterIntClass;

struct _FilterInt {
	FilterElement parent_object;

	gchar *type;
	gint val;
	gint min;
	gint max;
};

struct _FilterIntClass {
	FilterElementClass parent_class;

	/* virtual methods */

	/* signals */
};

GType filter_int_get_type (void);
FilterInt *filter_int_new (void);
FilterInt *filter_int_new_type (const gchar *type, gint min, gint max);
void filter_int_set_value (FilterInt *fi, gint val);

/* methods */

#endif /* ! _FILTER_INT_H */
