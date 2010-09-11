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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-util/e-alert-dialog.h"

#include "e-filter-rule.h"
#include "e-rule-context.h"

#define E_FILTER_RULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_FILTER_RULE, EFilterRulePrivate))

typedef struct _FilterPartData FilterPartData;
typedef struct _FilterRuleData FilterRuleData;

struct _EFilterRulePrivate {
	gint frozen;
};

struct _FilterPartData {
	EFilterRule *rule;
	ERuleContext *context;
	EFilterPart *part;
	GtkWidget *partwidget;
	GtkWidget *container;
};

struct _FilterRuleData {
	EFilterRule *rule;
	ERuleContext *context;
	GtkWidget *parts;
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EFilterRule,
	e_filter_rule,
	G_TYPE_OBJECT)

static void
filter_rule_grouping_changed_cb (GtkComboBox *combo_box,
                                 EFilterRule *rule)
{
	rule->grouping = gtk_combo_box_get_active (combo_box);
}

static void
filter_rule_threading_changed_cb (GtkComboBox *combo_box,
                                  EFilterRule *rule)
{
	rule->threading = gtk_combo_box_get_active (combo_box);
}

static void
part_combobox_changed (GtkComboBox *combobox,
                       FilterPartData *data)
{
	EFilterPart *part = NULL;
	EFilterPart *newpart;
	gint index, i;

	index = gtk_combo_box_get_active (combobox);
	for (i = 0, part = e_rule_context_next_part (data->context, part);
		part && i < index;
		i++, part = e_rule_context_next_part (data->context, part)) {
		/* traverse until reached index */
	}

	g_return_if_fail (part != NULL);
	g_return_if_fail (i == index);

	/* dont update if we haven't changed */
	if (!strcmp (part->title, data->part->title))
		return;

	/* here we do a widget shuffle, throw away the old widget/rulepart,
	   and create another */
	if (data->partwidget)
		gtk_container_remove (GTK_CONTAINER (data->container), data->partwidget);

	newpart = e_filter_part_clone (part);
	e_filter_part_copy_values (newpart, data->part);
	e_filter_rule_replace_part (data->rule, data->part, newpart);
	g_object_unref (data->part);
	data->part = newpart;
	data->partwidget = e_filter_part_get_widget (newpart);
	if (data->partwidget)
		gtk_box_pack_start (GTK_BOX (data->container), data->partwidget, TRUE, TRUE, 0);
}

static GtkWidget *
get_rule_part_widget (ERuleContext *context,
                      EFilterPart *newpart,
                      EFilterRule *rule)
{
	EFilterPart *part = NULL;
	GtkWidget *combobox;
	GtkWidget *hbox;
	GtkWidget *p;
	gint index = 0, current = 0;
	FilterPartData *data;

	data = g_malloc0 (sizeof (*data));
	data->rule = rule;
	data->context = context;
	data->part = newpart;

	hbox = gtk_hbox_new (FALSE, 0);
	/* only set to automatically clean up the memory */
	g_object_set_data_full ((GObject *) hbox, "data", data, g_free);

	p = e_filter_part_get_widget (newpart);

	data->partwidget = p;
	data->container = hbox;

	combobox = gtk_combo_box_new_text ();

	/* sigh, this is a little ugly */
	while ((part = e_rule_context_next_part (context, part))) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _(part->title));

		if (!strcmp (newpart->title, part->title))
			current = index;

		index++;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), current);
	g_signal_connect (combobox, "changed", G_CALLBACK (part_combobox_changed), data);
	gtk_widget_show (combobox);

	gtk_box_pack_start (GTK_BOX (hbox), combobox, FALSE, FALSE, 0);
	if (p)
		gtk_box_pack_start (GTK_BOX (hbox), p, TRUE, TRUE, 0);

	gtk_widget_show_all (hbox);

	return hbox;
}

static void
less_parts (GtkWidget *button,
            FilterRuleData *data)
{
	EFilterPart *part;
	GtkWidget *rule;
	FilterPartData *part_data;

	if (g_list_length (data->rule->parts) < 1)
		return;

	rule = g_object_get_data ((GObject *) button, "rule");
	part_data = g_object_get_data ((GObject *) rule, "data");

	g_return_if_fail (part_data != NULL);

	part = part_data->part;

	/* remove the part from the list */
	e_filter_rule_remove_part (data->rule, part);
	g_object_unref (part);

	/* and from the display */
	gtk_container_remove (GTK_CONTAINER (data->parts), rule);
	gtk_container_remove (GTK_CONTAINER (data->parts), button);
}

static void
attach_rule (GtkWidget *rule,
             FilterRuleData *data,
             EFilterPart *part, gint row)
{
	GtkWidget *remove;

	gtk_table_attach (GTK_TABLE (data->parts), rule, 0, 1, row, row + 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);

	remove = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	g_object_set_data ((GObject *) remove, "rule", rule);
	g_signal_connect (remove, "clicked", G_CALLBACK (less_parts), data);
	gtk_table_attach (GTK_TABLE (data->parts), remove, 1, 2, row, row + 1,
			  0, 0, 0, 0);

	gtk_widget_show (remove);
}

static void
do_grab_focus_cb (GtkWidget *widget,
                  gpointer data)
{
	gboolean *done = (gboolean *) data;

	if (*done)
		return;

	if (widget && gtk_widget_get_can_focus (widget)) {
		*done = TRUE;
		gtk_widget_grab_focus (widget);
	}
}

static void
more_parts (GtkWidget *button,
            FilterRuleData *data)
{
	EFilterPart *new;

	/* first make sure that the last part is ok */
	if (data->rule->parts) {
		EFilterPart *part;
		GList *l;
		EAlert *alert = NULL;

		l = g_list_last (data->rule->parts);
		part = l->data;
		if (!e_filter_part_validate (part, &alert)) {
			e_alert_run_dialog (GTK_WINDOW (gtk_widget_get_toplevel (button)), alert);
			return;
		}
	}

	/* create a new rule entry, use the first type of rule */
	new = e_rule_context_next_part (data->context, NULL);
	if (new) {
		GtkWidget *w;
		guint rows;

		new = e_filter_part_clone (new);
		e_filter_rule_add_part (data->rule, new);
		w = get_rule_part_widget (data->context, new, data->rule);

		g_object_get (data->parts, "n-rows", &rows, NULL);
		gtk_table_resize (GTK_TABLE (data->parts), rows + 1, 2);
		attach_rule (w, data, new, rows);

		if (GTK_IS_CONTAINER (w)) {
			gboolean done = FALSE;

			gtk_container_foreach (GTK_CONTAINER (w), do_grab_focus_cb, &done);
		} else
			gtk_widget_grab_focus (w);

		/* also scroll down to see new part */
		w = (GtkWidget*) g_object_get_data (G_OBJECT (button), "scrolled-window");
		if (w) {
			GtkAdjustment *adjustment;

			adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (w));
			if (adjustment) {
				gdouble upper;

				upper = gtk_adjustment_get_upper (adjustment);
				gtk_adjustment_set_value (adjustment, upper);
			}

		}
	}
}

static void
name_changed (GtkEntry *entry,
              EFilterRule *rule)
{
	g_free (rule->name);
	rule->name = g_strdup (gtk_entry_get_text (entry));
}

GtkWidget *
e_filter_rule_get_widget (EFilterRule *rule,
                          ERuleContext *context)
{
	EFilterRuleClass *class;

	g_return_val_if_fail (E_IS_FILTER_RULE (rule), NULL);
	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), NULL);

	class = E_FILTER_RULE_GET_CLASS (rule);
	g_return_val_if_fail (class->get_widget != NULL, NULL);

	return class->get_widget (rule, context);
}

static void
filter_rule_load_set (xmlNodePtr node,
                      EFilterRule *rule,
                      ERuleContext *context)
{
	xmlNodePtr work;
	gchar *rulename;
	EFilterPart *part;

	work = node->children;
	while (work) {
		if (!strcmp ((gchar *)work->name, "part")) {
			rulename = (gchar *)xmlGetProp (work, (xmlChar *)"name");
			part = e_rule_context_find_part (context, rulename);
			if (part) {
				part = e_filter_part_clone (part);
				e_filter_part_xml_decode (part, work);
				e_filter_rule_add_part (rule, part);
			} else {
				g_warning ("cannot find rule part '%s'\n", rulename);
			}
			xmlFree (rulename);
		} else if (work->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown xml node in part: %s", work->name);
		}
		work = work->next;
	}
}

static void
filter_rule_finalize (GObject *object)
{
	EFilterRule *rule = E_FILTER_RULE (object);

	g_free (rule->name);
	g_free (rule->source);

	g_list_foreach (rule->parts, (GFunc) g_object_unref, NULL);
	g_list_free (rule->parts);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_filter_rule_parent_class)->finalize (object);
}

static gint
filter_rule_validate (EFilterRule *rule,
                      EAlert **alert)
{
	gint valid = TRUE;
	GList *parts;

	g_warn_if_fail (alert == NULL || *alert == NULL);
	if (!rule->name || !*rule->name) {
		if (alert)
			*alert = e_alert_new ("filter:no-name", NULL);

		return FALSE;
	}

	/* validate rule parts */
	parts = rule->parts;
	valid = parts != NULL;
	while (parts && valid) {
		valid = e_filter_part_validate ((EFilterPart *) parts->data, alert);
		parts = parts->next;
	}

	return valid;
}

static gint
filter_rule_eq (EFilterRule *rule_a,
                EFilterRule *rule_b)
{
	GList *link_a;
	GList *link_b;

	if (rule_a->enabled != rule_b->enabled)
		return FALSE;

	if (rule_a->grouping != rule_b->grouping)
		return FALSE;

	if (rule_a->threading != rule_b->threading)
		return FALSE;

	if (g_strcmp0 (rule_a->name, rule_b->name) != 0)
		return FALSE;

	if (g_strcmp0 (rule_a->source, rule_b->source) != 0)
		return FALSE;

	link_a = rule_a->parts;
	link_b = rule_b->parts;

	while (link_a != NULL && link_b != NULL) {
		EFilterPart *part_a = link_a->data;
		EFilterPart *part_b = link_b->data;

		if (!e_filter_part_eq (part_a, part_b))
			return FALSE;

		link_a = g_list_next (link_a);
		link_b = g_list_next (link_b);
	}

	if (link_a != NULL || link_b != NULL)
		return FALSE;

	return TRUE;
}

static xmlNodePtr
filter_rule_xml_encode (EFilterRule *rule)
{
	xmlNodePtr node, set, work;
	GList *l;

	node = xmlNewNode (NULL, (xmlChar *)"rule");

	xmlSetProp (
		node, (xmlChar *)"enabled",
		(xmlChar *)(rule->enabled ? "true" : "false"));

	switch (rule->grouping) {
	case E_FILTER_GROUP_ALL:
		xmlSetProp (node, (xmlChar *)"grouping", (xmlChar *)"all");
		break;
	case E_FILTER_GROUP_ANY:
		xmlSetProp (node, (xmlChar *)"grouping", (xmlChar *)"any");
		break;
	}

	switch (rule->threading) {
	case E_FILTER_THREAD_NONE:
		break;
	case E_FILTER_THREAD_ALL:
		xmlSetProp (node, (xmlChar *)"threading", (xmlChar *)"all");
		break;
	case E_FILTER_THREAD_REPLIES:
		xmlSetProp (node, (xmlChar *)"threading", (xmlChar *)"replies");
		break;
	case E_FILTER_THREAD_REPLIES_PARENTS:
		xmlSetProp (node, (xmlChar *)"threading", (xmlChar *)"replies_parents");
		break;
	case E_FILTER_THREAD_SINGLE:
		xmlSetProp (node, (xmlChar *)"threading", (xmlChar *)"single");
		break;
	}

	if (rule->source) {
		xmlSetProp (node, (xmlChar *)"source", (xmlChar *)rule->source);
	} else {
		/* set to the default filter type */
		xmlSetProp (node, (xmlChar *)"source", (xmlChar *)"incoming");
	}

	if (rule->name) {
		gchar *escaped = g_markup_escape_text (rule->name, -1);

		work = xmlNewNode (NULL, (xmlChar *)"title");
		xmlNodeSetContent (work, (xmlChar *)escaped);
		xmlAddChild (node, work);

		g_free (escaped);
	}

	set = xmlNewNode (NULL, (xmlChar *)"partset");
	xmlAddChild (node, set);
	l = rule->parts;
	while (l) {
		work = e_filter_part_xml_encode ((EFilterPart *) l->data);
		xmlAddChild (set, work);
		l = l->next;
	}

	return node;
}

static gint
filter_rule_xml_decode (EFilterRule *rule,
                        xmlNodePtr node,
                        ERuleContext *context)
{
	xmlNodePtr work;
	gchar *grouping;
	gchar *source;

	g_free (rule->name);
	rule->name = NULL;

	grouping = (gchar *)xmlGetProp (node, (xmlChar *)"enabled");
	if (!grouping)
		rule->enabled = TRUE;
	else {
		rule->enabled = strcmp (grouping, "false") != 0;
		xmlFree (grouping);
	}

	grouping = (gchar *)xmlGetProp (node, (xmlChar *)"grouping");
	if (!strcmp (grouping, "any"))
		rule->grouping = E_FILTER_GROUP_ANY;
	else
		rule->grouping = E_FILTER_GROUP_ALL;
	xmlFree (grouping);

	rule->threading = E_FILTER_THREAD_NONE;
	if (context->flags & E_RULE_CONTEXT_THREADING
	    && (grouping = (gchar *)xmlGetProp (node, (xmlChar *)"threading"))) {
		if (!strcmp (grouping, "all"))
			rule->threading = E_FILTER_THREAD_ALL;
		else if (!strcmp (grouping, "replies"))
			rule->threading = E_FILTER_THREAD_REPLIES;
		else if (!strcmp (grouping, "replies_parents"))
			rule->threading = E_FILTER_THREAD_REPLIES_PARENTS;
		else if (!strcmp (grouping, "single"))
			rule->threading = E_FILTER_THREAD_SINGLE;
		xmlFree (grouping);
	}

	g_free (rule->source);
	source = (gchar *)xmlGetProp (node, (xmlChar *)"source");
	if (source) {
		rule->source = g_strdup (source);
		xmlFree (source);
	} else {
		/* default filter type */
		rule->source = g_strdup ("incoming");
	}

	work = node->children;
	while (work) {
		if (!strcmp ((gchar *)work->name, "partset")) {
			filter_rule_load_set (work, rule, context);
		} else if (!strcmp ((gchar *)work->name, "title") ||
			!strcmp ((gchar *)work->name, "_title")) {

			if (!rule->name) {
				gchar *str, *decstr = NULL;

				str = (gchar *)xmlNodeGetContent (work);
				if (str) {
					decstr = g_strdup (_(str));
					xmlFree (str);
				}
				rule->name = decstr;
			}
		}
		work = work->next;
	}

	return 0;
}

static void
filter_rule_build_code (EFilterRule *rule,
                        GString *out)
{
	switch (rule->threading) {
	case E_FILTER_THREAD_NONE:
		break;
	case E_FILTER_THREAD_ALL:
		g_string_append (out, " (match-threads \"all\" ");
		break;
	case E_FILTER_THREAD_REPLIES:
		g_string_append (out, " (match-threads \"replies\" ");
		break;
	case E_FILTER_THREAD_REPLIES_PARENTS:
		g_string_append (out, " (match-threads \"replies_parents\" ");
		break;
	case E_FILTER_THREAD_SINGLE:
		g_string_append (out, " (match-threads \"single\" ");
		break;
	}

	switch (rule->grouping) {
	case E_FILTER_GROUP_ALL:
		g_string_append (out, " (and\n  ");
		break;
	case E_FILTER_GROUP_ANY:
		g_string_append (out, " (or\n  ");
		break;
	default:
		g_warning ("Invalid grouping");
	}

	e_filter_part_build_code_list (rule->parts, out);
	g_string_append (out, ")\n");

	if (rule->threading != E_FILTER_THREAD_NONE)
		g_string_append (out, ")\n");
}

static void
filter_rule_copy (EFilterRule *dest, EFilterRule *src)
{
	GList *node;

	dest->enabled = src->enabled;

	g_free (dest->name);
	dest->name = g_strdup (src->name);

	g_free (dest->source);
	dest->source = g_strdup (src->source);

	dest->grouping = src->grouping;
	dest->threading = src->threading;

	if (dest->parts) {
		g_list_foreach (dest->parts, (GFunc) g_object_unref, NULL);
		g_list_free (dest->parts);
		dest->parts = NULL;
	}

	node = src->parts;
	while (node) {
		EFilterPart *part;

		part = e_filter_part_clone (node->data);
		dest->parts = g_list_append (dest->parts, part);
		node = node->next;
	}
}

static GtkWidget *
filter_rule_get_widget (EFilterRule *rule,
                        ERuleContext *context)
{
	GtkWidget *hbox, *vbox, *parts, *inruleame;
	GtkWidget *add, *label, *name, *w;
	GtkWidget *combobox;
	GtkWidget *scrolledwindow;
	GtkObject *hadj, *vadj;
	GList *l;
	gchar *text;
	EFilterPart *part;
	FilterRuleData *data;
	gint rows, i;

	/* this stuff should probably be a table, but the
	   rule parts need to be a vbox */
	vbox = gtk_vbox_new (FALSE, 6);

	label = gtk_label_new_with_mnemonic (_("R_ule name:"));
	name = gtk_entry_new ();
	gtk_label_set_mnemonic_widget ((GtkLabel *)label, name);

	if (!rule->name) {
		rule->name = g_strdup (_("Untitled"));
		gtk_entry_set_text (GTK_ENTRY (name), rule->name);
		/* FIXME: do we want the following code in the future? */
		/*gtk_editable_select_region (GTK_EDITABLE (name), 0, -1);*/
	} else {
		gtk_entry_set_text (GTK_ENTRY (name), rule->name);
	}

	g_signal_connect (
		name, "realize",
		G_CALLBACK (gtk_widget_grab_focus), name);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), name, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	g_signal_connect (name, "changed", G_CALLBACK (name_changed), rule);
	gtk_widget_show (label);
	gtk_widget_show (hbox);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	text = g_strdup_printf ("<b>%s</b>",
		_("Find items that meet the following conditions"));
	label = gtk_label_new (text);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	g_free (text);

	/* this is the parts table, it should probably be inside a scrolling list */
	rows = g_list_length (rule->parts);
	parts = gtk_table_new (rows, 2, FALSE);

	/* data for the parts part of the display */
	data = g_malloc0 (sizeof (*data));
	data->context = context;
	data->rule = rule;
	data->parts = parts;

	/* only set to automatically clean up the memory */
	g_object_set_data_full ((GObject *) vbox, "data", data, g_free);

	hbox = gtk_hbox_new (FALSE, 3);

	if (context->flags & E_RULE_CONTEXT_GROUPING) {
		const gchar *thread_types[] = {
			N_("If all conditions are met"),
			N_("If any conditions are met")
		};

		label = gtk_label_new_with_mnemonic (_("_Find items:"));
		combobox = gtk_combo_box_new_text ();

		for (i=0;i<2;i++) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _(thread_types[i]));
		}

		gtk_label_set_mnemonic_widget ((GtkLabel *)label, combobox);
		gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), rule->grouping);
		gtk_widget_show (combobox);

		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 12);
		gtk_box_pack_start (GTK_BOX (hbox), combobox, FALSE, FALSE, 12);

		g_signal_connect (
			combobox, "changed",
			G_CALLBACK (filter_rule_grouping_changed_cb), rule);
	}

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	hbox = gtk_hbox_new (FALSE, 3);

	if (context->flags & E_RULE_CONTEXT_THREADING) {
		const gchar *thread_types[] = {
			/* Translators: "None" for not including threads;
			 * part of "Include threads: None" */
			N_("None"),
			N_("All related"),
			N_("Replies"),
			N_("Replies and parents"),
			N_("No reply or parent")
		};

		label = gtk_label_new_with_mnemonic (_("I_nclude threads"));
		combobox = gtk_combo_box_new_text ();

		for (i=0;i<5;i++) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _(thread_types[i]));
		}

		gtk_label_set_mnemonic_widget ((GtkLabel *)label, combobox);
		gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), rule->threading);
		gtk_widget_show (combobox);

		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 12);
		gtk_box_pack_start (GTK_BOX (hbox), combobox, FALSE, FALSE, 12);

		g_signal_connect (
			combobox, "changed",
			G_CALLBACK (filter_rule_threading_changed_cb), rule);
	}

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	hbox = gtk_hbox_new (FALSE, 3);
	add = gtk_button_new_with_mnemonic (_("A_dd Condition"));
	gtk_button_set_image (
		GTK_BUTTON (add), gtk_image_new_from_stock (
		GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
	g_signal_connect (add, "clicked", G_CALLBACK (more_parts), data);
	gtk_box_pack_start (GTK_BOX (hbox), add, FALSE, FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show (hbox);

	label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	inruleame = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (hbox), inruleame, TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (inruleame), hbox, FALSE, FALSE, 3);

	l = rule->parts;
	i = 0;
	while (l) {
		part = l->data;
		w = get_rule_part_widget (context, part, rule);
		attach_rule (w, data, part, i++);
		l = g_list_next (l);
	}

	hadj = gtk_adjustment_new (0.0, 0.0, 1.0, 1.0, 1.0, 1.0);
	vadj = gtk_adjustment_new (0.0, 0.0, 1.0, 1.0, 1.0, 1.0);
	scrolledwindow = gtk_scrolled_window_new (
		GTK_ADJUSTMENT (hadj), GTK_ADJUSTMENT (vadj));
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolledwindow),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_add_with_viewport (
		GTK_SCROLLED_WINDOW (scrolledwindow), parts);

	gtk_box_pack_start (GTK_BOX (inruleame), scrolledwindow, TRUE, TRUE, 3);

	gtk_widget_show_all (vbox);

	g_object_set_data (G_OBJECT (add), "scrolled-window", scrolledwindow);

	return vbox;
}

static void
e_filter_rule_class_init (EFilterRuleClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EFilterRulePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = filter_rule_finalize;

	class->validate = filter_rule_validate;
	class->eq = filter_rule_eq;
	class->xml_encode = filter_rule_xml_encode;
	class->xml_decode = filter_rule_xml_decode;
	class->build_code = filter_rule_build_code;
	class->copy = filter_rule_copy;
	class->get_widget = filter_rule_get_widget;

	signals[CHANGED] = g_signal_new (
		"changed",
		E_TYPE_FILTER_RULE,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EFilterRuleClass, changed),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_filter_rule_init (EFilterRule *rule)
{
	rule->priv = E_FILTER_RULE_GET_PRIVATE (rule);
	rule->enabled = TRUE;
}

/**
 * filter_rule_new:
 *
 * Create a new EFilterRule object.
 *
 * Return value: A new #EFilterRule object.
 **/
EFilterRule *
e_filter_rule_new (void)
{
	return g_object_new (E_TYPE_FILTER_RULE, NULL);
}

EFilterRule *
e_filter_rule_clone (EFilterRule *rule)
{
	EFilterRule *clone;

	g_return_val_if_fail (E_IS_FILTER_RULE (rule), NULL);

	clone = g_object_new (G_OBJECT_TYPE (rule), NULL);
	e_filter_rule_copy (clone, rule);

	return clone;
}

void
e_filter_rule_set_name (EFilterRule *rule,
                        const gchar *name)
{
	g_return_if_fail (E_IS_FILTER_RULE (rule));

	if (g_strcmp0 (rule->name, name) == 0)
		return;

	g_free (rule->name);
	rule->name = g_strdup (name);

	e_filter_rule_emit_changed (rule);
}

void
e_filter_rule_set_source (EFilterRule *rule,
                          const gchar *source)
{
	g_return_if_fail (E_IS_FILTER_RULE (rule));

	if (g_strcmp0 (rule->source, source) == 0)
		return;

	g_free (rule->source);
	rule->source = g_strdup (source);

	e_filter_rule_emit_changed (rule);
}

gint
e_filter_rule_validate (EFilterRule *rule,
                        EAlert **alert)
{
	EFilterRuleClass *class;

	g_return_val_if_fail (E_IS_FILTER_RULE (rule), FALSE);

	class = E_FILTER_RULE_GET_CLASS (rule);
	g_return_val_if_fail (class->validate != NULL, FALSE);

	return class->validate (rule, alert);
}

gint
e_filter_rule_eq (EFilterRule *rule_a,
                  EFilterRule *rule_b)
{
	EFilterRuleClass *class;

	g_return_val_if_fail (E_IS_FILTER_RULE (rule_a), FALSE);
	g_return_val_if_fail (E_IS_FILTER_RULE (rule_b), FALSE);

	class = E_FILTER_RULE_GET_CLASS (rule_a);
	g_return_val_if_fail (class->eq != NULL, FALSE);

	if (G_OBJECT_TYPE (rule_a) != G_OBJECT_TYPE (rule_b))
		return FALSE;

	return class->eq (rule_a, rule_b);
}

xmlNodePtr
e_filter_rule_xml_encode (EFilterRule *rule)
{
	EFilterRuleClass *class;

	g_return_val_if_fail (E_IS_FILTER_RULE (rule), NULL);

	class = E_FILTER_RULE_GET_CLASS (rule);
	g_return_val_if_fail (class->xml_encode != NULL, NULL);

	return class->xml_encode (rule);
}

gint
e_filter_rule_xml_decode (EFilterRule *rule,
                          xmlNodePtr node,
                          ERuleContext *context)
{
	EFilterRuleClass *class;
	gint result;

	g_return_val_if_fail (E_IS_FILTER_RULE (rule), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);
	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), FALSE);

	class = E_FILTER_RULE_GET_CLASS (rule);
	g_return_val_if_fail (class->xml_decode != NULL, FALSE);

	rule->priv->frozen++;
	result = class->xml_decode (rule, node, context);
	rule->priv->frozen--;

	e_filter_rule_emit_changed (rule);

	return result;
}

void
e_filter_rule_copy (EFilterRule *dst_rule,
                    EFilterRule *src_rule)
{
	EFilterRuleClass *class;

	g_return_if_fail (E_IS_FILTER_RULE (dst_rule));
	g_return_if_fail (E_IS_FILTER_RULE (src_rule));

	class = E_FILTER_RULE_GET_CLASS (dst_rule);
	g_return_if_fail (class->copy != NULL);

	class->copy (dst_rule, src_rule);

	e_filter_rule_emit_changed (dst_rule);
}

void
e_filter_rule_add_part (EFilterRule *rule,
                        EFilterPart *part)
{
	g_return_if_fail (E_IS_FILTER_RULE (rule));
	g_return_if_fail (E_IS_FILTER_PART (part));

	rule->parts = g_list_append (rule->parts, part);

	e_filter_rule_emit_changed (rule);
}

void
e_filter_rule_remove_part (EFilterRule *rule,
                           EFilterPart *part)
{
	g_return_if_fail (E_IS_FILTER_RULE (rule));
	g_return_if_fail (E_IS_FILTER_PART (part));

	rule->parts = g_list_remove (rule->parts, part);

	e_filter_rule_emit_changed (rule);
}

void
e_filter_rule_replace_part (EFilterRule *rule,
                            EFilterPart *old_part,
                            EFilterPart *new_part)
{
	GList *link;

	g_return_if_fail (E_IS_FILTER_RULE (rule));
	g_return_if_fail (E_IS_FILTER_PART (old_part));
	g_return_if_fail (E_IS_FILTER_PART (new_part));

	link = g_list_find (rule->parts, old_part);
	if (link != NULL)
		link->data = new_part;
	else
		rule->parts = g_list_append (rule->parts, new_part);

	e_filter_rule_emit_changed (rule);
}

void
e_filter_rule_build_code (EFilterRule *rule,
                          GString *out)
{
	EFilterRuleClass *class;

	g_return_if_fail (E_IS_FILTER_RULE (rule));
	g_return_if_fail (out != NULL);

	class = E_FILTER_RULE_GET_CLASS (rule);
	g_return_if_fail (class->build_code != NULL);

	class->build_code (rule, out);
}

void
e_filter_rule_emit_changed (EFilterRule *rule)
{
	g_return_if_fail (E_IS_FILTER_RULE (rule));

	if (rule->priv->frozen == 0)
		g_signal_emit (rule, signals[CHANGED], 0);
}

EFilterRule *
e_filter_rule_next_list (GList *list,
                         EFilterRule *last,
                         const gchar *source)
{
	GList *link = list;

	if (last != NULL) {
		link = g_list_find (link, last);
		if (link == NULL)
			link = list;
		else
			link = g_list_next (link);
	}

	if (source != NULL) {
		while (link != NULL) {
			EFilterRule *rule = link->data;

			if (g_strcmp0 (rule->source, source) == 0)
				break;

			link = g_list_next (link);
		}
	}

	return (link != NULL) ? link->data : NULL;
}

EFilterRule *
e_filter_rule_find_list (GList * list,
                         const gchar *name,
                         const gchar *source)
{
	GList *link;

	g_return_val_if_fail (name != NULL, FALSE);

	for (link = list; link != NULL; link = g_list_next (link)) {
		EFilterRule *rule = link->data;

		if (strcmp (rule->name, name) == 0)
			if (source == NULL || (rule->source != NULL && strcmp (rule->source, source) == 0))
				return rule;
	}

	return NULL;
}

#ifdef FOR_TRANSLATIONS_ONLY

static gchar *list[] = {
  N_("Incoming"), N_("Outgoing")
};
#endif
