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

#include "score-context.h"
#include "score-rule.h"

static void score_context_class_init	(ScoreContextClass *class);
static void score_context_init	(ScoreContext *gspaper);
static void score_context_finalise	(GtkObject *obj);

static RuleContextClass *parent_class;

guint
score_context_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"ScoreContext",
			sizeof(ScoreContext),
			sizeof(ScoreContextClass),
			(GtkClassInitFunc)score_context_class_init,
			(GtkObjectInitFunc)score_context_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(rule_context_get_type (), &type_info);
	}
	
	return type;
}

static void
score_context_class_init (ScoreContextClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(rule_context_get_type ());

	object_class->finalize = score_context_finalise;
	/* override methods */

}

static void
score_context_init (ScoreContext *o)
{
	rule_context_add_part_set((RuleContext *)o, "partset", filter_part_get_type(),
				  rule_context_add_part, rule_context_next_part);
	
	rule_context_add_rule_set((RuleContext *)o, "ruleset", score_rule_get_type(),
				  rule_context_add_rule, rule_context_next_rule);
}

static void
score_context_finalise(GtkObject *obj)
{
	ScoreContext *o = (ScoreContext *)obj;

	o = o;

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * score_context_new:
 *
 * Create a new ScoreContext object.
 * 
 * Return value: A new #ScoreContext object.
 **/
ScoreContext *
score_context_new(void)
{
	ScoreContext *o = (ScoreContext *)gtk_type_new(score_context_get_type ());
	return o;
}
