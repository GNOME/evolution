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
#include "e-minicard-widget.h"
#include "e-minicard.h"

static void e_minicard_widget_init		(EMinicardWidget		 *card);
static void e_minicard_widget_class_init	(EMinicardWidgetClass	 *klass);
static void e_minicard_widget_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_minicard_widget_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_minicard_widget_destroy (GtkObject *object);
static void e_minicard_widget_size_request	 (GtkWidget	    *widget, GtkRequisition    *requisition);
static void e_minicard_widget_size_allocate	 (GtkWidget	    *widget, GtkAllocation     *allocation);
static void e_minicard_widget_reflow             (ECanvas           *canvas);

static ECanvasClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_CARD,
};

GtkType
e_minicard_widget_get_type (void)
{
	static GtkType type = 0;

	if (!type)
		{
			static const GtkTypeInfo info =
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

			type = gtk_type_unique (e_canvas_get_type (), &info);
		}

	return type;
}

static void
e_minicard_widget_class_init (EMinicardWidgetClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECanvasClass   *ecanvas_class;

	object_class = GTK_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);
	ecanvas_class = E_CANVAS_CLASS(klass);

	parent_class = gtk_type_class (e_canvas_get_type ());

	object_class->set_arg = e_minicard_widget_set_arg;
	object_class->get_arg = e_minicard_widget_get_arg;
	object_class->destroy = e_minicard_widget_destroy;

	widget_class->size_request  = e_minicard_widget_size_request;
	widget_class->size_allocate = e_minicard_widget_size_allocate;

	ecanvas_class->reflow = e_minicard_widget_reflow;

	gtk_object_add_arg_type ("EMinicardWidget::card", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_CARD);
}

static void
e_minicard_widget_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	double height;
	EMinicardWidget *emw = E_MINICARD_WIDGET(widget);
	gtk_object_get(GTK_OBJECT(emw->item),
		       "height", &height,
		       NULL);
	if (height <= 0)
		height = 1;
	widget->requisition.height = height;
	widget->requisition.width  = 200;
	requisition->height = height;
	requisition->width  = 200;
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
	gnome_canvas_set_scroll_region(GNOME_CANVAS( emw ), 0, 0, allocation->width - 1, height - 1);
	gnome_canvas_item_set( emw->rect,
			       "x2", (double) allocation->width,
			       "y2", (double) height,
			       NULL );
	if (GTK_WIDGET_CLASS(parent_class)->size_allocate)
		GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);
}

static void e_minicard_widget_reflow(ECanvas *canvas)
{
	double height;
	EMinicardWidget *emw = E_MINICARD_WIDGET(canvas);
	gtk_object_get(GTK_OBJECT(emw->item),
		       "height", &height,
		       NULL);

	height = MAX(height, GTK_WIDGET(emw)->allocation.height);

	gnome_canvas_set_scroll_region (GNOME_CANVAS(emw), 0, 0, GTK_WIDGET(emw)->allocation.width - 1, height - 1);
	gnome_canvas_item_set( emw->rect,
			       "x2", (double) GTK_WIDGET(emw)->allocation.width,
			       "y2", (double) height,
			       NULL );	
	
	gtk_widget_queue_resize(GTK_WIDGET(canvas));
}

static void
e_minicard_widget_init (EMinicardWidget *emw)
{
	emw->rect = gnome_canvas_item_new(gnome_canvas_root(GNOME_CANVAS(emw)),
					   gnome_canvas_rect_get_type(),
					   "x1", (double) 0,
					   "y1", (double) 0,
					   "x2", (double) 100,
					   "y2", (double) 100,
					   "fill_color", "white",
					   NULL );

	emw->item = gnome_canvas_item_new(gnome_canvas_root(GNOME_CANVAS(emw)),
					  e_minicard_get_type(),
					  "width", (double) 100,
					  NULL );

	gnome_canvas_set_scroll_region ( GNOME_CANVAS( emw ),
					 0, 0,
					 100, 100 );
	
	emw->card = NULL;
}

static void
e_minicard_widget_destroy (GtkObject *object)
{
	EMinicardWidget *emw = E_MINICARD_WIDGET(object);

	if (emw->card)
		gtk_object_unref(GTK_OBJECT(emw->card));
	
	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		GTK_OBJECT_CLASS(parent_class)->destroy(object);
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
		if (emw->card)
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
		if (emw->card)
			GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(emw->card);
		else
			GTK_VALUE_OBJECT (*arg) = NULL;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
