/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-minicard-view-widget.c
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

#include <gtk/gtksignal.h>
#include <gal/widgets/e-canvas-background.h>
#include <gal/widgets/e-canvas.h>
#include <libgnome/gnome-i18n.h>

#include "eab-marshal.h"
#include "e-minicard-view-widget.h"

static void e_minicard_view_widget_init		 (EMinicardViewWidget		 *widget);
static void e_minicard_view_widget_class_init	 (EMinicardViewWidgetClass	 *klass);
static void e_minicard_view_widget_set_property  (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_minicard_view_widget_get_property  (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_minicard_view_widget_dispose       (GObject *object);
static void e_minicard_view_widget_reflow        (ECanvas *canvas);
static void e_minicard_view_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void e_minicard_view_widget_style_set     (GtkWidget *widget, GtkStyle *previous_style);
static void e_minicard_view_widget_realize       (GtkWidget *widget);

static ECanvasClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_BOOK,
	PROP_QUERY,
	PROP_EDITABLE,
	PROP_COLUMN_WIDTH
};

enum {
	SELECTION_CHANGE,
	COLUMN_WIDTH_CHANGED,
	RIGHT_CLICK,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = {0, };

GType
e_minicard_view_widget_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EMinicardViewWidgetClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_minicard_view_widget_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EMinicardViewWidget),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_minicard_view_widget_init,
		};

		type = g_type_register_static (e_canvas_get_type (), "EMinicardViewWidget", &info, 0);
	}

	return type;
}

static void
e_minicard_view_widget_class_init (EMinicardViewWidgetClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECanvasClass *canvas_class;

	object_class = (GObjectClass*) klass;
	widget_class = GTK_WIDGET_CLASS (klass);
	canvas_class = E_CANVAS_CLASS (klass);

	parent_class = gtk_type_class (e_canvas_get_type ());

	object_class->set_property       = e_minicard_view_widget_set_property;
	object_class->get_property       = e_minicard_view_widget_get_property;
	object_class->dispose            = e_minicard_view_widget_dispose;

	g_object_class_install_property (object_class, PROP_BOOK, 
					 g_param_spec_object ("book",
							      _("Book"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_BOOK,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_QUERY, 
					 g_param_spec_string ("query",
							      _("Query"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE, 
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_COLUMN_WIDTH, 
					 g_param_spec_double ("column_width",
							      _("Column Width"),
							      /*_( */"XXX blurb" /*)*/,
							      0.0, G_MAXDOUBLE, 150.0,
							      G_PARAM_READWRITE));

	signals [SELECTION_CHANGE] =
		g_signal_new ("selection_change",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMinicardViewWidgetClass, selection_change),
			      NULL, NULL,
			      eab_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	signals [COLUMN_WIDTH_CHANGED] =
		g_signal_new ("column_width_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMinicardViewWidgetClass, column_width_changed),
			      NULL, NULL,
			      eab_marshal_NONE__DOUBLE,
			      G_TYPE_NONE, 1, G_TYPE_DOUBLE);

	signals [RIGHT_CLICK] =
		g_signal_new ("right_click",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMinicardViewWidgetClass, right_click),
			      NULL, NULL,
			      eab_marshal_INT__POINTER,
			      G_TYPE_INT, 1, G_TYPE_POINTER);

	widget_class->style_set     = e_minicard_view_widget_style_set;
	widget_class->realize       = e_minicard_view_widget_realize;
	widget_class->size_allocate = e_minicard_view_widget_size_allocate;

	canvas_class->reflow        = e_minicard_view_widget_reflow;

	klass->selection_change     = NULL;
	klass->column_width_changed = NULL;
	klass->right_click          = NULL;
}

static void
e_minicard_view_widget_init (EMinicardViewWidget *view)
{
	view->emv = NULL;

	view->book = NULL;
	view->query = NULL;
	view->editable = FALSE;
	view->column_width = 150;
}

GtkWidget *
e_minicard_view_widget_new (EAddressbookReflowAdapter *adapter)
{
	EMinicardViewWidget *widget = E_MINICARD_VIEW_WIDGET (g_object_new (e_minicard_view_widget_get_type (), NULL));

	widget->adapter = adapter;
	g_object_ref (widget->adapter);

	return GTK_WIDGET (widget);
}

static void
e_minicard_view_widget_set_property (GObject *object,
				     guint prop_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	EMinicardViewWidget *emvw;

	emvw = E_MINICARD_VIEW_WIDGET (object);

	switch (prop_id){
	case PROP_BOOK:
		if (emvw->book)
			g_object_unref (emvw->book);
		if (g_value_get_object (value)) {
			emvw->book = E_BOOK(g_value_get_object (value));
			if (emvw->book)
				g_object_ref(emvw->book);
		} else
			emvw->book = NULL;
		if (emvw->emv)
			g_object_set(emvw->emv,
				     "book", emvw->book,
				       NULL);
		break;
	case PROP_QUERY:
		emvw->query = g_strdup(g_value_get_string (value));
		if (emvw->emv)
			g_object_set(emvw->emv,
				     "query", emvw->query,
				     NULL);
		break;
	case PROP_EDITABLE:
		emvw->editable = g_value_get_boolean (value);
		if (emvw->emv)
			g_object_set (emvw->emv,
				      "editable", emvw->editable,
				      NULL);
		break;
	case PROP_COLUMN_WIDTH:
		emvw->column_width = g_value_get_double (value);
		if (emvw->emv) {
			g_object_set (emvw->emv,
				      "column_width", emvw->column_width,
				      NULL);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_minicard_view_widget_get_property (GObject *object,
				     guint prop_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	EMinicardViewWidget *emvw;

	emvw = E_MINICARD_VIEW_WIDGET (object);

	switch (prop_id) {
	case PROP_BOOK:
		g_value_set_object (value, emvw->book);
		break;
	case PROP_QUERY:
		g_value_set_string (value, emvw->query);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, emvw->editable);
		break;
	case PROP_COLUMN_WIDTH:
		g_value_set_double (value, emvw->column_width);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_minicard_view_widget_dispose (GObject *object)
{
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET(object);

	if (view->book) {
		g_object_unref (view->book);
		view->book = NULL;
	}
	if (view->query) {
		g_free(view->query);
		view->query = NULL;
	}

	if (view->adapter) {
		g_object_unref (view->adapter);
		view->adapter = NULL;
	}

	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose (object);
}

static void
selection_change (ESelectionModel *esm, EMinicardViewWidget *widget)
{
	g_signal_emit (widget,
		       signals [SELECTION_CHANGE], 0);
}

static void
selection_row_change (ESelectionModel *esm, int row, EMinicardViewWidget *widget)
{
	gboolean selected = e_selection_model_is_row_selected (esm, row);

	/* we only handle the selected case here */
	if (!selected)
		return;

	selection_change (esm, widget);
}

static void
column_width_changed (ESelectionModel *esm, double width, EMinicardViewWidget *widget)
{
	g_signal_emit (widget,
		       signals [COLUMN_WIDTH_CHANGED], 0, width);
}

static guint
right_click (EMinicardView *view, GdkEvent *event, EMinicardViewWidget *widget)
{
	guint ret_val;
	g_signal_emit (widget,
		       signals [RIGHT_CLICK], 0,
		       event, &ret_val);
	return ret_val;
}

static void
e_minicard_view_widget_style_set (GtkWidget *widget, GtkStyle *previous_style)
{
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET(widget);

	if (view->background)
		gnome_canvas_item_set (view->background,
				       "fill_color_gdk", &widget->style->base[GTK_STATE_NORMAL],
				       NULL );

	if (GTK_WIDGET_CLASS(parent_class)->style_set)
		GTK_WIDGET_CLASS(parent_class)->style_set (widget, previous_style);
}


static void
e_minicard_view_widget_realize (GtkWidget *widget)
{
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET(widget);
	GtkStyle *style = gtk_widget_get_style (widget);

	view->background = gnome_canvas_item_new(gnome_canvas_root( GNOME_CANVAS(view) ),
						 e_canvas_background_get_type(),
						 "fill_color_gdk", &style->base[GTK_STATE_NORMAL],
						 NULL );

	view->emv = gnome_canvas_item_new(
		gnome_canvas_root( GNOME_CANVAS(view) ),
		e_minicard_view_get_type(),
		"height", (double) 100,
		"minimum_width", (double) 100,
		"adapter", view->adapter,
		"column_width", view->column_width,
		NULL );

	g_signal_connect (E_REFLOW(view->emv)->selection,
			  "selection_changed",
			  G_CALLBACK (selection_change), view);
	g_signal_connect (E_REFLOW(view->emv)->selection,
			  "selection_row_changed",
			  G_CALLBACK (selection_row_change), view);
	g_signal_connect (view->emv,
			  "column_width_changed",
			  G_CALLBACK (column_width_changed), view);
	g_signal_connect (view->emv,
			  "right_click",
			  G_CALLBACK (right_click), view);

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
		g_object_get(view->emv,
			     "width", &width,
			     NULL);
		width = MAX(width, allocation->width);
		gnome_canvas_set_scroll_region (GNOME_CANVAS (view), 0, 0, width - 1, allocation->height - 1);
	}
}

static void
e_minicard_view_widget_reflow(ECanvas *canvas)
{
	double width;
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET(canvas);

	if (E_CANVAS_CLASS(parent_class)->reflow)
		E_CANVAS_CLASS(parent_class)->reflow (canvas);

	g_object_get(view->emv,
		     "width", &width,
		     NULL);
	width = MAX(width, GTK_WIDGET(canvas)->allocation.width);
	gnome_canvas_set_scroll_region(GNOME_CANVAS(canvas), 0, 0, width - 1, GTK_WIDGET(canvas)->allocation.height - 1);
}

ESelectionModel *
e_minicard_view_widget_get_selection_model (EMinicardViewWidget *view)
{
	if (view->emv)
		return E_SELECTION_MODEL (E_REFLOW (view->emv)->selection);
	else
		return NULL;
}

EMinicardView *
e_minicard_view_widget_get_view             (EMinicardViewWidget       *view)
{
	if (view->emv)
		return E_MINICARD_VIEW (view->emv);
	else
		return NULL;
}
