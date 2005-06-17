/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-minicard-label.c
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

#include "e-minicard-label.h"
#include "eab-marshal.h"

#include <gtk/gtksignal.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include <libgnome/gnome-i18n.h>
#include <e-util/e-util.h>
#include <text/e-text.h>
#include <widgets/misc/e-canvas.h>
#include <widgets/misc/e-canvas-utils.h>
#include <gdk/gdkkeysyms.h>

static void e_minicard_label_init		(EMinicardLabel		 *card);
static void e_minicard_label_class_init	(EMinicardLabelClass	 *klass);
static void e_minicard_label_set_property  (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_minicard_label_get_property  (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean e_minicard_label_event (GnomeCanvasItem *item, GdkEvent *event);
static void e_minicard_label_realize (GnomeCanvasItem *item);
static void e_minicard_label_unrealize (GnomeCanvasItem *item);
static void e_minicard_label_reflow(GnomeCanvasItem *item, int flags);
static void e_minicard_label_style_set (EMinicardLabel *label, GtkStyle *previous_style);

static void e_minicard_label_resize_children( EMinicardLabel *e_minicard_label );

static void set_colors (EMinicardLabel *label);

static GnomeCanvasGroupClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_HAS_FOCUS,
	PROP_FIELD,
	PROP_FIELDNAME,
	PROP_TEXT_MODEL,
	PROP_MAX_FIELD_NAME_WIDTH,
	PROP_EDITABLE
};

enum {
	STYLE_SET,
	LAST_SIGNAL
};

static guint e_minicard_label_signals [LAST_SIGNAL] = {0, };

GType
e_minicard_label_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EMinicardLabelClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_minicard_label_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EMinicardLabel),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_minicard_label_init,
		};

		type = g_type_register_static (gnome_canvas_group_get_type (), "EMinicardLabel", &info, 0);
	}

	return type;
}

static void
e_minicard_label_class_init (EMinicardLabelClass *klass)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (klass);
	item_class = (GnomeCanvasItemClass *) klass;

	klass->style_set = e_minicard_label_style_set;

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = e_minicard_label_set_property;
	object_class->get_property = e_minicard_label_get_property;
	/*  object_class->destroy = e_minicard_label_destroy; */

	g_object_class_install_property (object_class, PROP_WIDTH,
					 g_param_spec_double ("width",
							      _("Width"),
							      /*_( */"XXX blurb" /*)*/,
							      0.0, G_MAXDOUBLE, 10.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_HEIGHT,
					 g_param_spec_double ("height",
							      _("Height"),
							      /*_( */"XXX blurb" /*)*/,
							      0.0, G_MAXDOUBLE, 10.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_HAS_FOCUS,
					 g_param_spec_boolean ("has_focus",
							       _("Has Focus"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FIELD,
					 g_param_spec_string ("field",
							      _("Field"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FIELDNAME,
					 g_param_spec_string ("fieldname",
							      _("Field Name"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_TEXT_MODEL,
					 g_param_spec_object ("text_model",
							      _("Text Model"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_TEXT_MODEL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_MAX_FIELD_NAME_WIDTH,
					 g_param_spec_double ("max_field_name_length",
							      _("Max field name length"),
							      /*_( */"XXX blurb" /*)*/,
							      -1.0, G_MAXDOUBLE, -1.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	e_minicard_label_signals [STYLE_SET] =
		g_signal_new ("style_set",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMinicardLabelClass, style_set),
			      NULL, NULL,
			      eab_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1,
			      GTK_TYPE_STYLE);

	/* GnomeCanvasItem method overrides */
	item_class->realize     = e_minicard_label_realize;
	item_class->unrealize   = e_minicard_label_unrealize;
	item_class->event       = e_minicard_label_event;
}

static void
e_minicard_label_init (EMinicardLabel *minicard_label)
{
  minicard_label->width = 10;
  minicard_label->height = 10;
  minicard_label->rect = NULL;
  minicard_label->fieldname = NULL;
  minicard_label->field = NULL;

  minicard_label->max_field_name_length = -1;

  e_canvas_item_set_reflow_callback(GNOME_CANVAS_ITEM(minicard_label), e_minicard_label_reflow);
}

static void
e_minicard_label_set_property  (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	EMinicardLabel *e_minicard_label;
	GnomeCanvasItem *item;

	e_minicard_label = E_MINICARD_LABEL (object);
	item = GNOME_CANVAS_ITEM (object);	

	switch (prop_id){
	case PROP_WIDTH:
		e_minicard_label->width = g_value_get_double (value);
		e_minicard_label_resize_children(e_minicard_label);
		e_canvas_item_request_reflow (item);
		break;
	case PROP_HAS_FOCUS:
		if (e_minicard_label->field && (g_value_get_boolean (value) != E_FOCUS_NONE))
			e_canvas_item_grab_focus(e_minicard_label->field, FALSE);
		break;
	case PROP_FIELD:
		gnome_canvas_item_set( e_minicard_label->field, "text", g_value_get_string (value), NULL );
		break;
	case PROP_FIELDNAME:
		gnome_canvas_item_set( e_minicard_label->fieldname, "text", g_value_get_string (value), NULL );
		break;
	case PROP_TEXT_MODEL:
		gnome_canvas_item_set( e_minicard_label->field, "model", g_value_get_object (value), NULL);
		break;
	case PROP_MAX_FIELD_NAME_WIDTH:
		e_minicard_label->max_field_name_length = g_value_get_double (value);
		break;
	case PROP_EDITABLE:
		e_minicard_label->editable = g_value_get_boolean (value);
		g_object_set (e_minicard_label->field, "editable", FALSE /* e_minicard_label->editable */, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_minicard_label_get_property  (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	EMinicardLabel *e_minicard_label;

	e_minicard_label = E_MINICARD_LABEL (object);

	switch (prop_id) {
	case PROP_WIDTH:
		g_value_set_double (value, e_minicard_label->width);
		break;
	case PROP_HEIGHT:
		g_value_set_double (value, e_minicard_label->height);
		break;
	case PROP_HAS_FOCUS:
		g_value_set_boolean (value, e_minicard_label->has_focus ? E_FOCUS_CURRENT : E_FOCUS_NONE);
		break;
	case PROP_FIELD:
		g_object_get_property (G_OBJECT (e_minicard_label->field),
				       "text", value);
		break;
	case PROP_FIELDNAME:
		g_object_get_property (G_OBJECT (e_minicard_label->fieldname),
				       "text", value);
		break;
	case PROP_TEXT_MODEL:
		g_object_get_property (G_OBJECT (e_minicard_label->field),
				       "model", value);
		break;
	case PROP_MAX_FIELD_NAME_WIDTH:
		g_value_set_double (value, e_minicard_label->max_field_name_length);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, e_minicard_label->editable);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_minicard_label_realize (GnomeCanvasItem *item)
{
	EMinicardLabel *e_minicard_label;
	GnomeCanvasGroup *group;

	e_minicard_label = E_MINICARD_LABEL (item);
	group = GNOME_CANVAS_GROUP( item );

	if (GNOME_CANVAS_ITEM_CLASS( parent_class )->realize)
		(* GNOME_CANVAS_ITEM_CLASS( parent_class )->realize) (item);

	e_canvas_item_request_reflow(item);

	e_minicard_label->rect =
	  gnome_canvas_item_new( group,
				 gnome_canvas_rect_get_type(),
				 "x1", (double) 0,
				 "y1", (double) 0,
				 "x2", (double) e_minicard_label->width - 1,
				 "y2", (double) e_minicard_label->height - 1,
				 "outline_color", NULL,
				 NULL );
	e_minicard_label->fieldname =
	  gnome_canvas_item_new( group,
				 e_text_get_type(),
				 "anchor", GTK_ANCHOR_NW,
				 "clip_width", (double) ( e_minicard_label->width / 2 - 4 ),
				 "clip", TRUE,
				 "use_ellipsis", TRUE,
				 "fill_color", "black",
				 "draw_background", FALSE,
				 "im_context", E_CANVAS (item->canvas)->im_context,
				 NULL );
	e_canvas_item_move_absolute(e_minicard_label->fieldname, 2, 1);

	e_minicard_label->field =
	  gnome_canvas_item_new( group,
				 e_text_get_type(),
				 "anchor", GTK_ANCHOR_NW,
				 "clip_width", (double) ( ( e_minicard_label->width + 1 ) / 2 - 4 ),
				 "clip", TRUE,
				 "use_ellipsis", TRUE,
				 "fill_color", "black",
				 "editable", FALSE, /* e_minicard_label->editable, */
				 "draw_background", FALSE,
				 "im_context", E_CANVAS (item->canvas)->im_context,
				 NULL );
	e_canvas_item_move_absolute(e_minicard_label->field, ( e_minicard_label->width / 2 + 2), 1);

	set_colors (e_minicard_label);

	e_canvas_item_request_reflow(item);
}

static void
e_minicard_label_unrealize (GnomeCanvasItem *item)
{
	EMinicardLabel *e_minicard_label;

	e_minicard_label = E_MINICARD_LABEL (item);

	if (GNOME_CANVAS_ITEM_CLASS( parent_class )->unrealize)
		(* GNOME_CANVAS_ITEM_CLASS( parent_class )->unrealize) (item);
}

static gboolean
e_minicard_label_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EMinicardLabel *e_minicard_label;
 
	e_minicard_label = E_MINICARD_LABEL (item);

	switch( event->type ) {
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_Escape) {
			GnomeCanvasItem *parent;

			e_text_cancel_editing (E_TEXT (e_minicard_label->field));

			parent = GNOME_CANVAS_ITEM (e_minicard_label)->parent;
			if (parent)
				e_canvas_item_grab_focus(parent, FALSE);
		}
		break;
	case GDK_FOCUS_CHANGE: {
		GdkEventFocus *focus_event = (GdkEventFocus *) event;

		e_minicard_label->has_focus = focus_event->in;
		set_colors (e_minicard_label);

		g_object_set (e_minicard_label->field,
			      "handle_popup", e_minicard_label->has_focus,
			      NULL);
		break;
	}
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE: 
	case GDK_MOTION_NOTIFY:
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY: {
		gboolean return_val;
		g_signal_emit_by_name(e_minicard_label->field, "event", event, &return_val);
		return return_val;
	}
	default:
		break;
	}
  
	if (GNOME_CANVAS_ITEM_CLASS( parent_class )->event)
		return (* GNOME_CANVAS_ITEM_CLASS( parent_class )->event) (item, event);
	else
		return 0;
}

static void
e_minicard_label_resize_children(EMinicardLabel *e_minicard_label)
{
	double left_width;
	if (e_minicard_label->max_field_name_length != -1 && ((e_minicard_label->width / 2) - 4 > e_minicard_label->max_field_name_length))
		left_width = e_minicard_label->max_field_name_length;
	else
		left_width = e_minicard_label->width / 2 - 4;

	gnome_canvas_item_set( e_minicard_label->fieldname,
			       "clip_width", (double) MAX ( left_width, 0 ),
			       NULL );
	gnome_canvas_item_set( e_minicard_label->field,
			       "clip_width", (double) MAX ( e_minicard_label->width - 8 - left_width, 0 ),
			       NULL );
}

static void
set_colors (EMinicardLabel *label)
{
	if ( (GTK_OBJECT_FLAGS( label ) & GNOME_CANVAS_ITEM_REALIZED) ) {
		GtkWidget *canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (label)->canvas);
		GtkStyle *style = gtk_widget_get_style (canvas);
		if (label->has_focus) {
			gnome_canvas_item_set (label->rect, 
					       "outline_color_gdk", &style->mid[GTK_STATE_SELECTED],
					       "fill_color_gdk", &style->bg[GTK_STATE_NORMAL],
					       NULL);

			gnome_canvas_item_set (label->field,
					       "fill_color_gdk", &canvas->style->text[GTK_STATE_NORMAL],
					       NULL);

			gnome_canvas_item_set (label->fieldname,
					       "fill_color_gdk", &canvas->style->text[GTK_STATE_NORMAL],
					       NULL);
		}
		else {
			gnome_canvas_item_set (label->rect,
					       "outline_color_gdk", NULL,
					       "fill_color_gdk", NULL,
					       NULL);

			gnome_canvas_item_set (label->field,
					       "fill_color_gdk", &canvas->style->text[GTK_STATE_NORMAL],
					       NULL);

			gnome_canvas_item_set (label->fieldname,
					       "fill_color_gdk", &canvas->style->text[GTK_STATE_NORMAL],
					       NULL);
		}
	}
}

static void
e_minicard_label_style_set (EMinicardLabel *label, GtkStyle *previous_style)
{
	set_colors (label);
}

static void
e_minicard_label_reflow(GnomeCanvasItem *item, int flags)
{
	EMinicardLabel *e_minicard_label = E_MINICARD_LABEL(item);
	
	gint old_height;
	gdouble text_height;
	gdouble left_width;

	old_height = e_minicard_label->height;

	g_object_get(e_minicard_label->fieldname, 
		     "text_height", &text_height,
		     NULL);

	e_minicard_label->height = text_height;


	g_object_get(e_minicard_label->field, 
		     "text_height", &text_height,
		     NULL);

	if (e_minicard_label->height < text_height)
		e_minicard_label->height = text_height;
	e_minicard_label->height += 3;

	gnome_canvas_item_set( e_minicard_label->rect,
			       "x2", (double) e_minicard_label->width - 1,
			       "y2", (double) e_minicard_label->height - 1,
			       NULL );

	gnome_canvas_item_set( e_minicard_label->fieldname,
			       "clip_height", (double) e_minicard_label->height - 3,
			       NULL );

	if (e_minicard_label->max_field_name_length != -1 && ((e_minicard_label->width / 2) - 4 > e_minicard_label->max_field_name_length))
		left_width = e_minicard_label->max_field_name_length;
	else
		left_width = e_minicard_label->width / 2 - 4;

	e_canvas_item_move_absolute(e_minicard_label->field, left_width + 6, 1);

	if (old_height != e_minicard_label->height)
		e_canvas_item_request_parent_reflow(item);
}

GnomeCanvasItem *
e_minicard_label_new(GnomeCanvasGroup *parent)
{
	GnomeCanvasItem *item = gnome_canvas_item_new(parent, e_minicard_label_get_type(), NULL);
	return item;
}

