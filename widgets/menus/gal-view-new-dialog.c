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
}

#if 0
#define SPEC "<ETableSpecification cursor-mode=\"line\" draw-grid=\"true\">" \
	     "<ETableColumn model_col= \"0\" _tite=\"Name\" expansion=\"1.0\" minimum_width=\"18\" resizable=\"true\" cell=\"string\" compare=\"string\"/>" \
             "<ETableState> <column source=\"0\"/> <grouping> </grouping> </ETableState>" \
	     "</ETableSpecification>"

/* For use from libglade. */
GtkWidget *gal_view_new_dialog_create_etable(char *name, char *string1, char *string2, int int1, int int2);

GtkWidget *
gal_view_new_dialog_create_etable(char *name, char *string1, char *string2, int int1, int int2)
{
	GtkWidget *table;
	ETableModel *model;
	model = gal_define_views_model_new();
	table = e_table_scrolled_new(model, NULL, SPEC, NULL);
	return table;
}
#endif

static void
gal_view_new_dialog_init (GalViewNewDialog *gal_view_new_dialog)
{
	GladeXML *gui;
	GtkWidget *widget;

	gui = glade_xml_new (GAL_GLADEDIR "/gal-view-new-diallog.glade", NULL);
	gal_view_new_dialog->gui = gui;

	widget = glade_xml_get_widget(gui, "table-top");
	if (!widget) {
		return;
	}
	gtk_widget_ref(widget);
	gtk_widget_unparent(widget);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(gal_view_new_dialog)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);

	gnome_dialog_append_buttons(GNOME_DIALOG(gal_view_new_dialog),
				    GNOME_STOCK_BUTTON_OK,
				    GNOME_STOCK_BUTTON_CANCEL,
				    NULL);
	
	gtk_window_set_policy(GTK_WINDOW(gal_view_new_dialog), FALSE, TRUE, FALSE);
}

static void
gal_view_new_dialog_destroy (GtkObject *object) {
	GalViewNewDialog *gal_view_new_dialog = GAL_VIEW_NEW_DIALOG(object);
	
	gtk_object_unref(GTK_OBJECT(gal_view_new_dialog->gui));
}

GtkWidget*
gal_view_new_dialog_new (void)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (gal_view_new_dialog_get_type ()));
	return widget;
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
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
