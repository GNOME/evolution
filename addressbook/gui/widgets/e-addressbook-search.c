/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-addressbook-search.c
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

#include "e-addressbook-search.h"
#include <gal/widgets/e-unicode.h>

static void e_addressbook_search_init		(EAddressbookSearch		 *card);
static void e_addressbook_search_class_init	(EAddressbookSearchClass	 *klass);
static void e_addressbook_search_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_addressbook_search_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_addressbook_search_destroy (GtkObject *object);

enum {
	QUERY_CHANGED,

	LAST_SIGNAL
};

static gint eas_signals [LAST_SIGNAL] = { 0, };

static GtkHBoxClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_OPTION_CHOICE,
	ARG_TEXT,
};

GtkType
e_addressbook_search_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info =
		{
			"EAddressbookSearch",
			sizeof (EAddressbookSearch),
			sizeof (EAddressbookSearchClass),
			(GtkClassInitFunc) e_addressbook_search_class_init,
			(GtkObjectInitFunc) e_addressbook_search_init,
			/* reserved_1 */ NULL,
		       	/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (gtk_hbox_get_type (), &info);
	}

	return type;
}

static void
e_addressbook_search_class_init (EAddressbookSearchClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS(klass);

	parent_class = gtk_type_class (gtk_hbox_get_type ());

	object_class->set_arg = e_addressbook_search_set_arg;
	object_class->get_arg = e_addressbook_search_get_arg;
	object_class->destroy = e_addressbook_search_destroy;

	gtk_object_add_arg_type ("EAddressbookSearch::option_choice", GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE, ARG_OPTION_CHOICE);
	gtk_object_add_arg_type ("EAddressbookSearch::text", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_TEXT);

	eas_signals [QUERY_CHANGED] =
		gtk_signal_new ("query_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookSearchClass, query_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, eas_signals, LAST_SIGNAL);
}

static void
eas_query_changed(EAddressbookSearch *eas)
{
	gtk_signal_emit(GTK_OBJECT (eas),
			eas_signals [QUERY_CHANGED]);
}


typedef enum {
	EAS_ANY = 0,
	EAS_FULL_NAME = 1,
	EAS_EMAIL = 2,
} EasChoiceId;


typedef struct {
	char *text;
	char *name;
	int id;
} EasChoice;

static EasChoice eas_choices[] = {
	{ N_("Any field contains"), "x-evolution-any-field", EAS_ANY },
	{ N_("Name contains"), "full_name", EAS_FULL_NAME },
	{ N_("Email contains"), "email", EAS_EMAIL },
	{ NULL, NULL, 0 }
};

static void
eas_option_activated(GtkWidget *widget, EAddressbookSearch *eas)
{
	int id = GPOINTER_TO_INT(gtk_object_get_data (GTK_OBJECT (widget), "EasChoiceId"));

	eas->option_choice = id;
	eas_query_changed(eas);
}

static void
eas_entry_activated(GtkWidget *widget, EAddressbookSearch *eas)
{
	eas_query_changed(eas);
}

static void
eas_pack_option_menu(EAddressbookSearch *eas)
{
	GtkWidget *menu, *item, *firstitem = NULL;
	int i;

	menu = gtk_menu_new ();
	for (i = 0; eas_choices[i].name; i++) {

		item = gtk_menu_item_new_with_label (_(eas_choices[i].text));
		if (!firstitem)
			firstitem = item;

		gtk_menu_append (GTK_MENU (menu), item);

		gtk_object_set_data (GTK_OBJECT (item), "EasChoiceId", GINT_TO_POINTER(i));

		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (eas_option_activated),
				    eas);
	}
	gtk_widget_show_all (menu);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (eas->option), 
				  menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (eas->option), 0);
	gtk_widget_set_sensitive (eas->option, TRUE);
}

static void
e_addressbook_search_init (EAddressbookSearch *eas)
{
	gtk_box_set_spacing(GTK_BOX(eas), GNOME_PAD);

	eas->option = gtk_option_menu_new();
	eas_pack_option_menu(eas);
	gtk_widget_show(eas->option);
	gtk_box_pack_start(GTK_BOX(eas), eas->option, FALSE, FALSE, 0);

	eas->entry = gtk_entry_new();
	gtk_signal_connect (GTK_OBJECT (eas->entry), "activate",
			    GTK_SIGNAL_FUNC (eas_entry_activated), eas);
	gtk_widget_show(eas->entry);
	gtk_box_pack_start(GTK_BOX(eas), eas->entry, TRUE, TRUE, 0);
	eas->option_choice = 0;
}

static void
e_addressbook_search_destroy (GtkObject *object)
{
	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

GtkWidget*
e_addressbook_search_new (void)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_addressbook_search_get_type ()));
	return widget;
}

static void
e_addressbook_search_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EAddressbookSearch *eas = E_ADDRESSBOOK_SEARCH(object);

	switch (arg_id) {
	case ARG_OPTION_CHOICE:
		GTK_VALUE_ENUM (*arg) = eas->option_choice;
		break;

	case ARG_TEXT:
		GTK_VALUE_STRING (*arg) = e_utf8_gtk_editable_get_text(GTK_EDITABLE(eas->entry));
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
e_addressbook_search_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EAddressbookSearch *eas = E_ADDRESSBOOK_SEARCH(object);

	switch (arg_id) {
	case ARG_OPTION_CHOICE:
		eas->option_choice = GTK_VALUE_ENUM (*arg);
		gtk_option_menu_set_history (GTK_OPTION_MENU (eas->option), eas->option_choice);
		eas_query_changed(eas);
		break;

	case ARG_TEXT:
		e_utf8_gtk_editable_set_text(GTK_EDITABLE(eas->entry), GTK_VALUE_STRING (*arg));
		eas_query_changed(eas);
		break;

	default:
		break;
	}
}
