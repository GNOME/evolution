/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
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
 * ECalendar - displays a table of monthly calendars, allowing highlighting
 * and selection of one or more days. Like GtkCalendar with more features.
 * Most of the functionality is in the ECalendarItem canvas item, though
 * we also add GnomeCanvasWidget buttons to go to the previous/next month and
 * to got to the current day.
 */

#include <config.h>

#include "e-calendar.h"

#include <glib.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtksignal.h>
#include <libgnomecanvas/gnome-canvas-widget.h>
#include <gal/util/e-util.h>

#define E_CALENDAR_SMALL_FONT_PTSIZE 6

#define E_CALENDAR_SMALL_FONT	\
	"-adobe-utopia-regular-r-normal-*-*-100-*-*-p-*-iso8859-*"
#define E_CALENDAR_SMALL_FONT_FALLBACK	\
	"-adobe-helvetica-medium-r-normal-*-*-80-*-*-p-*-iso8859-*"

/* The space between the arrow buttons and the edge of the widget. */
#define E_CALENDAR_ARROW_BUTTON_X_PAD	2
#define E_CALENDAR_ARROW_BUTTON_Y_PAD	0

/* Vertical padding. The padding above the button includes the space for the
   horizontal line. */
#define	E_CALENDAR_YPAD_ABOVE_LOWER_BUTTONS		4
#define	E_CALENDAR_YPAD_BELOW_LOWER_BUTTONS		3

/* Horizontal padding inside & between buttons. */
#define E_CALENDAR_IXPAD_BUTTONS			4
#define E_CALENDAR_XPAD_BUTTONS				8

/* The time between steps when the prev/next buttons is pressed, in 1/1000ths
   of a second, and the number of timeouts we skip before we start
   automatically moving back/forward. */
#define E_CALENDAR_AUTO_MOVE_TIMEOUT		150
#define E_CALENDAR_AUTO_MOVE_TIMEOUT_DELAY	2

static void e_calendar_class_init	(ECalendarClass *class);
static void e_calendar_init		(ECalendar	*cal);
static void e_calendar_destroy		(GtkObject	*object);
static void e_calendar_realize		(GtkWidget	*widget);
static void e_calendar_style_set	(GtkWidget	*widget,
					 GtkStyle	*previous_style);
static void e_calendar_size_request	(GtkWidget      *widget,
					 GtkRequisition *requisition);
static void e_calendar_size_allocate	(GtkWidget	*widget,
					 GtkAllocation	*allocation);
static gint e_calendar_drag_motion	(GtkWidget      *widget,
					 GdkDragContext *context,
					 gint            x,
					 gint            y,
					 guint           time);
static void e_calendar_drag_leave	(GtkWidget      *widget,
					 GdkDragContext *context,
					 guint           time);

static void e_calendar_on_prev_pressed	(ECalendar	*cal);
static void e_calendar_on_prev_released	(ECalendar	*cal);
static void e_calendar_on_next_pressed	(ECalendar	*cal);
static void e_calendar_on_next_released	(ECalendar	*cal);

static void e_calendar_start_auto_move	(ECalendar	*cal,
					 gboolean	 moving_forward);
static gboolean e_calendar_auto_move_handler	(gpointer	 data);
static void e_calendar_stop_auto_move	(ECalendar	*cal);

static GnomeCanvasClass *parent_class;
static GtkLayoutClass *grandparent_class;

E_MAKE_TYPE (e_calendar, "ECalendar", ECalendar,
	     e_calendar_class_init, e_calendar_init, E_CANVAS_TYPE)


static void
e_calendar_class_init (ECalendarClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = g_type_class_ref(E_CANVAS_TYPE);
	grandparent_class = g_type_class_ref(GTK_TYPE_LAYOUT);

	object_class->destroy = e_calendar_destroy;

	widget_class->realize		   = e_calendar_realize;
	widget_class->style_set		   = e_calendar_style_set;
 	widget_class->size_request	   = e_calendar_size_request;
 	widget_class->size_allocate	   = e_calendar_size_allocate;
	widget_class->drag_motion	   = e_calendar_drag_motion;
	widget_class->drag_leave	   = e_calendar_drag_leave;
}


static void
e_calendar_init (ECalendar *cal)
{
	GnomeCanvasGroup *canvas_group;
	PangoFontDescription *small_font_desc;
	GtkWidget *button, *pixmap;

	GTK_WIDGET_UNSET_FLAGS (cal, GTK_CAN_FOCUS);

	/* Create the small font. */

	small_font_desc =
		pango_font_description_copy (gtk_widget_get_style (GTK_WIDGET (cal))->font_desc);
	pango_font_description_set_size (small_font_desc,
					 E_CALENDAR_SMALL_FONT_PTSIZE * PANGO_SCALE);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (cal)->root);

	cal->calitem = E_CALENDAR_ITEM (gnome_canvas_item_new (canvas_group,
							       e_calendar_item_get_type (),
							       "week_number_font_desc", small_font_desc,
							       NULL));

	pango_font_description_free (small_font_desc);

	/* Create the arrow buttons to move to the previous/next month. */
	button = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_show (button);
	gtk_signal_connect_object (GTK_OBJECT (button), "pressed",
				   G_CALLBACK (e_calendar_on_prev_pressed),
				   GTK_OBJECT (cal));
	gtk_signal_connect_object (GTK_OBJECT (button), "released",
				   G_CALLBACK (e_calendar_on_prev_released),
				   GTK_OBJECT (cal));

	pixmap = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE);
	gtk_widget_show (pixmap);
	gtk_container_add (GTK_CONTAINER (button), pixmap);

	cal->prev_item = gnome_canvas_item_new (canvas_group,
						gnome_canvas_widget_get_type (),
						"widget", button,
						NULL);

	button = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_show (button);
	gtk_signal_connect_object (GTK_OBJECT (button), "pressed",
				   G_CALLBACK (e_calendar_on_next_pressed),
				   GTK_OBJECT (cal));
	gtk_signal_connect_object (GTK_OBJECT (button), "released",
				   G_CALLBACK (e_calendar_on_next_released),
				   GTK_OBJECT (cal));

	pixmap = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	gtk_widget_show (pixmap);
	gtk_container_add (GTK_CONTAINER (button), pixmap);

	cal->next_item = gnome_canvas_item_new (canvas_group,
						gnome_canvas_widget_get_type (),
						"widget", button,
						NULL);

	cal->min_rows = 1;
	cal->min_cols = 1;
	cal->max_rows = -1;
	cal->max_cols = -1;

	cal->timeout_id = 0;
}


/**
 * e_calendar_new:
 * @Returns: a new #ECalendar.
 *
 * Creates a new #ECalendar.
 **/
GtkWidget *
e_calendar_new			(void)
{
	GtkWidget *cal;

	cal = gtk_type_new (e_calendar_get_type ());

	return cal;
}


static void
e_calendar_destroy		(GtkObject *object)
{
	ECalendar *cal;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_CALENDAR (object));

	cal = E_CALENDAR (object);

	if (cal->timeout_id != 0) {
		gtk_timeout_remove (cal->timeout_id);
		cal->timeout_id = 0;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
e_calendar_realize (GtkWidget *widget)
{
	(*GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	/* Set the background of the canvas window to the normal color,
	   or the arrow buttons are not displayed properly. */
	gdk_window_set_background (GTK_LAYOUT (widget)->bin_window,
				   &widget->style->bg[GTK_STATE_NORMAL]);
}


static void
e_calendar_style_set		(GtkWidget	*widget,
				 GtkStyle	*previous_style)
{
	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(*GTK_WIDGET_CLASS (parent_class)->style_set) (widget,
							       previous_style);

	/* Set the background of the canvas window to the normal color,
	   or the arrow buttons are not displayed properly. */
	if (GTK_WIDGET_REALIZED (widget->parent))
		gdk_window_set_background (GTK_LAYOUT (widget)->bin_window,
					   &widget->style->bg[GTK_STATE_NORMAL]);
}


static void
e_calendar_size_request		(GtkWidget      *widget,
				 GtkRequisition *requisition)
{
	ECalendar *cal;
	GtkStyle *style;
	gint col_width, row_height, width, height;

	cal = E_CALENDAR (widget);
	style = GTK_WIDGET (cal)->style;

	g_object_get((cal->calitem),
			"row_height", &row_height,
			"column_width", &col_width,
			NULL);

	height = row_height * cal->min_rows;
	width = col_width * cal->min_cols;

	requisition->width = width + style->xthickness * 2;
	requisition->height = height + style->ythickness * 2;
}


static void
e_calendar_size_allocate	(GtkWidget	*widget,
				 GtkAllocation	*allocation)
{
	ECalendar *cal;
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	gdouble old_x2, old_y2, new_x2, new_y2;
	gdouble xthickness, ythickness, arrow_button_size;

	cal = E_CALENDAR (widget);
	xthickness = widget->style->xthickness;
	ythickness = widget->style->ythickness;

	(*GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	/* Set up Pango prerequisites */
	font_desc = gtk_widget_get_style (widget)->font_desc;
	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (pango_context, font_desc,
						  pango_context_get_language (pango_context));

	/* Set the scroll region to its allocated size, if changed. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (cal),
					NULL, NULL, &old_x2, &old_y2);
	new_x2 = widget->allocation.width - 1;
	new_y2 = widget->allocation.height - 1;
	if (old_x2 != new_x2 || old_y2 != new_y2)
		gnome_canvas_set_scroll_region (GNOME_CANVAS (cal),
						0, 0, new_x2, new_y2);

	/* Take off space for line & buttons if shown. */
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (cal->calitem),
			       "x1", 0.0,
			       "y1", 0.0,
			       "x2", new_x2,
			       "y2", new_y2,
			       NULL);


	/* Position the arrow buttons. */
	arrow_button_size =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics))
		+ PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics))
		+ E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME
		+ E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME
		- E_CALENDAR_ARROW_BUTTON_Y_PAD * 2;

	gnome_canvas_item_set (cal->prev_item,
			       "x", xthickness * 2
			       + E_CALENDAR_ARROW_BUTTON_X_PAD,
			       "y", ythickness * 2
			       + E_CALENDAR_ARROW_BUTTON_Y_PAD,
			       "width", arrow_button_size,
			       "height", arrow_button_size,
			       NULL);

	gnome_canvas_item_set (cal->next_item,
			       "x", new_x2 + 1 - xthickness * 2
			       - E_CALENDAR_ARROW_BUTTON_X_PAD
			       - arrow_button_size,
			       "y", ythickness * 2
			       + E_CALENDAR_ARROW_BUTTON_Y_PAD,
			       "width", arrow_button_size,
			       "height", arrow_button_size,
			       NULL);

	pango_font_metrics_unref (font_metrics);
}

void
e_calendar_set_minimum_size	(ECalendar	*cal,
				 gint		 rows,
				 gint		 cols)
{
	g_return_if_fail (E_IS_CALENDAR (cal));

	cal->min_rows = rows;
	cal->min_cols = cols;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (cal->calitem),
			       "minimum_rows", rows,
			       "minimum_columns", cols,
			       NULL);

	gtk_widget_queue_resize (GTK_WIDGET (cal));
}


void
e_calendar_set_maximum_size	(ECalendar	*cal,
				 gint		 rows,
				 gint		 cols)
{
	g_return_if_fail (E_IS_CALENDAR (cal));

	cal->max_rows = rows;
	cal->max_cols = cols;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (cal->calitem),
			       "maximum_rows", rows,
			       "maximum_columns", cols,
			       NULL);

	gtk_widget_queue_resize (GTK_WIDGET (cal));
}


/* Returns the border size on each side of the month displays. */
void
e_calendar_get_border_size	(ECalendar	*cal,
				 gint		*top,
				 gint		*bottom,
				 gint		*left,
				 gint		*right)
{
	GtkStyle *style;

	g_return_if_fail (E_IS_CALENDAR (cal));

	style = GTK_WIDGET (cal)->style;

	if (style) {
		*top    = style->ythickness;
		*bottom = style->ythickness;
		*left   = style->xthickness;
		*right  = style->xthickness;
	} else {
		*top = *bottom = *left = *right = 0;
	}
}


static void
e_calendar_on_prev_pressed	(ECalendar	*cal)
{
	e_calendar_start_auto_move (cal, FALSE);
}


static void
e_calendar_on_next_pressed	(ECalendar	*cal)
{
	e_calendar_start_auto_move (cal, TRUE);
}


static void
e_calendar_start_auto_move	(ECalendar	*cal,
				 gboolean	 moving_forward)
{
	ECalendarItem *calitem;
	gint offset;

	if (cal->timeout_id == 0) {
		cal->timeout_id = g_timeout_add (E_CALENDAR_AUTO_MOVE_TIMEOUT,
						 e_calendar_auto_move_handler,
						 cal);
	}
	cal->timeout_delay = E_CALENDAR_AUTO_MOVE_TIMEOUT_DELAY;
	cal->moving_forward = moving_forward;

	calitem = cal->calitem;
	offset = cal->moving_forward ? 1 : -1;
	e_calendar_item_set_first_month (calitem, calitem->year,
					 calitem->month + offset);
}


static gboolean
e_calendar_auto_move_handler	(gpointer	 data)
{
	ECalendar *cal;
	ECalendarItem *calitem;
	gint offset;

	g_return_val_if_fail (E_IS_CALENDAR (data), FALSE);

	cal = E_CALENDAR (data);
	calitem = cal->calitem;

	GDK_THREADS_ENTER ();

	if (cal->timeout_delay > 0) {
		cal->timeout_delay--;
	} else {
		offset = cal->moving_forward ? 1 : -1;
		e_calendar_item_set_first_month (calitem, calitem->year,
						 calitem->month + offset);
	}

	GDK_THREADS_LEAVE ();
	return TRUE;
}


static void
e_calendar_on_prev_released	(ECalendar	*cal)
{
	e_calendar_stop_auto_move (cal);
}


static void
e_calendar_on_next_released	(ECalendar	*cal)
{
	e_calendar_stop_auto_move (cal);
}


static void
e_calendar_stop_auto_move	(ECalendar	*cal)
{
	if (cal->timeout_id != 0) {
		gtk_timeout_remove (cal->timeout_id);
		cal->timeout_id = 0;
	}
}


static gint
e_calendar_drag_motion (GtkWidget      *widget,
			GdkDragContext *context,
			gint            x,
			gint            y,
			guint           time)
{
	ECalendar *cal;

	g_return_val_if_fail (E_IS_CALENDAR (widget), FALSE);

	cal = E_CALENDAR (widget);

#if 0
	g_print ("In e_calendar_drag_motion\n");
#endif

	return FALSE;
}


static void
e_calendar_drag_leave (GtkWidget      *widget,
		       GdkDragContext *context,
		       guint           time)
{
	ECalendar *cal;

	g_return_if_fail (E_IS_CALENDAR (widget));

	cal = E_CALENDAR (widget);

#if 0
	g_print ("In e_calendar_drag_leave\n");
#endif
}

