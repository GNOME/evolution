/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser.c
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
#include <e-table-field-chooser.h>
#include <e-table-field-chooser-item.h>

static void e_table_field_chooser_init		(ETableFieldChooser		 *card);
static void e_table_field_chooser_class_init	(ETableFieldChooserClass	 *klass);
static void e_table_field_chooser_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_table_field_chooser_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_table_field_chooser_destroy (GtkObject *object);

static GtkVBoxClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
};

GtkType
e_table_field_chooser_get_type (void)
{
	static GtkType table_field_chooser_type = 0;

	if (!table_field_chooser_type)
		{
			static const GtkTypeInfo table_field_chooser_info =
			{
				"ETableFieldChooser",
				sizeof (ETableFieldChooser),
				sizeof (ETableFieldChooserClass),
				(GtkClassInitFunc) e_table_field_chooser_class_init,
				(GtkObjectInitFunc) e_table_field_chooser_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
				(GtkClassInitFunc) NULL,
			};

			table_field_chooser_type = gtk_type_unique (gtk_vbox_get_type (), &table_field_chooser_info);
		}

	return table_field_chooser_type;
}

static void
e_table_field_chooser_class_init (ETableFieldChooserClass *klass)
{
	GtkObjectClass *object_class;
	GtkVBoxClass *vbox_class;

	object_class = (GtkObjectClass*) klass;
	vbox_class = (GtkVBoxClass *) klass;

	parent_class = gtk_type_class (gtk_vbox_get_type ());

	object_class->set_arg = e_table_field_chooser_set_arg;
	object_class->get_arg = e_table_field_chooser_get_arg;
	object_class->destroy = e_table_field_chooser_destroy;
}

static void
e_table_field_chooser_init (ETableFieldChooser *e_table_field_chooser)
{
	GladeXML *gui;
	GtkWidget *widget;

	gui = glade_xml_new (ETABLE_GLADEDIR "/fullname.glade", NULL);
	e_table_field_chooser->gui = gui;

	widget = glade_xml_get_widget(gui, "vbox-top");
	if (!widget) {
		return;
	}
	gtk_widget_reparent(widget,
			    GTK_WIDGET(e_table_field_chooser));

	e_table_field_chooser->canvas = GNOME_CANVAS(glade_xml_get_widget(gui, "canvas-buttons"));
	e_table_field_chooser->item = gnome_canvas_item_new(GNOME_CANVAS_GROUP(e_table_field_chooser->canvas->root),
							    e_table_field_chooser_item_get_type(),
							    NULL);
}

void
e_table_field_chooser_destroy (GtkObject *object)
{
	ETableFieldChooser *e_table_field_chooser = E_TABLE_FIELD_CHOOSER(object);

	if (e_table_field_chooser->gui)
		gtk_object_unref(GTK_OBJECT(e_table_field_chooser->gui));
}

GtkWidget*
e_table_field_chooser_new (void)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_table_field_chooser_get_type ()));
	return widget;
}

static void
e_table_field_chooser_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	switch (arg_id){
	default:
		break;
	}
}

static void
e_table_field_chooser_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	switch (arg_id) {
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
