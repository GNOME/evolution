/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * This is similar to GtkLabel but clips itself and displays '...' if it
 * can't fit inside its allocated area. The intended use is for inside buttons
 * that are a fixed size. The GtkLabel would normally display only the middle
 * part of the text, which doesn't look very good. This only supports one line
 * of text (so no wrapping/justification), without underlined characters.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-clipped-label.h"

#include <math.h>
#include <string.h>

#include <glib.h>
#include <gdk/gdki18n.h>
#include <libgnome/gnome-i18n.h>


static void e_clipped_label_class_init (EClippedLabelClass *class);
static void e_clipped_label_init (EClippedLabel *label);
static void e_clipped_label_size_request (GtkWidget      *widget,
					  GtkRequisition *requisition);
static void e_clipped_label_size_allocate (GtkWidget *widget,
					   GtkAllocation *allocation);
static gint e_clipped_label_expose (GtkWidget      *widget,
				    GdkEventExpose *event);
static void e_clipped_label_recalc_chars_displayed (EClippedLabel *label, PangoLayout *layout);
static void e_clipped_label_finalize               (GObject *object);


static GtkMiscClass *parent_class;

/* This is the string to draw when the label is clipped, e.g. '...'. */
static gchar *e_clipped_label_ellipsis;

/* Flags used in chars_displayed field. Must be negative. */
#define E_CLIPPED_LABEL_NEED_RECALC		-1
#define E_CLIPPED_LABEL_SHOW_ENTIRE_LABEL	-2


GtkType
e_clipped_label_get_type (void)
{
	static GtkType e_clipped_label_type = 0;

	if (!e_clipped_label_type){
		GtkTypeInfo e_clipped_label_info = {
			"EClippedLabel",
			sizeof (EClippedLabel),
			sizeof (EClippedLabelClass),
			(GtkClassInitFunc) e_clipped_label_class_init,
			(GtkObjectInitFunc) e_clipped_label_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		parent_class = g_type_class_ref(GTK_TYPE_MISC);
		e_clipped_label_type = gtk_type_unique (GTK_TYPE_MISC,
							&e_clipped_label_info);
	}

	return e_clipped_label_type;
}


static void
e_clipped_label_class_init (EClippedLabelClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	/* Method override */
	widget_class->size_request	= e_clipped_label_size_request;
 	widget_class->size_allocate	= e_clipped_label_size_allocate;
	widget_class->expose_event	= e_clipped_label_expose;

	object_class->finalize          = e_clipped_label_finalize;

	e_clipped_label_ellipsis = _("...");
}


static void
e_clipped_label_init (EClippedLabel *label)
{
	GTK_WIDGET_SET_FLAGS (label, GTK_NO_WINDOW);

	label->label = NULL;
	label->chars_displayed = E_CLIPPED_LABEL_NEED_RECALC;
}


/**
 * e_clipped_label_new:
 *
 * @text: The label text.
 * @Returns: A new #EClippedLabel.
 *
 * Creates a new #EClippedLabel with the given text.
 **/
GtkWidget *
e_clipped_label_new (const gchar *text)
{
	GtkWidget *label;

	label = GTK_WIDGET (gtk_type_new (e_clipped_label_get_type ()));

	if (text && *text)
		e_clipped_label_set_text (E_CLIPPED_LABEL (label), text);

	return label;
}

static PangoLayout*
build_layout (GtkWidget *widget, char *text)
{
	PangoLayout *layout;

	layout = gtk_widget_create_pango_layout (widget, text);

#ifdef FROB_FONT_DESC
	{
		/* this makes the font used a little bigger than the
		   default style for this widget, as well as makes it
		   bold...  */
		GtkStyle *style = gtk_widget_get_style (widget);
		PangoFontDescription *desc = pango_font_description_copy (style->font_desc);
		pango_font_description_set_size (desc,
						 pango_font_description_get_size (desc) * 1.2);
		
		pango_font_description_set_weight (desc, PANGO_WEIGHT_BOLD);
		pango_layout_set_font_description (layout, desc);
		pango_font_description_free (desc);
	}
#endif

	return layout;
}

static void
e_clipped_label_size_request (GtkWidget      *widget,
			      GtkRequisition *requisition)
{
	EClippedLabel *label;
	GdkFont *font;
	PangoLayout *layout;
	int height;

	g_return_if_fail (E_IS_CLIPPED_LABEL (widget));
	g_return_if_fail (requisition != NULL);
  
	label = E_CLIPPED_LABEL (widget);

	layout = build_layout (widget, label->label);
	pango_layout_get_pixel_size (layout, NULL, &height);

	requisition->width = 0;
	requisition->height = height + 2 * GTK_MISC (widget)->ypad;

	g_object_unref (layout);
}


static void
e_clipped_label_size_allocate (GtkWidget *widget,
			       GtkAllocation *allocation)
{
	EClippedLabel *label;

	label = E_CLIPPED_LABEL (widget);

	widget->allocation = *allocation;

	/* Flag that we need to recalculate how many characters to display. */
	label->chars_displayed = E_CLIPPED_LABEL_NEED_RECALC;
}


static gint
e_clipped_label_expose (GtkWidget      *widget,
			GdkEventExpose *event)
{
	EClippedLabel *label;
	GtkMisc *misc;
	gint x, y;
	gchar *tmp_str, tmp_ch;
	PangoLayout *layout;
	PangoRectangle rect;

	g_return_val_if_fail (E_IS_CLIPPED_LABEL (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
  
	label = E_CLIPPED_LABEL (widget);
	misc = GTK_MISC (widget);

	/* If the label isn't visible or has no text, just return. */
	if (!GTK_WIDGET_VISIBLE (widget) || !GTK_WIDGET_MAPPED (widget)
	    || !label->label || (*label->label == '\0'))
		return TRUE;

	layout = build_layout (widget, label->label);

	/* Recalculate the number of characters displayed, if necessary. */
	if (label->chars_displayed == E_CLIPPED_LABEL_NEED_RECALC)
		e_clipped_label_recalc_chars_displayed (label, layout);

	pango_layout_set_text (layout, label->label, strlen (label->label));

	/*
	 * GC Clipping
	 */
	gdk_gc_set_clip_rectangle (widget->style->white_gc,
				   &event->area);
	gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state],
				   &event->area);

	pango_layout_get_pixel_extents (layout, &rect, NULL);

	y = floor (widget->allocation.y + (gint)misc->ypad 
		   + (((gint)widget->allocation.height - 2 * (gint)misc->ypad
		       + (gint)PANGO_ASCENT (rect) - PANGO_DESCENT(rect))
		      * misc->yalign) + 0.5);

	if (label->chars_displayed == E_CLIPPED_LABEL_SHOW_ENTIRE_LABEL) {
		x = floor (widget->allocation.x + (gint)misc->xpad
			   + (((gint)widget->allocation.width - 
			       (gint)label->label_width - 2 * (gint)misc->xpad)
			      * misc->xalign) + 0.5);

		gdk_draw_layout (widget->window, widget->style->fg_gc[widget->state],
				 x, y, layout);
	} else {
		int layout_width;

		x = widget->allocation.x + (gint)misc->xpad;

		pango_layout_set_text (layout, label->label, label->chars_displayed);

		gdk_draw_layout (widget->window, widget->style->fg_gc[widget->state],
				 x, y, layout);

		pango_layout_get_pixel_size (layout, &layout_width, NULL);

		x = widget->allocation.x + (gint)misc->xpad
			+ label->ellipsis_x;

		pango_layout_set_text (layout, e_clipped_label_ellipsis, strlen (e_clipped_label_ellipsis));

		gdk_draw_layout (widget->window, widget->style->fg_gc[widget->state],
				 x, y, layout);
	}

	gdk_gc_set_clip_mask (widget->style->white_gc, NULL);
	gdk_gc_set_clip_mask (widget->style->fg_gc[widget->state], NULL);

	g_object_unref (layout);

	return TRUE;
}


static void
e_clipped_label_finalize (GObject *object)
{
	EClippedLabel *label;

	g_return_if_fail (E_IS_CLIPPED_LABEL (object));

	label = E_CLIPPED_LABEL(object);

	g_free (label->label);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/**
 * e_clipped_label_get_text:
 *
 * @label: An #EClippedLabel.
 * @Return: The label text.
 *
 * Returns the label text, or NULL.
 **/
gchar*
e_clipped_label_get_text (EClippedLabel	*label)
{
	g_return_val_if_fail (E_IS_CLIPPED_LABEL (label), NULL);

	return label->label;
}


/**
 * e_clipped_label_set_text:
 *
 * @label: An #EClippedLabel.
 * @text: The new label text.
 *
 * Sets the label text.
 **/
void
e_clipped_label_set_text (EClippedLabel *label,
			  const gchar *text)
{
	gint len;

	g_return_if_fail (E_IS_CLIPPED_LABEL (label));

	if (label->label != text || !label->label || !text
	    || strcmp (label->label, text)) {
		g_free (label->label);
		label->label = NULL;

		if (text)
			label->label = g_strdup (text);

		/* Reset the number of characters displayed, so it is
		   recalculated when needed. */
		label->chars_displayed = E_CLIPPED_LABEL_NEED_RECALC;

		/* We don't queue a resize, since the label should not affect
		   the widget size, but we queue a draw. */
		gtk_widget_queue_draw (GTK_WIDGET (label));
	}
}


static void
e_clipped_label_recalc_chars_displayed (EClippedLabel *label, PangoLayout *layout)
{
	gint max_width, width, ch, last_width, ellipsis_width;

	max_width = GTK_WIDGET (label)->allocation.width
		- 2 * GTK_MISC (label)->xpad;

	if (!label->label) {
		label->chars_displayed = 0;
		return;
	}

	/* See if the entire label fits in the allocated width. */
	pango_layout_get_pixel_size (layout, &label->label_width, NULL);
	if (label->label_width <= max_width) {
		label->chars_displayed = E_CLIPPED_LABEL_SHOW_ENTIRE_LABEL;
		return;
	}

	/* Calculate the width of the ellipsis string. */
	pango_layout_set_text (layout, e_clipped_label_ellipsis, strlen (e_clipped_label_ellipsis));
	pango_layout_get_pixel_size (layout, &ellipsis_width, NULL);
	max_width -= ellipsis_width;

	if (max_width <= 0) {
		label->chars_displayed = 0;
		label->ellipsis_x = 0;
		return;
	}

	/* Step through the label, adding on the widths of the
	   characters, until we can't fit any more in. */
	width = last_width = 0;
	for (ch = 0; ch < strlen (label->label); ch++) {
		pango_layout_set_text (layout, label->label, ch);
		pango_layout_get_pixel_size (layout, &width, NULL);

		if (width > max_width) {
			label->chars_displayed = ch;
			label->ellipsis_x = last_width;
			return;
		}

		last_width = width;
	}

	g_warning ("Clipped label width not exceeded as expected");
	label->chars_displayed = E_CLIPPED_LABEL_SHOW_ENTIRE_LABEL;
}

