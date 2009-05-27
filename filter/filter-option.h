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
	gchar *title;		/* button title */
	gchar *value;		/* value, if it has one */
	gchar *code;		/* used to string code segments together */

	gboolean is_dynamic;	/* whether is the option dynamic, FALSE if static */
};

struct _FilterOption {
	FilterElement parent_object;

	const gchar *type;	/* static memory, type name written to xml */

	GList *options;
	struct _filter_option *current;
	gchar *dynamic_func;	/* name of the dynamic fill func, called in get_widget */
};

struct _FilterOptionClass {
	FilterElementClass parent_class;

	/* virtual methods */

	/* signals */
};

GType filter_option_get_type (void);
FilterOption *filter_option_new (void);

/* methods */
void filter_option_set_current (FilterOption *option, const gchar *name);
const gchar *filter_option_get_current (FilterOption *option);

struct _filter_option *filter_option_add (FilterOption *fo, const gchar *name, const gchar *title, const gchar *code, gboolean is_dynamic);
void filter_option_remove_all (FilterOption *fo);

#endif /* ! _FILTER_OPTION_H */
