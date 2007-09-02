/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
	
	char *type;		/* name of type */
	GList *values;		/* strings */
};

struct _FilterInputClass {
	FilterElementClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};

GType filter_input_get_type (void);
FilterInput *filter_input_new (void);

FilterInput *filter_input_new_type_name (const char *type);

/* methods */
void filter_input_set_value (FilterInput *fi, const char *value);

#endif /* ! _FILTER_INPUT_H */
