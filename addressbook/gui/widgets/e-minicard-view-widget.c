/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-minicard-view-widget.c
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
#include "e-minicard-view-widget.h"
static void e_minicard_view_widget_init		 (EMinicardViewWidget		 *widget);
static void e_minicard_view_widget_class_init	 (EMinicardViewWidgetClass	 *klass);
static void e_minicard_view_widget_set_arg       (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_minicard_view_widget_get_arg       (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_minicard_view_widget_destroy       (GtkObject *object);
static void e_minicard_view_widget_reflow        (ECanvas *canvas);
static void e_minicard_view_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void e_minicard_view_widget_realize       (GtkWidget *widget);

static ECanvasClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_BOOK,
	ARG_QUERY
};

GtkType
e_minicard_view_widget_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      static const GtkTypeInfo info =
      {
        "EMinicardViewWidget",
        sizeof (EMinicardViewWidget),
        sizeof (EMinicardViewWidgetClass),
        (GtkClassInitFunc) e_minicard_view_widget_class_init,
        (GtkObjectInitFunc) e_minicard_view_widget_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      type = gtk_type_unique (e_canvas_get_type (), &info);
    }

  return type;
}

static void
e_minicard_view_widget_class_init (EMinicardViewWidgetClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECanvasClass *canvas_class;

	object_class = (GtkObjectClass*) klass;
	widget_class = GTK_WIDGET_CLASS (klass);
	canvas_class = E_CANVAS_CLASS (klass);

	parent_class = gtk_type_class (e_canvas_get_type ());

	gtk_object_add_arg_type ("EMinicardViewWidget::book", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_BOOK);
	gtk_object_add_arg_type ("EMinicardViewWidget::query", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_QUERY);

	object_class->set_arg       = e_minicard_view_widget_set_arg;
	object_class->get_arg       = e_minicard_view_widget_get_arg;
	object_class->destroy       = e_minicard_view_widget_destroy;

	widget_class->realize       = e_minicard_view_widget_realize;
	widget_class->size_allocate = e_minicard_view_widget_size_allocate;

	canvas_class->reflow        = e_minicard_view_widget_reflow;
}

static void
e_minicard_view_widget_init (EMinicardViewWidget *view)
{
	view->emv = NULL;
	view->rect = NULL;

	view->book = NULL;
	view->query = NULL;
}

GtkWidget *
e_minicard_view_widget_new (void)
{
	return GTK_WIDGET (gtk_type_new (e_minicard_view_widget_get_type ()));
}

static void
e_minicard_view_widget_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EMinicardViewWidget *emvw;

	emvw = E_MINICARD_VIEW_WIDGET (o);

	switch (arg_id){
	case ARG_BOOK:
		if (emvw->book)
			gtk_object_unref(GTK_OBJECT(emvw->book));
		emvw->book = E_BOOK(GTK_VALUE_OBJECT (*arg));
		if (emvw->book)
			gtk_object_ref(GTK_OBJECT(emvw->book));
		if (emvw->emv)
			gtk_object_set(GTK_OBJECT(emvw->emv),
				       "book", emvw->book,
				       NULL);
		break;
	case ARG_QUERY:
		emvw->query = g_strdup(GTK_VALUE_STRING (*arg));
		if (emvw->emv)
			gtk_object_set(GTK_OBJECT(emvw->emv),
				       "query", emvw->query,
				       NULL);
		break;
	}
}

static void
e_minicard_view_widget_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EMinicardViewWidget *emvw;

	emvw = E_MINICARD_VIEW_WIDGET (object);

	switch (arg_id) {
	case ARG_BOOK:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(emvw->book);
		break;
	case ARG_QUERY:
		GTK_VALUE_STRING (*arg) = g_strdup(emvw->query);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
e_minicard_view_widget_destroy (GtkObject *object)
{
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET(object);

	if (view->book)
		gtk_object_unref(GTK_OBJECT(view->book));
	g_free(view->query);
  
	GTK_OBJECT_CLASS(parent_class)->destroy (object);
}

static void
e_minicard_view_widget_realize (GtkWidget *widget)
{
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET(widget);

	view->rect = gnome_canvas_item_new(
		gnome_canvas_root( GNOME_CANVAS(view) ),
		gnome_canvas_rect_get_type(),
		"x1", (double) 0,
		"y1", (double) 0,
		"x2", (double) 100,
		"y2", (double) 100,
		"fill_color", "white",
		NULL );

	view->emv = gnome_canvas_item_new(
		gnome_canvas_root( GNOME_CANVAS(view) ),
		e_minicard_view_get_type(),
		"height", (double) 100,
		"minimum_width", (double) 100,
		NULL );
	gtk_object_set(GTK_OBJECT(view->emv),
		       "book", view->book,
		       "query", view->query,
		       NULL);

	if (GTK_WIDGET_CLASS(parent_class)->realize)
		GTK_WIDGET_CLASS(parent_class)->realize (widget);
}

static void
e_minicard_view_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	if (GTK_WIDGET_CLASS(parent_class)->size_allocate)
		GTK_WIDGET_CLASS(parent_class)->size_allocate (widget, allocation);
	
	if (GTK_WIDGET_REALIZED(widget)) {
		double width;
		EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET(widget);

		gnome_canvas_item_set( view->emv,
				       "height", (double) allocation->height,
				       NULL );
		gnome_canvas_item_set( view->emv,
				       "minimum_width", (double) allocation->width,
				       NULL );
		gtk_object_get(GTK_OBJECT(view->emv),
			       "width", &width,
			       NULL);
		width = MAX(width, allocation->width);
		gnome_canvas_set_scroll_region (GNOME_CANVAS (view), 0, 0, width - 1, allocation->height - 1);
		gnome_canvas_item_set( view->rect,
				       "x2", (double) width,
				       "y2", (double) allocation->height,
				       NULL );
	}
}

static void
e_minicard_view_widget_reflow(ECanvas *canvas)
{
	double width;
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET(canvas);

	if (E_CANVAS_CLASS(parent_class)->reflow)
		E_CANVAS_CLASS(parent_class)->reflow (canvas);

	gtk_object_get(GTK_OBJECT(view->emv),
		       "width", &width,
		       NULL);
	width = MAX(width, GTK_WIDGET(canvas)->allocation.width);
	gnome_canvas_set_scroll_region(GNOME_CANVAS(canvas), 0, 0, width - 1, GTK_WIDGET(canvas)->allocation.width - 1);
	gnome_canvas_item_set( view->rect,
			       "x2", (double) width,
			       "y2", (double) GTK_WIDGET(canvas)->allocation.height,
			       NULL );	
}

void
e_minicard_view_widget_remove_selection(EMinicardViewWidget *view,
					EBookCallback  cb,
					gpointer       closure)
{
	if (view->emv)
		e_minicard_view_remove_selection(E_MINICARD_VIEW(view->emv), cb, closure);
}

void       e_minicard_view_widget_jump_to_letter   (EMinicardViewWidget *view,
						    char           letter)
{
	if (view->emv)
		e_minicard_view_jump_to_letter(E_MINICARD_VIEW(view->emv), letter);
}
