/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-titlebar.c
 *
 * Authors: Iain Holmes <iain@helixcode.com>
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-font.h>

#include <gal/e-text/e-text.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "e-summary-titlebar.h"
#include "e-summary-title-button.h"

/* XPMS */
#include "edit.xpm"
#include "x.xpm"
#include "shade.xpm"

#define PARENT_TYPE (gnome_canvas_group_get_type ())
#define TITLEBAR_BORDER_WIDTH 2

enum {
	ARG_0,
	ARG_TEXT,
	ARG_WIDTH,
	ARG_HEIGHT
};

enum {
	EDIT,
	SHADE,
	CLOSE,
	LAST_SIGNAL
};

static void e_summary_titlebar_destroy (GtkObject *object);
static void e_summary_titlebar_class_init (GtkObjectClass *object_class);
static void e_summary_titlebar_init (GtkObject *object);

static GnomeCanvasGroupClass *parent_class;
static guint titlebar_signals[LAST_SIGNAL] = { 0 };

struct _ESummaryTitlebarPrivate {
	GnomeCanvasItem *rect;
	GnomeCanvasItem *titletext;
	
	GnomeCanvasItem *edit;
	GnomeCanvasItem *shade;
	GnomeCanvasItem *close;
	
	char *text;
	double width, height;
};

static void
e_summary_titlebar_destroy (GtkObject *object)
{
	ESummaryTitlebar *titlebar;
	ESummaryTitlebarPrivate *priv;
	
	titlebar = E_SUMMARY_TITLEBAR (object);
	priv = titlebar->private;
	
	if (priv == NULL)
		return;
	
	g_free (priv);
	titlebar->private = NULL;
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
edit_cb (GnomeCanvasItem *item,
	 ESummaryTitlebar *titlebar)
{
	gtk_signal_emit (GTK_OBJECT (titlebar), titlebar_signals[EDIT]);
}

static void
shade_cb (GnomeCanvasItem *item,
	  ESummaryTitlebar *titlebar)
{
	gtk_signal_emit (GTK_OBJECT (titlebar), titlebar_signals[SHADE]);
}

static void
close_cb (GnomeCanvasItem *item,
	  ESummaryTitlebar *titlebar)
{
	gtk_signal_emit (GTK_OBJECT (titlebar), titlebar_signals[CLOSE]);
}

static void
e_summary_titlebar_realize (GnomeCanvasItem *item)
{
	GnomeCanvasGroup *group;
	ESummaryTitlebar *titlebar;
	ESummaryTitlebarPrivate *priv;
	GdkPixbuf *pb;
	EFont *font;
	int font_height;
	
	group = GNOME_CANVAS_GROUP (item);
	titlebar = E_SUMMARY_TITLEBAR (item);
	priv = titlebar->private;
	
	font = e_font_from_gdk_font (GTK_WIDGET (item->canvas)->style->font);
	font_height = e_font_height (font);
	priv->height = 18 + 2 * TITLEBAR_BORDER_WIDTH; /* FIXME: Not hardcoded */
	
	priv->rect = gnome_canvas_item_new (group,
					    gnome_canvas_rect_get_type (),
					    "x1", 0.0,
					    "y1", 0.0,
					    "y2", (double) priv->height,
					    "x2", priv->width, 
					    "fill_color_rgba", 0x88AAFFFF,
					    NULL);
	
	pb = gdk_pixbuf_new_from_xpm_data ((const char**) x_xpm);
	priv->close = gnome_canvas_item_new (group,
					     e_summary_title_button_get_type (),
					     "x", (double) priv->width - TITLEBAR_BORDER_WIDTH - 18,
					     "y", (double) TITLEBAR_BORDER_WIDTH,
					     "pixbuf", pb,
					     NULL);
	gdk_pixbuf_unref (pb);
	gtk_signal_connect (GTK_OBJECT (priv->close), "clicked",
			    GTK_SIGNAL_FUNC (close_cb), titlebar);
	
	pb = gdk_pixbuf_new_from_xpm_data ((const char**) shade_xpm);
	priv->shade = gnome_canvas_item_new (group,
					     e_summary_title_button_get_type (),
					     "x", (double) priv->width - (TITLEBAR_BORDER_WIDTH * 2) - 36,
					     "y", (double) TITLEBAR_BORDER_WIDTH,
					     "pixbuf", pb,
					     NULL);
	gdk_pixbuf_unref (pb);
	gtk_signal_connect (GTK_OBJECT (priv->shade), "clicked",
			    GTK_SIGNAL_FUNC (shade_cb), titlebar);
	
	pb = gdk_pixbuf_new_from_xpm_data ((const char**) edit_xpm);
	priv->edit = gnome_canvas_item_new (group,
					    e_summary_title_button_get_type (),
					    "x", (double) priv->width - (TITLEBAR_BORDER_WIDTH * 3) - 54,
					    "y", (double) TITLEBAR_BORDER_WIDTH,
					    "pixbuf", pb,
					    NULL);
	gdk_pixbuf_unref (pb);
	gtk_signal_connect (GTK_OBJECT (priv->edit), "clicked",
			    GTK_SIGNAL_FUNC (edit_cb), titlebar);
	
	priv->titletext = gnome_canvas_item_new (group,
						 e_text_get_type (),
						 "text", priv->text,
						 "font_gdk", GTK_WIDGET (item->canvas)->style->font,
						 "clip_width", (double) priv->width - 
						 (TITLEBAR_BORDER_WIDTH*4)- 50,
						 "clip_height", (double) e_font_height (font),

						 "clip", TRUE,
						 "use_ellipsis", TRUE,
						 "fill_color", "black",
						 "anchor", GTK_ANCHOR_NW,
						 NULL);
	gnome_canvas_item_move (priv->titletext, TITLEBAR_BORDER_WIDTH,
				(priv->height - font_height) / 2);
	
	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->realize)
		(* GNOME_CANVAS_ITEM_CLASS (parent_class)->realize) (item);
}

static void
e_summary_titlebar_unrealize (GnomeCanvasItem *item)
{
	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->unrealize)
		(* GNOME_CANVAS_ITEM_CLASS (parent_class)->unrealize) (item);
}

static void
e_summary_titlebar_set_arg (GtkObject *object,
			    GtkArg *arg,
			    guint arg_id)
{
	ESummaryTitlebar *titlebar;
	ESummaryTitlebarPrivate *priv;
	
	titlebar = E_SUMMARY_TITLEBAR (object);
	priv = titlebar->private;
	
	switch (arg_id) {
	case ARG_TEXT:
		if (priv->text)
			g_free (priv->text);
		
		priv->text = g_strdup (GTK_VALUE_STRING (*arg));
		
		if (priv->titletext)
			gnome_canvas_item_set (priv->titletext,
					       "text", priv->text,
					       NULL);
		break;
		
	case ARG_WIDTH:
		priv->width = GTK_VALUE_DOUBLE (*arg);
		
		if (priv->rect)
			gnome_canvas_item_set (priv->rect,
					       "x2", priv->width,
					       NULL);
		if (priv->titletext)
			gnome_canvas_item_set (priv->titletext,
					       "clip_width", priv->width - 
					       (TITLEBAR_BORDER_WIDTH* 4) - 42,
					       NULL);
		break;
		
	default:
		break;
	}
}

static void
e_summary_titlebar_get_arg (GtkObject *object,
			    GtkArg *arg,
			    guint arg_id)
{
	ESummaryTitlebar *titlebar;
	ESummaryTitlebarPrivate *priv;
	
	titlebar = E_SUMMARY_TITLEBAR (object);
	priv = titlebar->private;
	
	switch (arg_id) {
	case ARG_TEXT:
		GTK_VALUE_STRING (*arg) = priv->text;
		break;
		
	case ARG_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = priv->width;
		break;
		
	case ARG_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = priv->height;
		break;
		
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static gint
e_summary_titlebar_event (GnomeCanvasItem *item,
			  GdkEvent *event)
{
	if (event->type == GDK_2BUTTON_PRESS &&
	    event->button.button == 1) {
		gtk_signal_emit (GTK_OBJECT (item), titlebar_signals[SHADE]);
		return FALSE;
	} else {
		return TRUE;
	}
}

static double
e_summary_titlebar_point (GnomeCanvasItem *item,
			  double x,
			  double y,
			  int cx,
			  int cy,
			  GnomeCanvasItem **actual_item)
{
	ESummaryTitlebar *est;
	ESummaryTitlebarPrivate *priv;
	GnomeCanvasItem *ret_item;
	double d;

	est = E_SUMMARY_TITLEBAR (item);
	priv = est->private;

	d = (* GNOME_CANVAS_ITEM_CLASS 
	     (GTK_OBJECT (priv->edit)->klass)->point) (priv->edit,
						       x, y, 
						       cx, cy,
						       &ret_item);
	if (d == 0.0) {
		*actual_item = ret_item;
		return 0.0;
	}

	d = (* GNOME_CANVAS_ITEM_CLASS 
	     (GTK_OBJECT (priv->shade)->klass)->point) (priv->shade,
							x, y,
							cx, cy,
							&ret_item);
	if (d == 0.0) {
		*actual_item = ret_item;
		return 0.0;
	}

	d = (* GNOME_CANVAS_ITEM_CLASS
	     (GTK_OBJECT (priv->close)->klass)->point) (priv->close,
							x, y,
							cx, cy,
							&ret_item);
	if (d == 0.0) {
		*actual_item = ret_item;
		return 0.0;
	}

	*actual_item = item;
	return 0.0;
}

static void
e_summary_titlebar_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class;
	
	item_class = (GnomeCanvasItemClass *) object_class;
	
	object_class->destroy = e_summary_titlebar_destroy;
	object_class->set_arg = e_summary_titlebar_set_arg;
	object_class->get_arg = e_summary_titlebar_get_arg;
	
	item_class->realize = e_summary_titlebar_realize;
	item_class->unrealize = e_summary_titlebar_unrealize;
	item_class->event = e_summary_titlebar_event;
	item_class->point = e_summary_titlebar_point;
	
	gtk_object_add_arg_type ("ESummaryTitlebar::text",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE,
				 ARG_TEXT);
	gtk_object_add_arg_type ("ESummaryTitlebar::width",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE,
				 ARG_WIDTH);
	gtk_object_add_arg_type ("ESummaryTitlebar::height",
				 GTK_TYPE_DOUBLE, GTK_ARG_READABLE,
				 ARG_HEIGHT);
	
	titlebar_signals[EDIT] = gtk_signal_new ("edit", GTK_RUN_LAST,
						 object_class->type,
						 GTK_SIGNAL_OFFSET (ESummaryTitlebarClass,
								    edit),
						 gtk_marshal_NONE__NONE,
						 GTK_TYPE_NONE, 0);
	titlebar_signals[SHADE] = gtk_signal_new ("shade", GTK_RUN_LAST,
						  object_class->type,
						  GTK_SIGNAL_OFFSET (ESummaryTitlebarClass,
								     shade),
						  gtk_marshal_NONE__NONE,
						  GTK_TYPE_NONE, 0);
	titlebar_signals[CLOSE] = gtk_signal_new ("close", GTK_RUN_LAST,
						  object_class->type,
						  GTK_SIGNAL_OFFSET (ESummaryTitlebarClass,
								     close),
						  gtk_marshal_NONE__NONE,
						  GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals (object_class, titlebar_signals,
				      LAST_SIGNAL);
	
	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
e_summary_titlebar_init (GtkObject *object)
{
	ESummaryTitlebar *titlebar;
	ESummaryTitlebarPrivate *priv;
	
	titlebar = E_SUMMARY_TITLEBAR (object);
	titlebar->private = g_new0 (ESummaryTitlebarPrivate, 1);
	priv = titlebar->private;
	
	priv->width = 100.0;
	priv->text = NULL;
	
	gdk_rgb_init ();
}

E_MAKE_TYPE (e_summary_titlebar, "ESummaryTitlebar", ESummaryTitlebar,
	     e_summary_titlebar_class_init, e_summary_titlebar_init,
	     PARENT_TYPE);

