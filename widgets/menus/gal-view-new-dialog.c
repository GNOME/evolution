/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gal-view-new-dialog.c
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
#include "gal-view-new-dialog.h"
#include "gal-define-views-model.h"
#include <gal/widgets/e-unicode.h>
#include <gal/e-table/e-table-scrolled.h>

static void gal_view_new_dialog_init		(GalViewNewDialog		 *card);
static void gal_view_new_dialog_class_init	(GalViewNewDialogClass	 *klass);
static void gal_view_new_dialog_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void gal_view_new_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void gal_view_new_dialog_destroy (GtkObject *object);

static GnomeDialogClass *parent_class = NULL;
#define PARENT_TYPE gnome_dialog_get_type()

/* The arguments we take */
enum {
	ARG_0,
	ARG_NAME,
	ARG_FACTORY,
};

GtkType
gal_view_new_dialog_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info =
		{
			"GalViewNewDialog",
			sizeof (GalViewNewDialog),
			sizeof (GalViewNewDialogClass),
			(GtkClassInitFunc) gal_view_new_dialog_class_init,
			(GtkObjectInitFunc) gal_view_new_dialog_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

static void
gal_view_new_dialog_class_init (GalViewNewDialogClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->set_arg = gal_view_new_dialog_set_arg;
	object_class->get_arg = gal_view_new_dialog_get_arg;
	object_class->destroy = gal_view_new_dialog_destroy;

	gtk_object_add_arg_type ("GalViewNewDialog::name", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_NAME);
	gtk_object_add_arg_type ("GalViewNewDialog::factory", GTK_TYPE_OBJECT,
				 GTK_ARG_READABLE, ARG_FACTORY);
}

static void
gal_view_new_dialog_init (GalViewNewDialog *dialog)
{
	GladeXML *gui;
	GtkWidget *widget;

	gui = glade_xml_new (GAL_GLADEDIR "/gal-view-new-dialog.glade", NULL);
	dialog->gui = gui;

	widget = glade_xml_get_widget(gui, "table-top");
	if (!widget) {
		return;
	}
	gtk_widget_ref(widget);
	gtk_widget_unparent(widget);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialog)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);

	gnome_dialog_append_buttons(GNOME_DIALOG(dialog),
				    GNOME_STOCK_BUTTON_OK,
				    GNOME_STOCK_BUTTON_CANCEL,
				    NULL);

	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);

	dialog->collection = NULL;
	dialog->selected_factory = NULL;
}

static void
gal_view_new_dialog_destroy (GtkObject *object) {
	GalViewNewDialog *gal_view_new_dialog = GAL_VIEW_NEW_DIALOG(object);
	
	gtk_object_unref(GTK_OBJECT(gal_view_new_dialog->gui));
}

GtkWidget*
gal_view_new_dialog_new (GalViewCollection *collection)
{
	GtkWidget *widget =
		gal_view_new_dialog_construct(gtk_type_new (gal_view_new_dialog_get_type ()),
					      collection);
	return widget;
}


static void
gal_view_new_dialog_select_row_callback(GtkCList *list,
					gint row,
					gint column,
					GdkEventButton *event,
					GalViewNewDialog *dialog)
{
	dialog->selected_factory = gtk_clist_get_row_data(list,
							  row);
}

GtkWidget*
gal_view_new_dialog_construct (GalViewNewDialog  *dialog,
			       GalViewCollection *collection)
{
	GtkWidget *list = glade_xml_get_widget(dialog->gui,
					       "clist-type-list");
	GList *iterator;
	dialog->collection = collection;

	iterator = dialog->collection->factory_list;

	for ( ; iterator; iterator = g_list_next(iterator) ) {
		GalViewFactory *factory = iterator->data;
		char *text[1];
		int row;

		gtk_object_ref(GTK_OBJECT(factory));
		text[0] = (char *) gal_view_factory_get_title(factory);
		row = gtk_clist_append(GTK_CLIST(list), text);
		gtk_clist_set_row_data(GTK_CLIST(list), row, factory);
	}

	gtk_signal_connect(GTK_OBJECT (list),
			   "select_row",
			   GTK_SIGNAL_FUNC(gal_view_new_dialog_select_row_callback),
			   dialog);

	return GTK_WIDGET(dialog);
}

static void
gal_view_new_dialog_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GalViewNewDialog *dialog;
	GtkWidget *entry;

	dialog = GAL_VIEW_NEW_DIALOG (o);
	
	switch (arg_id){
	case ARG_NAME:
		entry = glade_xml_get_widget(dialog->gui, "entry-name");
		if (entry && GTK_IS_EDITABLE(entry)) {
			e_utf8_gtk_editable_set_text(GTK_EDITABLE(entry), GTK_VALUE_STRING(*arg));
		}
		break;
	default:
		return;
	}
}

static void
gal_view_new_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GalViewNewDialog *dialog;
	GtkWidget *entry;

	dialog = GAL_VIEW_NEW_DIALOG (object);

	switch (arg_id) {
	case ARG_NAME:
		entry = glade_xml_get_widget(dialog->gui, "entry-name");
		if (entry && GTK_IS_EDITABLE(entry)) {
			GTK_VALUE_STRING(*arg) = e_utf8_gtk_editable_get_text(GTK_EDITABLE(entry));
		}
		break;
	case ARG_FACTORY:
		GTK_VALUE_OBJECT(*arg) = dialog->selected_factory ? GTK_OBJECT(dialog->selected_factory) : NULL;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
