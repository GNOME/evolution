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

#ifndef _EM_FILTER_CONTEXT_H
#define _EM_FILTER_CONTEXT_H

#include "filter/rule-context.h"

#define EM_TYPE_FILTER_CONTEXT            (em_filter_context_get_type ())
#define EM_FILTER_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_CONTEXT, EMFilterContext))
#define EM_FILTER_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_CONTEXT, EMFilterContextClass))
#define EM_IS_FILTER_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_CONTEXT))
#define EM_IS_FILTER_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_CONTEXT))
#define EM_FILTER_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_CONTEXT, EMFilterContextClass))

typedef struct _EMFilterContext EMFilterContext;
typedef struct _EMFilterContextClass EMFilterContextClass;

struct _EMFilterContext {
	RuleContext parent_object;
	
	GList *actions;
};

struct _EMFilterContextClass {
	RuleContextClass parent_class;
};

GType em_filter_context_get_type (void);
EMFilterContext *em_filter_context_new (void);

/* methods */
void em_filter_context_add_action (EMFilterContext *fc, FilterPart *action);
FilterPart *em_filter_context_find_action (EMFilterContext *fc, const char *name);
FilterPart *em_filter_context_create_action (EMFilterContext *fc, const char *name);
FilterPart *em_filter_context_next_action (EMFilterContext *fc, FilterPart *last);

#endif /* ! _EM_FILTER_CONTEXT_H */
