/*
 *
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "em-search-context.h"
#include "filter/e-filter-rule.h"
#include "filter/e-filter-option.h"
#include "filter/e-filter-int.h"

static gpointer parent_class;

static EFilterElement *
search_context_new_element (ERuleContext *context,
                            const gchar *type)
{
	if (strcmp (type, "system-flag") == 0)
		return (EFilterElement *) e_filter_option_new ();

	if (strcmp (type, "score") == 0)
		return (EFilterElement *) e_filter_int_new_type ("score", -3, 3);

	/* Chain up to parent's new_element() method. */
	return E_RULE_CONTEXT_CLASS (parent_class)->new_element (context, type);
}

static void
search_context_class_init (EMSearchContextClass *class)
{
	ERuleContextClass *rule_context_class;

	parent_class = g_type_class_peek_parent (class);

	rule_context_class = E_RULE_CONTEXT_CLASS (class);
	rule_context_class->new_element = search_context_new_element;
}

static void
search_context_init (EMSearchContext *vc)
{
	ERuleContext *rule_context;

	rule_context = E_RULE_CONTEXT (vc);
	rule_context->flags = E_RULE_CONTEXT_THREADING | E_RULE_CONTEXT_GROUPING;
}

GType
em_search_context_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMSearchContextClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) search_context_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMSearchContext),
			0,     /* n_preallocs */
			(GInstanceInitFunc) search_context_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_RULE_CONTEXT, "EMSearchContext", &type_info, 0);
	}

	return type;
}

ERuleContext *
em_search_context_new (void)
{
	return g_object_new (EM_SEARCH_TYPE_CONTEXT, NULL);
}
