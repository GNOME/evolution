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


#ifndef _FILTER_CONTEXT_H
#define _FILTER_CONTEXT_H

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
FilterPart	*filter_context_find_action(FilterContext *f, const char *name);
FilterPart	*filter_context_create_action(FilterContext *f, const char *name);
FilterPart 	*filter_context_next_action(FilterContext *f, FilterPart *last);

#endif /* ! _FILTER_CONTEXT_H */

