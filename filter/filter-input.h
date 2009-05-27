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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _FILTER_INPUT_H
#define _FILTER_INPUT_H

#include "filter-element.h"

#define FILTER_TYPE_INPUT            (filter_input_get_type ())
#define FILTER_INPUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_INPUT, FilterInput))
#define FILTER_INPUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_INPUT, FilterInputClass))
#define IS_FILTER_INPUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_INPUT))
#define IS_FILTER_INPUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_INPUT))
#define FILTER_INPUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_INPUT, FilterInputClass))

typedef struct _FilterInput FilterInput;
typedef struct _FilterInputClass FilterInputClass;

struct _FilterInput {
	FilterElement parent_object;

	gchar *type;		/* name of type */
	GList *values;		/* strings */
};

struct _FilterInputClass {
	FilterElementClass parent_class;

	/* virtual methods */

	/* signals */
};

GType filter_input_get_type (void);
FilterInput *filter_input_new (void);

FilterInput *filter_input_new_type_name (const gchar *type);

/* methods */
void filter_input_set_value (FilterInput *fi, const gchar *value);

#endif /* ! _FILTER_INPUT_H */
