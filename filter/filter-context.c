/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
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

#include <gtk/gtktypeutils.h>
#include <gtk/gtkobject.h>

#include "filter-context.h"
#include "filter-filter.h"

/* For poking into filter-folder guts */
#include "filter-folder.h"

#define d(x) 

static void filter_context_class_init	(FilterContextClass *class);
static void filter_context_init	(FilterContext *gspaper);
static void filter_context_finalise	(GtkObject *obj);

static GList *filter_rename_uri(RuleContext *f, const char *olduri, const char *newuri, GCompareFunc cmp);
static GList *filter_delete_uri(RuleContext *f, const char *uri, GCompareFunc cmp);

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
	RuleContextClass *rule_class = (RuleContextClass *)class;

	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(rule_context_get_type ());

	object_class->finalize = filter_context_finalise;

	/* override methods */
	rule_class->rename_uri = filter_rename_uri;
	rule_class->delete_uri = filter_delete_uri;

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

	g_list_foreach(o->actions, (GFunc)gtk_object_unref, NULL);
	g_list_free(o->actions);

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

FilterPart *filter_context_find_action(FilterContext *f, const char *name)
{
	d(printf("find action : "));
	return filter_part_find_list(f->actions, name);
}

FilterPart 	*filter_context_create_action(FilterContext *f, const char *name)
{
	FilterPart *part;

	part = filter_context_find_action(f, name);
	if (part)
		part = filter_part_clone(part);
	return part;
}

FilterPart 	*filter_context_next_action(FilterContext *f, FilterPart *last)
{
	return filter_part_next_list(f->actions, last);
}

/* We search for any folders in our actions list that need updating, update them */
static GList *filter_rename_uri(RuleContext *f, const char *olduri, const char *newuri, GCompareFunc cmp)
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
	while ( (rule = rule_context_next_rule(f, rule, NULL)) ) {
		int rulecount = 0;

		d(printf("checking rule '%s'\n", rule->name));
		
		l = FILTER_FILTER(rule)->actions;
		while (l) {
			action = l->data;

			d(printf("checking action '%s'\n", action->name));
			
			el = action->elements;
			while (el) {
				element = el->data;

				d(printf("checking element '%s'\n", element->name));
				if (IS_FILTER_FOLDER(element))
					d(printf(" is folder, existing uri = '%s'\n", FILTER_FOLDER(element)->uri));

				if (IS_FILTER_FOLDER(element)
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
			changed = g_list_append(changed, g_strdup(rule->name));
			filter_rule_emit_changed(rule);
		}

		count += rulecount;
	}

	/* might need to call parent class, if it did anything ... parent_class->rename_uri(f, olduri, newuri, cmp); */

	return changed;
}

static GList *filter_delete_uri(RuleContext *f, const char *uri, GCompareFunc cmp)
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
	while ( (rule = rule_context_next_rule(f, rule, NULL)) ) {
		int recorded = 0;

		d(printf("checking rule '%s'\n", rule->name));
		
		l = FILTER_FILTER(rule)->actions;
		while (l) {
			action = l->data;

			d(printf("checking action '%s'\n", action->name));
			
			el = action->elements;
			while (el) {
				element = el->data;

				d(printf("checking element '%s'\n", element->name));
				if (IS_FILTER_FOLDER(element))
					d(printf(" is folder, existing uri = '%s'\n", FILTER_FOLDER(element)->uri));

				if (IS_FILTER_FOLDER(element)
				    && cmp(((FilterFolder *)element)->uri, uri)) {
					d(printf(" Deleted!\n"));
					/* check if last action, if so, remove rule instead? */
					l = l->next;
					filter_filter_remove_action((FilterFilter *)rule, action);
					gtk_object_unref((GtkObject *)action);
					count++;
					if (!recorded)
						deleted = g_list_append(deleted, g_strdup(rule->name));
					goto next_action;
				}
				el = el->next;
			}
			l = l->next;
		next_action:
		}
	}

	/* TODO: could call parent and merge lists */

	return deleted;
}

