/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-addressbook-search-dialog.c
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
#include <e-util/e-canvas.h>
#include "e-addressbook-search-dialog.h"
#include "addressbook/gui/minicard/e-minicard-view-widget.h"
#include "widgets/misc/e-scroll-frame.h"
static void e_addressbook_search_dialog_init		 (EAddressbookSearchDialog		 *widget);
static void e_addressbook_search_dialog_class_init	 (EAddressbookSearchDialogClass	 *klass);
static void e_addressbook_search_dialog_set_arg       (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_addressbook_search_dialog_get_arg       (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_addressbook_search_dialog_destroy       (GtkObject *object);

static ECanvasClass *parent_class = NULL;

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
	GtkWidgetClass *widget_class;
	ECanvasClass *canvas_class;

	object_class = (GtkObjectClass*) klass;
	widget_class = GTK_WIDGET_CLASS (klass);
	canvas_class = E_CANVAS_CLASS (klass);

	parent_class = gtk_type_class (PARENT_TYPE);

	gtk_object_add_arg_type ("EAddressbookSearchDialog::book", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_BOOK);

	object_class->set_arg       = e_addressbook_search_dialog_set_arg;
	object_class->get_arg       = e_addressbook_search_dialog_get_arg;
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
	rule_context_load(view->context, SEARCH_RULE_DIR "/addresstypes.xml", "", NULL, NULL);
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
button_press (GtkWidget *widget, EAddressbookSearchDialog *dialog)
{
	char *query;

	gtk_widget_show(dialog->scrolled_window);
	query = get_query(dialog);
	gtk_object_set(GTK_OBJECT(dialog->view),
		       "query", query,
		       NULL);
	g_free(query);
}

static void
e_addressbook_search_dialog_init (EAddressbookSearchDialog *view)
{
	GtkWidget *button;
	GnomeDialog *dialog = GNOME_DIALOG (view);

	gtk_window_set_policy(GTK_WINDOW(view), FALSE, TRUE, FALSE);

	view->search = get_widget(view);
	gtk_box_pack_start(GTK_BOX(dialog->vbox), view->search, TRUE, TRUE, 0);
	gtk_widget_show(view->search);

	button = gtk_button_new_with_label(_("Search"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(button_press), view);
	gtk_widget_show(button);

	view->view = e_minicard_view_widget_new();
	gtk_widget_show(view->view);

	view->scrolled_window = e_scroll_frame_new(NULL, NULL);
	e_scroll_frame_set_policy(E_SCROLL_FRAME(view->scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_container_add(GTK_CONTAINER(view->scrolled_window), view->view);
	
	gtk_box_pack_start(GTK_BOX(dialog->vbox), view->scrolled_window, TRUE, TRUE, 0);
}

GtkWidget *
e_addressbook_search_dialog_new (EBook *book)
{
	EAddressbookSearchDialog *view = gtk_type_new (e_addressbook_search_dialog_get_type ());
	gtk_object_set(GTK_OBJECT(view->view),
		       "book", book,
		       NULL);
	return GTK_WIDGET(view);
}

static void
e_addressbook_search_dialog_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EAddressbookSearchDialog *emvw;

	emvw = E_ADDRESSBOOK_SEARCH_DIALOG (o);

	switch (arg_id){
	case ARG_BOOK:
		gtk_object_set(GTK_OBJECT(emvw->view),
			       "book", GTK_VALUE_OBJECT (*arg),
			       NULL);
		break;
	}
}

static void
e_addressbook_search_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EAddressbookSearchDialog *emvw;

	emvw = E_ADDRESSBOOK_SEARCH_DIALOG (object);

	switch (arg_id) {
	case ARG_BOOK:
		gtk_object_get(GTK_OBJECT(emvw->view),
			       "book", &(GTK_VALUE_OBJECT (*arg)),
			       NULL);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
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
