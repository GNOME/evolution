/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
	
	char *type;
	int val;
	int min;
	int max;
};

struct _FilterIntClass {
	FilterElementClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};

GType filter_int_get_type (void);
FilterInt *filter_int_new (void);
FilterInt *filter_int_new_type (const char *type, int min, int max);
void filter_int_set_value (FilterInt *fi, int val);

/* methods */

#endif /* ! _FILTER_INT_H */
