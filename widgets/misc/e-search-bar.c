/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-search-bar.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
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
#include <gnome.h>

#include "e-search-bar.h"
#include <gal/widgets/e-unicode.h>

static void e_search_bar_init		(ESearchBar		 *card);
static void e_search_bar_class_init	(ESearchBarClass	 *klass);
static void e_search_bar_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_search_bar_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_search_bar_destroy (GtkObject *object);

enum {
	QUERY_CHANGED,
	MENU_ACTIVATED,

	LAST_SIGNAL
};

static gint esb_signals [LAST_SIGNAL] = { 0, };

static GtkHBoxClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_OPTION_CHOICE,
	ARG_TEXT,
};

GtkType
e_search_bar_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info =
		{
			"ESearchBar",
			sizeof (ESearchBar),
			sizeof (ESearchBarClass),
			(GtkClassInitFunc) e_search_bar_class_init,
			(GtkObjectInitFunc) e_search_bar_init,
			/* reserved_1 */ NULL,
		       	/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (gtk_hbox_get_type (), &info);
	}

	return type;
}

static void
e_search_bar_class_init (ESearchBarClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS(klass);

	parent_class = gtk_type_class (gtk_hbox_get_type ());

	object_class->set_arg = e_search_bar_set_arg;
	object_class->get_arg = e_search_bar_get_arg;
	object_class->destroy = e_search_bar_destroy;

	gtk_object_add_arg_type ("ESearchBar::option_choice", GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE, ARG_OPTION_CHOICE);
	gtk_object_add_arg_type ("ESearchBar::text", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_TEXT);

	esb_signals [QUERY_CHANGED] =
		gtk_signal_new ("query_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ESearchBarClass, query_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	esb_signals [MENU_ACTIVATED] =
		gtk_signal_new ("menu_activated",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ESearchBarClass, menu_activated),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, esb_signals, LAST_SIGNAL);
}

static void
esb_query_changed(ESearchBar *esb)
{
	gtk_signal_emit(GTK_OBJECT (esb),
			esb_signals [QUERY_CHANGED]);
}

static void
esb_menu_activated(ESearchBar *esb, int item)
{
	gtk_signal_emit(GTK_OBJECT (esb),
			esb_signals [MENU_ACTIVATED],
			item);
}

static void
esb_menubar_activated(GtkWidget *widget, ESearchBar *esb)
{
	int id = GPOINTER_TO_INT(gtk_object_get_data (GTK_OBJECT (widget), "EsbMenuId"));

	esb_menu_activated(esb, id);
}

static void
esb_pack_menubar(ESearchBar *esb, ESearchBarItem *items)
{
	GtkWidget *menu, *menuitem;
	int i;

	menu = gtk_menu_new ();
	for (i = 0; items[i].id != -1; i++) {
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (_(items[i].text));

		gtk_menu_append (GTK_MENU (menu), item);

		gtk_object_set_data (GTK_OBJECT (item), "EsbMenuId", GINT_TO_POINTER(items[i].id));

		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (esb_menubar_activated),
				    esb);
	}
	gtk_widget_show_all (menu);

	menuitem = gtk_menu_item_new_with_label(_("Search"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);

	gtk_widget_show (menuitem);

	gtk_menu_bar_append (GTK_MENU_BAR(esb->menubar), menuitem);
	gtk_widget_set_sensitive (esb->menubar, TRUE);
}

static void
esb_option_activated(GtkWidget *widget, ESearchBar *esb)
{
	int id = GPOINTER_TO_INT(gtk_object_get_data (GTK_OBJECT (widget), "EsbChoiceId"));

	esb->option_choice = id;
	esb_query_changed(esb);
}

static void
esb_entry_activated(GtkWidget *widget, ESearchBar *esb)
{
	esb_query_changed(esb);
}

static void
esb_pack_option_menu(ESearchBar *esb, ESearchBarItem *items)
{
	GtkWidget *menu;
	int i;

	menu = gtk_menu_new ();
	for (i = 0; items[i].id != -1; i++) {
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (_(items[i].text));

		gtk_menu_append (GTK_MENU (menu), item);

		gtk_object_set_data (GTK_OBJECT (item), "EsbChoiceId", GINT_TO_POINTER(items[i].id));

		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (esb_option_activated),
				    esb);
	}
	gtk_widget_show_all (menu);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (esb->option), 
				  menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (esb->option), 0);
	gtk_widget_set_sensitive (esb->option, TRUE);
}

static void
e_search_bar_init (ESearchBar *esb)
{
	GtkWidget *spacer;

	gtk_box_set_spacing(GTK_BOX(esb), GNOME_PAD);

	esb->menubar = gtk_menu_bar_new();
	gtk_widget_show(esb->menubar);
	gtk_box_pack_start(GTK_BOX(esb), esb->menubar, FALSE, FALSE, 0);

	esb->option = gtk_option_menu_new();
	gtk_widget_show(esb->option);
	gtk_box_pack_start(GTK_BOX(esb), esb->option, FALSE, FALSE, 0);

	esb->entry = gtk_entry_new();
	gtk_signal_connect (GTK_OBJECT (esb->entry), "activate",
			    GTK_SIGNAL_FUNC (esb_entry_activated), esb);
	gtk_widget_show(esb->entry);
	gtk_box_pack_start(GTK_BOX(esb), esb->entry, TRUE, TRUE, 0);
	esb->option_choice = 0;

	spacer = gtk_drawing_area_new();
	gtk_widget_show(spacer);
	gtk_box_pack_start(GTK_BOX(esb), spacer, FALSE, FALSE, 0);
	gtk_widget_set_usize(spacer, 100, 1);
}

static void
e_search_bar_destroy (GtkObject *object)
{
	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

GtkWidget *
e_search_bar_new        (ESearchBarItem *menu_items,
			 ESearchBarItem *option_items)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_search_bar_get_type ()));
	esb_pack_menubar(E_SEARCH_BAR(widget), menu_items);
	esb_pack_option_menu(E_SEARCH_BAR(widget), option_items);
	return widget;
}

static void
e_search_bar_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESearchBar *esb = E_SEARCH_BAR(object);

	switch (arg_id) {
	case ARG_OPTION_CHOICE:
		GTK_VALUE_ENUM (*arg) = esb->option_choice;
		break;

	case ARG_TEXT:
		GTK_VALUE_STRING (*arg) = e_utf8_gtk_editable_get_text(GTK_EDITABLE(esb->entry));
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
e_search_bar_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESearchBar *esb = E_SEARCH_BAR(object);

	switch (arg_id) {
	case ARG_OPTION_CHOICE:
		esb->option_choice = GTK_VALUE_ENUM (*arg);
		gtk_option_menu_set_history (GTK_OPTION_MENU (esb->option), esb->option_choice);
		esb_query_changed(esb);
		break;

	case ARG_TEXT:
		e_utf8_gtk_editable_set_text(GTK_EDITABLE(esb->entry), GTK_VALUE_STRING (*arg));
		esb_query_changed(esb);
		break;

	default:
		break;
	}
}
