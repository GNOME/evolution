/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-search-bar.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Authors:
 *  Chris Lahey <clahey@ximian.com>
 *  Ettore Perazzoli <ettore@ximian.com>
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
#include "e-dropdown-button.h"

#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-gui-utils.h>


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


/* Signals.  */

static void
emit_query_changed (ESearchBar *esb)
{
	gtk_signal_emit(GTK_OBJECT (esb),
			esb_signals [QUERY_CHANGED]);
}

static void
emit_menu_activated (ESearchBar *esb, int item)
{
	gtk_signal_emit(GTK_OBJECT (esb),
			esb_signals [MENU_ACTIVATED],
			item);
}


/* Callbacks.  */

static void
menubar_activated_cb (GtkWidget *widget, ESearchBar *esb)
{
	int id;

	id = GPOINTER_TO_INT(gtk_object_get_data (GTK_OBJECT (widget), "EsbMenuId"));

	emit_menu_activated(esb, id);
}

static void
option_activated_cb (GtkWidget *widget,
		     ESearchBar *esb)
{
	int id;

	id = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (widget), "EsbChoiceId"));

	esb->option_choice = id;
	emit_query_changed (esb);
}

static void
entry_activated_cb (GtkWidget *widget,
		     ESearchBar *esb)
{
	emit_query_changed (esb);
}


/* Widgetry creation.  */

static void
add_dropdown (ESearchBar *esb,
	      ESearchBarItem *items)
{
	GtkWidget *menu;
	GtkWidget *event_box;
	int i;

	menu = gtk_menu_new ();
	for (i = 0; items[i].id != -1; i++) {
		GtkWidget *item;

		if (items[i].text)
			item = gtk_menu_item_new_with_label (_(items[i].text));
		else
			item = gtk_menu_item_new();

		gtk_menu_append (GTK_MENU (menu), item);

		gtk_object_set_data (GTK_OBJECT (item), "EsbMenuId", GINT_TO_POINTER(items[i].id));

		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (menubar_activated_cb),
				    esb);
	}
	gtk_widget_show_all (menu);

	esb->dropdown = e_dropdown_button_new (_("Sear_ch"), GTK_MENU (menu));
	GTK_WIDGET_UNSET_FLAGS (esb->dropdown, GTK_CAN_FOCUS);
	gtk_widget_show (esb->dropdown);

	/* So, GtkOptionMenu is stupid; it adds a 1-pixel-wide empty border
           around the button for no reason.  So we add a 1-pixel-wide border
           around the button as well, by using an event box.  */
	event_box = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (event_box), 1);
	gtk_container_add (GTK_CONTAINER (event_box), esb->dropdown);
	gtk_widget_show (event_box);

	gtk_box_pack_start(GTK_BOX(esb), event_box, FALSE, FALSE, 0);
}

static void
add_option(ESearchBar *esb, ESearchBarItem *items)
{
	GtkWidget *menu;
	GtkRequisition dropdown_requisition;
	GtkRequisition option_requisition;
	int i;

	esb->option = gtk_option_menu_new();
	gtk_widget_show(esb->option);
	gtk_box_pack_start(GTK_BOX(esb), esb->option, FALSE, FALSE, 0);

	menu = gtk_menu_new ();
	for (i = 0; items[i].id != -1; i++) {
		GtkWidget *item;

		if (items[i].text)
			item = gtk_menu_item_new_with_label (_(items[i].text));
		else
			item = gtk_menu_item_new();

		gtk_menu_append (GTK_MENU (menu), item);

		gtk_object_set_data (GTK_OBJECT (item), "EsbChoiceId", GINT_TO_POINTER(items[i].id));

		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (option_activated_cb),
				    esb);
	}
	gtk_widget_show_all (menu);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (esb->option), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (esb->option), 0);

	gtk_widget_set_sensitive (esb->option, TRUE);

	/* Set the minimum height of this widget to that of the dropdown
           button, for a better look.  */
	g_assert (esb->dropdown != NULL);

	gtk_widget_size_request (esb->dropdown, &dropdown_requisition);
	gtk_widget_size_request (esb->option, &option_requisition);

	gtk_container_set_border_width (GTK_CONTAINER (esb->dropdown), GTK_CONTAINER (esb->option)->border_width);
}

static void
add_entry (ESearchBar *esb)
{
	esb->entry = gtk_entry_new();
	gtk_signal_connect (GTK_OBJECT (esb->entry), "activate",
			    GTK_SIGNAL_FUNC (entry_activated_cb), esb);
	gtk_widget_show(esb->entry);
	gtk_box_pack_start(GTK_BOX(esb), esb->entry, TRUE, TRUE, 0);
}

static void
add_spacer (ESearchBar *esb)
{
	GtkWidget *spacer;

	spacer = gtk_drawing_area_new();
	gtk_widget_show(spacer);
	gtk_box_pack_start(GTK_BOX(esb), spacer, FALSE, FALSE, 0);

	gtk_widget_set_usize(spacer, 19, 1);
}


/* GtkObject methods.  */

static void
impl_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
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
impl_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESearchBar *esb = E_SEARCH_BAR(object);

	switch (arg_id) {
	case ARG_OPTION_CHOICE:
		esb->option_choice = GTK_VALUE_ENUM (*arg);
		gtk_option_menu_set_history (GTK_OPTION_MENU (esb->option), esb->option_choice);
		emit_query_changed (esb);
		break;

	case ARG_TEXT:
		e_utf8_gtk_editable_set_text(GTK_EDITABLE(esb->entry), GTK_VALUE_STRING (*arg));
		emit_query_changed (esb);
		break;

	default:
		break;
	}
}

static void
impl_destroy (GtkObject *object)
{
	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		GTK_OBJECT_CLASS(parent_class)->destroy (object);
}


static void
class_init (ESearchBarClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS(klass);

	parent_class = gtk_type_class (gtk_hbox_get_type ());

	object_class->set_arg = impl_set_arg;
	object_class->get_arg = impl_get_arg;
	object_class->destroy = impl_destroy;

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
init (ESearchBar *esb)
{
	esb->dropdown      = NULL;
	esb->option        = NULL;
	esb->entry         = NULL;

	esb->option_choice = 0;
}


/* Object construction.  */

void
e_search_bar_construct (ESearchBar *search_bar,
			ESearchBarItem *menu_items,
			ESearchBarItem *option_items)
{
	g_return_if_fail (search_bar != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	g_return_if_fail (menu_items != NULL);
	g_return_if_fail (option_items != NULL);
	
	gtk_box_set_spacing (GTK_BOX (search_bar), 1);

	add_dropdown (search_bar, menu_items);

	add_option (search_bar, option_items);

	add_entry (search_bar);

	add_spacer (search_bar);
}

GtkWidget *
e_search_bar_new (ESearchBarItem *menu_items,
		  ESearchBarItem *option_items)
{
	GtkWidget *widget;

	g_return_val_if_fail (menu_items != NULL, NULL);
	g_return_val_if_fail (option_items != NULL, NULL);
	
	widget = GTK_WIDGET (gtk_type_new (e_search_bar_get_type ()));

	e_search_bar_construct (E_SEARCH_BAR (widget), menu_items, option_items);

	return widget;
}

GtkType
e_search_bar_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info = {
			"ESearchBar",
			sizeof (ESearchBar),
			sizeof (ESearchBarClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
		       	/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (gtk_hbox_get_type (), &info);
	}

	return type;
}

