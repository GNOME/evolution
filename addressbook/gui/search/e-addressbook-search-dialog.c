/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-addressbook-search-dialog.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
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

#include <gtk/gtkbox.h>
#include <gtk/gtkentry.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>

#include "e-addressbook-search-dialog.h"


static void e_addressbook_search_dialog_init		 (EAddressbookSearchDialog		 *widget);
static void e_addressbook_search_dialog_class_init	 (EAddressbookSearchDialogClass	 *klass);
static void e_addressbook_search_dialog_destroy       (GtkObject *object);

static GnomeDialog *parent_class = NULL;

#define PARENT_TYPE (gnome_dialog_get_type())

/* The arguments we take */
enum {
	ARG_0,
	ARG_BOOK,
};

GtkType
e_addressbook_search_dialog_get_type (void)
{
	static GtkType type = 0;

	if (!type)
	{
		static const GtkTypeInfo info =
		{
			"EAddressbookSearchDialog",
			sizeof (EAddressbookSearchDialog),
			sizeof (EAddressbookSearchDialogClass),
			(GtkClassInitFunc) e_addressbook_search_dialog_class_init,
			(GtkObjectInitFunc) e_addressbook_search_dialog_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

static void
e_addressbook_search_dialog_class_init (EAddressbookSearchDialogClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (PARENT_TYPE);

	gtk_object_add_arg_type ("EAddressbookSearchDialog::book", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_BOOK);

	object_class->destroy       = e_addressbook_search_dialog_destroy;
}

static GtkWidget *
get_widget (EAddressbookSearchDialog *view)
{
	FilterPart *part;

	view->context = rule_context_new();
	/* FIXME: hide this in a class */
	rule_context_add_part_set(view->context, "partset", filter_part_get_type(),
				  rule_context_add_part, rule_context_next_part);
	rule_context_load(view->context, SEARCH_RULE_DIR "/addresstypes.xml", "");
	view->rule = filter_rule_new();
	part = rule_context_next_part(view->context, NULL);
	if (part == NULL) {
		g_warning("Problem loading search for addressbook no parts to load");
		return gtk_entry_new();
	} else {
		filter_rule_add_part(view->rule, filter_part_clone(part));
		return filter_rule_get_widget(view->rule, view->context);
	}
}

static char *
get_query (EAddressbookSearchDialog *view)
{
	GString *out = g_string_new("");
	char *ret;

	filter_rule_build_code(view->rule, out);
	ret = out->str;
	printf("Searching using %s\n", ret);
	g_string_free(out, FALSE);
	return ret;
}

static void
button_press (GtkWidget *widget, int button, EAddressbookSearchDialog *dialog)
{
	char *query;

	if (button == 0) {
		query = get_query(dialog);
		gtk_object_set(GTK_OBJECT(dialog->view),
			       "query", query,
			       NULL);
		g_free(query);
	}

	gnome_dialog_close(GNOME_DIALOG (dialog));
}

static void
e_addressbook_search_dialog_init (EAddressbookSearchDialog *view)
{
	GnomeDialog *dialog = GNOME_DIALOG (view);

	gtk_window_set_policy(GTK_WINDOW(view), FALSE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (view), 550, 400);
	gtk_window_set_title(GTK_WINDOW(view), _("Advanced Search"));
	view->search = get_widget(view);
	gtk_box_pack_start(GTK_BOX(dialog->vbox), view->search, TRUE, TRUE, 0);
	gtk_widget_show(view->search);

	gnome_dialog_append_buttons(dialog,
				    _("Search"),
				    GNOME_STOCK_BUTTON_CLOSE, NULL);
	
	gnome_dialog_set_default(dialog, 0);

	gtk_signal_connect(GTK_OBJECT(dialog), "clicked",
			   GTK_SIGNAL_FUNC(button_press), view);
}

GtkWidget *
e_addressbook_search_dialog_new (EAddressbookView *addr_view)
{
	EAddressbookSearchDialog *view = gtk_type_new (e_addressbook_search_dialog_get_type ());
	view->view = addr_view;
	return GTK_WIDGET(view);
}

static void
e_addressbook_search_dialog_destroy (GtkObject *object)
{
	EAddressbookSearchDialog *view;

	view = E_ADDRESSBOOK_SEARCH_DIALOG (object);

	gtk_object_unref((GtkObject *)view->context);
	gtk_object_unref((GtkObject *)view->rule);

	GTK_OBJECT_CLASS(parent_class)->destroy (object);
}
