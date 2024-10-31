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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <glib/gi18n.h>

#include <libedataserver/libedataserver.h>

#include "e-alert-dialog.h"
#include "e-filter-code.h"
#include "e-filter-color.h"
#include "e-filter-datespec.h"
#include "e-filter-file.h"
#include "e-filter-input.h"
#include "e-filter-int.h"
#include "e-filter-label.h"
#include "e-filter-option.h"
#include "e-filter-rule.h"
#include "e-rule-context.h"
#include "e-xml-utils.h"

struct _ERuleContextPrivate {
	gint frozen;
};

enum {
	RULE_ADDED,
	RULE_REMOVED,
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _revert_data {
	GHashTable *rules;
	gint rank;
};

G_DEFINE_TYPE_WITH_PRIVATE (ERuleContext, e_rule_context, G_TYPE_OBJECT)

static void
rule_context_set_error (ERuleContext *context,
                        gchar *error)
{
	g_free (context->error);
	context->error = error;
}

static void
new_rule_response (GtkWidget *dialog,
                   gint button,
                   ERuleContext *context)
{
	if (button == GTK_RESPONSE_OK) {
		EFilterRule *rule = g_object_get_data ((GObject *) dialog, "rule");
		gchar *user = g_object_get_data ((GObject *) dialog, "path");
		EAlert *alert = NULL;

		if (!e_filter_rule_validate (rule, &alert)) {
			e_alert_run_dialog (GTK_WINDOW (dialog), alert);
			g_object_unref (alert);
			return;
		}

		if (e_rule_context_find_rule (context, rule->name, rule->source)) {
			e_alert_run_dialog_for_args ((GtkWindow *) dialog,
						     "filter:bad-name-notunique",
						     rule->name, NULL);

			return;
		}

		g_object_ref (rule);
		e_rule_context_add_rule (context, rule);
		if (user)
			e_rule_context_save (context, user);
	}

	gtk_widget_destroy (dialog);
}

static void
revert_rule_remove (gpointer key,
                    EFilterRule *rule,
                    ERuleContext *context)
{
	e_rule_context_remove_rule (context, rule);
	g_object_unref (rule);
}

static void
revert_source_remove (gpointer key,
                      struct _revert_data *rest_data,
                      ERuleContext *context)
{
	g_hash_table_foreach (
		rest_data->rules, (GHFunc) revert_rule_remove, context);
	g_hash_table_destroy (rest_data->rules);
	g_free (rest_data);
}

static guint
source_hashf (const gchar *a)
{
	return (a != NULL) ? g_str_hash (a) : 0;
}

static gint
source_eqf (const gchar *a,
            const gchar *b)
{
	return (g_strcmp0 (a, b) == 0);
}

static void
free_part_set (struct _part_set_map *map)
{
	g_free (map->name);
	g_free (map);
}

static void
free_rule_set (struct _rule_set_map *map)
{
	g_free (map->name);
	g_free (map);
}

static void
rule_context_finalize (GObject *obj)
{
	ERuleContext *context =(ERuleContext *) obj;

	g_list_foreach (context->rule_set_list, (GFunc) free_rule_set, NULL);
	g_list_free (context->rule_set_list);
	g_hash_table_destroy (context->rule_set_map);

	g_list_foreach (context->part_set_list, (GFunc) free_part_set, NULL);
	g_list_free (context->part_set_list);
	g_hash_table_destroy (context->part_set_map);

	g_free (context->error);

	g_list_foreach (context->parts, (GFunc) g_object_unref, NULL);
	g_list_free (context->parts);

	g_list_foreach (context->rules, (GFunc) g_object_unref, NULL);
	g_list_free (context->rules);

	G_OBJECT_CLASS (e_rule_context_parent_class)->finalize (obj);
}

static gint
rule_context_load (ERuleContext *context,
                   const gchar *system,
                   const gchar *user)
{
	xmlNodePtr set, rule, root;
	xmlDocPtr systemdoc, userdoc;
	struct _part_set_map *part_map;
	struct _rule_set_map *rule_map;

	rule_context_set_error (context, NULL);

	systemdoc = e_xml_parse_file (system);
	if (systemdoc == NULL) {
		gchar * err_msg;

		err_msg = g_strdup_printf (
			"Unable to load system rules '%s': %s",
			system, g_strerror (errno));
		g_warning ("%s: %s", G_STRFUNC, err_msg);
		rule_context_set_error (context, err_msg);
		/* no need to free err_msg here */
		return -1;
	}

	root = xmlDocGetRootElement (systemdoc);
	if (root == NULL || strcmp ((gchar *) root->name, "filterdescription")) {
		gchar * err_msg;

		err_msg = g_strdup_printf (
			"Unable to load system rules '%s': "
			"Invalid format", system);
		g_warning ("%s: %s", G_STRFUNC, err_msg);
		rule_context_set_error (context, err_msg);
		/* no need to free err_msg here */
		xmlFreeDoc (systemdoc);
		return -1;
	}
	/* doesn't matter if this doens't exist */
	userdoc = NULL;
	if (g_file_test (user, G_FILE_TEST_IS_REGULAR))
		userdoc = e_xml_parse_file (user);

	/* now parse structure */
	/* get rule parts */
	set = root->children;
	while (set) {
		part_map = g_hash_table_lookup (context->part_set_map, set->name);
		if (part_map) {
			rule = set->children;
			while (rule) {
				if (!strcmp ((gchar *) rule->name, "part")) {
					EFilterPart *part =
						E_FILTER_PART (g_object_new (
						part_map->type, NULL, NULL));

					if (e_filter_part_xml_create (part, rule, context) == 0) {
						part_map->append (context, part);
					} else {
						g_object_unref (part);
						g_warning ("Cannot load filter part");
					}
				}
				rule = rule->next;
			}
		} else if ((rule_map = g_hash_table_lookup (
				context->rule_set_map, set->name))) {
			rule = set->children;
			while (rule) {
				if (!strcmp ((gchar *) rule->name, "rule")) {
					EFilterRule *part =
						E_FILTER_RULE (g_object_new (
						rule_map->type, NULL, NULL));

					if (e_filter_rule_xml_decode (part, rule, context) == 0) {
						part->system = TRUE;
						rule_map->append (context, part);
					} else {
						g_object_unref (part);
						g_warning ("Cannot load filter part");
					}
				}
				rule = rule->next;
			}
		}
		set = set->next;
	}

	/* now load actual rules */
	if (userdoc) {
		root = xmlDocGetRootElement (userdoc);
		set = root ? root->children : NULL;
		while (set) {
			rule_map = g_hash_table_lookup (context->rule_set_map, set->name);
			if (rule_map) {
				rule = set->children;
				while (rule) {
					if (!strcmp ((gchar *) rule->name, "rule")) {
						EFilterRule *part =
							E_FILTER_RULE (g_object_new (
							rule_map->type, NULL, NULL));

						if (e_filter_rule_xml_decode (part, rule, context) == 0) {
							rule_map->append (context, part);
						} else {
							g_object_unref (part);
							g_warning ("Cannot load filter part");
						}
					}
					rule = rule->next;
				}
			}
			set = set->next;
		}
	}

	xmlFreeDoc (userdoc);
	xmlFreeDoc (systemdoc);

	return 0;
}

static gint
rule_context_save (ERuleContext *context,
                   const gchar *user)
{
	xmlDocPtr doc;
	xmlNodePtr root, rules, work;
	GList *l;
	EFilterRule *rule;
	struct _rule_set_map *map;
	gint ret;

	doc = xmlNewDoc ((xmlChar *)"1.0");
	/* FIXME: set character encoding to UTF-8? */
	root = xmlNewDocNode (doc, NULL, (xmlChar *)"filteroptions", NULL);
	xmlDocSetRootElement (doc, root);
	l = context->rule_set_list;
	while (l) {
		map = l->data;
		rules = xmlNewDocNode (doc, NULL, (xmlChar *) map->name, NULL);
		xmlAddChild (root, rules);
		rule = NULL;
		while ((rule = map->next (context, rule, NULL))) {
			if (!rule->system) {
				work = e_filter_rule_xml_encode (rule);
				xmlAddChild (rules, work);
			}
		}
		l = g_list_next (l);
	}

	ret = e_xml_save_file (user, doc);

	xmlFreeDoc (doc);

	return ret;
}

static gint
rule_context_revert (ERuleContext *context,
                     const gchar *user)
{
	xmlNodePtr set, rule;
	/*struct _part_set_map *part_map;*/
	struct _rule_set_map *rule_map;
	struct _revert_data *rest_data;
	GHashTable *source_hash;
	xmlDocPtr userdoc;
	EFilterRule *frule;

	rule_context_set_error (context, NULL);

	userdoc = e_xml_parse_file (user);
	if (userdoc == NULL)
		/* clear out anything we have? */
		return 0;

	source_hash = g_hash_table_new (
		(GHashFunc) source_hashf,
		(GCompareFunc) source_eqf);

	/* setup stuff we have now */
	/* Note that we assume there is only 1 set of rules in a given rule context,
	 * although other parts of the code don't assume this */
	frule = NULL;
	while ((frule = e_rule_context_next_rule (context, frule, NULL))) {
		rest_data = g_hash_table_lookup (source_hash, frule->source);
		if (rest_data == NULL) {
			rest_data = g_malloc0 (sizeof (*rest_data));
			rest_data->rules = g_hash_table_new (g_str_hash, g_str_equal);
			g_hash_table_insert (source_hash, frule->source, rest_data);
		}
		g_hash_table_insert (rest_data->rules, frule->name, frule);
	}

	/* make what we have, match what we load */
	set = xmlDocGetRootElement (userdoc);
	set = set ? set->children : NULL;
	while (set) {
		rule_map = g_hash_table_lookup (context->rule_set_map, set->name);
		if (rule_map) {
			rule = set->children;
			while (rule) {
				if (!strcmp ((gchar *) rule->name, "rule")) {
					EFilterRule *part =
						E_FILTER_RULE (g_object_new (
						rule_map->type, NULL, NULL));

					if (e_filter_rule_xml_decode (part, rule, context) == 0) {
						/* Use the revert data to keep
						 * track of the right rank of
						 * this rule part. */
						rest_data = g_hash_table_lookup (source_hash, part->source);
						if (rest_data == NULL) {
							rest_data = g_malloc0 (sizeof (*rest_data));
							rest_data->rules = g_hash_table_new (
								g_str_hash,
								g_str_equal);
							g_hash_table_insert (
								source_hash,
								part->source,
								rest_data);
						}
						frule = g_hash_table_lookup (
							rest_data->rules,
							part->name);
						if (frule) {
							if (context->priv->frozen == 0 &&
							    !e_filter_rule_eq (frule, part))
								e_filter_rule_copy (frule, part);

							g_object_unref (part);
							e_rule_context_rank_rule (
								context, frule,
								frule->source,
								rest_data->rank);
							g_hash_table_remove (rest_data->rules, frule->name);
						} else {
							e_rule_context_add_rule (context, part);
							e_rule_context_rank_rule (
								context,
								part,
								part->source,
								rest_data->rank);
						}
						rest_data->rank++;
					} else {
						g_object_unref (part);
						g_warning ("Cannot load filter part");
					}
				}
				rule = rule->next;
			}
		}
		set = set->next;
	}

	xmlFreeDoc (userdoc);

	/* remove any we still have that weren't in the file */
	g_hash_table_foreach (source_hash, (GHFunc) revert_source_remove, context);
	g_hash_table_destroy (source_hash);

	return 0;
}

static EFilterElement *
rule_context_new_element (ERuleContext *context,
                          const gchar *type)
{
	if (!strcmp (type, "label")) {
		return (EFilterElement *) e_filter_label_new ();
	} else if (!strcmp (type, "string")) {
		return (EFilterElement *) e_filter_input_new ();
	} else if (!strcmp (type, "address")) {
		/* FIXME: temporary ... need real address type */
		return (EFilterElement *) e_filter_input_new_type_name (type);
	} else if (!strcmp (type, "code")) {
		return (EFilterElement *) e_filter_code_new (FALSE);
	} else if (!strcmp (type, "rawcode")) {
		return (EFilterElement *) e_filter_code_new (TRUE);
	} else if (!strcmp (type, "colour")) {
		return (EFilterElement *) e_filter_color_new ();
	} else if (!strcmp (type, "optionlist")) {
		return (EFilterElement *) e_filter_option_new ();
	} else if (!strcmp (type, "datespec")) {
		return (EFilterElement *) e_filter_datespec_new ();
	} else if (!strcmp (type, "command")) {
		return (EFilterElement *) e_filter_file_new_type_name (type);
	} else if (!strcmp (type, "file")) {
		return (EFilterElement *) e_filter_file_new_type_name (type);
	} else if (!strcmp (type, "integer")) {
		return (EFilterElement *) e_filter_int_new ();
	} else if (!strcmp (type, "regex")) {
		return (EFilterElement *) e_filter_input_new_type_name (type);
	} else if (!strcmp (type, "completedpercent")) {
		return (EFilterElement *) e_filter_int_new_type (
			"completedpercent", 0,100);
	} else {
		g_warning ("Unknown filter type '%s'", type);
		return NULL;
	}
}

static void
e_rule_context_class_init (ERuleContextClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = rule_context_finalize;

	class->load = rule_context_load;
	class->save = rule_context_save;
	class->revert = rule_context_revert;
	class->new_element = rule_context_new_element;

	signals[RULE_ADDED] = g_signal_new (
		"rule-added",
		E_TYPE_RULE_CONTEXT,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ERuleContextClass, rule_added),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	signals[RULE_REMOVED] = g_signal_new (
		"rule-removed",
		E_TYPE_RULE_CONTEXT,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ERuleContextClass, rule_removed),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	signals[CHANGED] = g_signal_new (
		"changed",
		E_TYPE_RULE_CONTEXT,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ERuleContextClass, changed),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_rule_context_init (ERuleContext *context)
{
	context->priv = e_rule_context_get_instance_private (context);

	context->part_set_map = g_hash_table_new (g_str_hash, g_str_equal);
	context->rule_set_map = g_hash_table_new (g_str_hash, g_str_equal);

	context->flags = E_RULE_CONTEXT_GROUPING;
}

/**
 * e_rule_context_new:
 *
 * Create a new ERuleContext object.
 *
 * Return value: A new #ERuleContext object.
 **/
ERuleContext *
e_rule_context_new (void)
{
	return g_object_new (E_TYPE_RULE_CONTEXT, NULL);
}

void
e_rule_context_add_part_set (ERuleContext *context,
                             const gchar *setname,
                             GType part_type,
                             ERuleContextPartFunc append,
                             ERuleContextNextPartFunc next)
{
	struct _part_set_map *map;

	g_return_if_fail (E_IS_RULE_CONTEXT (context));
	g_return_if_fail (setname != NULL);
	g_return_if_fail (append != NULL);
	g_return_if_fail (next != NULL);

	map = g_hash_table_lookup (context->part_set_map, setname);
	if (map != NULL) {
		g_hash_table_remove (context->part_set_map, setname);
		context->part_set_list = g_list_remove (context->part_set_list, map);
		free_part_set (map);
		map = NULL;
	}

	map = g_malloc0 (sizeof (*map));
	map->type = part_type;
	map->append = append;
	map->next = next;
	map->name = g_strdup (setname);
	g_hash_table_insert (context->part_set_map, map->name, map);
	context->part_set_list = g_list_append (context->part_set_list, map);
}

void
e_rule_context_add_rule_set (ERuleContext *context,
                             const gchar *setname,
                             GType rule_type,
                             ERuleContextRuleFunc append,
                             ERuleContextNextRuleFunc next)
{
	struct _rule_set_map *map;

	g_return_if_fail (E_IS_RULE_CONTEXT (context));
	g_return_if_fail (setname != NULL);
	g_return_if_fail (append != NULL);
	g_return_if_fail (next != NULL);

	map = g_hash_table_lookup (context->rule_set_map, setname);
	if (map != NULL) {
		g_hash_table_remove (context->rule_set_map, setname);
		context->rule_set_list = g_list_remove (context->rule_set_list, map);
		free_rule_set (map);
		map = NULL;
	}

	map = g_malloc0 (sizeof (*map));
	map->type = rule_type;
	map->append = append;
	map->next = next;
	map->name = g_strdup (setname);
	g_hash_table_insert (context->rule_set_map, map->name, map);
	context->rule_set_list = g_list_append (context->rule_set_list, map);
}

/**
 * e_rule_context_load:
 * @context:
 * @system:
 * @user:
 *
 * Load a rule context from a system and user description file.
 *
 * Return value:
 **/
gint
e_rule_context_load (ERuleContext *context,
                     const gchar *system,
                     const gchar *user)
{
	ERuleContextClass *class;
	gint result;

	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), -1);
	g_return_val_if_fail (system != NULL, -1);
	g_return_val_if_fail (user != NULL, -1);

	class = E_RULE_CONTEXT_GET_CLASS (context);
	g_return_val_if_fail (class != NULL, -1);
	g_return_val_if_fail (class->load != NULL, -1);

	context->priv->frozen++;
	result = class->load (context, system, user);
	context->priv->frozen--;

	return result;
}

/**
 * e_rule_context_save:
 * @context:
 * @user:
 *
 * Save a rule context to disk.
 *
 * Return value:
 **/
gint
e_rule_context_save (ERuleContext *context,
                     const gchar *user)
{
	ERuleContextClass *class;

	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), -1);
	g_return_val_if_fail (user != NULL, -1);

	class = E_RULE_CONTEXT_GET_CLASS (context);
	g_return_val_if_fail (class != NULL, -1);
	g_return_val_if_fail (class->save != NULL, -1);

	return class->save (context, user);
}

/**
 * e_rule_context_revert:
 * @context:
 * @user:
 *
 * Reverts a rule context from a user description file.  Assumes the
 * system description file is unchanged from when it was loaded.
 *
 * Return value:
 **/
gint
e_rule_context_revert (ERuleContext *context,
                       const gchar *user)
{
	ERuleContextClass *class;

	g_return_val_if_fail (E_RULE_CONTEXT (context), 0);
	g_return_val_if_fail (user != NULL, 0);

	class = E_RULE_CONTEXT_GET_CLASS (context);
	g_return_val_if_fail (class != NULL, 0);
	g_return_val_if_fail (class->revert != NULL, 0);

	return class->revert (context, user);
}

EFilterPart *
e_rule_context_find_part (ERuleContext *context,
                          const gchar *name)
{
	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	return e_filter_part_find_list (context->parts, name);
}

EFilterPart *
e_rule_context_create_part (ERuleContext *context,
                            const gchar *name)
{
	EFilterPart *part;

	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	part = e_rule_context_find_part (context, name);

	if (part == NULL)
		return NULL;

	return e_filter_part_clone (part);
}

EFilterPart *
e_rule_context_next_part (ERuleContext *context,
                          EFilterPart *last)
{
	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), NULL);

	return e_filter_part_next_list (context->parts, last);
}

EFilterRule *
e_rule_context_next_rule (ERuleContext *context,
                          EFilterRule *last,
                          const gchar *source)
{
	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), NULL);

	return e_filter_rule_next_list (context->rules, last, source);
}

EFilterRule *
e_rule_context_find_rule (ERuleContext *context,
                          const gchar *name,
                          const gchar *source)
{
	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	return e_filter_rule_find_list (context->rules, name, source);
}

void
e_rule_context_add_part (ERuleContext *context,
                         EFilterPart *part)
{
	g_return_if_fail (E_IS_RULE_CONTEXT (context));
	g_return_if_fail (E_IS_FILTER_PART (part));

	context->parts = g_list_append (context->parts, part);
}

void
e_rule_context_add_rule (ERuleContext *context,
                         EFilterRule *rule)
{
	g_return_if_fail (E_IS_RULE_CONTEXT (context));
	g_return_if_fail (E_IS_FILTER_RULE (rule));

	context->rules = g_list_append (context->rules, rule);

	if (context->priv->frozen == 0) {
		g_signal_emit (context, signals[RULE_ADDED], 0, rule);
		g_signal_emit (context, signals[CHANGED], 0);
	}
}

/* Add a rule, with a gui, asking for confirmation first,
 * and optionally save to path. */
void
e_rule_context_add_rule_gui (ERuleContext *context,
                             EFilterRule *rule,
                             const gchar *title,
                             const gchar *path)
{
	GtkDialog *dialog;
	GtkWidget *widget;
	GtkWidget *content_area;

	g_return_if_fail (E_IS_RULE_CONTEXT (context));
	g_return_if_fail (E_IS_FILTER_RULE (rule));

	widget = e_filter_rule_get_widget (rule, context);
	gtk_widget_show (widget);

	dialog =(GtkDialog *) gtk_dialog_new ();
	gtk_dialog_add_buttons (
		dialog,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	gtk_window_set_title ((GtkWindow *) dialog, title);
	gtk_window_set_default_size ((GtkWindow *) dialog, 600, 400);
	gtk_window_set_resizable ((GtkWindow *) dialog, TRUE);

	content_area = gtk_dialog_get_content_area (dialog);
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);

	g_object_set_data_full ((GObject *) dialog, "rule", rule, g_object_unref);
	if (path)
		g_object_set_data_full ((GObject *) dialog, "path", g_strdup (path), g_free);

	g_signal_connect (
		dialog, "response",
		G_CALLBACK (new_rule_response), context);

	g_object_ref (context);

	g_object_set_data_full ((GObject *) dialog, "context", context, g_object_unref);

	gtk_widget_show ((GtkWidget *) dialog);
}

void
e_rule_context_remove_rule (ERuleContext *context,
                            EFilterRule *rule)
{
	g_return_if_fail (E_IS_RULE_CONTEXT (context));
	g_return_if_fail (E_IS_FILTER_RULE (rule));

	context->rules = g_list_remove (context->rules, rule);

	if (context->priv->frozen == 0) {
		g_signal_emit (context, signals[RULE_REMOVED], 0, rule);
		g_signal_emit (context, signals[CHANGED], 0);
	}
}

void
e_rule_context_rank_rule (ERuleContext *context,
                          EFilterRule *rule,
                          const gchar *source,
                          gint rank)
{
	GList *node;
	gint i = 0, index = 0;

	g_return_if_fail (E_IS_RULE_CONTEXT (context));
	g_return_if_fail (E_IS_FILTER_RULE (rule));

	if (e_rule_context_get_rank_rule (context, rule, source) == rank)
		return;

	context->rules = g_list_remove (context->rules, rule);
	node = context->rules;
	while (node) {
		EFilterRule *r = node->data;

		if (i == rank) {
			context->rules = g_list_insert (context->rules, rule, index);
			if (context->priv->frozen == 0)
				g_signal_emit (context, signals[CHANGED], 0);

			return;
		}

		index++;
		if (source == NULL || (r->source && strcmp (r->source, source) == 0))
			i++;

		node = node->next;
	}

	context->rules = g_list_append (context->rules, rule);
	if (context->priv->frozen == 0)
		g_signal_emit (context, signals[CHANGED], 0);
}

gint
e_rule_context_get_rank_rule (ERuleContext *context,
                              EFilterRule *rule,
                              const gchar *source)
{
	GList *node;
	gint i = 0;

	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), -1);
	g_return_val_if_fail (E_IS_FILTER_RULE (rule), -1);

	node = context->rules;
	while (node) {
		EFilterRule *r = node->data;

		if (r == rule)
			return i;

		if (source == NULL || (r->source && strcmp (r->source, source) == 0))
			i++;

		node = node->next;
	}

	return -1;
}

EFilterRule *
e_rule_context_find_rank_rule (ERuleContext *context,
                               gint rank,
                               const gchar *source)
{
	GList *node;
	gint i = 0;

	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), NULL);

	node = context->rules;
	while (node) {
		EFilterRule *r = node->data;

		if (source == NULL || (r->source && strcmp (r->source, source) == 0)) {
			if (rank == i)
				return r;
			i++;
		}

		node = node->next;
	}

	return NULL;
}

GList *
e_rule_context_rename_uri (ERuleContext *context,
                           const gchar *old_uri,
                           const gchar *new_uri,
                           GCompareFunc compare)
{
	ERuleContextClass *class;

	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), NULL);
	g_return_val_if_fail (old_uri != NULL, NULL);
	g_return_val_if_fail (new_uri != NULL, NULL);
	g_return_val_if_fail (compare != NULL, NULL);

	class = E_RULE_CONTEXT_GET_CLASS (context);
	g_return_val_if_fail (class != NULL, NULL);

	/* This method is optional. */
	if (class->rename_uri == NULL)
		return NULL;

	return class->rename_uri (context, old_uri, new_uri, compare);
}

GList *
e_rule_context_delete_uri (ERuleContext *context,
                           const gchar *uri,
                           GCompareFunc compare)
{
	ERuleContextClass *class;

	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), NULL);
	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (compare != NULL, NULL);

	class = E_RULE_CONTEXT_GET_CLASS (context);
	g_return_val_if_fail (class != NULL, NULL);

	/* This method is optional. */
	if (class->delete_uri == NULL)
		return NULL;

	return class->delete_uri (context, uri, compare);
}

void
e_rule_context_free_uri_list (ERuleContext *context,
                              GList *uris)
{
	g_return_if_fail (E_IS_RULE_CONTEXT (context));

	/* TODO: should be virtual */

	g_list_foreach (uris, (GFunc) g_free, NULL);
	g_list_free (uris);
}

/**
 * e_rule_context_new_element:
 * @context:
 * @name:
 *
 * create a new filter element based on name.
 *
 * Return value:
 **/
EFilterElement *
e_rule_context_new_element (ERuleContext *context,
                            const gchar *name)
{
	ERuleContextClass *class;

	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	class = E_RULE_CONTEXT_GET_CLASS (context);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->new_element != NULL, NULL);

	return class->new_element (context, name);
}
