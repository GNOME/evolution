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


#ifndef _FILTER_FILTER_H
#define _FILTER_FILTER_H

#include "filter-rule.h"

#define FILTER_TYPE_FILTER            (filter_filter_get_type ())
#define FILTER_FILTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_FILTER, FilterFilter))
#define FILTER_FILTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_FILTER, FilterFilterClass))
#define IS_FILTER_FILTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_FILTER))
#define IS_FILTER_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_FILTER))
#define FILTER_FILTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_FILTER, FilterFilterClass))

typedef struct _FilterFilter FilterFilter;
typedef struct _FilterFilterClass FilterFilterClass;

struct _FilterFilter {
	FilterRule parent_object;
	
	GList *actions;
};

struct _FilterFilterClass {
	FilterRuleClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};

GType           filter_filter_get_type (void);
FilterFilter   *filter_filter_new      (void);

/* methods */
void            filter_filter_add_action     (FilterFilter *fr, FilterPart *fp);
void            filter_filter_remove_action  (FilterFilter *fr, FilterPart *fp);
void            filter_filter_replace_action (FilterFilter *fr, FilterPart *fp, FilterPart *new);

void            filter_filter_build_action   (FilterFilter *fr, GString *out);

#endif /* ! _FILTER_FILTER_H */
