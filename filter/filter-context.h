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

#ifndef _FILTER_CONTEXT_H
#define _FILTER_CONTEXT_H

#include <gtk/gtk.h>
#include "rule-context.h"

#define FILTER_CONTEXT(obj)	GTK_CHECK_CAST (obj, filter_context_get_type (), FilterContext)
#define FILTER_CONTEXT_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_context_get_type (), FilterContextClass)
#define IS_FILTER_CONTEXT(obj)      GTK_CHECK_TYPE (obj, filter_context_get_type ())

typedef struct _FilterContext	FilterContext;
typedef struct _FilterContextClass	FilterContextClass;

struct _FilterContext {
	RuleContext parent;
	struct _FilterContextPrivate *priv;

	GList *actions;
};

struct _FilterContextClass {
	RuleContextClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		filter_context_get_type	(void);
FilterContext	*filter_context_new	(void);

/* methods */
void		filter_context_add_action(FilterContext *f, FilterPart *action);
FilterPart	*filter_context_find_action(FilterContext *f, char *name);
/*FilterPart	*filter_context_create_action(FilterContext *f, char *name);*/
FilterPart 	*filter_context_next_action(FilterContext *f, FilterPart *last);

#endif /* ! _FILTER_CONTEXT_H */

