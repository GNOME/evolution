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

#include "vfolder-context.h"
#include "vfolder-rule.h"

static void vfolder_context_class_init (VfolderContextClass *klass);
static void vfolder_context_init (VfolderContext *vc);
static void vfolder_context_finalise (GObject *obj);


static RuleContextClass *parent_class = NULL;


GType
vfolder_context_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (VfolderContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) vfolder_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (VfolderContext),
			0,    /* n_preallocs */
			(GInstanceInitFunc) vfolder_context_init,
		};
		
		type = g_type_register_static (RULE_TYPE_CONTEXT, "VfolderContext", &info, 0);
	}
	
	return type;
}

static void
vfolder_context_class_init (VfolderContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (RULE_TYPE_CONTEXT);
	
	object_class->finalize = vfolder_context_finalise;
}

static void
vfolder_context_init (VfolderContext *vc)
{
	rule_context_add_part_set ((RuleContext *) vc, "partset", filter_part_get_type (),
				   rule_context_add_part, rule_context_next_part);
	
	rule_context_add_rule_set ((RuleContext *) vc, "ruleset", vfolder_rule_get_type (),
				   rule_context_add_rule, rule_context_next_rule);

	((RuleContext *)vc)->flags = RULE_CONTEXT_THREADING | RULE_CONTEXT_GROUPING;
}

static void
vfolder_context_finalise (GObject *obj)
{
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * vfolder_context_new:
 *
 * Create a new VfolderContext object.
 * 
 * Return value: A new #VfolderContext object.
 **/
VfolderContext *
vfolder_context_new (void)
{
	return (VfolderContext *) g_object_new (VFOLDER_TYPE_CONTEXT, NULL, NULL);
}
