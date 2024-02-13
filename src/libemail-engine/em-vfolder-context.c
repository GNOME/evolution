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

#include "em-vfolder-context.h"

#include <string.h>

#include "em-filter-folder-element.h"
#include "em-vfolder-rule.h"

struct _EMVFolderContextPrivate {
	gint placeholder;
};

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE_WITH_PRIVATE (EMVFolderContext, em_vfolder_context, E_TYPE_RULE_CONTEXT)

static EFilterElement *
vfolder_context_new_element (ERuleContext *context,
                             const gchar *type)
{
	if (strcmp (type, "system-flag") == 0)
		return e_filter_option_new ();

	if (strcmp (type, "score") == 0)
		return e_filter_int_new_type ("score", -3, 3);

	if (strcmp (type, "folder") == 0)
		return em_filter_folder_element_new ();

	/* XXX Legacy type name.  Same as "folder" now. */
	if (strcmp (type, "folder-curi") == 0)
		return em_filter_folder_element_new ();

	return E_RULE_CONTEXT_CLASS (em_vfolder_context_parent_class)->
		new_element (context, type);
}

static void
em_vfolder_context_class_init (EMVFolderContextClass *class)
{
	ERuleContextClass *rule_context_class;

	rule_context_class = E_RULE_CONTEXT_CLASS (class);
	rule_context_class->new_element = vfolder_context_new_element;
}

static void
em_vfolder_context_init (EMVFolderContext *context)
{
	context->priv = em_vfolder_context_get_instance_private (context);

	e_rule_context_add_part_set (
		E_RULE_CONTEXT (context), "partset", E_TYPE_FILTER_PART,
		(ERuleContextPartFunc) e_rule_context_add_part,
		(ERuleContextNextPartFunc) e_rule_context_next_part);

	e_rule_context_add_rule_set (
		E_RULE_CONTEXT (context), "ruleset", EM_TYPE_VFOLDER_RULE,
		(ERuleContextRuleFunc) e_rule_context_add_rule,
		(ERuleContextNextRuleFunc) e_rule_context_next_rule);

	E_RULE_CONTEXT (context)->flags =
		E_RULE_CONTEXT_THREADING | E_RULE_CONTEXT_GROUPING;
}

EMVFolderContext *
em_vfolder_context_new (void)
{
	return g_object_new (
		EM_TYPE_VFOLDER_CONTEXT, NULL);
}
