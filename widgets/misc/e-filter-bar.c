/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-search-bar.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors:
 *  Michael Zucchi <notzed@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include <libgnome/gnome-i18n.h>

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
	PROP_STATE,
};


/* Callbacks.  */

static void rule_changed (FilterRule *rule, gpointer user_data);


/* rule editor thingy */
static void
rule_editor_destroyed (EFilterBar *efb, GObject *deadbeef)
{
	efb->save_dialog = NULL;
	e_search_bar_set_menu_sensitive (E_SEARCH_BAR (efb), E_FILTERBAR_SAVE_ID, TRUE);
	gtk_widget_set_sensitive (E_SEARCH_BAR (efb)->entry, TRUE);
}

/* FIXME: need to update the popup menu to match any edited rules, sigh */
static void
full_rule_editor_response (GtkWidget *dialog, int response, void *data)
{
	EFilterBar *efb = data;
	
	if (response == GTK_RESPONSE_OK)
		rule_context_save (efb->context, efb->userrules);
	
	gtk_widget_destroy (dialog);
}

static void
rule_editor_response (GtkWidget *dialog, int response, void *data)
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
rule_advanced_response (GtkWidget *dialog, int response, void *data)
{
	EFilterBar *efb = data;
	FilterRule *rule;
	
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
		rule = g_object_get_data ((GObject *) dialog, "rule");
		if (rule) {
			if (!filter_rule_validate (rule))
				return;
			
			efb->current_query = rule;
			g_object_ref (rule);
			g_signal_emit_by_name (efb, "query_changed");
			
			if (response == GTK_RESPONSE_APPLY) {
				if (!rule_context_find_rule (efb->context, rule->name, rule->source))
					rule_context_add_rule (efb->context, rule);
				/* FIXME: check return */
				rule_context_save (efb->context, efb->userrules);
			}
		}
	}
	
	if (response != GTK_RESPONSE_APPLY)
		gtk_widget_destroy (dialog);
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
		else
			rule = filter_rule_new ();
		
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
		
		g_signal_connect (dialog, "response", G_CALLBACK (rule_advanced_response), efb);
		g_object_weak_ref ((GObject *) dialog, (GWeakNotify) rule_editor_destroyed, efb);
		
		e_search_bar_set_menu_sensitive (esb, E_FILTERBAR_SAVE_ID, FALSE);
		gtk_widget_set_sensitive (esb->entry, FALSE);
		
		gtk_widget_show (dialog);
	}
}

static void
menubar_activated (ESearchBar *esb, int id, void *data)
{
	EFilterBar *efb = (EFilterBar *)esb;
	GtkWidget *dialog, *w;
	
	d(printf ("menubar activated!\n"));
	
	switch (id) {
	case E_FILTERBAR_EDIT_ID:
		if (!efb->save_dialog) {
			efb->save_dialog = dialog = (GtkWidget *) rule_editor_new (efb->context, FILTER_SOURCE_INCOMING, _("_Searches"));
			
			gtk_window_set_title (GTK_WINDOW (dialog), _("Search Editor"));
			g_signal_connect (dialog, "response", G_CALLBACK (full_rule_editor_response), efb);
			g_object_weak_ref ((GObject *) dialog, (GWeakNotify) rule_editor_destroyed, efb);
			gtk_widget_show (dialog);
		}
		break;
	case E_FILTERBAR_SAVE_ID:
		if (efb->current_query && !efb->save_dialog) {
			FilterRule *rule;
			char *name, *text;
			
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
			
			e_search_bar_set_menu_sensitive (esb, E_FILTERBAR_SAVE_ID, FALSE);
			gtk_widget_set_sensitive (esb->entry, FALSE);
			
			gtk_widget_show (dialog);
		}
		
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
			
			gtk_widget_set_sensitive (esb->entry, FALSE);
		} else {
			gtk_widget_set_sensitive (esb->entry, TRUE);
			return;
		}
	}
	
	g_signal_stop_emission_by_name (esb, "menu_activated");
}

static void
option_changed (ESearchBar *esb, void *data)
{
	EFilterBar *efb = (EFilterBar *)esb;
	int id = e_search_bar_get_item_id (esb);
	char *query;
	
	d(printf("option changed, id = %d, setquery = %s\n", id, efb->setquery ? "true" : "false"));
	
	if (efb->setquery)
		return;
	
	switch (id) {
	case E_FILTERBAR_ADVANCED_ID:
		d(printf ("do_advanced\n"));
		do_advanced (esb);
		break;
	default:
		if (id >= efb->option_base && id < efb->option_base + efb->option_rules->len) {
			efb->current_query = (FilterRule *)efb->option_rules->pdata[id - efb->option_base];
			if (efb->config && efb->current_query) {
				g_object_get (G_OBJECT (esb), "text", &query, NULL);
				efb->config (efb, efb->current_query, id, query, efb->config_data);
				g_free (query);
			}
			gtk_widget_set_sensitive (esb->entry, TRUE);
		} else {
			gtk_widget_set_sensitive (esb->entry, id == E_SEARCHBAR_CLEAR_ID);
			efb->current_query = NULL;
		}
	}
}

static void
dup_item_no_subitems (ESearchBarItem *dest,
		      const ESearchBarItem *src)
{
	g_assert (src->subitems == NULL);
	
	dest->id = src->id;
	dest->text = g_strdup (src->text);
	dest->subitems = NULL;
}

static GArray *
build_items (ESearchBar *esb, ESearchBarItem *items, int type, int *start, GPtrArray *rules)
{
	FilterRule *rule = NULL;
	EFilterBar *efb = (EFilterBar *)esb;
	int id = 0, i;
	GArray *menu = g_array_new (FALSE, FALSE, sizeof (ESearchBarItem));
	ESearchBarItem item;
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
		ESearchBarItem dup_item;

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
			item.subitems = NULL;
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

		item.subitems = NULL;
		g_array_append_vals (menu, &item, 1);

		if (g_slist_find(gtksux, rule) == NULL) {
			g_object_ref (rule);
			g_signal_connect (rule, "changed", G_CALLBACK (rule_changed), efb);
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

		g_signal_handlers_disconnect_by_func (rule, G_CALLBACK (rule_changed), efb);
		g_object_unref (rule);

		g_slist_free_1(gtksux);
		gtksux = next;
	}
	
	/* always add on the advanced menu */
	if (type == 1) {
		ESearchBarItem items[2] = { E_FILTERBAR_SEPARATOR, E_FILTERBAR_ADVANCED };
		ESearchBarItem dup_items[2];

		dup_item_no_subitems (&dup_items[0], &items[0]);
		dup_item_no_subitems (&dup_items[1], &items[1]);
		g_array_append_vals (menu, &dup_items, 2);
	}
	
	item.id = -1;
	item.text = NULL;
	item.subitems = NULL;
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

		g_assert (item->subitems == NULL);
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

static ESearchBarSubitem *
copy_subitems (ESearchBarSubitem *subitems)
{
	ESearchBarSubitem *items;
	int i, num;
	
	for (num = 0; subitems[num].id != -1; num++)
		;
	
	items = g_new (ESearchBarSubitem, num + 1);
	for (i = 0; i < num + 1; i++) {
		items[i].text = g_strdup (subitems[i].text);
		items[i].id = subitems[i].id;
		items[i].translate = subitems[i].translate;
	}
	
	return items;
}

static void
free_items (ESearchBarItem *items)
{
	int i, j;
	
	for (i = 0; items[i].id != -1; i++) {
		g_free (items[i].text);
		if (items[i].subitems) {
			for (j = 0; items[i].subitems[j].id != -1; j++)
				g_free (items[i].subitems[j].text);
			
			g_free (items[i].subitems);
		}
	}
	
	g_free (items);
}

/* Virtual methods */
static void
set_menu (ESearchBar *esb, ESearchBarItem *items)
{
	EFilterBar *efb = E_FILTER_BAR (esb);
	ESearchBarItem *default_items;
	int i, num;
	
	if (efb->default_items)
		free_items (efb->default_items);
	
	for (num = 0; items[num].id != -1; num++)
		;
	
	default_items = g_new (ESearchBarItem, num + 1);
	for (i = 0; i < num + 1; i++) {
		default_items[i].text = g_strdup (items[i].text);
		default_items[i].id = items[i].id;
		if (items[i].subitems)
			default_items[i].subitems = copy_subitems (items[i].subitems);
		else
			default_items[i].subitems = NULL;
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
	
	switch (property_id) {
	case PROP_QUERY:
		if (efb->current_query) {
			GString *out = g_string_new ("");
			
			filter_rule_build_code (efb->current_query, out);
			g_value_set_string_take_ownership (value, out->str);
			g_string_free (out, FALSE);
		} else {
			g_value_set_string (value, NULL);
		}
		break;
	case PROP_STATE: {
		/* FIXME: we should have ESearchBar save its own state to the xmlDocPtr */
		char *xmlbuf, *text, buf[12];
		int subitem_id, item_id, n;
		xmlNodePtr root, node;
		xmlDocPtr doc;
		
		item_id = e_search_bar_get_item_id ((ESearchBar *) efb);
		
		doc = xmlNewDoc ("1.0");
		root = xmlNewDocNode (doc, NULL, "state", NULL);
		xmlDocSetRootElement (doc, root);
		
		if (item_id == E_FILTERBAR_ADVANCED_ID) {
			/* advanced query, save the filterbar state */
			node = xmlNewChild (root, NULL, "filter-bar", NULL);
			xmlAddChild (node, filter_rule_xml_encode (efb->current_query));
		} else {
			/* simple query, save the searchbar state */
			text = e_search_bar_get_text ((ESearchBar *) efb);
			subitem_id = e_search_bar_get_subitem_id ((ESearchBar *) efb);
			
			node = xmlNewChild (root, NULL, "search-bar", NULL);
			xmlSetProp (node, "text", text ? text : "");
			sprintf (buf, "%d", item_id);
			xmlSetProp (node, "item_id", buf);
			sprintf (buf, "%d", subitem_id);
			xmlSetProp (node, "subitem_id", buf);
			g_free (text);
		}
		
		xmlDocDumpMemory (doc, (xmlChar **) &xmlbuf, &n);
		xmlFreeDoc (doc);
		
		/* remap to glib memory */
		text = g_malloc (n + 1);
		memcpy (text, xmlbuf, n);
		text[n] = '\0';
		xmlFree (xmlbuf);
		
		g_value_set_string_take_ownership (value, text);
		
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
	
	if ((buf = xmlGetProp (node, prop))) {
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
	xmlNodePtr root, node;
	const char *state;
	xmlDocPtr doc;
	
	
	
	switch (property_id) {
	case PROP_STATE:
		if ((state = g_value_get_string (value))) {
			if (!(doc = xmlParseDoc ((char *) state)))
				return;
			
			root = doc->children;
			if (strcmp (root->name, "state") != 0) {
				xmlFreeDoc (doc);
				return;
			}
			
			node = root->children;
			while (node != NULL) {
				if (!strcmp (node->name, "filter-bar")) {
					FilterRule *rule = NULL;
					
					if ((node = node->children)) {
						rule = filter_rule_new ();
						if (filter_rule_xml_decode (rule, node, efb->context) != 0) {
							g_object_unref (rule);
							rule = NULL;
						} else {
							g_object_set_data_full (object, "rule", rule, (GDestroyNotify) g_object_unref);
						}
					}
					
					efb->current_query = rule;
					
					efb->setquery = TRUE;
					e_search_bar_set_item_id ((ESearchBar *) efb, E_FILTERBAR_ADVANCED_ID);
					efb->setquery = FALSE;
					
					break;
				} else if (!strcmp (node->name, "search-bar")) {
					int subitem_id, item_id;
					char *text;
					
					/* set the text first (it doesn't emit a signal) */
					text = xmlGetProp (node, "text");
					e_search_bar_set_text ((ESearchBar *) efb, text);
					xmlFree (text);
					
					/* now set the item_id and subitem_id */
					item_id = xml_get_prop_int (node, "item_id");
					subitem_id = xml_get_prop_int (node, "subitem_id");
					
					if (subitem_id >= 0)
						e_search_bar_set_ids ((ESearchBar *) efb, item_id, subitem_id);
					else
						e_search_bar_set_item_id ((ESearchBar *) efb, item_id);
					
					break;
				}
				
				node = node->next;
			}
			
			xmlFreeDoc (doc);
		} else {
			/* set default state */
			e_search_bar_set_text ((ESearchBar *) efb, "");
			e_search_bar_set_item_id ((ESearchBar *) efb, 0);
		}
		
		/* we don't want to run option_changed */
		efb->setquery = TRUE;
		g_signal_emit_by_name (efb, "search-activated", NULL);
		efb->setquery = FALSE;
		
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;	
	}
}

static void clear_rules(EFilterBar *efb, GPtrArray *rules)
{
	int i;
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
	
	/*gtk_object_add_arg_type ("EFilterBar::query", GTK_TYPE_STRING, GTK_ARG_READABLE, ARG_QUERY);*/
	
#if 0
	esb_signals [QUERY_CHANGED] =
		gtk_signal_new ("query_changed",
				GTK_RUN_LAST,
				object_class->type,
				G_STRUCT_OFFSET (EFilterBarClass, query_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	esb_signals [MENU_ACTIVATED] =
		gtk_signal_new ("menu_activated",
				GTK_RUN_LAST,
				object_class->type,
				G_STRUCT_OFFSET (EFilterBarClass, menu_activated),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);
	
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
		  const char *systemrules,
		  const char *userrules,
		  EFilterBarConfigRule config,
		  void *data)
{
	EFilterBar *bar;
	ESearchBarItem item = { NULL, -1, NULL };
	
	bar = gtk_type_new (e_filter_bar_get_type ());

	bar->context = context;
	g_object_ref (context);
	
	bar->config = config;
	bar->config_data = data;
	
	bar->systemrules = g_strdup (systemrules);
	bar->userrules = g_strdup (userrules);
	
	e_search_bar_construct ((ESearchBar *)bar, &item, &item);
	
	g_signal_connect (context, "changed", G_CALLBACK (context_changed), bar);
	g_signal_connect (context, "rule_removed", G_CALLBACK (context_rule_removed), bar);
	
	return bar;
}

GtkType
e_filter_bar_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		static const GtkTypeInfo info = {
			"EFilterBar",
			sizeof (EFilterBar),
			sizeof (EFilterBarClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
		       	/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (e_search_bar_get_type (), &info);
	}
	
	return type;
}
