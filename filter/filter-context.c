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

#include <gtk/gtk.h>
#include <gnome.h>

#include "filter-context.h"
#include "filter-filter.h"

#define d(x)

static void filter_context_class_init	(FilterContextClass *class);
static void filter_context_init	(FilterContext *gspaper);
static void filter_context_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((FilterContext *)(x))->priv)

struct _FilterContextPrivate {
};

static RuleContextClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_context_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterContext",
			sizeof(FilterContext),
			sizeof(FilterContextClass),
			(GtkClassInitFunc)filter_context_class_init,
			(GtkObjectInitFunc)filter_context_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(rule_context_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_context_class_init (FilterContextClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(rule_context_get_type ());

	object_class->finalize = filter_context_finalise;
	/* override methods */

	/* signals */

	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
filter_context_init (FilterContext *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));

	rule_context_add_part_set((RuleContext *)o, "partset", filter_part_get_type(),
				  rule_context_add_part, rule_context_next_part);
	rule_context_add_part_set((RuleContext *)o, "actionset", filter_part_get_type(),
				  (RCPartFunc)filter_context_add_action,
				  (RCNextPartFunc)filter_context_next_action);

	rule_context_add_rule_set((RuleContext *)o, "ruleset", filter_filter_get_type(),
				  (RCRuleFunc)rule_context_add_rule, rule_context_next_rule);
}

static void
filter_context_finalise(GtkObject *obj)
{
	FilterContext *o = (FilterContext *)obj;

	o = o;

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_context_new:
 *
 * Create a new FilterContext object.
 * 
 * Return value: A new #FilterContext object.
 **/
FilterContext *
filter_context_new(void)
{
	FilterContext *o = (FilterContext *)gtk_type_new(filter_context_get_type ());
	return o;
}

void filter_context_add_action(FilterContext *f, FilterPart *action)
{
	d(printf("find action : "));
	f->actions = g_list_append(f->actions, action);
}

FilterPart *filter_context_find_action(FilterContext *f, char *name)
{
	d(printf("find action : "));
	return filter_part_find_list(f->actions, name);
}

FilterPart 	*filter_context_next_action(FilterContext *f, FilterPart *last)
{
	return filter_part_next_list(f->actions, last);
}
