/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary.c
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

#include <gal/util/e-util.h>
#include <gal/widgets/e-font.h>

#include <gal/e-text/e-text.h>

#include "e-summary-subwindow.h"
#include "e-summary-titlebar.h"

#define PARENT_TYPE (gnome_canvas_group_get_type ())
#define TITLEBAR_BORDER_WIDTH 2

static void e_summary_subwindow_destroy (GtkObject *object);
static void e_summary_subwindow_class_init (GtkObjectClass *object_class);
static void e_summary_subwindow_init (GtkObject *object);

static GnomeCanvasGroupClass *parent_class;

struct _ESummarySubwindowPrivate {
  GnomeCanvasItem *titlebar;
  GnomeCanvasItem *contents;

  GtkWidget *container;

  char *title;
};

enum {
  ARG_0,
  ARG_X,
  ARG_Y,
  ARG_WIDTH,
  ARG_HEIGHT,
  ARG_STATE
};

enum {
  CLOSE_CLICKED,
  SHADE_CLICKED,
  EDIT_CLICKED,
  LAST_SIGNAL
};

static guint32 e_summary_subwindow_signals[LAST_SIGNAL] = { 0 };

static void
e_summary_subwindow_destroy (GtkObject *object)
{
  ESummarySubwindow *subwindow = E_SUMMARY_SUBWINDOW (object);
  ESummarySubwindowPrivate *priv;

  priv = subwindow->private;

  if (priv == NULL)
    return;

  if (priv->container) {
    gtk_widget_destroy (priv->container);
    priv->container = NULL;
  }

  if (priv->title) {
    g_free (priv->title);
    priv->title = NULL;
  }

  g_free (subwindow->private);
  subwindow->private = NULL;

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
  (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
e_summary_subwindow_realize (GnomeCanvasItem *item)
{
  if (GNOME_CANVAS_ITEM_CLASS (parent_class)->realize)
    (* GNOME_CANVAS_ITEM_CLASS (parent_class)->realize) (item);
}

static void
e_summary_subwindow_unrealize (GnomeCanvasItem *item)
{
  if (GNOME_CANVAS_ITEM_CLASS (parent_class)->unrealize)
    (* GNOME_CANVAS_ITEM_CLASS (parent_class)->unrealize) (item);
}

static void
e_summary_subwindow_class_init (GtkObjectClass *object_class)
{
  GnomeCanvasItemClass *item_class;

  item_class = (GnomeCanvasItemClass *) object_class;

  object_class->destroy = e_summary_subwindow_destroy;

  item_class->realize = e_summary_subwindow_realize;
  item_class->unrealize = e_summary_subwindow_unrealize;

  parent_class = gtk_type_class (PARENT_TYPE);
}

static void
e_summary_subwindow_init (GtkObject *object)
{
  ESummarySubwindow *subwindow = E_SUMMARY_SUBWINDOW (object);
  ESummarySubwindowPrivate *priv;

  subwindow->private = g_new0 (ESummarySubwindowPrivate, 1);
  priv = subwindow->private;

  priv->title = NULL;
}

E_MAKE_TYPE (e_summary_subwindow, "ESummarySubwindow", ESummarySubwindow,
	     e_summary_subwindow_class_init, e_summary_subwindow_init,
	     PARENT_TYPE);

static void
container_size_allocate (GtkWidget *widget,
			 GtkAllocation *allocation,
			 ESummarySubwindow *subwindow)
{
  ESummarySubwindowPrivate *priv;

  g_return_if_fail (subwindow != NULL);
  g_return_if_fail (IS_E_SUMMARY_SUBWINDOW (subwindow));

  priv = subwindow->private;

  gnome_canvas_item_set (priv->titlebar,
			 "width", (double) allocation->width - 1,
			 NULL);

}

static void
edit_cb (GnomeCanvasItem *item,
	 ESummarySubwindow *subwindow)
{
	g_print ("EDIT!\n");
}

static void
shade_cb (GnomeCanvasItem *item,
	  ESummarySubwindow *subwindow)
{
	g_print ("SHADE!\n");
}

static void
close_cb (GnomeCanvasItem *item,
	  ESummarySubwindow *subwindow)
{
	g_print ("CLOSE!\n");
	gtk_object_destroy (GTK_OBJECT (subwindow));
}

void
e_summary_subwindow_construct (GnomeCanvasItem *item)
{
  GnomeCanvasGroup *group;
  ESummarySubwindow *subwindow;
  ESummarySubwindowPrivate *priv;
  EFont *font;
  int titlebar_height;

  g_return_if_fail (item != NULL);
  g_return_if_fail (IS_E_SUMMARY_SUBWINDOW (item));

  subwindow = E_SUMMARY_SUBWINDOW (item);
  priv = subwindow->private;

  group = GNOME_CANVAS_GROUP (item);

  font = e_font_from_gdk_font ( ((GtkWidget *) item->canvas)->style->font);

  titlebar_height = 18 + 2 * TITLEBAR_BORDER_WIDTH; /* FIXME: Not hardcoded */

  priv->titlebar = gnome_canvas_item_new (group,
					  e_summary_titlebar_get_type (),
					  "text", "Titlebar",
					  "width", 100.0,
					  NULL);
  gtk_signal_connect (GTK_OBJECT (priv->titlebar), "edit",
		      GTK_SIGNAL_FUNC (edit_cb), subwindow);
  gtk_signal_connect (GTK_OBJECT (priv->titlebar), "shade",
		      GTK_SIGNAL_FUNC (shade_cb), subwindow);
  gtk_signal_connect (GTK_OBJECT (priv->titlebar), "close",
		      GTK_SIGNAL_FUNC (close_cb), subwindow);

  priv->container = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (priv->container), GTK_SHADOW_ETCHED_IN);
  gtk_widget_show (priv->container);
  
  priv->contents = gnome_canvas_item_new (group,
					  gnome_canvas_widget_get_type (),
					  "x", (double) 0,
					  "y", (double) titlebar_height + 1,
					  "widget", priv->container,
					  NULL);
  gtk_signal_connect (GTK_OBJECT (priv->container), "size_allocate",
		      GTK_SIGNAL_FUNC (container_size_allocate), subwindow);
  
}

GnomeCanvasItem *
e_summary_subwindow_new (GnomeCanvasGroup *parent,
			 double x,
			 double y)
{
  GnomeCanvasItem *item;

  item = gnome_canvas_item_new (parent, e_summary_subwindow_get_type (), 
				"x", x,
				"y", y,
				NULL);
  e_summary_subwindow_construct (item);

  return item;
}

/* These functions mimic the GtkContainer methods */

void
e_summary_subwindow_add (ESummarySubwindow *subwindow,
			 GtkWidget *widget)
{
  ESummarySubwindowPrivate *priv;

  g_return_if_fail (subwindow != NULL);
  g_return_if_fail (IS_E_SUMMARY_SUBWINDOW (subwindow));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = subwindow->private;

  gtk_container_add (GTK_CONTAINER (priv->container), widget);
  
}

void
e_summary_subwindow_remove (ESummarySubwindow *subwindow,
			    GtkWidget *widget)
{
  ESummarySubwindowPrivate *priv;

  g_return_if_fail (subwindow != NULL);
  g_return_if_fail (IS_E_SUMMARY_SUBWINDOW (subwindow));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = subwindow->private;

  gtk_container_remove (GTK_CONTAINER (priv->container), widget);
}

void
e_summary_subwindow_set_title (ESummarySubwindow *subwindow,
			       const char *title)
{
  ESummarySubwindowPrivate *priv;

  g_return_if_fail (subwindow != NULL);
  g_return_if_fail (IS_E_SUMMARY_SUBWINDOW (subwindow));
  g_return_if_fail (title != NULL);

  priv = subwindow->private;
  if (priv->title)
    g_free (priv->title);

  priv->title = g_strdup (title);
}
