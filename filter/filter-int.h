/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Ripped off by Sam Creasey <sammy@oh.verio.com> from filter-score by:
 *
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _FILTER_INT_H
#define _FILTER_INT_H

#include "filter-element.h"

#define FILTER_INT(obj)           GTK_CHECK_CAST (obj, filter_int_get_type (), FilterInt)
#define FILTER_INT_CLASS(klass)   GTK_CHECK_CLASS_CAST (klass, filter_int_get_type (), FilterIntClass)
#define IS_FILTER_INT(obj)        GTK_CHECK_TYPE (obj, filter_int_get_type ())

typedef struct _FilterInt        FilterInt;
typedef struct _FilterIntClass   FilterIntClass;

struct _FilterInt {
	FilterElement parent;
	struct _FilterIntPrivate *priv;

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

GtkType		filter_int_get_type	(void);
FilterInt	*filter_int_new	(void);
FilterInt	*filter_int_new_type(const char *type, int min, int max);
void            filter_int_set_value(FilterInt *fi, int val);

/* methods */

#endif /* ! _FILTER_INT_H */

