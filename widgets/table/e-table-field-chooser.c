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
	ARG_FULL_HEADER,
	ARG_DND_CODE,
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
	gtk_object_add_arg_type ("ETableFieldChooser::dnd_code", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_DND_CODE);
	gtk_object_add_arg_type ("ETableFieldChooser::full_header", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_FULL_HEADER);
}

static void allocate_callback(GtkWidget *canvas, GtkAllocation *allocation, ETableFieldChooser *etfc)
{
	double height;
	etfc->last_alloc = *allocation;
	gnome_canvas_item_set( etfc->item,
			       "width", (double) allocation->width,
			       NULL );
	gtk_object_get(GTK_OBJECT(etfc->item),
		       "height", &height,
		       NULL);
	height = MAX(height, allocation->height);
	gnome_canvas_set_scroll_region(GNOME_CANVAS( etfc->canvas ), 0, 0, allocation->width, height);
	gnome_canvas_item_set( etfc->rect,
			       "x2", (double) allocation->width,
			       "y2", (double) height,
			       NULL );
}

static void resize(GnomeCanvas *canvas, ETableFieldChooser *etfc)
{
	double height;
	gtk_object_get(GTK_OBJECT(etfc->item),
		       "height", &height,
		       NULL);

	height = MAX(height, etfc->last_alloc.height);

	gnome_canvas_set_scroll_region (GNOME_CANVAS(etfc->canvas), 0, 0, etfc->last_alloc.width, height);
	gnome_canvas_item_set( etfc->rect,
			       "x2", (double) etfc->last_alloc.width,
			       "y2", (double) height,
			       NULL );	
}

static void
e_table_field_chooser_init (ETableFieldChooser *etfc)
{
	GladeXML *gui;
	GtkWidget *widget;

	gui = glade_xml_new (ETABLE_GLADEDIR "/e-table-field-chooser.glade", NULL);
	etfc->gui = gui;

	widget = glade_xml_get_widget(gui, "vbox-top");
	if (!widget) {
		return;
	}
	gtk_widget_reparent(widget,
			    GTK_WIDGET(etfc));

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	etfc->canvas = GNOME_CANVAS(glade_xml_get_widget(gui, "canvas-buttons"));

	etfc->rect = gnome_canvas_item_new(gnome_canvas_root( GNOME_CANVAS( etfc->canvas ) ),
					   gnome_canvas_rect_get_type(),
					   "x1", (double) 0,
					   "y1", (double) 0,
					   "x2", (double) 100,
					   "y2", (double) 100,
					   "fill_color", "white",
					   NULL );

	etfc->item = gnome_canvas_item_new(gnome_canvas_root(etfc->canvas),
					   e_table_field_chooser_item_get_type(),
					   "width", (double) 100,
					   "full_header", etfc->full_header,
					   "dnd_code", etfc->dnd_code,
					   NULL );

	gtk_signal_connect( GTK_OBJECT( etfc->canvas ), "reflow",
			    GTK_SIGNAL_FUNC( resize ),
			    etfc);

	gnome_canvas_set_scroll_region ( GNOME_CANVAS( etfc->canvas ),
					 0, 0,
					 100, 100 );

	/* Connect the signals */
	gtk_signal_connect (GTK_OBJECT (etfc->canvas), "size_allocate",
			    GTK_SIGNAL_FUNC (allocate_callback),
			    etfc);

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();
	gtk_widget_show(widget);
}

static void
e_table_field_chooser_destroy (GtkObject *object)
{
	ETableFieldChooser *etfc = E_TABLE_FIELD_CHOOSER(object);

	g_free(etfc->dnd_code);
	if (etfc->full_header)
		gtk_object_unref(GTK_OBJECT(etfc->full_header));

	if (etfc->gui)
		gtk_object_unref(GTK_OBJECT(etfc->gui));
}

GtkWidget*
e_table_field_chooser_new (void)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_table_field_chooser_get_type ()));
	return widget;
}

static void
e_table_field_chooser_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableFieldChooser *etfc = E_TABLE_FIELD_CHOOSER(object);

	switch (arg_id){
	case ARG_DND_CODE:
		g_free(etfc->dnd_code);
		etfc->dnd_code = g_strdup(GTK_VALUE_STRING (*arg));
		if (etfc->item)
			gtk_object_set(GTK_OBJECT(etfc->item),
				       "dnd_code", etfc->dnd_code,
				       NULL);
		break;
	case ARG_FULL_HEADER:
		if (etfc->full_header)
			gtk_object_unref(GTK_OBJECT(etfc->full_header));
		if (GTK_VALUE_OBJECT(*arg))
			etfc->full_header = E_TABLE_HEADER(GTK_VALUE_OBJECT(*arg));
		else
			etfc->full_header = NULL;
		if (etfc->full_header)
			gtk_object_ref(GTK_OBJECT(etfc->full_header));
		if (etfc->item)
			gtk_object_set(GTK_OBJECT(etfc->item),
				       "full_header", etfc->full_header,
				       NULL);
		break;
	default:
		break;
	}
}

static void
e_table_field_chooser_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableFieldChooser *etfc = E_TABLE_FIELD_CHOOSER(object);

	switch (arg_id) {
	case ARG_DND_CODE:
		GTK_VALUE_STRING (*arg) = g_strdup (etfc->dnd_code);
		break;
	case ARG_FULL_HEADER:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(etfc->full_header);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
