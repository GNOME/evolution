/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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

#include <config.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <glib/gi18n.h>

#include <libedataserver/e-xml-utils.h>

#include "e-util/e-error.h"
#include "e-util/e-xml-utils.h"

#include "filter-code.h"
#include "filter-colour.h"
#include "filter-datespec.h"
#include "filter-file.h"
#include "filter-input.h"
#include "filter-int.h"
#include "filter-option.h"
#include "filter-rule.h"
#include "rule-context.h"

#define d(x)

static gint load(RuleContext *rc, const gchar *system, const gchar *user);
static gint save(RuleContext *rc, const gchar *user);
static gint revert(RuleContext *rc, const gchar *user);
static GList *rename_uri(RuleContext *rc, const gchar *olduri, const gchar *newuri, GCompareFunc cmp);
static GList *delete_uri(RuleContext *rc, const gchar *uri, GCompareFunc cmp);
static FilterElement *new_element(RuleContext *rc, const gchar *name);

static void rule_context_class_init(RuleContextClass *klass);
static void rule_context_init(RuleContext *rc);
static void rule_context_finalise(GObject *obj);

#define _PRIVATE(x)(((RuleContext *)(x))->priv)

struct _RuleContextPrivate {
	gint frozen;
};

static GObjectClass *parent_class = NULL;

enum {
	RULE_ADDED,
	RULE_REMOVED,
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

GType
rule_context_get_type(void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(RuleContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) rule_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(RuleContext),
			0,    /* n_preallocs */
			(GInstanceInitFunc) rule_context_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "RuleContext", &info, 0);
	}

	return type;
}

static void
rule_context_class_init(RuleContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref(G_TYPE_OBJECT);

	object_class->finalize = rule_context_finalise;

	/* override methods */
	klass->load = load;
	klass->save = save;
	klass->revert = revert;
	klass->rename_uri = rename_uri;
	klass->delete_uri = delete_uri;
	klass->new_element = new_element;

	/* signals */
	signals[RULE_ADDED] =
		g_signal_new("rule_added",
			      RULE_TYPE_CONTEXT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(RuleContextClass, rule_added),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[RULE_REMOVED] =
		g_signal_new("rule_removed",
			      RULE_TYPE_CONTEXT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(RuleContextClass, rule_removed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[CHANGED] =
		g_signal_new("changed",
			      RULE_TYPE_CONTEXT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(RuleContextClass, changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
rule_context_init(RuleContext *rc)
{
	rc->priv = g_malloc0(sizeof(*rc->priv));

	rc->part_set_map = g_hash_table_new(g_str_hash, g_str_equal);
	rc->rule_set_map = g_hash_table_new(g_str_hash, g_str_equal);

	rc->flags = RULE_CONTEXT_GROUPING;
}

static void
free_part_set(struct _part_set_map *map, gpointer data)
{
	g_free(map->name);
	g_free(map);
}

static void
free_rule_set(struct _rule_set_map *map, gpointer data)
{
	g_free(map->name);
	g_free(map);
}

static void
rule_context_finalise(GObject *obj)
{
	RuleContext *rc =(RuleContext *) obj;

	g_list_foreach(rc->rule_set_list, (GFunc)free_rule_set, NULL);
	g_list_free(rc->rule_set_list);
	g_hash_table_destroy(rc->rule_set_map);

	g_list_foreach(rc->part_set_list, (GFunc)free_part_set, NULL);
	g_list_free(rc->part_set_list);
	g_hash_table_destroy(rc->part_set_map);

	g_free(rc->error);

	g_list_foreach(rc->parts, (GFunc)g_object_unref, NULL);
	g_list_free(rc->parts);
	g_list_foreach(rc->rules, (GFunc)g_object_unref, NULL);
	g_list_free(rc->rules);

	g_free(rc->priv);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

/**
 * rule_context_new:
 *
 * Create a new RuleContext object.
 *
 * Return value: A new #RuleContext object.
 **/
RuleContext *
rule_context_new(void)
{
	return(RuleContext *) g_object_new(RULE_TYPE_CONTEXT, NULL, NULL);
}

void
rule_context_add_part_set(RuleContext *rc, const gchar *setname, GType part_type, RCPartFunc append, RCNextPartFunc next)
{
	struct _part_set_map *map;

	g_return_if_fail (g_hash_table_lookup(rc->part_set_map, setname) == NULL);

	map = g_malloc0(sizeof(*map));
	map->type = part_type;
	map->append = append;
	map->next = next;
	map->name = g_strdup(setname);
	g_hash_table_insert(rc->part_set_map, map->name, map);
	rc->part_set_list = g_list_append(rc->part_set_list, map);
	d(printf("adding part set '%s'\n", setname));
}

void
rule_context_add_rule_set(RuleContext *rc, const gchar *setname, GType rule_type, RCRuleFunc append, RCNextRuleFunc next)
{
	struct _rule_set_map *map;

	g_return_if_fail (g_hash_table_lookup(rc->rule_set_map, setname) == NULL);

	map = g_malloc0(sizeof(*map));
	map->type = rule_type;
	map->append = append;
	map->next = next;
	map->name = g_strdup(setname);
	g_hash_table_insert(rc->rule_set_map, map->name, map);
	rc->rule_set_list = g_list_append(rc->rule_set_list, map);
	d(printf("adding rule set '%s'\n", setname));
}

/**
 * rule_context_set_error:
 * @f:
 * @error:
 *
 * Set the text error for the context, or NULL to clear it.
 **/
static void
rule_context_set_error(RuleContext *rc, gchar *error)
{
	g_return_if_fail (rc);

	g_free(rc->error);
	rc->error = error;
}

/**
 * rule_context_load:
 * @f:
 * @system:
 * @user:
 *
 * Load a rule context from a system and user description file.
 *
 * Return value:
 **/
gint
rule_context_load(RuleContext *rc, const gchar *system, const gchar *user)
{
	gint res;

	g_return_val_if_fail (rc, -1);

	d(printf("rule_context: loading %s %s\n", system, user));

	rc->priv->frozen++;
	res = RULE_CONTEXT_GET_CLASS(rc)->load(rc, system, user);
	rc->priv->frozen--;

	return res;
}

static gint
load(RuleContext *rc, const gchar *system, const gchar *user)
{
	xmlNodePtr set, rule, root;
	xmlDocPtr systemdoc, userdoc;
	struct _part_set_map *part_map;
	struct _rule_set_map *rule_map;

	rule_context_set_error(rc, NULL);

	d(printf("loading rules %s %s\n", system, user));

	systemdoc = e_xml_parse_file (system);
	if (systemdoc == NULL) {
		gchar * err_msg = g_strdup_printf("Unable to load system rules '%s': %s",
						  system, g_strerror(errno));
		g_warning("%s: %s", G_STRFUNC, err_msg);
		rule_context_set_error(rc, err_msg);
		/* no need to free err_msg here */
		return -1;
	}

	root = xmlDocGetRootElement(systemdoc);
	if (root == NULL || strcmp((gchar *)root->name, "filterdescription")) {
		gchar * err_msg = g_strdup_printf(
			"Unable to load system rules '%s': Invalid format",
			system);
		g_warning("%s: %s", G_STRFUNC, err_msg);
		rule_context_set_error(rc, err_msg);
		/* no need to free err_msg here */
		xmlFreeDoc(systemdoc);
		return -1;
	}
	/* doesn't matter if this doens't exist */
	userdoc = NULL;
	if (g_file_test(user, G_FILE_TEST_IS_REGULAR))
		userdoc = e_xml_parse_file (user);

	/* now parse structure */
	/* get rule parts */
	set = root->children;
	while (set) {
		d(printf("set name = %s\n", set->name));
		part_map = g_hash_table_lookup(rc->part_set_map, set->name);
		if (part_map) {
			d(printf("loading parts ...\n"));
			rule = set->children;
			while (rule) {
				if (!strcmp((gchar *)rule->name, "part")) {
					FilterPart *part = FILTER_PART(g_object_new(part_map->type, NULL, NULL));

					if (filter_part_xml_create(part, rule, rc) == 0) {
						part_map->append(rc, part);
					} else {
						g_object_unref(part);
						g_warning("Cannot load filter part");
					}
				}
				rule = rule->next;
			}
		} else if ((rule_map = g_hash_table_lookup(rc->rule_set_map, set->name))) {
			d(printf("loading system rules ...\n"));
			rule = set->children;
			while (rule) {
				d(printf("checking node: %s\n", rule->name));
				if (!strcmp((gchar *)rule->name, "rule")) {
					FilterRule *part = FILTER_RULE(g_object_new(rule_map->type, NULL, NULL));

					if (filter_rule_xml_decode(part, rule, rc) == 0) {
						part->system = TRUE;
						rule_map->append(rc, part);
					} else {
						g_object_unref(part);
						g_warning("Cannot load filter part");
					}
				}
				rule = rule->next;
			}
		}
		set = set->next;
	}

	/* now load actual rules */
	if (userdoc) {
		root = xmlDocGetRootElement(userdoc);
		set = root?root->children:NULL;
		while (set) {
			d(printf("set name = %s\n", set->name));
			rule_map = g_hash_table_lookup(rc->rule_set_map, set->name);
			if (rule_map) {
				d(printf("loading rules ...\n"));
				rule = set->children;
				while (rule) {
					d(printf("checking node: %s\n", rule->name));
					if (!strcmp((gchar *)rule->name, "rule")) {
						FilterRule *part = FILTER_RULE(g_object_new(rule_map->type, NULL, NULL));

						if (filter_rule_xml_decode(part, rule, rc) == 0) {
							rule_map->append(rc, part);
						} else {
							g_object_unref(part);
							g_warning("Cannot load filter part");
						}
					}
					rule = rule->next;
				}
			}
			set = set->next;
		}
	}

	xmlFreeDoc(userdoc);
	xmlFreeDoc(systemdoc);

	return 0;
}

/**
 * rule_context_save:
 * @f:
 * @user:
 *
 * Save a rule context to disk.
 *
 * Return value:
 **/
gint
rule_context_save(RuleContext *rc, const gchar *user)
{
	g_return_val_if_fail (rc, -1);
	g_return_val_if_fail (user, -1);

	return RULE_CONTEXT_GET_CLASS(rc)->save(rc, user);
}

static gint
save(RuleContext *rc, const gchar *user)
{
	xmlDocPtr doc;
	xmlNodePtr root, rules, work;
	GList *l;
	FilterRule *rule;
	struct _rule_set_map *map;
	gint ret;

	doc = xmlNewDoc((const guchar *)"1.0");
	/* FIXME: set character encoding to UTF-8? */
	root = xmlNewDocNode(doc, NULL, (const guchar *)"filteroptions", NULL);
	xmlDocSetRootElement(doc, root);
	l = rc->rule_set_list;
	while (l) {
		map = l->data;
		rules = xmlNewDocNode(doc, NULL, (guchar *)map->name, NULL);
		xmlAddChild(root, rules);
		rule = NULL;
		while ((rule = map->next(rc, rule, NULL))) {
			if (!rule->system) {
				d(printf("processing rule %s\n", rule->name));
				work = filter_rule_xml_encode(rule);
				xmlAddChild(rules, work);
			}
		}
		l = g_list_next(l);
	}

	ret = e_xml_save_file(user, doc);

	xmlFreeDoc(doc);

	return ret;
}

/**
 * rule_context_revert:
 * @f:
 * @user:
 *
 * Reverts a rule context from a user description file.  Assumes the
 * system description file is unchanged from when it was loaded.
 *
 * Return value:
 **/
gint
rule_context_revert(RuleContext *rc, const gchar *user)
{
	g_return_val_if_fail (rc, 0);

	d(printf("rule_context: restoring %s\n", user));

	return RULE_CONTEXT_GET_CLASS(rc)->revert(rc, user);
}

struct _revert_data {
	GHashTable *rules;
	gint rank;
};

static void
revert_rule_remove(gpointer key, FilterRule *frule, RuleContext *rc)
{
	rule_context_remove_rule(rc, frule);
	g_object_unref(frule);
}

static void
revert_source_remove(gpointer key, struct _revert_data *rest_data, RuleContext *rc)
{
	g_hash_table_foreach(rest_data->rules, (GHFunc)revert_rule_remove, rc);
	g_hash_table_destroy(rest_data->rules);
	g_free(rest_data);
}

static guint
source_hashf(const gchar *a)
{
	if (a)
		return g_str_hash(a);
	return 0;
}

static gint
source_eqf(const gchar *a, const gchar *b)
{
	return((a && b && strcmp(a, b) == 0))
		|| (a == NULL && b == NULL);
}

static gint
revert(RuleContext *rc, const gchar *user)
{
	xmlNodePtr set, rule;
	/*struct _part_set_map *part_map;*/
	struct _rule_set_map *rule_map;
	struct _revert_data *rest_data;
	GHashTable *source_hash;
	xmlDocPtr userdoc;
	FilterRule *frule;

	rule_context_set_error(rc, NULL);

	d(printf("restoring rules %s\n", user));

	userdoc = e_xml_parse_file (user);
	if (userdoc == NULL)
		/* clear out anythign we have? */
		return 0;

	source_hash = g_hash_table_new((GHashFunc)source_hashf, (GCompareFunc)source_eqf);

	/* setup stuff we have now */
	/* Note that we assume there is only 1 set of rules in a given rule context,
	   although other parts of the code dont assume this */
	frule = NULL;
	while ((frule = rule_context_next_rule(rc, frule, NULL))) {
		rest_data = g_hash_table_lookup(source_hash, frule->source);
		if (rest_data == NULL) {
			rest_data = g_malloc0(sizeof(*rest_data));
			rest_data->rules = g_hash_table_new(g_str_hash, g_str_equal);
			g_hash_table_insert(source_hash, frule->source, rest_data);
		}
		g_hash_table_insert(rest_data->rules, frule->name, frule);
	}

	/* make what we have, match what we load */
	set = xmlDocGetRootElement(userdoc);
	set = set?set->children:NULL;
	while (set) {
		d(printf("set name = %s\n", set->name));
		rule_map = g_hash_table_lookup(rc->rule_set_map, set->name);
		if (rule_map) {
			d(printf("loading rules ...\n"));
			rule = set->children;
			while (rule) {
				d(printf("checking node: %s\n", rule->name));
				if (!strcmp((gchar *)rule->name, "rule")) {
					FilterRule *part = FILTER_RULE(g_object_new(rule_map->type, NULL, NULL));

					if (filter_rule_xml_decode(part, rule, rc) == 0) {
						/* use the revert data to keep track of the right rank of this rule part */
						rest_data = g_hash_table_lookup(source_hash, part->source);
						if (rest_data == NULL) {
							rest_data = g_malloc0(sizeof(*rest_data));
							rest_data->rules = g_hash_table_new(g_str_hash, g_str_equal);
							g_hash_table_insert(source_hash, part->source, rest_data);
						}
						frule = g_hash_table_lookup(rest_data->rules, part->name);
						if (frule) {
							if (rc->priv->frozen == 0 && !filter_rule_eq(frule, part))
								filter_rule_copy(frule, part);

							g_object_unref(part);
							rule_context_rank_rule(rc, frule, frule->source, rest_data->rank);
							g_hash_table_remove(rest_data->rules, frule->name);
						} else {
							rule_context_add_rule(rc, part);
							rule_context_rank_rule(rc, part, part->source, rest_data->rank);
						}
						rest_data->rank++;
					} else {
						g_object_unref(part);
						g_warning("Cannot load filter part");
					}
				}
				rule = rule->next;
			}
		}
		set = set->next;
	}

	xmlFreeDoc(userdoc);

	/* remove any we still have that weren't in the file */
	g_hash_table_foreach(source_hash, (GHFunc)revert_source_remove, rc);
	g_hash_table_destroy(source_hash);

	return 0;
}

FilterPart *
rule_context_find_part(RuleContext *rc, const gchar *name)
{
	g_return_val_if_fail (rc, NULL);
	g_return_val_if_fail (name, NULL);

	d(printf("find part : "));
	return filter_part_find_list(rc->parts, name);
}

FilterPart *
rule_context_create_part(RuleContext *rc, const gchar *name)
{
	FilterPart *part;

	g_return_val_if_fail (rc, NULL);
	g_return_val_if_fail (name, NULL);

	if ((part = rule_context_find_part(rc, name)))
		return filter_part_clone(part);

	return NULL;
}

FilterPart *
rule_context_next_part(RuleContext *rc, FilterPart *last)
{
	g_return_val_if_fail (rc, NULL);

	return filter_part_next_list(rc->parts, last);
}

FilterRule *
rule_context_next_rule(RuleContext *rc, FilterRule *last, const gchar *source)
{
	g_return_val_if_fail (rc, NULL);

	return filter_rule_next_list(rc->rules, last, source);
}

FilterRule *
rule_context_find_rule(RuleContext *rc, const gchar *name, const gchar *source)
{
	g_return_val_if_fail (name, NULL);
	g_return_val_if_fail (rc, NULL);

	return filter_rule_find_list(rc->rules, name, source);
}

void
rule_context_add_part(RuleContext *rc, FilterPart *part)
{
	g_return_if_fail (rc);
	g_return_if_fail (part);

	rc->parts = g_list_append(rc->parts, part);
}

void
rule_context_add_rule(RuleContext *rc, FilterRule *new)
{
	g_return_if_fail (rc);
	g_return_if_fail (new);

	d(printf("add rule '%s'\n", new->name));

	rc->rules = g_list_append(rc->rules, new);

	if (rc->priv->frozen == 0) {
		g_signal_emit(rc, signals[RULE_ADDED], 0, new);
		g_signal_emit(rc, signals[CHANGED], 0);
	}
}

static void
new_rule_response(GtkWidget *dialog, gint button, RuleContext *context)
{
	if (button == GTK_RESPONSE_OK) {
		FilterRule *rule = g_object_get_data((GObject *) dialog, "rule");
		gchar *user = g_object_get_data((GObject *) dialog, "path");

		if (!filter_rule_validate(rule)) {
			/* no need to popup a dialog because the validate code does that. */
			return;
		}

		if (rule_context_find_rule (context, rule->name, rule->source)) {
			e_error_run((GtkWindow *)dialog, "filter:bad-name-notunique", rule->name, NULL);

			return;
		}

		g_object_ref(rule);
		rule_context_add_rule(context, rule);
		if (user)
			rule_context_save(context, user);
	}

	gtk_widget_destroy(dialog);
}

/* add a rule, with a gui, asking for confirmation first ... optionally save to path */
void
rule_context_add_rule_gui(RuleContext *rc, FilterRule *rule, const gchar *title, const gchar *path)
{
	GtkDialog *dialog;
	GtkWidget *widget;

	d(printf("add rule gui '%s'\n", rule->name));

	g_return_if_fail (rc);
	g_return_if_fail (rule);

	widget = filter_rule_get_widget(rule, rc);
	gtk_widget_show(widget);

	dialog =(GtkDialog *) gtk_dialog_new();
	gtk_dialog_add_buttons(dialog,
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			       GTK_STOCK_OK, GTK_RESPONSE_OK,
			       NULL);
	gtk_dialog_set_has_separator (dialog, FALSE);

	gtk_window_set_title((GtkWindow *) dialog, title);
	gtk_window_set_default_size((GtkWindow *) dialog, 600, 400);
	gtk_window_set_resizable((GtkWindow *) dialog, TRUE);

	gtk_box_pack_start((GtkBox *) dialog->vbox, widget, TRUE, TRUE, 0);

	g_object_set_data_full((GObject *) dialog, "rule", rule, g_object_unref);
	if (path)
		g_object_set_data_full((GObject *) dialog, "path", g_strdup(path), g_free);

	g_signal_connect(dialog, "response", G_CALLBACK(new_rule_response), rc);

	g_object_ref(rc);

	g_object_set_data_full((GObject *) dialog, "context", rc, g_object_unref);

	gtk_widget_show((GtkWidget *) dialog);
}

void
rule_context_remove_rule(RuleContext *rc, FilterRule *rule)
{
	g_return_if_fail (rc);
	g_return_if_fail (rule);

	d(printf("remove rule '%s'\n", rule->name));

	rc->rules = g_list_remove(rc->rules, rule);

	if (rc->priv->frozen == 0) {
		g_signal_emit(rc, signals[RULE_REMOVED], 0, rule);
		g_signal_emit(rc, signals[CHANGED], 0);
	}
}

void
rule_context_rank_rule(RuleContext *rc, FilterRule *rule, const gchar *source, gint rank)
{
	GList *node;
	gint i = 0, index = 0;

	g_return_if_fail (rc);
	g_return_if_fail (rule);

	if (rule_context_get_rank_rule (rc, rule, source) == rank)
		return;

	rc->rules = g_list_remove(rc->rules, rule);
	node = rc->rules;
	while (node) {
		FilterRule *r = node->data;

		if (i == rank) {
			rc->rules = g_list_insert(rc->rules, rule, index);
			if (rc->priv->frozen == 0)
				g_signal_emit(rc, signals[CHANGED], 0);

			return;
		}

		index++;
		if (source == NULL || (r->source && strcmp(r->source, source) == 0))
			i++;

		node = node->next;
	}

	rc->rules = g_list_append(rc->rules, rule);
	if (rc->priv->frozen == 0)
		g_signal_emit(rc, signals[CHANGED], 0);
}

gint
rule_context_get_rank_rule(RuleContext *rc, FilterRule *rule, const gchar *source)
{
	GList *node;
	gint i = 0;

	g_return_val_if_fail (rc, -1);
	g_return_val_if_fail (rule, -1);

	d(printf("getting rank of rule '%s'\n", rule->name));

	node = rc->rules;
	while (node) {
		FilterRule *r = node->data;

		d(printf(" checking against rule '%s' rank '%d'\n", r->name, i));

		if (r == rule)
			return i;

		if (source == NULL || (r->source && strcmp(r->source, source) == 0))
			i++;

		node = node->next;
	}

	return -1;
}

FilterRule *
rule_context_find_rank_rule(RuleContext *rc, gint rank, const gchar *source)
{
	GList *node;
	gint i = 0;

	g_return_val_if_fail (rc, NULL);

	d(printf("getting rule at rank %d source '%s'\n", rank, source?source:"<any>"));

	node = rc->rules;
	while (node) {
		FilterRule *r = node->data;

		d(printf(" checking against rule '%s' rank '%d'\n", r->name, i));

		if (source == NULL || (r->source && strcmp(r->source, source) == 0)) {
			if (rank == i)
				return r;
			i++;
		}

		node = node->next;
	}

	return NULL;
}

static GList *
delete_uri(RuleContext *rc, const gchar *uri, GCompareFunc cmp)
{
	return NULL;
}

GList *
rule_context_delete_uri(RuleContext *rc, const gchar *uri, GCompareFunc cmp)
{
	return RULE_CONTEXT_GET_CLASS(rc)->delete_uri(rc, uri, cmp);
}

static GList *
rename_uri(RuleContext *rc, const gchar *olduri, const gchar *newuri, GCompareFunc cmp)
{
	return NULL;
}

GList *
rule_context_rename_uri(RuleContext *rc, const gchar *olduri, const gchar *newuri, GCompareFunc cmp)
{
	return RULE_CONTEXT_GET_CLASS(rc)->rename_uri(rc, olduri, newuri, cmp);
}

void
rule_context_free_uri_list(RuleContext *rc, GList *uris)
{
	GList *l = uris, *n;

	/* TODO: should be virtual */

	while (l) {
		n = l->next;
		g_free(l->data);
		g_list_free_1(l);
		l = n;
	}
}

static FilterElement *
new_element(RuleContext *rc, const gchar *type)
{
	if (!strcmp (type, "string")) {
		return (FilterElement *) filter_input_new ();
	} else if (!strcmp (type, "address")) {
		/* FIXME: temporary ... need real address type */
		return (FilterElement *) filter_input_new_type_name (type);
	} else if (!strcmp (type, "code")) {
		return (FilterElement *) filter_code_new (FALSE);
	} else if (!strcmp (type, "rawcode")) {
		return (FilterElement *) filter_code_new (TRUE);
	} else if (!strcmp (type, "colour")) {
		return (FilterElement *) filter_colour_new ();
	} else if (!strcmp (type, "optionlist")) {
		return (FilterElement *) filter_option_new ();
	} else if (!strcmp (type, "datespec")) {
		return (FilterElement *) filter_datespec_new ();
	} else if (!strcmp (type, "command")) {
		return (FilterElement *) filter_file_new_type_name (type);
	} else if (!strcmp (type, "file")) {
		return (FilterElement *) filter_file_new_type_name (type);
	} else if (!strcmp (type, "integer")) {
		return (FilterElement *) filter_int_new ();
	} else if (!strcmp (type, "regex")) {
		return (FilterElement *) filter_input_new_type_name (type);
	}else if (!strcmp(type, "completedpercent")) {
                 return (FilterElement *) filter_int_new_type("completedpercent", 0,100);

	} else {
		g_warning("Unknown filter type '%s'", type);
		return NULL;
	}
}

/**
 * rule_context_new_element:
 * @rc:
 * @name:
 *
 * create a new filter element based on name.
 *
 * Return value:
 **/
FilterElement *
rule_context_new_element(RuleContext *rc, const gchar *name)
{
	if (name == NULL)
		return NULL;

	return RULE_CONTEXT_GET_CLASS(rc)->new_element(rc, name);
}

