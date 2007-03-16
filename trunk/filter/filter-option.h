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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef _FILTER_OPTION_H
#define _FILTER_OPTION_H

#include "filter-element.h"

#define FILTER_TYPE_OPTION            (filter_option_get_type ())
#define FILTER_OPTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_OPTION, FilterOption))
#define FILTER_OPTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_OPTION, FilterOptionClass))
#define IS_FILTER_OPTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_OPTION))
#define IS_FILTER_OPTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_OPTION))
#define FILTER_OPTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_OPTION, FilterOptionClass))

typedef struct _FilterOption FilterOption;
typedef struct _FilterOptionClass FilterOptionClass;

struct _filter_option {
	char *title;		/* button title */
	char *value;		/* value, if it has one */
	char *code;		/* used to string code segments together */
};

struct _FilterOption {
	FilterElement parent_object;
	
	const char *type;	/* static memory, type name written to xml */
	
	GList *options;
	struct _filter_option *current;
};

struct _FilterOptionClass {
	FilterElementClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};

GType filter_option_get_type (void);
FilterOption *filter_option_new (void);

/* methods */
void filter_option_set_current (FilterOption *option, const char *name);
struct _filter_option *filter_option_add (FilterOption *fo, const char *name, const char *title, const char *code);

#endif /* ! _FILTER_OPTION_H */
