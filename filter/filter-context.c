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

#include "filter-context.h"
#include "filter-filter.h"

/* For poking into filter-folder guts */
#include "filter-folder.h"

#define d(x) 

static void filter_context_class_init (FilterContextClass *klass);
static void filter_context_init (FilterContext *fc);
static void filter_context_finalise (GObject *obj);

static GList *filter_rename_uri (RuleContext *rc, const char *olduri, const char *newuri, GCompareFunc cmp);
static GList *filter_delete_uri (RuleContext *rc, const char *uri, GCompareFunc cmp);


static RuleContextClass *parent_class = NULL;


GType
filter_context_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterContext),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_context_init,
		};
		
		type = g_type_register_static (RULE_TYPE_CONTEXT, "FilterContext", &info, 0);
	}
	
	return type;
}

static void
filter_context_class_init (FilterContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	RuleContextClass *rc_class = RULE_CONTEXT_CLASS (klass);
	
	parent_class = g_type_class_ref (RULE_TYPE_CONTEXT);
	
	object_class->finalize = filter_context_finalise;
	
	/* override methods */
	rc_class->rename_uri = filter_rename_uri;
	rc_class->delete_uri = filter_delete_uri;
}

static void
filter_context_init (FilterContext *fc)
{
	fc->priv = g_malloc0 (sizeof (*rc->priv));
	
	rule_context_add_part_set ((RuleContext *) fc, "partset", filter_part_get_type (),
				   rule_context_add_part, rule_context_next_part);
	rule_context_add_part_set ((RuleContext *) fc, "actionset", filter_part_get_type (),
				   (RCPartFunc) filter_context_add_action,
				   (RCNextPartFunc) filter_context_next_action);
	
	rule_context_add_rule_set ((RuleContext *) fc, "ruleset", filter_filter_get_type (),
				   (RCRuleFunc) rule_context_add_rule, rule_context_next_rule);
}

static void
filter_context_finalise (GObject *obj)
{
	FilterContext *fc = (FilterContext *)obj;
	
	g_list_foreach (fc->actions, (GFunc)g_object_unref, NULL);
	g_list_free (fc->actions);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_context_new:
 *
 * Create a new FilterContext object.
 * 
 * Return value: A new #FilterContext object.
 **/
FilterContext *
filter_context_new (void)
{
	return (FilterContext *) g_object_new (FILTER_TYPE_CONTEXT, NULL, NULL);
}

void
filter_context_add_action (FilterContext *fc, FilterPart *action)
{
	d(printf("find action : "));
	fc->actions = g_list_append (fc->actions, action);
}

FilterPart *
filter_context_find_action (FilterContext *fc, const char *name)
{
	d(printf("find action : "));
	return filter_part_find_list (fc->actions, name);
}

FilterPart *
filter_context_create_action (FilterContext *fc, const char *name)
{
	FilterPart *part;
	
	if ((part = filter_context_find_action (fc, name)))
		return filter_part_clone (part);
	
	return NULL;
}

FilterPart *
filter_context_next_action (FilterContext *fc, FilterPart *last)
{
	return filter_part_next_list (fc->actions, last);
}

/* We search for any folders in our actions list that need updating, update them */
static GList *
filter_rename_uri (RuleContext *rc, const char *olduri, const char *newuri, GCompareFunc cmp)
{
	FilterRule *rule;
	GList *l, *el;
	FilterPart *action;
	FilterElement *element;
	int count = 0;
	GList *changed = NULL;
	
	d(printf("uri '%s' renamed to '%s'\n", olduri, newuri));
	
	/* For all rules, for all actions, for all elements, rename any folder elements */
	/* Yes we could do this inside each part itself, but not today */
	rule = NULL;
	while ((rule = rule_context_next_rule (rc, rule, NULL))) {
		int rulecount = 0;
		
		d(printf("checking rule '%s'\n", rule->name));
		
		l = FILTER_FILTER (rule)->actions;
		while (l) {
			action = l->data;
			
			d(printf("checking action '%s'\n", action->name));
			
			el = action->elements;
			while (el) {
				element = el->data;
				
				d(printf("checking element '%s'\n", element->name));
				if (IS_FILTER_FOLDER (element)) {
					d(printf(" is folder, existing uri = '%s'\n",
						 FILTER_FOLDER (element)->uri));
				}
				
				if (IS_FILTER_FOLDER (element)
				    && cmp(((FilterFolder *)element)->uri, olduri)) {
					d(printf(" Changed!\n"));
					filter_folder_set_value((FilterFolder *)element, newuri);
					rulecount++;
				}
				el = el->next;
			}
			l = l->next;
		}
		
		if (rulecount) {
			changed = g_list_append (changed, g_strdup (rule->name));
			filter_rule_emit_changed (rule);
		}
		
		count += rulecount;
	}
	
	/* might need to call parent class, if it did anything ... parent_class->rename_uri(f, olduri, newuri, cmp); */
	
	return changed;
}

static GList *
filter_delete_uri (RuleContext *rc, const char *uri, GCompareFunc cmp)
{
	/* We basically do similar to above, but when we find it,
	   Remove the action, and if thats the last action, this might create an empty rule?  remove the rule? */
	
	FilterRule *rule;
	GList *l, *el;
	FilterPart *action;
	FilterElement *element;
	int count = 0;
	GList *deleted = NULL;
	
	d(printf("uri '%s' deleted\n", uri));
	
	/* For all rules, for all actions, for all elements, check deleted folder elements */
	/* Yes we could do this inside each part itself, but not today */
	rule = NULL;
	while ((rule = rule_context_next_rule (rc, rule, NULL))) {
		int recorded = 0;
		
		d(printf("checking rule '%s'\n", rule->name));
		
		l = FILTER_FILTER (rule)->actions;
		while (l) {
			action = l->data;
			
			d(printf("checking action '%s'\n", action->name));
			
			el = action->elements;
			while (el) {
				element = el->data;
				
				d(printf("checking element '%s'\n", element->name));
				if (IS_FILTER_FOLDER (element)) {
					d(printf(" is folder, existing uri = '%s'\n",
						 FILTER_FOLDER (element)->uri));
				}
				
				if (IS_FILTER_FOLDER (element)
				    && cmp(((FilterFolder *)element)->uri, uri)) {
					d(printf(" Deleted!\n"));
					/* check if last action, if so, remove rule instead? */
					l = l->next;
					filter_filter_remove_action ((FilterFilter *)rule, action);
					g_object_unref (action);
					count++;
					if (!recorded)
						deleted = g_list_append (deleted, g_strdup (rule->name));
					goto next_action;
				}
				el = el->next;
			}
			l = l->next;
		next_action:
			;
		}
	}
	
	/* TODO: could call parent and merge lists */
	
	return deleted;
}
