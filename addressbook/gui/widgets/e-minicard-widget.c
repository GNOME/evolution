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

static void e_minicard_widget_init		(EMinicardWidget		 *card);
static void e_minicard_widget_class_init	(EMinicardWidgetClass	 *klass);
static void e_minicard_widget_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_minicard_widget_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_minicard_widget_destroy (GtkObject *object);

static ECanvas *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_CARD,
};

GtkType
e_minicard_widget_get_type (void)
{
	static GtkType table_field_chooser_type = 0;

	if (!table_field_chooser_type)
		{
			static const GtkTypeInfo table_field_chooser_info =
			{
				"EMinicardWidget",
				sizeof (EMinicardWidget),
				sizeof (EMinicardWidgetClass),
				(GtkClassInitFunc) e_minicard_widget_class_init,
				(GtkObjectInitFunc) e_minicard_widget_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
				(GtkClassInitFunc) NULL,
			};

			table_field_chooser_type = gtk_type_unique (e_canvas_get_type (), &table_field_chooser_info);
		}

	return table_field_chooser_type;
}

static void
e_minicard_widget_class_init (EMinicardWidgetClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass*) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = gtk_type_class (gtk_vbox_get_type ());

	object_class->set_arg = e_minicard_widget_set_arg;
	object_class->get_arg = e_minicard_widget_get_arg;
	object_class->destroy = e_minicard_widget_destroy;

	widget_class->size_request  = e_minicard_widget_size_request;
	widget_class->size_allocate = e_minicard_widget_size_allocate;
	gtk_object_add_arg_type ("EMinicardWidget::card", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_CARD);
}

static void
e_minicard_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	double height;
	EMinicardWidget *emw = E_MINICARD_WIDGET(widget);
	gnome_canvas_item_set( emw->item,
			       "width", (double) allocation->width,
			       NULL );
	gtk_object_get(GTK_OBJECT(emw->item),
		       "height", &height,
		       NULL);
	height = MAX(height, allocation->height);
	gnome_canvas_set_scroll_region(GNOME_CANVAS( emw->canvas ), 0, 0, allocation->width - 1, height - 1);
	gnome_canvas_item_set( emw->rect,
			       "x2", (double) allocation->width,
			       "y2", (double) height,
			       NULL );
	if (GTK_WIDGET_CLASS(parent_class)->size_allocate)
		GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);
}

static void resize(GnomeCanvas *canvas, EMinicardWidget *emw)
{
	double height;
	gtk_object_get(GTK_OBJECT(emw->item),
		       "height", &height,
		       NULL);

	height = MAX(height, emw->last_alloc.height);

	gnome_canvas_set_scroll_region (GNOME_CANVAS(emw->canvas), 0, 0, emw->last_alloc.width - 1, height - 1);
	gnome_canvas_item_set( emw->rect,
			       "x2", (double) emw->last_alloc.width,
			       "y2", (double) height,
			       NULL );	
}

static void
e_minicard_widget_init (EMinicardWidget *emw)
{
	emw->rect = gnome_canvas_item_new(gnome_canvas_root( GNOME_CANVAS( emw->canvas ) ),
					   gnome_canvas_rect_get_type(),
					   "x1", (double) 0,
					   "y1", (double) 0,
					   "x2", (double) 100,
					   "y2", (double) 100,
					   "fill_color", "white",
					   NULL );

	emw->item = gnome_canvas_item_new(gnome_canvas_root(emw->canvas),
					  e_minicard_widget_item_get_type(),
					  "width", (double) 100,
					  NULL );

	gtk_signal_connect( GTK_OBJECT( emw->canvas ), "reflow",
			    GTK_SIGNAL_FUNC( resize ),
			    emw);

	gnome_canvas_set_scroll_region ( GNOME_CANVAS( emw->canvas ),
					 0, 0,
					 100, 100 );

	/* Connect the signals */
	gtk_signal_connect (GTK_OBJECT (emw->canvas), "size_allocate",
			    GTK_SIGNAL_FUNC (allocate_callback),
			    emw);
}

static void
e_minicard_widget_destroy (GtkObject *object)
{
	EMinicardWidget *emw = E_MINICARD_WIDGET(object);

	g_free(emw->dnd_code);
	if (emw->full_header)
		gtk_object_unref(GTK_OBJECT(emw->full_header));

	if (emw->gui)
		gtk_object_unref(GTK_OBJECT(emw->gui));
}

GtkWidget*
e_minicard_widget_new (void)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_minicard_widget_get_type ()));
	return widget;
}

static void
e_minicard_widget_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EMinicardWidget *emw = E_MINICARD_WIDGET(object);

	switch (arg_id){
	case ARG_CARD:
		if (emw->full_header)
			gtk_object_unref(GTK_OBJECT(emw->card));
		if (GTK_VALUE_OBJECT(*arg)) {
		  emw->card = E_CARD(GTK_VALUE_OBJECT(*arg));
			gtk_object_ref(GTK_OBJECT(emw->card));
		}
		else
			emw->card = NULL;
		if (emw->item)
			gtk_object_set(GTK_OBJECT(emw->item),
				       "card", emw->card,
				       NULL);
		break;
	default:
		break;
	}
}

static void
e_minicard_widget_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EMinicardWidget *emw = E_MINICARD_WIDGET(object);

	switch (arg_id) {
	case ARG_CARD:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(emw->card);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
