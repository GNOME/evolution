/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "em-search-context.h"

#include <string.h>

G_DEFINE_TYPE (EMSearchContext, em_search_context, E_TYPE_RULE_CONTEXT)

static EFilterElement *
search_context_new_element (ERuleContext *context,
                            const gchar *type)
{
	if (strcmp (type, "system-flag") == 0)
		return (EFilterElement *) e_filter_option_new ();

	if (strcmp (type, "score") == 0)
		return (EFilterElement *) e_filter_int_new_type ("score", -3, 3);

	/* Chain up to parent's new_element() method. */
	return E_RULE_CONTEXT_CLASS (em_search_context_parent_class)->
		new_element (context, type);
}

static void
em_search_context_class_init (EMSearchContextClass *class)
{
	ERuleContextClass *rule_context_class;

	rule_context_class = E_RULE_CONTEXT_CLASS (class);
	rule_context_class->new_element = search_context_new_element;
}

static void
em_search_context_init (EMSearchContext *vc)
{
	ERuleContext *rule_context;

	rule_context = E_RULE_CONTEXT (vc);

	rule_context->flags =
		E_RULE_CONTEXT_THREADING |
		E_RULE_CONTEXT_GROUPING;
}

ERuleContext *
em_search_context_new (void)
{
	return g_object_new (EM_SEARCH_TYPE_CONTEXT, NULL);
}
