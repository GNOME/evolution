/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _FILTER_INPUT_H
#define _FILTER_INPUT_H

#include <gtk/gtk.h>
#include "filter-element.h"

#define FILTER_INPUT(obj)	GTK_CHECK_CAST (obj, filter_input_get_type (), FilterInput)
#define FILTER_INPUT_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_input_get_type (), FilterInputClass)
#define IS_FILTER_INPUT(obj)      GTK_CHECK_TYPE (obj, filter_input_get_type ())

typedef struct _FilterInput	FilterInput;
typedef struct _FilterInputClass	FilterInputClass;

struct _FilterInput {
	FilterElement parent;
	struct _FilterInputPrivate *priv;

	char *type;		/* name of type */
	GList *values;		/* strings */
};

struct _FilterInputClass {
	FilterElementClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		filter_input_get_type	(void);
FilterInput	*filter_input_new	(void);

FilterInput	*filter_input_new_type_name	(const char *type);

/* methods */
void		filter_input_set_value(FilterInput *fi, const char *value);

#endif /* ! _FILTER_INPUT_H */

