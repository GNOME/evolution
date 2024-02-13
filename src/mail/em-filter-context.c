/*
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

#include <string.h>

#include "em-filter-context.h"
#include "em-filter-rule.h"
#include "em-filter-editor-folder-element.h"
#include "em-filter-mail-identity-element.h"
#include "em-filter-source-element.h"

struct _EMFilterContextPrivate {
	EMailSession *session;
	GList *actions;
};

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE_WITH_PRIVATE (EMFilterContext, em_filter_context, E_TYPE_RULE_CONTEXT)

static void
filter_context_set_session (EMFilterContext *context,
                            EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (context->priv->session == NULL);

	context->priv->session = g_object_ref (session);
}

static void
filter_context_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			filter_context_set_session (
				EM_FILTER_CONTEXT (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
filter_context_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				em_filter_context_get_session (
				EM_FILTER_CONTEXT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
filter_context_dispose (GObject *object)
{
	EMFilterContext *self = EM_FILTER_CONTEXT (object);

	g_clear_object (&self->priv->session);

	g_list_free_full (self->priv->actions, g_object_unref);
	self->priv->actions = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_filter_context_parent_class)->dispose (object);
}

/* We search for any folders in our actions list that need updating
 * and update them. */
static GList *
filter_context_rename_uri (ERuleContext *context,
                           const gchar *olduri,
                           const gchar *newuri,
                           GCompareFunc cmp)
{
	EFilterRule *rule;
	GList *l, *el;
	EFilterPart *action;
	EFilterElement *element;
	GList *changed = NULL;

	/* For all rules, for all actions, for all elements, rename any
	 * folder elements.  XXX Yes we could do this inside each part
	 * itself, but not today. */
	rule = NULL;
	while ((rule = e_rule_context_next_rule (context, rule, NULL))) {
		gint rulecount = 0;

		l = em_filter_rule_get_actions (EM_FILTER_RULE (rule));
		while (l) {
			action = l->data;

			el = action->elements;
			while (el) {
				element = el->data;

				if (EM_IS_FILTER_FOLDER_ELEMENT (element)
				    && cmp (em_filter_folder_element_get_uri (
				    EM_FILTER_FOLDER_ELEMENT (element)), olduri)) {
					em_filter_folder_element_set_uri (
						EM_FILTER_FOLDER_ELEMENT (
						element), newuri);
					rulecount++;
				}
				el = el->next;
			}
			l = l->next;
		}

		if (rulecount) {
			changed = g_list_append (changed, g_strdup (rule->name));
			e_filter_rule_emit_changed (rule);
		}
	}

	return changed;
}

static GList *
filter_context_delete_uri (ERuleContext *context,
                           const gchar *uri,
                           GCompareFunc cmp)
{
	/* We basically do similar to above, but when we find it,
	 * remove the action, and if thats the last action, this
	 * might create an empty rule?  Remove the rule? */

	EFilterRule *rule;
	GList *l, *el;
	EFilterPart *action;
	EFilterElement *element;
	GList *deleted = NULL;

	/* For all rules, for all actions, for all elements, check
	 * deleted folder elements. XXX  Yes we could do this inside
	 * each part itself, but not today. */
	rule = NULL;
	while ((rule = e_rule_context_next_rule (context, rule, NULL))) {
		gint recorded = 0;

		l = em_filter_rule_get_actions (EM_FILTER_RULE (rule));
		while (l) {
			action = l->data;

			el = action->elements;
			while (el) {
				element = el->data;

				if (EM_IS_FILTER_FOLDER_ELEMENT (element)
				    && cmp (em_filter_folder_element_get_uri (
				    EM_FILTER_FOLDER_ELEMENT (element)), uri)) {
					/* check if last action, if so, remove rule instead? */
					l = l->next;
					em_filter_rule_remove_action ((EMFilterRule *) rule, action);
					g_object_unref (action);
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

	return deleted;
}

static EFilterElement *
filter_context_new_element (ERuleContext *context,
                            const gchar *type)
{
	EMFilterContext *self = EM_FILTER_CONTEXT (context);

	if (strcmp (type, "folder") == 0)
		return em_filter_editor_folder_element_new (self->priv->session);

	if (strcmp (type, "system-flag") == 0)
		return e_filter_option_new ();

	if (strcmp (type, "score") == 0)
		return e_filter_int_new_type ("score", -3, 3);

	if (strcmp (type, "source") == 0)
		return em_filter_source_element_new (self->priv->session);

	if (strcmp (type, "mail-identity") == 0)
		return em_filter_mail_identity_element_new (e_mail_session_get_registry (self->priv->session));

	return E_RULE_CONTEXT_CLASS (em_filter_context_parent_class)->new_element (context, type);
}

static void
em_filter_context_class_init (EMFilterContextClass *class)
{
	GObjectClass *object_class;
	ERuleContextClass *rule_context_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = filter_context_set_property;
	object_class->get_property = filter_context_get_property;
	object_class->dispose = filter_context_dispose;

	rule_context_class = E_RULE_CONTEXT_CLASS (class);
	rule_context_class->rename_uri = filter_context_rename_uri;
	rule_context_class->delete_uri = filter_context_delete_uri;
	rule_context_class->new_element = filter_context_new_element;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
em_filter_context_init (EMFilterContext *context)
{
	context->priv = em_filter_context_get_instance_private (context);

	e_rule_context_add_part_set (
		E_RULE_CONTEXT (context),
		"partset", E_TYPE_FILTER_PART,
		(ERuleContextPartFunc) e_rule_context_add_part,
		(ERuleContextNextPartFunc) e_rule_context_next_part);

	e_rule_context_add_part_set (
		E_RULE_CONTEXT (context),
		"actionset", E_TYPE_FILTER_PART,
		(ERuleContextPartFunc) em_filter_context_add_action,
		(ERuleContextNextPartFunc) em_filter_context_next_action);

	e_rule_context_add_rule_set (
		E_RULE_CONTEXT (context),
		"ruleset", EM_TYPE_FILTER_RULE,
		(ERuleContextRuleFunc) e_rule_context_add_rule,
		(ERuleContextNextRuleFunc) e_rule_context_next_rule);
}

EMFilterContext *
em_filter_context_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_FILTER_CONTEXT, "session", session, NULL);
}

EMailSession *
em_filter_context_get_session (EMFilterContext *context)
{
	g_return_val_if_fail (EM_IS_FILTER_CONTEXT (context), NULL);

	return context->priv->session;
}

void
em_filter_context_add_action (EMFilterContext *context,
                              EFilterPart *action)
{
	context->priv->actions =
		g_list_append (context->priv->actions, action);
}

EFilterPart *
em_filter_context_find_action (EMFilterContext *context,
                               const gchar *name)
{
	return e_filter_part_find_list (context->priv->actions, name);
}

EFilterPart *
em_filter_context_create_action (EMFilterContext *context,
                                 const gchar *name)
{
	EFilterPart *part;

	if ((part = em_filter_context_find_action (context, name)))
		return e_filter_part_clone (part);

	return NULL;
}

EFilterPart *
em_filter_context_next_action (EMFilterContext *context,
                               EFilterPart *last)
{
	return e_filter_part_next_list (context->priv->actions, last);
}

