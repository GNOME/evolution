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

#ifndef _FILTER_FILTER_H
#define _FILTER_FILTER_H

#include <gtk/gtk.h>
#include "filter-rule.h"

#define FILTER_FILTER(obj)	GTK_CHECK_CAST (obj, filter_filter_get_type (), FilterFilter)
#define FILTER_FILTER_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_filter_get_type (), FilterFilterClass)
#define IS_FILTER_FILTER(obj)      GTK_CHECK_TYPE (obj, filter_filter_get_type ())

typedef struct _FilterFilter	FilterFilter;
typedef struct _FilterFilterClass	FilterFilterClass;

struct _FilterFilter {
	FilterRule parent;
	struct _FilterFilterPrivate *priv;

	GList *actions;
};

struct _FilterFilterClass {
	FilterRuleClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		filter_filter_get_type	(void);
FilterFilter	*filter_filter_new	(void);

/* methods */
void		filter_filter_add_action	(FilterFilter *fr, FilterPart *fp);
void		filter_filter_remove_action	(FilterFilter *fr, FilterPart *fp);
void		filter_filter_replace_action	(FilterFilter *fr, FilterPart *fp, FilterPart *new);

void		filter_filter_build_action	(FilterFilter *fr, GString *out);

#endif /* ! _FILTER_FILTER_H */

