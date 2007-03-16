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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "em-search-context.h"
#include "filter/filter-rule.h"
#include "filter/filter-option.h"
#include "filter/filter-int.h"

static FilterElement *em_search_new_element(RuleContext *rc, const char *type);

static RuleContextClass *parent_class = NULL;

static void
em_search_context_finalise (GObject *obj)
{
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
em_search_context_class_init (EMSearchContextClass *klass)
{
	parent_class = g_type_class_ref (RULE_TYPE_CONTEXT);
	
	((GObjectClass *)klass)->finalize = em_search_context_finalise;
	((RuleContextClass *)klass)->new_element = em_search_new_element;
}

static void
em_search_context_init (EMSearchContext *vc)
{
	rule_context_add_part_set ((RuleContext *)vc, "partset", filter_part_get_type (),
				   rule_context_add_part, rule_context_next_part);
		
	rule_context_add_rule_set ((RuleContext *)vc, "ruleset", filter_rule_get_type (),
				   rule_context_add_rule, rule_context_next_rule);

	((RuleContext *)vc)->flags = RULE_CONTEXT_THREADING | RULE_CONTEXT_GROUPING;
}

GType
em_search_context_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMSearchContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_search_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMSearchContext),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_search_context_init,
		};
		
		type = g_type_register_static (RULE_TYPE_CONTEXT, "EMSearchContext", &info, 0);
	}
	
	return type;
}

/**
 * em_search_context_new:
 *
 * Create a new EMSearchContext object.
 * 
 * Return value: A new #EMSearchContext object.
 **/
EMSearchContext *
em_search_context_new (void)
{
	return (EMSearchContext *) g_object_new (EM_SEARCH_TYPE_CONTEXT, NULL, NULL);
}

static FilterElement *
em_search_new_element(RuleContext *rc, const char *type)
{
	if (!strcmp (type, "system-flag")) {
		return (FilterElement *) filter_option_new ();
	} else if (!strcmp (type, "score")) {
		return (FilterElement *) filter_int_new_type("score", -3, 3);
	} else {
		return parent_class->new_element(rc, type);
	}
}
