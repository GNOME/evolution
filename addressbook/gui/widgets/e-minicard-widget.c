/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser.c
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
#include <libgnome/gnome-i18n.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include "e-minicard-widget.h"
#include "e-minicard.h"

static void e_minicard_widget_init		(EMinicardWidget		 *card);
static void e_minicard_widget_class_init	(EMinicardWidgetClass	 *klass);
static void e_minicard_widget_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_minicard_widget_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_minicard_widget_dispose (GObject *object);
static void e_minicard_widget_size_request	 (GtkWidget	    *widget, GtkRequisition    *requisition);
static void e_minicard_widget_size_allocate	 (GtkWidget	    *widget, GtkAllocation     *allocation);
static void e_minicard_widget_reflow             (ECanvas           *canvas);

static ECanvasClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_CARD,
};

GType
e_minicard_widget_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EMinicardWidgetClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_minicard_widget_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EMinicardWidget),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_minicard_widget_init,
		};

		type = g_type_register_static (e_canvas_get_type (), "EMinicardWidget", &info, 0);
	}

	return type;
}

static void
e_minicard_widget_class_init (EMinicardWidgetClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECanvasClass   *ecanvas_class;

	object_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);
	ecanvas_class = E_CANVAS_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);
	
	object_class->set_property = e_minicard_widget_set_property;
	object_class->get_property = e_minicard_widget_get_property;
	object_class->dispose = e_minicard_widget_dispose;

	widget_class->size_request  = e_minicard_widget_size_request;
	widget_class->size_allocate = e_minicard_widget_size_allocate;

	ecanvas_class->reflow = e_minicard_widget_reflow;

	g_object_class_install_property (object_class, PROP_CARD, 
					 g_param_spec_object ("card",
							      _("Card"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_CARD,
							      G_PARAM_READWRITE));
}

static void
e_minicard_widget_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	double height;
	EMinicardWidget *emw = E_MINICARD_WIDGET(widget);
	g_object_get(emw->item,
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
	g_object_get(emw->item,
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
	g_object_get(emw->item,
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
e_minicard_widget_dispose (GObject *object)
{
	EMinicardWidget *emw = E_MINICARD_WIDGET(object);

	if (emw->card) {
		g_object_unref (emw->card);
		emw->card = NULL;
	}
	
	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose(object);
}

GtkWidget*
e_minicard_widget_new (void)
{
	GtkWidget *widget = GTK_WIDGET (g_object_new (E_TYPE_MINICARD_WIDGET, NULL));
	return widget;
}

static void
e_minicard_widget_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
	EMinicardWidget *emw = E_MINICARD_WIDGET(object);
	gpointer ptr;

	switch (prop_id){
	case PROP_CARD:
		ptr = g_value_get_object (value);
		e_minicard_widget_set_card (emw, ptr ? E_CARD (ptr) : NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_minicard_widget_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec)
{
	EMinicardWidget *emw = E_MINICARD_WIDGET(object);

	switch (prop_id) {
	case PROP_CARD:
		if (emw->card)
			g_value_set_object (value, emw->card);
		else
			g_value_set_object (value, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

void
e_minicard_widget_set_card (EMinicardWidget *emw, ECard *card)
{
	g_return_if_fail (emw && E_IS_MINICARD_WIDGET (emw));
	g_return_if_fail (card == NULL || E_IS_CARD (card));

	if (card != emw->card) {

		if (emw->card)
			g_object_unref (emw->card);

		emw->card = card;

		if (emw->card)
			g_object_ref (emw->card);

		if (emw->item)
			g_object_set (emw->item,
				      "card", emw->card,
				      NULL);
	}
}
