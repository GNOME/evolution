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

#include "em-vfolder-context.h"
#include "em-vfolder-rule.h"
#include "filter/filter-option.h"
#include "filter/filter-int.h"

#include "em-filter-folder-element.h"

static FilterElement *vfolder_new_element(RuleContext *rc, const gchar *type);

static RuleContextClass *parent_class = NULL;

static void
em_vfolder_context_finalise(GObject *obj)
{
        G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
em_vfolder_context_class_init(EMVFolderContextClass *klass)
{
	parent_class = g_type_class_ref(RULE_TYPE_CONTEXT);

	((GObjectClass *)klass)->finalize = em_vfolder_context_finalise;
	((RuleContextClass *)klass)->new_element = vfolder_new_element;
}

static void
em_vfolder_context_init(EMVFolderContext *vc)
{
	rule_context_add_part_set((RuleContext *) vc, "partset", filter_part_get_type(),
				   rule_context_add_part, rule_context_next_part);

	rule_context_add_rule_set((RuleContext *) vc, "ruleset", em_vfolder_rule_get_type(),
				   rule_context_add_rule, rule_context_next_rule);

	((RuleContext *)vc)->flags = RULE_CONTEXT_THREADING | RULE_CONTEXT_GROUPING;
}

GType
em_vfolder_context_get_type(void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMVFolderContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_vfolder_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(EMVFolderContext),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_vfolder_context_init,
		};

		type = g_type_register_static(RULE_TYPE_CONTEXT, "EMVFolderContext", &info, 0);
	}

	return type;
}

/**
 * em_vfolder_context_new:
 *
 * Create a new EMVFolderContext object.
 *
 * Return value: A new #EMVFolderContext object.
 **/
EMVFolderContext *
em_vfolder_context_new(void)
{
	return (EMVFolderContext *)g_object_new(em_vfolder_context_get_type(), NULL, NULL);
}

static FilterElement *
vfolder_new_element(RuleContext *rc, const gchar *type)
{
	if (!strcmp(type, "system-flag")) {
		return (FilterElement *) filter_option_new();
	} else if (!strcmp(type, "score")) {
		return (FilterElement *) filter_int_new_type("score", -3, 3);
	} else if (!strcmp(type, "folder-curi")) {
		EMFilterFolderElement *ff = em_filter_folder_element_new ();
		if (ff)
			ff->store_camel_uri = TRUE;
		return (FilterElement *) ff;
	} else if (!strcmp(type, "folder")) {
		return (FilterElement *) em_filter_folder_element_new();
	} else {
		return parent_class->new_element(rc, type);
	}
}

