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
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <gtk/gtk.h>

#include "e-dropdown-button.h"
#include "e-filter-bar.h"
#include "filter/rule-editor.h"

#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-gui-utils.h>


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

/* rule editor thingy */
static void rule_editor_destroyed(GtkWidget *w, EFilterBar *efb)
{
	efb->save_dialogue = NULL;
	e_search_bar_set_menu_sensitive((ESearchBar *)efb, E_FILTERBAR_SAVE_ID, TRUE);
}

/* FIXME: need to update the popup menu to match any edited rules, sigh */
static void full_rule_editor_clicked(GtkWidget *w, int button, void *data)
{
	EFilterBar *efb = data;

	switch (button) {
	case 0:
		rule_context_save(efb->context, efb->userrules);
	case 1:
	default:
		gnome_dialog_close((GnomeDialog *)w);
	case -1:
	}
}

static void rule_editor_clicked(GtkWidget *w, int button, void *data)
{
	EFilterBar *efb = data;
	ESearchBarItem item;
	FilterRule *rule;

	switch(button) {
	case 0:
		rule = gtk_object_get_data((GtkObject *)w, "rule");
		if (rule) {
			item.text = rule->name;
			item.id = efb->menu_base + efb->menu_rules->len;

			g_ptr_array_add(efb->menu_rules, rule);

			rule_context_add_rule(efb->context, rule);
			/* FIXME: check return */
			rule_context_save(efb->context, efb->userrules);
			e_search_bar_add_menu((ESearchBar *)efb, &item);
		}
	case 1:
		gnome_dialog_close((GnomeDialog *)w);
		break;
	case -1:
	}
}

static void rule_advanced_clicked(GtkWidget *w, int button, void *data)
{
	EFilterBar *efb = data;
	FilterRule *rule;

	switch(button) {
	case 0:			/* 'ok' */
	case 1:
		rule = gtk_object_get_data((GtkObject *)w, "rule");
		if (rule) {
			efb->current_query = rule;
			gtk_object_ref((GtkObject *)rule);
			gtk_signal_emit_by_name((GtkObject *)efb, "query_changed");
		}
		if (button == 1)
			rule_editor_clicked(w, 0, data);
	case 2:
		gnome_dialog_close((GnomeDialog *)w);
		break;
	case -1:
	}
}

static void
menubar_activated (ESearchBar *esb, int id, void *data)
{
	EFilterBar *efb = (EFilterBar *)esb;

	switch(id) {
	case E_FILTERBAR_RESET_ID:
		printf("Reset menu\n");
		efb->current_query = NULL;
		gtk_object_set((GtkObject *)esb, "option_choice", efb->option_base, NULL);
		gtk_object_set((GtkObject *)esb, "text", NULL, NULL);
		gtk_widget_set_sensitive(esb->entry, TRUE);
		break;
	case E_FILTERBAR_EDIT_ID:
		if (!efb->save_dialogue) {
			GnomeDialog *gd;

			gd = (GnomeDialog *)rule_editor_new(efb->context, FILTER_SOURCE_INCOMING);
			gtk_signal_connect((GtkObject *)gd, "clicked", full_rule_editor_clicked, efb);
			gtk_signal_connect((GtkObject *)gd, "destroy", rule_editor_destroyed, efb);
			gtk_widget_show((GtkWidget *)gd);
		}
		break;
	case E_FILTERBAR_SAVE_ID:
		if (efb->current_query && !efb->save_dialogue) {
			GtkWidget *w;
			GnomeDialog *gd;
			FilterRule *rule;

			rule = filter_rule_clone(efb->current_query, efb->context);

			w = filter_rule_get_widget(rule, efb->context);
			filter_rule_set_source(rule, FILTER_SOURCE_INCOMING);
			gd = (GnomeDialog *)gnome_dialog_new(_("Save Search"),
							     GNOME_STOCK_BUTTON_OK,
							     GNOME_STOCK_BUTTON_CANCEL,
							     NULL);
			efb->save_dialogue = (GtkWidget *)gd;
			gnome_dialog_set_default (gd, 0);

			gtk_window_set_policy(GTK_WINDOW(gd), FALSE, TRUE, FALSE);
			/*gtk_window_set_default_size (GTK_WINDOW (gd), 500, 500);*/
			gtk_box_pack_start((GtkBox *)gd->vbox, w, TRUE, TRUE, 0);
			gtk_widget_show((GtkWidget *)gd);
			gtk_object_ref((GtkObject *)rule);
			gtk_object_set_data_full((GtkObject *)gd, "rule", rule, (GtkDestroyNotify)gtk_object_unref);
			gtk_signal_connect((GtkObject *)gd, "clicked", rule_editor_clicked, efb);
			gtk_signal_connect((GtkObject *)gd, "destroy", rule_editor_destroyed, efb);

			e_search_bar_set_menu_sensitive(esb, E_FILTERBAR_SAVE_ID, FALSE);
			gtk_widget_set_sensitive(esb->entry, FALSE);

			gtk_widget_show((GtkWidget *)gd);
		}

		printf("Save menu\n");
		break;
	default:
		if (id >= efb->menu_base && id < efb->menu_base + efb->menu_rules->len) {
			GString *out = g_string_new("");
			printf("Selected rule: %s\n", ((FilterRule *)efb->menu_rules->pdata[id - efb->menu_base])->name);
			filter_rule_build_code(efb->menu_rules->pdata[id - efb->menu_base], out);
			printf("query: '%s'\n", out->str);
			g_string_free(out, 1);

			efb->current_query = (FilterRule *)efb->menu_rules->pdata[id - efb->menu_base];
			efb->setquery = TRUE;
			gtk_object_set((GtkObject *)esb, "option_choice", E_FILTERBAR_ADVANCED_ID, NULL);

			gtk_widget_set_sensitive(esb->entry, FALSE);
		} else {
			gtk_widget_set_sensitive(esb->entry, TRUE);
			return;
		}
	}

	gtk_signal_emit_stop_by_name((GtkObject *)esb, "menu_activated");
}

static void
option_changed (ESearchBar *esb, void *data)
{
	EFilterBar *efb = (EFilterBar *)esb;
	int id = esb->option_choice;
	char *query;

	printf("option changed, id = %d\n", id);

	switch(id) {
	case E_FILTERBAR_ADVANCED_ID: {
		printf("Advanced search!\n");

		if (!efb->save_dialogue && !efb->setquery) {
			GtkWidget *w;
			GnomeDialog *gd;
			FilterRule *rule;

			if (efb->current_query)
				rule = filter_rule_clone(efb->current_query, efb->context);
			else
				rule = filter_rule_new();

			w = filter_rule_get_widget(rule, efb->context);
			filter_rule_set_source(rule, FILTER_SOURCE_INCOMING);
			gd = (GnomeDialog *)gnome_dialog_new(_("Advanced Search"),
							     GNOME_STOCK_BUTTON_OK,
							     _("Save"),
							     GNOME_STOCK_BUTTON_CANCEL,
							     NULL);
			efb->save_dialogue = (GtkWidget *)gd;
			gnome_dialog_set_default (gd, 0);
		
			gtk_window_set_policy(GTK_WINDOW(gd), FALSE, TRUE, FALSE);
			/*gtk_window_set_default_size (GTK_WINDOW (gd), 500, 500);*/
			gtk_box_pack_start((GtkBox *)gd->vbox, w, TRUE, TRUE, 0);
			gtk_widget_show((GtkWidget *)gd);
			gtk_object_ref((GtkObject *)rule);
			gtk_object_set_data_full((GtkObject *)gd, "rule", rule, (GtkDestroyNotify)gtk_object_unref);
			gtk_signal_connect((GtkObject *)gd, "clicked", rule_advanced_clicked, efb);
			gtk_signal_connect((GtkObject *)gd, "destroy", rule_editor_destroyed, efb);
		
			e_search_bar_set_menu_sensitive(esb, E_FILTERBAR_SAVE_ID, FALSE);
			gtk_widget_set_sensitive(esb->entry, FALSE);
		
			gtk_widget_show((GtkWidget *)gd);
		}
	}	break;
	default:
		if (id >= efb->option_base && id < efb->option_base + efb->option_rules->len) {
			efb->current_query = (FilterRule *)efb->option_rules->pdata[id - efb->option_base];
			if (efb->config) {
				gtk_object_get((GtkObject *)esb, "text", &query, NULL);
				efb->config(efb, efb->current_query, id, query, efb->config_data);
				g_free(query);
			}
			gtk_widget_set_sensitive(esb->entry, TRUE);
		} else {
			gtk_widget_set_sensitive(esb->entry, FALSE);
			efb->current_query = NULL;
		}
	}
	efb->setquery = FALSE;
}

static GArray *build_items(ESearchBar *esb, ESearchBarItem *items, int type, int *start, GPtrArray *rules)
{
	FilterRule *rule = NULL;
	EFilterBar *efb = (EFilterBar *)esb;
	int id = 0, i;
	GArray *menu = g_array_new(FALSE, FALSE, sizeof(ESearchBarItem));
	ESearchBarItem item;
	char *source;

	/* find a unique starting point for the id's of our items */
	for (i=0;items[i].id != -1;i++) {
		if (items[i].id >= id)
			id = items[i].id+1;
	}

	/* add the user menus */
	g_array_append_vals(menu, items, i);

	*start = id;

	if (type == 0) {
		/* and add ours */
		item.id = 0;
		item.text = NULL;
		g_array_append_vals(menu, &item, 1);
		source = FILTER_SOURCE_INCOMING;
	} else {
		source = FILTER_SOURCE_DEMAND;
	}

	while ( (rule = rule_context_next_rule(efb->context, rule, source)) ) {
		item.id = id++;
		item.text = rule->name;
		g_array_append_vals(menu, &item, 1);
		g_ptr_array_add(rules, rule);
	}

	/* always add on the advanced menu */
	if (type == 1) {
		item.id = E_FILTERBAR_ADVANCED_ID;
		item.text = _("Advanced ...");
		g_array_append_vals(menu, &item, 1);
	}

	item.id = -1;
	item.text = NULL;
	g_array_append_vals(menu, &item, 1);

	return menu;
}

/* Virtual methods */
static void
set_menu(ESearchBar *esb, ESearchBarItem *items)
{
	GArray *menu;
	EFilterBar *efb = (EFilterBar *)esb;

	g_ptr_array_set_size(efb->menu_rules, 0);
	menu = build_items(esb, items, 0, &efb->menu_base, efb->menu_rules);
	((ESearchBarClass *)parent_class)->set_menu(esb, (ESearchBarItem *)menu->data);
	g_array_free(menu, TRUE);
}

static void
set_option(ESearchBar *esb, ESearchBarItem *items)
{
	GArray *menu;
	EFilterBar *efb = (EFilterBar *)esb;

	g_ptr_array_set_size(efb->option_rules, 0);
	menu = build_items(esb, items, 1, &efb->option_base, efb->option_rules);
	((ESearchBarClass *)parent_class)->set_option(esb, (ESearchBarItem *)menu->data);
	g_array_free(menu, TRUE);
}


/* GtkObject methods.  */

static void
impl_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EFilterBar *efb = E_FILTER_BAR(object);

	switch (arg_id) {
	case ARG_QUERY:
		if (efb->current_query) {
			GString *out = g_string_new("");

			filter_rule_build_code(efb->current_query, out);
			GTK_VALUE_STRING(*arg) = out->str;
			g_string_free(out, FALSE);
		} else {
			GTK_VALUE_STRING(*arg) = NULL;
		}
		break;
	}
}


static void
class_init (EFilterBarClass *klass)
{
	GtkObjectClass *object_class;
	ESearchBarClass *esb_class = (ESearchBarClass *)klass;

	object_class = GTK_OBJECT_CLASS(klass);

	parent_class = gtk_type_class(e_search_bar_get_type());

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
	gtk_signal_connect((GtkObject *)efb, "menu_activated", menubar_activated, NULL);
	gtk_signal_connect((GtkObject *)efb, "query_changed", option_changed, NULL);

	efb->menu_rules = g_ptr_array_new();
	efb->option_rules = g_ptr_array_new();
}


/* Object construction.  */

EFilterBar *e_filter_bar_new        (RuleContext *context, const char *systemrules, const char *userrules, EFilterBarConfigRule config, void *data)
{
	EFilterBar *bar;
	ESearchBarItem item = { NULL, -1 };

	bar = gtk_type_new(e_filter_bar_get_type());

	bar->context = context;
	gtk_object_ref((GtkObject *)context);
	bar->systemrules = g_strdup(systemrules);
	bar->userrules = g_strdup(userrules);
	rule_context_load(context, systemrules, userrules);

	bar->config = config;
	bar->config_data = data;

	e_search_bar_construct((ESearchBar *)bar, &item, &item);

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

