/*
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

#include "em-filter-context.h"
#include "em-filter-rule.h"
#include "filter/e-filter-option.h"
#include "filter/e-filter-int.h"
#include "em-filter-source-element.h"

/* For poking into filter-folder guts */
#include "em-filter-folder-element.h"

#define d(x)

static void em_filter_context_class_init(EMFilterContextClass *klass);
static void em_filter_context_init(EMFilterContext *fc);
static void em_filter_context_finalise(GObject *obj);

static GList *filter_rename_uri(ERuleContext *rc, const gchar *olduri, const gchar *newuri, GCompareFunc cmp);
static GList *filter_delete_uri(ERuleContext *rc, const gchar *uri, GCompareFunc cmp);
static EFilterElement *filter_new_element(ERuleContext *rc, const gchar *name);

static ERuleContextClass *parent_class = NULL;

GType
em_filter_context_get_type(void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMFilterContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_filter_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(EMFilterContext),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_filter_context_init,
		};

		type = g_type_register_static(E_TYPE_RULE_CONTEXT, "EMFilterContext", &info, 0);
	}

	return type;
}

static void
em_filter_context_class_init(EMFilterContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	ERuleContextClass *rc_class = E_RULE_CONTEXT_CLASS(klass);

	parent_class = g_type_class_ref(E_TYPE_RULE_CONTEXT);

	object_class->finalize = em_filter_context_finalise;

	/* override methods */
	rc_class->rename_uri = filter_rename_uri;
	rc_class->delete_uri = filter_delete_uri;
	rc_class->new_element = filter_new_element;
}

static void
em_filter_context_init(EMFilterContext *fc)
{
	e_rule_context_add_part_set((ERuleContext *) fc, "partset", e_filter_part_get_type(),
				   e_rule_context_add_part, e_rule_context_next_part);
	e_rule_context_add_part_set((ERuleContext *) fc, "actionset", e_filter_part_get_type(),
				  (ERuleContextPartFunc) em_filter_context_add_action,
				  (ERuleContextNextPartFunc) em_filter_context_next_action);

	e_rule_context_add_rule_set((ERuleContext *) fc, "ruleset", em_filter_rule_get_type(),
				  (ERuleContextRuleFunc) e_rule_context_add_rule, e_rule_context_next_rule);
}

static void
em_filter_context_finalise(GObject *obj)
{
	EMFilterContext *fc = (EMFilterContext *)obj;

	g_list_foreach(fc->actions, (GFunc)g_object_unref, NULL);
	g_list_free(fc->actions);

        G_OBJECT_CLASS(parent_class)->finalize(obj);
}

/**
 * em_filter_context_new:
 *
 * Create a new EMFilterContext object.
 *
 * Return value: A new #EMFilterContext object.
 **/
EMFilterContext *
em_filter_context_new(void)
{
	return (EMFilterContext *) g_object_new(em_filter_context_get_type(), NULL, NULL);
}

void
em_filter_context_add_action(EMFilterContext *fc, EFilterPart *action)
{
	d(printf("find action : "));
	fc->actions = g_list_append(fc->actions, action);
}

EFilterPart *
em_filter_context_find_action(EMFilterContext *fc, const gchar *name)
{
	d(printf("find action : "));
	return e_filter_part_find_list(fc->actions, name);
}

EFilterPart *
em_filter_context_create_action(EMFilterContext *fc, const gchar *name)
{
	EFilterPart *part;

	if ((part = em_filter_context_find_action(fc, name)))
		return e_filter_part_clone(part);

	return NULL;
}

EFilterPart *
em_filter_context_next_action(EMFilterContext *fc, EFilterPart *last)
{
	return e_filter_part_next_list(fc->actions, last);
}

/* We search for any folders in our actions list that need updating, update them */
static GList *
filter_rename_uri(ERuleContext *rc, const gchar *olduri, const gchar *newuri, GCompareFunc cmp)
{
	EFilterRule *rule;
	GList *l, *el;
	EFilterPart *action;
	EFilterElement *element;
	gint count = 0;
	GList *changed = NULL;

	d(printf("uri '%s' renamed to '%s'\n", olduri, newuri));

	/* For all rules, for all actions, for all elements, rename any folder elements */
	/* Yes we could do this inside each part itself, but not today */
	rule = NULL;
	while ((rule = e_rule_context_next_rule(rc, rule, NULL))) {
		gint rulecount = 0;

		d(printf("checking rule '%s'\n", rule->name));

		l = EM_FILTER_RULE(rule)->actions;
		while (l) {
			action = l->data;

			d(printf("checking action '%s'\n", action->name));

			el = action->elements;
			while (el) {
				element = el->data;

				d(printf("checking element '%s'\n", element->name));
				if (EM_IS_FILTER_FOLDER_ELEMENT(element)) {
					d(printf(" is folder, existing uri = '%s'\n",
						 FILTER_FOLDER(element)->uri));
				}

				if (EM_IS_FILTER_FOLDER_ELEMENT(element)
				    && cmp(((EMFilterFolderElement *)element)->uri, olduri)) {
					d(printf(" Changed!\n"));
					em_filter_folder_element_set_value((EMFilterFolderElement *)element, newuri);
					rulecount++;
				}
				el = el->next;
			}
			l = l->next;
		}

		if (rulecount) {
			changed = g_list_append(changed, g_strdup(rule->name));
			e_filter_rule_emit_changed(rule);
		}

		count += rulecount;
	}

	/* might need to call parent class, if it did anything ... parent_class->rename_uri(f, olduri, newuri, cmp); */

	return changed;
}

static GList *
filter_delete_uri(ERuleContext *rc, const gchar *uri, GCompareFunc cmp)
{
	/* We basically do similar to above, but when we find it,
	   Remove the action, and if thats the last action, this might create an empty rule?  remove the rule? */

	EFilterRule *rule;
	GList *l, *el;
	EFilterPart *action;
	EFilterElement *element;
	gint count = 0;
	GList *deleted = NULL;

	d(printf("uri '%s' deleted\n", uri));

	/* For all rules, for all actions, for all elements, check deleted folder elements */
	/* Yes we could do this inside each part itself, but not today */
	rule = NULL;
	while ((rule = e_rule_context_next_rule(rc, rule, NULL))) {
		gint recorded = 0;

		d(printf("checking rule '%s'\n", rule->name));

		l = EM_FILTER_RULE(rule)->actions;
		while (l) {
			action = l->data;

			d(printf("checking action '%s'\n", action->name));

			el = action->elements;
			while (el) {
				element = el->data;

				d(printf("checking element '%s'\n", element->name));
				if (EM_IS_FILTER_FOLDER_ELEMENT(element)) {
					d(printf(" is folder, existing uri = '%s'\n",
						 FILTER_FOLDER(element)->uri));
				}

				if (EM_IS_FILTER_FOLDER_ELEMENT(element)
				    && cmp(((EMFilterFolderElement *)element)->uri, uri)) {
					d(printf(" Deleted!\n"));
					/* check if last action, if so, remove rule instead? */
					l = l->next;
					em_filter_rule_remove_action((EMFilterRule *)rule, action);
					g_object_unref(action);
					count++;
					if (!recorded)
						deleted = g_list_append(deleted, g_strdup(rule->name));
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

static EFilterElement *
filter_new_element(ERuleContext *rc, const gchar *type)
{
	if (!strcmp(type, "folder")) {
		return (EFilterElement *) em_filter_folder_element_new();
	} else if (!strcmp(type, "system-flag")) {
		return (EFilterElement *) e_filter_option_new();
	} else if (!strcmp(type, "score")) {
		return (EFilterElement *) e_filter_int_new_type("score", -3, 3);
	} else if (!strcmp(type, "source")) {
		return (EFilterElement *) em_filter_source_element_new();
	} else {
		return parent_class->new_element(rc, type);
	}
}
