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

#ifndef _FILTER_OPTION_H
#define _FILTER_OPTION_H

#include <gtk/gtk.h>

#include "filter-element.h"

#define FILTER_OPTION(obj)	GTK_CHECK_CAST (obj, filter_option_get_type (), FilterOption)
#define FILTER_OPTION_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_option_get_type (), FilterOptionClass)
#define IS_FILTER_OPTION(obj)      GTK_CHECK_TYPE (obj, filter_option_get_type ())

typedef struct _FilterOption	FilterOption;
typedef struct _FilterOptionClass	FilterOptionClass;

struct _filter_option {
	char *title;		/* button title */
	char *value;		/* value, if it has one */
	char *code;		/* used to string code segments together */
};

struct _FilterOption {
	FilterElement parent;
	struct _FilterOptionPrivate *priv;

	GList *options;
	struct _filter_option *current;
};

struct _FilterOptionClass {
	FilterElementClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		filter_option_get_type	(void);
FilterOption	*filter_option_new	(void);

/* methods */
void		filter_option_set_current(FilterOption *option, const char *name);

#endif /* ! _FILTER_OPTION_H */

