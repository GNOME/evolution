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
#endif

#include <string.h>
#include <glib/gi18n.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include "e-dropdown-button.h"
#include "e-filter-bar.h"
#include "filter/rule-editor.h"

/* The arguments we take */
enum {
	PROP_0,
	PROP_QUERY,
	PROP_STATE,
};

static gpointer parent_class;

/* Callbacks.  */

static void rule_changed (FilterRule *rule, gpointer user_data);

/* rule editor thingy */
static void
rule_editor_destroyed (EFilterBar *filter_bar, GObject *deadbeef)
{
	filter_bar->save_dialog = NULL;
	e_search_bar_set_menu_sensitive (E_SEARCH_BAR (filter_bar), E_FILTERBAR_SAVE_ID, TRUE);
}

static void
rule_advanced_response (GtkWidget *dialog, int response, void *data)
{
	EFilterBar *filter_bar = data;
	/* the below generates a compiler warning about incompatible pointer types */
	ESearchBar *search_bar = (ESearchBar *)filter_bar;
	FilterRule *rule;

	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
		rule = g_object_get_data ((GObject *) dialog, "rule");
		if (rule) {
			GtkStyle *style = gtk_widget_get_default_style ();

			if (!filter_rule_validate (rule))
				return;

			filter_bar->current_query = rule;
			g_object_ref (rule);
			g_signal_emit_by_name (filter_bar, "search_activated");

			gtk_widget_modify_base (search_bar->entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
			gtk_widget_modify_text (search_bar->entry, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));
			gtk_widget_modify_base (search_bar->icon_entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
			gtk_widget_modify_base (search_bar->viewoption, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
			e_search_bar_set_text (search_bar,_("Advanced Search"));
			gtk_widget_set_sensitive (search_bar->clear_button, TRUE);

			if (response == GTK_RESPONSE_APPLY) {
				if (!rule_context_find_rule (filter_bar->context, rule->name, rule->source))
					rule_context_add_rule (filter_bar->context, rule);
				/* FIXME: check return */
				rule_context_save (filter_bar->context, filter_bar->userrules);
			}
		}
	} else {
		e_search_bar_set_item_id (search_bar, search_bar->last_search_option);
	}

	if (response != GTK_RESPONSE_APPLY)
		gtk_widget_destroy (dialog);
}

static void
dialog_rule_changed (FilterRule *fr, GtkWidget *dialog)
{
	/* mbarnes: converted */
}

static void
do_advanced (ESearchBar *search_bar)
{
	EFilterBar *filter_bar = (EFilterBar *)search_bar;

	if (!filter_bar->save_dialog && !filter_bar->setquery) {
		GtkWidget *dialog, *w;
		FilterRule *rule;

		if (filter_bar->current_query)
			rule = filter_rule_clone (filter_bar->current_query);
		else {
			rule = filter_rule_new ();
			filter_bar->current_query = rule;
		}

		w = filter_rule_get_widget (rule, filter_bar->context);
		filter_rule_set_source (rule, FILTER_SOURCE_INCOMING);
		gtk_container_set_border_width (GTK_CONTAINER (w), 12);

		/* FIXME: get the toplevel window... */
		dialog = gtk_dialog_new_with_buttons (_("Advanced Search"), NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						      GTK_STOCK_SAVE, GTK_RESPONSE_APPLY,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

		filter_bar->save_dialog = dialog;
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

		g_signal_connect (dialog, "response", G_CALLBACK (rule_advanced_response), filter_bar);
		g_object_weak_ref ((GObject *) dialog, (GWeakNotify) rule_editor_destroyed, filter_bar);

		e_search_bar_set_menu_sensitive (search_bar, E_FILTERBAR_SAVE_ID, FALSE);

		gtk_widget_show (dialog);
	}
}

static void
save_search_dialog (ESearchBar *search_bar)
{
	/* mbarnes: converted */
}

static void
menubar_activated (ESearchBar *search_bar, int id, void *data)
{
	EFilterBar *filter_bar = (EFilterBar *)search_bar;
	GtkWidget *dialog;
	GtkStyle *style;

	switch (id) {
	case E_FILTERBAR_EDIT_ID:
		/* mbarnes: converted */
		break;
	case E_FILTERBAR_SAVE_ID:
		if (filter_bar->current_query && !filter_bar->save_dialog)
			save_search_dialog (search_bar);

		break;
	case E_FILTERBAR_ADVANCED_ID:
		e_search_bar_set_item_id (search_bar, E_FILTERBAR_ADVANCED_ID);
		break;
	default:
		if (id >= filter_bar->menu_base && id < filter_bar->menu_base + filter_bar->menu_rules->len) {
			filter_bar->current_query = (FilterRule *)filter_bar->menu_rules->pdata[id - filter_bar->menu_base];

			filter_bar->setquery = TRUE;
			e_search_bar_set_item_id (search_bar, E_FILTERBAR_ADVANCED_ID);
			filter_bar->setquery = FALSE;

			/* saved searches activated */
			style = gtk_widget_get_default_style ();
			filter_bar->setquery = TRUE;
			gtk_widget_modify_base (search_bar->entry , GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED] ));
			gtk_widget_modify_text (search_bar->entry, GTK_STATE_NORMAL, &(style->text [GTK_STATE_SELECTED] ));
			gtk_widget_modify_base (search_bar->icon_entry, GTK_STATE_NORMAL, &(style->base [GTK_STATE_SELECTED] ));
			gtk_widget_modify_base (search_bar->viewoption, GTK_STATE_NORMAL, &(style->base [GTK_STATE_SELECTED] ));
			e_search_bar_set_text (search_bar,_("Advanced Search"));
			g_signal_emit_by_name (filter_bar, "search_activated", NULL);
			filter_bar->setquery = FALSE;
		} else {
			return;
		}
	}

	g_signal_stop_emission_by_name (search_bar, "menu_activated");
}

static void
option_changed (ESearchBar *search_bar, void *data)
{
	EFilterBar *filter_bar = (EFilterBar *)search_bar;
	int id = e_search_bar_get_item_id (search_bar);
	char *query;

	if (search_bar->scopeitem_id == E_FILTERBAR_CURRENT_MESSAGE_ID) {
		gtk_widget_set_sensitive (search_bar->option_button, FALSE);
	} else {
		gtk_widget_set_sensitive (search_bar->option_button, TRUE);
	}

	if (filter_bar->setquery)
		return;

	switch (id) {
	case E_FILTERBAR_SAVE_ID:
		/* Fixme */
		/* save_search_dialog (search_bar); */
		break;
	case E_FILTERBAR_ADVANCED_ID:
		if (!search_bar->block_search)
			do_advanced (search_bar);
		break;
	default:
		if (id >= filter_bar->option_base && id < filter_bar->option_base + filter_bar->option_rules->len) {
			filter_bar->current_query = (FilterRule *)filter_bar->option_rules->pdata[id - filter_bar->option_base];
			if (filter_bar->config && filter_bar->current_query) {
				query = e_search_bar_get_text (search_bar);
				filter_bar->config (filter_bar, filter_bar->current_query, id, query, filter_bar->config_data);
				g_free (query);
			}
		} else {
			gtk_widget_modify_base (search_bar->entry, GTK_STATE_NORMAL, NULL);
			gtk_widget_modify_text (search_bar->entry, GTK_STATE_NORMAL, NULL);
			gtk_widget_modify_base (search_bar->icon_entry, GTK_STATE_NORMAL, NULL);
			filter_bar->current_query = NULL;
			gtk_entry_set_text ((GtkEntry *)search_bar->entry, "");
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

static GArray *
build_items (ESearchBar *search_bar, ESearchBarItem *items, int type, int *start, GPtrArray *rules)
{
	FilterRule *rule = NULL;
	EFilterBar *filter_bar = (EFilterBar *)search_bar;
	int id = 0, i;
	GArray *menu = g_array_new (FALSE, FALSE, sizeof (ESearchBarItem));
	ESearchBarItem item = { NULL, -1, 2 };
	char *source;
	GSList *gtksux = NULL;
	int num;

	/* So gtk calls a signal again if you connect to it WHILE inside a changed event.
	   So this snot is to work around that shit fucked up situation */
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
		if (rule_context_next_rule (filter_bar->context, rule, source) != NULL) {
			item.id = 0;
			item.text = NULL;
			item.type = 0;
			g_array_append_vals (menu, &item, 1);
		}
	} else {
		source = FILTER_SOURCE_DEMAND;
	}

	num = 1;
	while ((rule = rule_context_next_rule (filter_bar->context, rule, source))) {
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
			g_signal_connect (rule, "changed", G_CALLBACK (rule_changed), filter_bar);
		} else {
			gtksux = g_slist_remove(gtksux, rule);
		}
		g_ptr_array_add (rules, rule);
	}

	/* anything elft in gtksux has gone away, and we need to unref/disconnect from it */
	while (gtksux) {
		GSList *next;

		next = gtksux->next;
		rule = gtksux->data;

		g_signal_handlers_disconnect_by_func (rule, G_CALLBACK (rule_changed), filter_bar);
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
	int i;

	for (i = 0; i < menu->len; i ++) {
		ESearchBarItem *item;

		item = & g_array_index (menu, ESearchBarItem, i);
		g_free (item->text);
	}

	g_array_free (menu, TRUE);
}

static void
generate_menu (ESearchBar *search_bar, ESearchBarItem *items)
{
	EFilterBar *filter_bar = (EFilterBar *)search_bar;
	GArray *menu;

	menu = build_items (search_bar, items, 0, &filter_bar->menu_base, filter_bar->menu_rules);
	((ESearchBarClass *)parent_class)->set_menu (search_bar, (ESearchBarItem *)menu->data);
	free_built_items (menu);
}

static void
free_items (ESearchBarItem *items)
{
	int i;

	for (i = 0; items[i].id != -1; i++)
		g_free (items[i].text);


	g_free (items);
}

/* Virtual methods */
static void
set_menu (ESearchBar *search_bar, ESearchBarItem *items)
{
	EFilterBar *filter_bar = E_FILTER_BAR (search_bar);
	ESearchBarItem *default_items;
	int i, num;

	if (filter_bar->default_items)
		free_items (filter_bar->default_items);

	for (num = 0; items[num].id != -1; num++)
		;

	default_items = g_new (ESearchBarItem, num + 1);
	for (i = 0; i < num + 1; i++) {
		default_items[i].text = g_strdup (items[i].text);
		default_items[i].id = items[i].id;
		default_items[i].type = items[i].type;
	}

	filter_bar->default_items = default_items;

	generate_menu (search_bar, default_items);
}

static void
set_option (ESearchBar *search_bar, ESearchBarItem *items)
{
	GArray *menu;
	EFilterBar *filter_bar = (EFilterBar *)search_bar;

	menu = build_items (search_bar, items, 1, &filter_bar->option_base, filter_bar->option_rules);
	((ESearchBarClass *)parent_class)->set_option (search_bar, (ESearchBarItem *)menu->data);
	free_built_items (menu);

	e_search_bar_set_item_id (search_bar, filter_bar->option_base);
}

static void
context_changed (RuleContext *context, gpointer user_data)
{
	EFilterBar *filter_bar = E_FILTER_BAR (user_data);
	ESearchBar *search_bar = E_SEARCH_BAR (user_data);

	/* just generate whole menu again */
	generate_menu (search_bar, filter_bar->default_items);
}

static void
context_rule_removed (RuleContext *context, FilterRule *rule, gpointer user_data)
{
	EFilterBar *filter_bar = E_FILTER_BAR (user_data);
	ESearchBar *search_bar = E_SEARCH_BAR (user_data);

	/* just generate whole menu again */
	generate_menu (search_bar, filter_bar->default_items);
}

static void
rule_changed (FilterRule *rule, gpointer user_data)
{
	EFilterBar *filter_bar = E_FILTER_BAR (user_data);
	ESearchBar *search_bar = E_SEARCH_BAR (user_data);

	/* just generate whole menu again */
	generate_menu (search_bar, filter_bar->default_items);
}

static void
filter_bar_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	EFilterBar *filter_bar = (EFilterBar *) object;
	ESearchBar *search_bar = E_SEARCH_BAR (object);

	switch (property_id) {
	case PROP_QUERY: {
		char *text = e_search_bar_get_text (E_SEARCH_BAR (filter_bar));

		/* empty search text means searching turned off */
		if (filter_bar->current_query && text && *text) {
			GString *out = g_string_new ("");

			filter_rule_build_code (filter_bar->current_query, out);
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
		char *text, buf[12];
		int searchscope, item_id, n, view_id;
		xmlNodePtr root, node;
		xmlDocPtr doc;

		item_id = e_search_bar_get_item_id ((ESearchBar *) filter_bar);

		doc = xmlNewDoc ((const unsigned char *)"1.0");
		root = xmlNewDocNode (doc, NULL, (const unsigned char *)"state", NULL);
		xmlDocSetRootElement (doc, root);
		searchscope = e_search_bar_get_search_scope ((ESearchBar *) filter_bar);
		view_id = e_search_bar_get_viewitem_id ((ESearchBar *) filter_bar);

		if (searchscope < E_FILTERBAR_CURRENT_FOLDER_ID)
			item_id = search_bar->last_search_option;

		if (item_id == E_FILTERBAR_ADVANCED_ID) {
			/* advanced query, save the filterbar state */
			node = xmlNewChild (root, NULL, (const unsigned char *)"filter-bar", NULL);

			sprintf (buf, "%d", search_bar->last_search_option);
			xmlSetProp (node, (const unsigned char *)"item_id", (unsigned char *)buf);
			sprintf (buf, "%d", searchscope);
			xmlSetProp (node, (const unsigned char *)"searchscope", (unsigned char *)buf);
			sprintf (buf, "%d", view_id);
			xmlSetProp (node, (const unsigned char *)"view_id", (unsigned char *)buf);

			xmlAddChild (node, filter_rule_xml_encode (filter_bar->current_query));
		} else {
			/* simple query, save the searchbar state */
			text = e_search_bar_get_text ((ESearchBar *) filter_bar);

			node = xmlNewChild (root, NULL, (const unsigned char *)"search-bar", NULL);
			xmlSetProp (node, (const unsigned char *)"text", (unsigned char *)(text ? text : ""));
			sprintf (buf, "%d", item_id);
			xmlSetProp (node, (const unsigned char *)"item_id", (unsigned char *)buf);
			sprintf (buf, "%d", searchscope);
			xmlSetProp (node, (const unsigned char *)"searchscope", (unsigned char *)buf);
			sprintf (buf, "%d", view_id);
			xmlSetProp (node, (const unsigned char *)"view_id", (unsigned char *)buf);
			g_free (text);
		}

		xmlDocDumpMemory (doc, &xmlbuf, &n);
		xmlFreeDoc (doc);

		/* remap to glib memory */
		text = g_malloc (n + 1);
		memcpy (text, (char *)xmlbuf, n);
		text[n] = '\0';
		xmlFree (xmlbuf);

		g_value_take_string (value, text);

		break; }
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static int
xml_get_prop_int (xmlNodePtr node, const char *prop)
{
	char *buf;
	int ret;

	if ((buf = (char *)xmlGetProp (node, (unsigned char *)prop))) {
		ret = strtol (buf, NULL, 10);
		xmlFree (buf);
	} else {
		ret = -1;
	}

	return ret;
}

static void
filter_bar_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	EFilterBar *filter_bar = (EFilterBar *) object;
	ESearchBar *search_bar = E_SEARCH_BAR (object);
	xmlNodePtr root, node;
	const char *state;
	xmlDocPtr doc;
	gboolean rule_set = FALSE, is_cur_folder=FALSE;
	int view_id, scope, item_id;

	switch (property_id) {
	case PROP_STATE:
		if ((state = g_value_get_string (value))) {
			if (!(doc = xmlParseDoc ((unsigned char *) state)))
				return;

			root = doc->children;
			if (strcmp ((char *)root->name, "state") != 0) {
				xmlFreeDoc (doc);
				return;
			}

			node = root->children;
			while (node != NULL) {
				if (!strcmp ((char *)node->name, "filter-bar")) {
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
						if (filter_rule_xml_decode (rule, node, filter_bar->context) != 0) {
							gtk_widget_modify_base (E_SEARCH_BAR (filter_bar)->entry, GTK_STATE_NORMAL, NULL);
							gtk_widget_modify_text (((ESearchBar *)filter_bar)->entry, GTK_STATE_NORMAL, NULL);
							gtk_widget_modify_base (((ESearchBar *)filter_bar)->icon_entry, GTK_STATE_NORMAL, NULL);
							g_object_unref (rule);
							rule = NULL;
						} else {
							rule_set = TRUE;
							gtk_widget_set_sensitive (search_bar->clear_button, TRUE);
							gtk_widget_modify_base (((ESearchBar *)filter_bar)->entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
							gtk_widget_modify_text (((ESearchBar *)filter_bar)->entry, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));
							gtk_widget_modify_base (((ESearchBar *)filter_bar)->icon_entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
							gtk_widget_modify_base (((ESearchBar *)filter_bar)->viewoption, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
							g_object_set_data_full (object, "rule", rule, (GDestroyNotify) g_object_unref);
						}
					}


					if (rule_set) {
						search_bar->block_search = TRUE;
						e_search_bar_set_text (search_bar, _("Advanced Search"));
						e_search_bar_set_item_menu ((ESearchBar *) filter_bar, item_id);
						e_search_bar_set_search_scope ((ESearchBar *) filter_bar, scope);
						search_bar->block_search = FALSE;
						filter_bar->current_query = (FilterRule *)filter_bar->option_rules->pdata[item_id - filter_bar->option_base];
						if (filter_bar->config && filter_bar->current_query) {
							char *query = e_search_bar_get_text (search_bar);
							filter_bar->config (filter_bar, filter_bar->current_query, item_id, query, filter_bar->config_data);
							g_free (query);

						}
					}
					e_search_bar_set_viewitem_id ((ESearchBar *) filter_bar, view_id);
					filter_bar->current_query = rule;
					filter_bar->setquery = TRUE;
					e_search_bar_set_item_id ((ESearchBar *) filter_bar, E_FILTERBAR_ADVANCED_ID);
					filter_bar->setquery = FALSE;

					break;
				} else if (!strcmp ((char *)node->name, "search-bar")) {
					int subitem_id, item_id, scope, view_id;
					char *text;
					GtkStyle *style = gtk_widget_get_default_style ();

					/* set the text first (it doesn't emit a signal) */


					/* now set the item_id and subitem_id */
					item_id = xml_get_prop_int (node, "item_id");
					subitem_id = xml_get_prop_int (node, "subitem_id");

					search_bar->block_search = TRUE;
					if (subitem_id >= 0)
						e_search_bar_set_ids (E_SEARCH_BAR (filter_bar), item_id, subitem_id);
					else
						e_search_bar_set_item_menu (E_SEARCH_BAR (filter_bar), item_id);
					search_bar->block_search = FALSE;
					view_id = xml_get_prop_int (node, "view_id");
					e_search_bar_set_viewitem_id (E_SEARCH_BAR (filter_bar), view_id);
					scope = xml_get_prop_int (node, "searchscope");
					e_search_bar_set_search_scope (E_SEARCH_BAR (filter_bar), scope);

					text = (char *)xmlGetProp (node, (const unsigned char *)"text");
					e_search_bar_set_text (E_SEARCH_BAR (filter_bar), text);
					if (text && *text) {
						filter_bar->current_query = (FilterRule *)filter_bar->option_rules->pdata[item_id - filter_bar->option_base];
						if (filter_bar->config && filter_bar->current_query)
							filter_bar->config (filter_bar, filter_bar->current_query, item_id, text, filter_bar->config_data);
						gtk_widget_set_sensitive (search_bar->clear_button, TRUE);
						gtk_widget_modify_base (((ESearchBar *)filter_bar)->entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
						gtk_widget_modify_text (((ESearchBar *)filter_bar)->entry, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));
						gtk_widget_modify_base (((ESearchBar *)filter_bar)->icon_entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
						gtk_widget_modify_base (((ESearchBar *)filter_bar)->viewoption, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
					} else {
						gtk_widget_modify_base (((ESearchBar *)filter_bar)->entry, GTK_STATE_NORMAL, NULL);
						gtk_widget_modify_text (((ESearchBar *)filter_bar)->entry, GTK_STATE_NORMAL, NULL);
						gtk_widget_modify_base (((ESearchBar *)filter_bar)->icon_entry, GTK_STATE_NORMAL, NULL);
						e_search_bar_paint (search_bar);
						filter_bar->current_query = (FilterRule *)filter_bar->option_rules->pdata[item_id - filter_bar->option_base];
						if (filter_bar->config && filter_bar->current_query)
							filter_bar->config (filter_bar, filter_bar->current_query, item_id, "", filter_bar->config_data);
					}

					xmlFree (text);


					break;
				}

				node = node->next;
			}

			xmlFreeDoc (doc);
		} else {
			/* set default state */
			e_search_bar_set_item_id ((ESearchBar *) filter_bar, 0);
			e_search_bar_set_viewitem_id ((ESearchBar *) filter_bar, 0);
			e_search_bar_set_search_scope ((ESearchBar *) filter_bar, E_FILTERBAR_CURRENT_FOLDER_ID);
		}

		/* we don't want to run option_changed */
		filter_bar->setquery = TRUE;
		g_signal_emit_by_name (filter_bar, "search_activated", NULL);
		filter_bar->setquery = FALSE;

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
filter_bar_clear_rules (EFilterBar *filter_bar,
                        GPtrArray *rules)
{
	FilterRule *rule;
	gint ii;

	/* Clear out any data on old rules. */
	for (ii = 0; ii < rules->len; ii++) {
		FilterRule *rule = rules->pdata[ii];

		g_signal_handlers_disconnect_by_func (
			rule, G_CALLBACK (rule_changed), filter_bar);
		g_object_unref(rule);
	}

	g_ptr_array_set_size (rules, 0);
}

static void
filter_bar_dispose (GObject *object)
{
	EFilterBar *bar;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_FILTER_BAR (object));

	bar = E_FILTER_BAR (object);

	if (bar->context != NULL && bar->userrules != NULL)
		rule_context_save (bar->context, bar->userrules);

	if (bar->menu_rules != NULL) {
		filter_bar_clear_rules (bar, bar->menu_rules);
		filter_bar_clear_rules (bar, bar->option_rules);

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

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
class_init (EFilterBarClass *class)
{
	GObjectClass *object_class;
	ESearchBarClass *search_bar_class;
	GParamSpec *pspec;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (object_class);
	object_class->set_property = filter_bar_set_property;
	object_class->get_property = filter_bar_get_property;
	object_class->dispose = filter_bar_dispose;

	search_bar_class = E_SEARCH_BAR_CLASS (class);
	search_bar_class->set_menu = set_menu;
	search_bar_class->set_option = set_option;

	g_object_class_install_property (
		object_class,
		PROP_QUERY,
		g_param_spec_string (
			"query",
			NULL,
			NULL,
			NULL,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_STATE,
		g_param_spec_string (
			"state",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));
}

static void
filter_bar_init (EFilterBar *filter_bar)
{
	g_signal_connect (filter_bar, "menu_activated", G_CALLBACK (menubar_activated), NULL);
	g_signal_connect (filter_bar, "query_changed", G_CALLBACK (option_changed), NULL);
	g_signal_connect (filter_bar, "search_activated", G_CALLBACK (option_changed), NULL);

	filter_bar->menu_rules = g_ptr_array_new ();
	filter_bar->option_rules = g_ptr_array_new ();
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
			(GClassInitFunc) filter_bar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EFilterBar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) filter_bar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SEARCH_BAR, "EFilterBar", &type_info, 0);
	}

	return type;
}

EFilterBar *
e_filter_bar_new (RuleContext *context,
		  const gchar *systemrules,
		  const gchar *userrules,
		  EFilterBarConfigRule config,
		  gpointer data)
{
	EFilterBar *bar;

	bar = g_object_new (E_TYPE_FILTER_BAR, NULL);

	bar->context = g_object_ref (context);

	bar->config = config;
	bar->config_data = data;

	bar->systemrules = g_strdup (systemrules);
	bar->userrules = g_strdup (userrules);

	bar->all_account_search_vf = NULL;
	bar->account_search_vf = NULL;
 	bar->account_search_cancel = NULL;

	g_signal_connect (context, "changed", G_CALLBACK (context_changed), bar);
	g_signal_connect (context, "rule_removed", G_CALLBACK (context_rule_removed), bar);

	return bar;
}
