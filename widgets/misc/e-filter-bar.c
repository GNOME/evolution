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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>


#include "e-dropdown-button.h"
#include "e-filter-bar.h"
#include "filter/rule-editor.h"

#define d(x)

enum {
	LAST_SIGNAL
};

/*static gint esb_signals [LAST_SIGNAL] = { 0, };*/

static ESearchBarClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_QUERY,
	PROP_STATE
};

/* Callbacks.  */

static void rule_changed (FilterRule *rule, gpointer user_data);

/* rule editor thingy */
static void
rule_editor_destroyed (EFilterBar *efb, GObject *deadbeef)
{
	efb->save_dialog = NULL;
	e_search_bar_set_menu_sensitive (E_SEARCH_BAR (efb), E_FILTERBAR_SAVE_ID, TRUE);
}

/* FIXME: need to update the popup menu to match any edited rules, sigh */
static void
full_rule_editor_response (GtkWidget *dialog, gint response, gpointer data)
{
	EFilterBar *efb = data;

	if (response == GTK_RESPONSE_OK)
		rule_context_save (efb->context, efb->userrules);

	gtk_widget_destroy (dialog);
}

static void
rule_editor_response (GtkWidget *dialog, gint response, gpointer data)
{
	EFilterBar *efb = data;
	FilterRule *rule;

	if (response == GTK_RESPONSE_OK) {
		rule = g_object_get_data (G_OBJECT (dialog), "rule");
		if (rule) {
			if (!filter_rule_validate (rule))
				return;

			rule_context_add_rule (efb->context, rule);
			/* FIXME: check return */
			rule_context_save (efb->context, efb->userrules);
		}
	}

	gtk_widget_destroy (dialog);
}

static void
rule_advanced_response (GtkWidget *dialog, gint response, gpointer data)
{
	EFilterBar *efb = data;
	/* the below generates a compiler warning about incompatible pointer types */
	ESearchBar *esb = (ESearchBar *)efb;
	FilterRule *rule;

	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
		rule = g_object_get_data ((GObject *) dialog, "rule");
		if (rule) {
			GtkStyle *style = gtk_widget_get_default_style ();

			if (!filter_rule_validate (rule))
				return;

			efb->current_query = rule;
			g_object_ref (rule);

			gtk_widget_modify_base (esb->entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
			gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));
			gtk_widget_modify_base (esb->icon_entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
			gtk_widget_modify_base (esb->viewoption, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
			e_search_bar_set_text (esb,_("Advanced Search"));
			gtk_widget_set_sensitive (esb->clear_button, TRUE);

			g_signal_emit_by_name (efb, "search_activated");

			if (response == GTK_RESPONSE_APPLY) {
				if (!rule_context_find_rule (efb->context, rule->name, rule->source))
					rule_context_add_rule (efb->context, rule);
				/* FIXME: check return */
				rule_context_save (efb->context, efb->userrules);
			}
		}
	} else {
		e_search_bar_set_item_id (esb, esb->last_search_option);
	}

	if (response != GTK_RESPONSE_APPLY)
		gtk_widget_destroy (dialog);
}

static void
dialog_rule_changed (FilterRule *fr, GtkWidget *dialog)
{
	gboolean sensitive;

	g_return_if_fail (dialog != NULL);

	sensitive = fr && fr->parts;
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, sensitive);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_APPLY, sensitive);
}

static void
do_advanced (ESearchBar *esb)
{
	EFilterBar *efb = (EFilterBar *)esb;

	d(printf("Advanced search!\n"));

	if (!efb->save_dialog && !efb->setquery) {
		GtkWidget *dialog, *w;
		FilterRule *rule;

		if (efb->current_query)
			rule = filter_rule_clone (efb->current_query);
		else {
			rule = filter_rule_new ();
			efb->current_query = rule;
		}

		w = filter_rule_get_widget (rule, efb->context);
		filter_rule_set_source (rule, FILTER_SOURCE_INCOMING);
		gtk_container_set_border_width (GTK_CONTAINER (w), 12);

		/* FIXME: get the toplevel window... */
		dialog = gtk_dialog_new_with_buttons (_("Advanced Search"), NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						      GTK_STOCK_SAVE, GTK_RESPONSE_APPLY,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

		efb->save_dialog = dialog;
		gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

		gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
		gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 300);
		gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 0);
		gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 12);

		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), w, TRUE, TRUE, 0);

		g_object_ref (rule);
		g_object_set_data_full ((GObject *) dialog, "rule", rule, (GDestroyNotify) g_object_unref);

		g_signal_connect (rule, "changed", G_CALLBACK (dialog_rule_changed), dialog);
		dialog_rule_changed (rule, dialog);

		g_signal_connect (dialog, "response", G_CALLBACK (rule_advanced_response), efb);
		g_object_weak_ref ((GObject *) dialog, (GWeakNotify) rule_editor_destroyed, efb);

		e_search_bar_set_menu_sensitive (esb, E_FILTERBAR_SAVE_ID, FALSE);

		gtk_widget_show (dialog);
	}
}

static void
save_search_dialog (ESearchBar *esb)
{
	FilterRule *rule;
	gchar *name, *text;
	GtkWidget *dialog, *w;

	EFilterBar *efb = (EFilterBar *)esb;

	rule = filter_rule_clone (efb->current_query);
	text = e_search_bar_get_text (esb);
	name = g_strdup_printf ("%s %s", rule->name, text && text[0] ? text : "''");
	filter_rule_set_name (rule, name);
	g_free (text);
	g_free (name);

	w = filter_rule_get_widget (rule, efb->context);
	filter_rule_set_source (rule, FILTER_SOURCE_INCOMING);
	gtk_container_set_border_width (GTK_CONTAINER (w), 12);

	/* FIXME: get the toplevel window... */
	dialog = gtk_dialog_new_with_buttons (_("Save Search"), NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	efb->save_dialog = dialog;
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 12);

	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 300);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), w, TRUE, TRUE, 0);

	g_object_ref (rule);
	g_object_set_data_full ((GObject *) dialog, "rule", rule, (GDestroyNotify) g_object_unref);
	g_signal_connect (dialog, "response", G_CALLBACK (rule_editor_response), efb);
	g_object_weak_ref ((GObject *) dialog, (GWeakNotify) rule_editor_destroyed, efb);

	g_signal_connect (rule, "changed", G_CALLBACK (dialog_rule_changed), dialog);
	dialog_rule_changed (rule, dialog);

	e_search_bar_set_menu_sensitive (esb, E_FILTERBAR_SAVE_ID, FALSE);

	gtk_widget_show (dialog);
}

static void
menubar_activated (ESearchBar *esb, gint id, gpointer data)
{
	EFilterBar *efb = (EFilterBar *)esb;
	GtkWidget *dialog;
	GtkStyle *style;

	d(printf ("menubar activated!\n"));

	switch (id) {
	case E_FILTERBAR_EDIT_ID:
		if (!efb->save_dialog) {
			efb->save_dialog = dialog = (GtkWidget *) rule_editor_new (efb->context, FILTER_SOURCE_INCOMING, _("_Searches"));

			gtk_window_set_title (GTK_WINDOW (dialog), _("Searches"));
			g_signal_connect (dialog, "response", G_CALLBACK (full_rule_editor_response), efb);
			g_object_weak_ref ((GObject *) dialog, (GWeakNotify) rule_editor_destroyed, efb);
			gtk_widget_show (dialog);
		}
		break;
	case E_FILTERBAR_SAVE_ID:
		if (efb->current_query && !efb->save_dialog)
			save_search_dialog (esb);

		d(printf("Save menu\n"));
		break;
	case E_FILTERBAR_ADVANCED_ID:
		e_search_bar_set_item_id (esb, E_FILTERBAR_ADVANCED_ID);
		break;
	default:
		if (id >= efb->menu_base && id < efb->menu_base + efb->menu_rules->len) {
#if d(!)0
			GString *out = g_string_new ("");

			printf("Selected rule: %s\n", ((FilterRule *)efb->menu_rules->pdata[id - efb->menu_base])->name);
			filter_rule_build_code (efb->menu_rules->pdata[id - efb->menu_base], out);
			printf("query: '%s'\n", out->str);
			g_string_free (out, TRUE);
#endif
			efb->current_query = (FilterRule *)efb->menu_rules->pdata[id - efb->menu_base];

			efb->setquery = TRUE;
			e_search_bar_set_item_id (esb, E_FILTERBAR_ADVANCED_ID);
			efb->setquery = FALSE;

			/* saved searches activated */
			style = gtk_widget_get_default_style ();
			efb->setquery = TRUE;
			gtk_widget_modify_base (esb->entry , GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED] ));
			gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, &(style->text [GTK_STATE_SELECTED] ));
			gtk_widget_modify_base (esb->icon_entry, GTK_STATE_NORMAL, &(style->base [GTK_STATE_SELECTED] ));
			gtk_widget_modify_base (esb->viewoption, GTK_STATE_NORMAL, &(style->base [GTK_STATE_SELECTED] ));
			e_search_bar_set_text (esb,_("Advanced Search"));
			g_signal_emit_by_name (efb, "search_activated", NULL);
			efb->setquery = FALSE;
		} else {
			return;
		}
	}

	g_signal_stop_emission_by_name (esb, "menu_activated");
}

static void
option_changed (ESearchBar *esb, gpointer data)
{
	EFilterBar *efb = (EFilterBar *)esb;
	gint id = e_search_bar_get_item_id (esb);
	gchar *query;

	d(printf("option changed, id = %d, setquery = %s %d\n", id, efb->setquery ? "true" : "false", esb->block_search));

	if (efb->setquery)
		return;

	switch (id) {
	case E_FILTERBAR_SAVE_ID:
		/* Fixme */
		/* save_search_dialog (esb); */
		break;
	case E_FILTERBAR_ADVANCED_ID:
		d(printf ("do_advanced\n"));
		if (!esb->block_search)
			do_advanced (esb);
		break;
	default:
		if (id >= efb->option_base && id < efb->option_base + efb->option_rules->len) {
			efb->current_query = (FilterRule *)efb->option_rules->pdata[id - efb->option_base];
			if (efb->config && efb->current_query) {
				query = e_search_bar_get_text (esb);
				efb->config (efb, efb->current_query, id, query, efb->config_data);
				g_free (query);
			}
		} else {
			gtk_widget_modify_base (esb->entry, GTK_STATE_NORMAL, NULL);
			gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, NULL);
			gtk_widget_modify_base (esb->icon_entry, GTK_STATE_NORMAL, NULL);
			efb->current_query = NULL;
			gtk_entry_set_text ((GtkEntry *)esb->entry, "");
		}
	}
}

static void
dup_item_no_subitems (ESearchBarItem *dest,
		      const ESearchBarItem *src)
{
	dest->id = src->id;
	dest->text = g_strdup (src->text);
	dest->type = src->type;
}

/* Build filter bar menu items from system/user rules + custom items +
   "Advanced Search" */
static GArray *
build_items (ESearchBar *esb, ESearchBarItem *items, gint type, gint *start, GPtrArray *rules)
{
	FilterRule *rule = NULL;
	EFilterBar *efb = (EFilterBar *)esb;
	gint id = 0, i;
	GArray *menu = g_array_new (FALSE, FALSE, sizeof (ESearchBarItem));
	ESearchBarItem item = { NULL, -1, 2 };
	const gchar *source;
	GSList *gtksux = NULL;
	gint num;

	for (i=0;i<rules->len;i++)
		gtksux = g_slist_prepend(gtksux, rules->pdata[i]);

	g_ptr_array_set_size(rules, 0);

	/* find a unique starting point for the id's of our items */
	for (i = 0; items[i].id != -1; i++) {
		ESearchBarItem dup_item = { NULL, -1, 2 };

		if (items[i].id >= id)
			id = items[i].id + 1;

		dup_item_no_subitems (&dup_item, items + i);
		g_array_append_vals (menu, &dup_item, 1);
	}

	*start = id;

	if (type == 0) {
		source = FILTER_SOURCE_INCOMING;

		/* Add a separator if there is at least one custom rule.  */
		if (rule_context_next_rule (efb->context, rule, source) != NULL) {
			item.id = 0;
			item.text = NULL;
			item.type = 0;
			g_array_append_vals (menu, &item, 1);
		}
	} else {
		source = FILTER_SOURCE_DEMAND;
	}

	num = 1;
	while ((rule = rule_context_next_rule (efb->context, rule, source))) {
		item.id = id++;

		if (type == 0 && num <= 10) {
			item.text = g_strdup_printf ("_%d. %s", num % 10, rule->name);
			num ++;
		} else {
			item.text = g_strdup (rule->name);
		}
		g_array_append_vals (menu, &item, 1);

		if (g_slist_find(gtksux, rule) == NULL) {
			g_object_ref (rule);
			g_signal_connect (rule, "changed", G_CALLBACK (rule_changed), efb);
		} else {
			gtksux = g_slist_remove(gtksux, rule);
		}
		g_ptr_array_add (rules, rule);
	}

	/* anything left in gtksux has gone away, and we need to unref/disconnect from it */
	while (gtksux) {
		GSList *next;

		next = gtksux->next;
		rule = gtksux->data;

		g_signal_handlers_disconnect_by_func (rule, G_CALLBACK (rule_changed), efb);
		g_object_unref (rule);

		g_slist_free_1(gtksux);
		gtksux = next;
	}

	/* always add on the advanced menu */
	if (type == 1) {
		ESearchBarItem sb_items[2] = { E_FILTERBAR_SEPARATOR, E_FILTERBAR_ADVANCED,
					       /* E_FILTERBAR_SEPARATOR, E_FILTERBAR_SAVE */ };
		ESearchBarItem dup_items[2];

		dup_item_no_subitems (&dup_items[0], &sb_items[0]);
		dup_item_no_subitems (&dup_items[1], &sb_items[1]);
		/* dup_item_no_subitems (&dup_items[2], &sb_items[2]); */
		/* dup_item_no_subitems (&dup_items[3], &sb_items[3]); */
		g_array_append_vals (menu, &dup_items, 2);
	}

	item.id = -1;
	item.text = NULL;
	g_array_append_vals (menu, &item, 1);

	return menu;
}

static void
free_built_items (GArray *menu)
{
	gint i;

	for (i = 0; i < menu->len; i ++) {
		ESearchBarItem *item;

		item = & g_array_index (menu, ESearchBarItem, i);
		g_free (item->text);
	}

	g_array_free (menu, TRUE);
}

static void
generate_menu (ESearchBar *esb, ESearchBarItem *items)
{
	EFilterBar *efb = (EFilterBar *)esb;
	GArray *menu;

	menu = build_items (esb, items, 0, &efb->menu_base, efb->menu_rules);
	((ESearchBarClass *)parent_class)->set_menu (esb, (ESearchBarItem *)menu->data);
	free_built_items (menu);
}

static void
free_items (ESearchBarItem *items)
{
	gint i;

	for (i = 0; items[i].id != -1; i++)
		g_free (items[i].text);

	g_free (items);
}

/* Virtual methods */
static void
set_menu (ESearchBar *esb, ESearchBarItem *items)
{
	EFilterBar *efb = E_FILTER_BAR (esb);
	ESearchBarItem *default_items;
	gint i, num;

	if (efb->default_items)
		free_items (efb->default_items);

	for (num = 0; items[num].id != -1; num++)
		;

	default_items = g_new (ESearchBarItem, num + 1);
	for (i = 0; i < num + 1; i++) {
		default_items[i].text = g_strdup (items[i].text);
		default_items[i].id = items[i].id;
		default_items[i].type = items[i].type;
	}

	efb->default_items = default_items;

	generate_menu (esb, default_items);
}

static void
set_option (ESearchBar *esb, ESearchBarItem *items)
{
	GArray *menu;
	EFilterBar *efb = (EFilterBar *)esb;

	menu = build_items (esb, items, 1, &efb->option_base, efb->option_rules);
	((ESearchBarClass *)parent_class)->set_option (esb, (ESearchBarItem *)menu->data);
	free_built_items (menu);

	e_search_bar_set_item_id (esb, efb->option_base);
}

static void
context_changed (RuleContext *context, gpointer user_data)
{
	EFilterBar *efb = E_FILTER_BAR (user_data);
	ESearchBar *esb = E_SEARCH_BAR (user_data);

	/* just generate whole menu again */
	generate_menu (esb, efb->default_items);
}

static void
context_rule_removed (RuleContext *context, FilterRule *rule, gpointer user_data)
{
	EFilterBar *efb = E_FILTER_BAR (user_data);
	ESearchBar *esb = E_SEARCH_BAR (user_data);

	/* just generate whole menu again */
	generate_menu (esb, efb->default_items);
}

static void
rule_changed (FilterRule *rule, gpointer user_data)
{
	EFilterBar *efb = E_FILTER_BAR (user_data);
	ESearchBar *esb = E_SEARCH_BAR (user_data);

	/* just generate whole menu again */
	generate_menu (esb, efb->default_items);
}


/* GtkObject methods.  */

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	EFilterBar *efb = (EFilterBar *) object;
	ESearchBar *esb = E_SEARCH_BAR (object);

	switch (property_id) {
	case PROP_QUERY: {
		gchar *text = e_search_bar_get_text (E_SEARCH_BAR (efb));

		/* empty search text means searching turned off */
		if (efb->current_query && text && *text) {
			GString *out = g_string_new ("");

			filter_rule_build_code (efb->current_query, out);
			g_value_take_string (value, out->str);
			g_string_free (out, FALSE);
		} else {
			g_value_set_string (value, NULL);
		}

		g_free (text);
		break; }
	case PROP_STATE: {
		/* FIXME: we should have ESearchBar save its own state to the xmlDocPtr */
		xmlChar *xmlbuf;
		gchar *text, buf[12];
		gint searchscope, item_id, n, view_id;
		xmlNodePtr root, node;
		xmlDocPtr doc;

		item_id = e_search_bar_get_item_id ((ESearchBar *) efb);

		doc = xmlNewDoc ((const guchar *)"1.0");
		root = xmlNewDocNode (doc, NULL, (const guchar *)"state", NULL);
		xmlDocSetRootElement (doc, root);
		searchscope = e_search_bar_get_search_scope ((ESearchBar *) efb);
		view_id = e_search_bar_get_viewitem_id ((ESearchBar *) efb);

		if (searchscope < E_FILTERBAR_CURRENT_FOLDER_ID)
			item_id = esb->last_search_option;

		if (item_id == E_FILTERBAR_ADVANCED_ID) {
			/* advanced query, save the filterbar state */
			node = xmlNewChild (root, NULL, (const guchar *)"filter-bar", NULL);

			sprintf (buf, "%d", esb->last_search_option);
			xmlSetProp (node, (const guchar *)"item_id", (guchar *)buf);
			sprintf (buf, "%d", searchscope);
			xmlSetProp (node, (const guchar *)"searchscope", (guchar *)buf);
			sprintf (buf, "%d", view_id);
			xmlSetProp (node, (const guchar *)"view_id", (guchar *)buf);

			xmlAddChild (node, filter_rule_xml_encode (efb->current_query));
		} else {
			/* simple query, save the searchbar state */
			text = e_search_bar_get_text ((ESearchBar *) efb);

			node = xmlNewChild (root, NULL, (const guchar *)"search-bar", NULL);
			xmlSetProp (node, (const guchar *)"text", (guchar *)(text ? text : ""));
			sprintf (buf, "%d", item_id);
			xmlSetProp (node, (const guchar *)"item_id", (guchar *)buf);
			sprintf (buf, "%d", searchscope);
			xmlSetProp (node, (const guchar *)"searchscope", (guchar *)buf);
			sprintf (buf, "%d", view_id);
			xmlSetProp (node, (const guchar *)"view_id", (guchar *)buf);
			g_free (text);
		}

		xmlDocDumpMemory (doc, &xmlbuf, &n);
		xmlFreeDoc (doc);

		/* remap to glib memory */
		text = g_malloc (n + 1);
		memcpy (text, (gchar *)xmlbuf, n);
		text[n] = '\0';
		xmlFree (xmlbuf);

		g_value_take_string (value, text);

		break; }
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static gint
xml_get_prop_int (xmlNodePtr node, const gchar *prop)
{
	gchar *buf;
	gint ret;

	if ((buf = (gchar *)xmlGetProp (node, (guchar *)prop))) {
		ret = strtol (buf, NULL, 10);
		xmlFree (buf);
	} else {
		ret = -1;
	}

	return ret;
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	EFilterBar *efb = (EFilterBar *) object;
	ESearchBar *esb = E_SEARCH_BAR (object);
	xmlNodePtr root, node;
	const gchar *state;
	xmlDocPtr doc;
	gboolean rule_set = FALSE, is_cur_folder=FALSE;
	gint view_id, scope, item_id;

	switch (property_id) {
	case PROP_STATE:
		if ((state = g_value_get_string (value))) {
			if (!(doc = xmlParseDoc ((guchar *) state)))
				return;

			root = doc->children;
			if (strcmp ((gchar *)root->name, "state") != 0) {
				xmlFreeDoc (doc);
				return;
			}

			node = root->children;
			while (node != NULL) {
				if (!strcmp ((gchar *)node->name, "filter-bar")) {
					FilterRule *rule = NULL;

					view_id = xml_get_prop_int (node, "view_id");
					scope = xml_get_prop_int (node, "searchscope");
					item_id = xml_get_prop_int (node, "item_id");
					if (item_id == -1)
						item_id = 0;

					if (scope == E_FILTERBAR_CURRENT_FOLDER_ID)
						is_cur_folder = TRUE;

					if ((node = node->children)) {
						GtkStyle *style = gtk_widget_get_default_style ();

						rule = filter_rule_new ();
						if (filter_rule_xml_decode (rule, node, efb->context) != 0) {
							gtk_widget_modify_base (E_SEARCH_BAR (efb)->entry, GTK_STATE_NORMAL, NULL);
							gtk_widget_modify_text (((ESearchBar *)efb)->entry, GTK_STATE_NORMAL, NULL);
							gtk_widget_modify_base (((ESearchBar *)efb)->icon_entry, GTK_STATE_NORMAL, NULL);
							g_object_unref (rule);
							rule = NULL;
						} else {
							rule_set = TRUE;
							gtk_widget_set_sensitive (esb->clear_button, TRUE);
							gtk_widget_modify_base (((ESearchBar *)efb)->entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
							gtk_widget_modify_text (((ESearchBar *)efb)->entry, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));
							gtk_widget_modify_base (((ESearchBar *)efb)->icon_entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
							gtk_widget_modify_base (((ESearchBar *)efb)->viewoption, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
							g_object_set_data_full (object, "rule", rule, (GDestroyNotify) g_object_unref);
						}
					}

					if (rule_set) {
						esb->block_search = TRUE;
						e_search_bar_set_text (esb, _("Advanced Search"));
						e_search_bar_set_item_menu ((ESearchBar *) efb, item_id);
						e_search_bar_set_search_scope ((ESearchBar *) efb, scope);
						esb->block_search = FALSE;
						efb->current_query = (FilterRule *)efb->option_rules->pdata[item_id - efb->option_base];
						if (efb->config && efb->current_query) {
							gchar *query = e_search_bar_get_text (esb);
							efb->config (efb, efb->current_query, item_id, query, efb->config_data);
							g_free (query);

						}
					}
					e_search_bar_set_viewitem_id ((ESearchBar *) efb, view_id);
					efb->current_query = rule;
					efb->setquery = TRUE;
					e_search_bar_set_item_id ((ESearchBar *) efb, E_FILTERBAR_ADVANCED_ID);
					efb->setquery = FALSE;

					break;
				} else if (!strcmp ((gchar *)node->name, "search-bar")) {
					gint subitem_id, item_id, scope, view_id;
					gchar *text;
					GtkStyle *style = gtk_widget_get_default_style ();

					/* set the text first (it doesn't emit a signal) */

					/* now set the item_id and subitem_id */
					item_id = xml_get_prop_int (node, "item_id");
					subitem_id = xml_get_prop_int (node, "subitem_id");

					esb->block_search = TRUE;
					if (subitem_id >= 0)
						e_search_bar_set_ids (E_SEARCH_BAR (efb), item_id, subitem_id);
					else
						e_search_bar_set_item_menu (E_SEARCH_BAR (efb), item_id);
					esb->block_search = FALSE;
					view_id = xml_get_prop_int (node, "view_id");
					e_search_bar_set_viewitem_id (E_SEARCH_BAR (efb), view_id);
					scope = xml_get_prop_int (node, "searchscope");
					e_search_bar_set_search_scope (E_SEARCH_BAR (efb), scope);

					text = (gchar *)xmlGetProp (node, (const guchar *)"text");
					e_search_bar_set_text (E_SEARCH_BAR (efb), text);
					if (text && *text) {
						efb->current_query = (FilterRule *)efb->option_rules->pdata[item_id - efb->option_base];
						if (efb->config && efb->current_query)
							efb->config (efb, efb->current_query, item_id, text, efb->config_data);
						gtk_widget_set_sensitive (esb->clear_button, TRUE);
						gtk_widget_modify_base (((ESearchBar *)efb)->entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
						gtk_widget_modify_text (((ESearchBar *)efb)->entry, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));
						gtk_widget_modify_base (((ESearchBar *)efb)->icon_entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
						gtk_widget_modify_base (((ESearchBar *)efb)->viewoption, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
					} else {
						gtk_widget_modify_base (((ESearchBar *)efb)->entry, GTK_STATE_NORMAL, NULL);
						gtk_widget_modify_text (((ESearchBar *)efb)->entry, GTK_STATE_NORMAL, NULL);
						gtk_widget_modify_base (((ESearchBar *)efb)->icon_entry, GTK_STATE_NORMAL, NULL);
						e_search_bar_paint (esb);
						efb->current_query = (FilterRule *)efb->option_rules->pdata[item_id - efb->option_base];
						if (efb->config && efb->current_query)
							efb->config (efb, efb->current_query, item_id, "", efb->config_data);
					}

					xmlFree (text);

					break;
				}

				node = node->next;
			}

			xmlFreeDoc (doc);
		} else {
			/* set default state */
			e_search_bar_set_item_id ((ESearchBar *) efb, 0);
			e_search_bar_set_viewitem_id ((ESearchBar *) efb, 0);
			e_search_bar_set_search_scope ((ESearchBar *) efb, E_FILTERBAR_CURRENT_FOLDER_ID);
		}

		/* we don't want to run option_changed */
		efb->setquery = TRUE;
		g_signal_emit_by_name (efb, "search_activated", NULL);
		efb->setquery = FALSE;

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void clear_rules(EFilterBar *efb, GPtrArray *rules)
{
	gint i;
	FilterRule *rule;

	/* clear out any data on old rules */
	for (i=0;i<rules->len;i++) {
		rule = rules->pdata[i];
		g_signal_handlers_disconnect_by_func (rule, G_CALLBACK (rule_changed), efb);
		g_object_unref(rule);
	}
	g_ptr_array_set_size (rules, 0);
}

static void
dispose (GObject *object)
{
	EFilterBar *bar;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_FILTER_BAR (object));

	bar = E_FILTER_BAR (object);

	if (bar->context != NULL && bar->userrules != NULL)
		rule_context_save (bar->context, bar->userrules);

	if (bar->menu_rules != NULL) {
		clear_rules(bar, bar->menu_rules);
		clear_rules(bar, bar->option_rules);

		g_ptr_array_free (bar->menu_rules, TRUE);
		g_ptr_array_free (bar->option_rules, TRUE);

		g_free (bar->systemrules);
		g_free (bar->userrules);

		bar->menu_rules = NULL;
		bar->option_rules = NULL;
		bar->systemrules = NULL;
		bar->userrules = NULL;
	}

	if (bar->context != NULL) {
		g_signal_handlers_disconnect_by_func (bar->context, G_CALLBACK (context_changed), bar);
		g_signal_handlers_disconnect_by_func (bar->context, G_CALLBACK (context_rule_removed), bar);

		g_object_unref (bar->context);
		bar->context = NULL;
	}

	if (bar->default_items) {
		free_items (bar->default_items);
		bar->default_items = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}


static void
class_init (EFilterBarClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	ESearchBarClass *esb_class = (ESearchBarClass *) klass;
	GParamSpec *pspec;

	parent_class = g_type_class_ref (e_search_bar_get_type ());

	object_class->dispose = dispose;
	object_class->get_property = get_property;
	object_class->set_property = set_property;

	esb_class->set_menu = set_menu;
	esb_class->set_option = set_option;

	pspec = g_param_spec_string ("query", NULL, NULL, NULL, G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_QUERY, pspec);

	pspec = g_param_spec_string ("state", NULL, NULL, NULL, G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_STATE, pspec);

	/*gtk_object_add_arg_type ("EFilterBar::query", G_TYPE_STRING, GTK_ARG_READABLE, ARG_QUERY);*/

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

#if 0
	esb_signals [QUERY_CHANGED] =
		g_signal_new ("query_changed",
				G_SIGNAL_RUN_LAST,
				object_class->type,
				G_STRUCT_OFFSET (EFilterBarClass, query_changed),
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);

	esb_signals [MENU_ACTIVATED] =
		g_signal_new ("menu_activated",
				G_SIGNAL_RUN_LAST,
				object_class->type,
				G_STRUCT_OFFSET (EFilterBarClass, menu_activated),
				g_cclosure_marshal_VOID__INT,
				G_TYPE_NONE, 1, G_TYPE_INT);

	gtk_object_class_add_signals (object_class, esb_signals, LAST_SIGNAL);
#endif
}

static void
init (EFilterBar *efb)
{
	g_signal_connect (efb, "menu_activated", G_CALLBACK (menubar_activated), NULL);
	g_signal_connect (efb, "query_changed", G_CALLBACK (option_changed), NULL);
	g_signal_connect (efb, "search_activated", G_CALLBACK (option_changed), NULL);

	efb->menu_rules = g_ptr_array_new ();
	efb->option_rules = g_ptr_array_new ();
}


/* Object construction.  */

EFilterBar *
e_filter_bar_new (RuleContext *context,
		  const gchar *systemrules,
		  const gchar *userrules,
		  EFilterBarConfigRule config,
		  gpointer data)
{
	EFilterBar *bar;

	bar = g_object_new (e_filter_bar_get_type (), NULL);
	((ESearchBar *)bar)->lite = FALSE;

	e_filter_bar_new_construct (context, systemrules, userrules, config, data, bar);

	return bar;
}

EFilterBar *
e_filter_bar_lite_new (RuleContext *context,
		  const gchar *systemrules,
		  const gchar *userrules,
		  EFilterBarConfigRule config,
		  gpointer data)
{
	EFilterBar *bar;

	bar = g_object_new (e_filter_bar_get_type (), NULL);
	((ESearchBar *)bar)->lite = TRUE;
	e_filter_bar_new_construct (context, systemrules, userrules, config, data, bar);

	return bar;
}

void
e_filter_bar_new_construct (RuleContext *context,
		  const gchar *systemrules,
		  const gchar *userrules,
		  EFilterBarConfigRule config,
		  gpointer data ,EFilterBar *bar )
{
	ESearchBarItem item = { NULL, -1, 0 };

	bar->context = context;
	g_object_ref (context);

	bar->config = config;
	bar->config_data = data;

	bar->systemrules = g_strdup (systemrules);
	bar->userrules = g_strdup (userrules);

	bar->all_account_search_vf = NULL;
	bar->account_search_vf = NULL;
	bar->account_search_cancel = NULL;

	e_search_bar_construct ((ESearchBar *)bar, &item, &item);

	g_signal_connect (context, "changed", G_CALLBACK (context_changed), bar);
	g_signal_connect (context, "rule_removed", G_CALLBACK (context_rule_removed), bar);

}

GType
e_filter_bar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EFilterBarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EFilterBar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			e_search_bar_get_type (), "EFilterBar", &type_info, 0);
	}

	return type;
}
