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

#include <config.h>

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>

#include "e-dropdown-button.h"
#include "e-filter-bar.h"
#include "filter/rule-editor.h"

#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-gui-utils.h>

#define d(x)

enum {
	LAST_SIGNAL
};

/*static gint esb_signals [LAST_SIGNAL] = { 0, };*/

static ESearchBarClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_QUERY,
};


/* Callbacks.  */

static void rule_changed (FilterRule *rule, gpointer user_data);


/* rule editor thingy */
static void
rule_editor_destroyed (GtkWidget *w, EFilterBar *efb)
{
	efb->save_dialogue = NULL;
	e_search_bar_set_menu_sensitive (E_SEARCH_BAR (efb), E_FILTERBAR_SAVE_ID, TRUE);
	gtk_widget_set_sensitive (E_SEARCH_BAR (efb)->entry, TRUE);
}

/* FIXME: need to update the popup menu to match any edited rules, sigh */
static void
full_rule_editor_clicked (GtkWidget *dialog, int button, void *data)
{
	EFilterBar *efb = data;
	
	switch (button) {
	case 0:
		rule_context_save (efb->context, efb->userrules);
	case 1:
	default:
		gnome_dialog_close (GNOME_DIALOG (dialog));
	case -1:
		break;
	}
}

static void
rule_editor_clicked (GtkWidget *dialog, int button, void *data)
{
	EFilterBar *efb = data;
	FilterRule *rule;
	
	switch (button) {
	case 0:
		rule = gtk_object_get_data (GTK_OBJECT (dialog), "rule");
		if (rule) {
			if (!filter_rule_validate (rule))
				return;
			
			rule_context_add_rule (efb->context, rule);
			/* FIXME: check return */
			rule_context_save (efb->context, efb->userrules);
		}
	case 1:
		gnome_dialog_close (GNOME_DIALOG (dialog));
	case -1:
		break;
	}
}

static void
rule_advanced_clicked (GtkWidget *dialog, int button, void *data)
{
	EFilterBar *efb = data;
	FilterRule *rule;
	
	switch (button) {
	case 0:			/* 'ok' */
	case 1:
		rule = gtk_object_get_data (GTK_OBJECT (dialog), "rule");
		if (rule) {
			efb->current_query = rule;
			gtk_object_ref (GTK_OBJECT (rule));
			gtk_signal_emit_by_name (GTK_OBJECT (efb), "query_changed");
		}
		if (button == 1)
			rule_editor_clicked (dialog, 0, data);
	case 2:
		gnome_dialog_close (GNOME_DIALOG (dialog));
	case -1:
		break;
	}
}

static void
do_advanced (ESearchBar *esb)
{
	EFilterBar *efb = (EFilterBar *)esb;
	
	d(printf("Advanced search!\n"));
		
	if (!efb->save_dialogue && !efb->setquery) {
		GtkWidget *w, *gd;
		FilterRule *rule;
		
		if (efb->current_query)
			rule = filter_rule_clone (efb->current_query);
		else
			rule = filter_rule_new ();
		
		w = filter_rule_get_widget (rule, efb->context);
		filter_rule_set_source (rule, FILTER_SOURCE_INCOMING);
		gd = gnome_dialog_new (_("Advanced Search"),
				       GNOME_STOCK_BUTTON_OK,
				       _("Save"),
				       GNOME_STOCK_BUTTON_CANCEL,
				       NULL);
		efb->save_dialogue = gd;
		gnome_dialog_set_default (GNOME_DIALOG (gd), 0);
		
		gtk_window_set_policy (GTK_WINDOW (gd), FALSE, TRUE, FALSE);
		gtk_window_set_default_size (GTK_WINDOW (gd), 600, 300);
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (gd)->vbox), w, TRUE, TRUE, 0);
		gtk_widget_show (gd);
		gtk_object_ref (GTK_OBJECT (rule));
		gtk_object_set_data_full (GTK_OBJECT (gd), "rule", rule, (GtkDestroyNotify)gtk_object_unref);
		gtk_signal_connect (GTK_OBJECT (gd), "clicked", rule_advanced_clicked, efb);
		gtk_signal_connect (GTK_OBJECT (gd), "destroy", rule_editor_destroyed, efb);
			
		e_search_bar_set_menu_sensitive (esb, E_FILTERBAR_SAVE_ID, FALSE);
		gtk_widget_set_sensitive (esb->entry, FALSE);
		
		gtk_widget_show (gd);
	}
}

static void
menubar_activated (ESearchBar *esb, int id, void *data)
{
	EFilterBar *efb = (EFilterBar *)esb;
	
	switch (id) {
	case E_FILTERBAR_EDIT_ID:
		if (!efb->save_dialogue) {
			GnomeDialog *gd;
			
			gd = (GnomeDialog *) rule_editor_new (efb->context, FILTER_SOURCE_INCOMING);
			efb->save_dialogue = (GtkWidget *) gd;
			gtk_window_set_title (GTK_WINDOW (gd), _("Search Editor"));
			gtk_signal_connect (GTK_OBJECT (gd), "clicked", full_rule_editor_clicked, efb);
			gtk_signal_connect (GTK_OBJECT (gd), "destroy", rule_editor_destroyed, efb);
			gtk_widget_show (GTK_WIDGET (gd));
		}
		break;
	case E_FILTERBAR_SAVE_ID:
		if (efb->current_query && !efb->save_dialogue) {
			GtkWidget *w;
			GtkWidget *gd;
			FilterRule *rule;
			char *name, *text;

			rule = filter_rule_clone (efb->current_query);
			text = e_search_bar_get_text(esb);
			name = g_strdup_printf("%s %s", rule->name, text&&text[0]?text:"''");
			g_free(text);
			filter_rule_set_name(rule, name);
			g_free(name);

			w = filter_rule_get_widget (rule, efb->context);
			filter_rule_set_source (rule, FILTER_SOURCE_INCOMING);
			gd = gnome_dialog_new (_("Save Search"), GNOME_STOCK_BUTTON_OK,
					       GNOME_STOCK_BUTTON_CANCEL, NULL);
			efb->save_dialogue = gd;
			gnome_dialog_set_default (GNOME_DIALOG (gd), 0);
			gtk_window_set_default_size (GTK_WINDOW (gd), 600, 300);
			gtk_window_set_policy (GTK_WINDOW (gd), FALSE, TRUE, FALSE);
			
			gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (gd)->vbox), w, TRUE, TRUE, 0);
			gtk_widget_show (gd);
			gtk_object_ref (GTK_OBJECT (rule));
			gtk_object_set_data_full (GTK_OBJECT (gd), "rule", rule, (GtkDestroyNotify)gtk_object_unref);
			gtk_signal_connect (GTK_OBJECT (gd), "clicked", rule_editor_clicked, efb);
			gtk_signal_connect (GTK_OBJECT (gd), "destroy", rule_editor_destroyed, efb);
			
			e_search_bar_set_menu_sensitive (esb, E_FILTERBAR_SAVE_ID, FALSE);
			gtk_widget_set_sensitive (esb->entry, FALSE);
			
			gtk_widget_show (gd);
		}
		
		d(printf("Save menu\n"));
		break;
	case E_FILTERBAR_ADVANCED_ID:
		do_advanced (esb);
		break;
	default:
		if (id >= efb->menu_base && id < efb->menu_base + efb->menu_rules->len) {
			GString *out = g_string_new ("");
			d(printf("Selected rule: %s\n", ((FilterRule *)efb->menu_rules->pdata[id - efb->menu_base])->name));
			filter_rule_build_code (efb->menu_rules->pdata[id - efb->menu_base], out);
			d(printf("query: '%s'\n", out->str));
			g_string_free (out, TRUE);
			
			efb->current_query = (FilterRule *)efb->menu_rules->pdata[id - efb->menu_base];
			efb->setquery = TRUE;

			e_search_bar_set_item_id (esb, E_FILTERBAR_ADVANCED_ID);
			
			gtk_widget_set_sensitive (esb->entry, FALSE);
		} else {
			gtk_widget_set_sensitive (esb->entry, TRUE);
			return;
		}
	}
	
	gtk_signal_emit_stop_by_name (GTK_OBJECT (esb), "menu_activated");
}

static void
option_changed (ESearchBar *esb, void *data)
{
	EFilterBar *efb = (EFilterBar *)esb;
	int id = e_search_bar_get_item_id (esb);
	char *query;
	
	d(printf("option changed, id = %d\n", id));
	
	switch (id) {
	case E_FILTERBAR_ADVANCED_ID:
		query = e_search_bar_get_text (esb);
		if (query && *query)
			do_advanced (esb);
		else if (!efb->setquery) 
			/* clearing advanced search, reset because search may 
			 * have rules not dependent on query text */
			e_search_bar_set_item_id (esb, 0);
		g_free (query);
		break;
	default:
		if (id >= efb->option_base && id < efb->option_base + efb->option_rules->len) {
			efb->current_query = (FilterRule *)efb->option_rules->pdata[id - efb->option_base];
			if (efb->config) {
				gtk_object_get (GTK_OBJECT (esb), "text", &query, NULL);
				efb->config (efb, efb->current_query, id, query, efb->config_data);
				g_free (query);
			}
			gtk_widget_set_sensitive (esb->entry, TRUE);
		} else {
			gtk_widget_set_sensitive (esb->entry, FALSE);
			efb->current_query = NULL;
		}
	}
	efb->setquery = FALSE;
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
			gtk_object_ref((GtkObject *)rule);
			gtk_signal_connect((GtkObject *)rule, "changed", rule_changed, efb);
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

		gtk_signal_disconnect_by_func((GtkObject *)rule, rule_changed, efb);
		gtk_object_unref((GtkObject *)rule);

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

	generate_menu (esb, efb->default_items);
}

static void
context_rule_removed (RuleContext *context, FilterRule *rule, gpointer user_data)
{
	/*gtk_signal_disconnect_by_func((GtkObject *)rule, rule_changed, efb);*/
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
impl_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EFilterBar *efb = E_FILTER_BAR(object);
	
	switch (arg_id) {
	case ARG_QUERY:
		if (efb->current_query) {
			GString *out = g_string_new ("");
			
			filter_rule_build_code (efb->current_query, out);
			GTK_VALUE_STRING (*arg) = out->str;
			g_string_free (out, FALSE);
		} else {
			GTK_VALUE_STRING (*arg) = NULL;
		}
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
		gtk_signal_disconnect_by_func((GtkObject *)rule, rule_changed, efb);
		gtk_object_unref((GtkObject *)rule);
	}
	g_ptr_array_set_size (rules, 0);
}

static void
destroy (GtkObject *object)
{
	EFilterBar *bar;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_FILTER_BAR (object));
	
	bar = E_FILTER_BAR (object);

	gtk_signal_disconnect_by_func(GTK_OBJECT (bar->context), context_changed, bar);
	gtk_signal_disconnect_by_func(GTK_OBJECT (bar->context), context_rule_removed, bar);

	clear_rules(bar, bar->menu_rules);
	clear_rules(bar, bar->option_rules);

	gtk_object_unref (GTK_OBJECT (bar->context));
	g_ptr_array_free (bar->menu_rules, TRUE);
	g_ptr_array_free (bar->option_rules, TRUE);
	g_free (bar->systemrules);
	g_free (bar->userrules);

	if (bar->default_items)
		free_items (bar->default_items);
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EFilterBarClass *klass)
{
	GtkObjectClass *object_class;
	ESearchBarClass *esb_class = (ESearchBarClass *)klass;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	parent_class = gtk_type_class (e_search_bar_get_type ());
	
	object_class->destroy = destroy;
	
	object_class->get_arg = impl_get_arg;
	
	esb_class->set_menu = set_menu;
	esb_class->set_option = set_option;
	
	gtk_object_add_arg_type ("EFilterBar::query", GTK_TYPE_STRING, GTK_ARG_READABLE, ARG_QUERY);
	
#if 0
	esb_signals [QUERY_CHANGED] =
		gtk_signal_new ("query_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EFilterBarClass, query_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	esb_signals [MENU_ACTIVATED] =
		gtk_signal_new ("menu_activated",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EFilterBarClass, menu_activated),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);
	
	gtk_object_class_add_signals (object_class, esb_signals, LAST_SIGNAL);
#endif
}

static void
init (EFilterBar *efb)
{
	gtk_signal_connect (GTK_OBJECT (efb), "menu_activated", menubar_activated, NULL);
	gtk_signal_connect (GTK_OBJECT (efb), "query_changed", option_changed, NULL);
	gtk_signal_connect (GTK_OBJECT (efb), "search_activated", option_changed, NULL);
	
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
	gtk_object_ref (GTK_OBJECT (context));
	
	bar->config = config;
	bar->config_data = data;
	
	bar->systemrules = g_strdup (systemrules);
	bar->userrules = g_strdup (userrules);
	
	e_search_bar_construct ((ESearchBar *)bar, &item, &item);
	
	gtk_signal_connect (GTK_OBJECT (context), "changed", context_changed, bar);
	gtk_signal_connect (GTK_OBJECT (context), "rule_removed", context_rule_removed, bar);
	
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
