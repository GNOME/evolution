/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <e-util/e-util.h>
#include "e-calendar.h"

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
static void e_calendar_draw		(GtkWidget	*widget,
					 GdkRectangle	*area);
static gint e_calendar_focus_in		(GtkWidget	*widget,
					 GdkEventFocus	*event);
static gint e_calendar_focus_out	(GtkWidget	*widget,
					 GdkEventFocus	*event);
static gint e_calendar_key_press	(GtkWidget	*widget,
					 GdkEventKey	*event);
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

	parent_class = gtk_type_class (E_CANVAS_TYPE);
	grandparent_class = gtk_type_class (GTK_TYPE_LAYOUT);

	object_class->destroy = e_calendar_destroy;

	widget_class->realize		   = e_calendar_realize;
	widget_class->style_set		   = e_calendar_style_set;
 	widget_class->size_request	   = e_calendar_size_request;
 	widget_class->size_allocate	   = e_calendar_size_allocate;
	widget_class->draw		   = e_calendar_draw;
	widget_class->focus_in_event	   = e_calendar_focus_in;
	widget_class->focus_out_event	   = e_calendar_focus_out;
	widget_class->key_press_event	   = e_calendar_key_press;
	widget_class->drag_motion	   = e_calendar_drag_motion;
	widget_class->drag_leave	   = e_calendar_drag_leave;
}


static void
e_calendar_init (ECalendar *cal)
{
	GnomeCanvasGroup *canvas_group;
	GdkFont *small_font;
	GtkWidget *button, *arrow;

	/* Create the small font. */
	small_font = gdk_font_load (E_CALENDAR_SMALL_FONT);
	if (!small_font)
		small_font = gdk_font_load (E_CALENDAR_SMALL_FONT_FALLBACK);
	if (!small_font)
		g_warning ("Couldn't load font");

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (cal)->root);

	cal->calitem = E_CALENDAR_ITEM (gnome_canvas_item_new (canvas_group,
							       e_calendar_item_get_type (),
							       "week_number_font", small_font,
							       NULL));

	if (small_font)
		gdk_font_unref (small_font);


	/* Create the 'Today' and 'None' buttons but don't show them. */
	cal->today_button = gtk_button_new_with_label (_("Today"));
	cal->today_button_item = gnome_canvas_item_new (canvas_group,
							gnome_canvas_widget_get_type (),
							"widget", cal->today_button,
							NULL);
	gnome_canvas_item_hide (cal->today_button_item);


	cal->none_button = gtk_button_new_with_label (_("None"));
	cal->none_button_item = gnome_canvas_item_new (canvas_group,
						       gnome_canvas_widget_get_type (),
						       "widget", cal->none_button,
						       NULL);
	gnome_canvas_item_hide (cal->none_button_item);


	/* Create the arrow buttons to move to the previous/next month. */
	button = gtk_button_new ();
	/* FIXME: The buttons doesn't display properly if we do this. */
	/*gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);*/
	gtk_widget_show (button);
	gtk_signal_connect_object (GTK_OBJECT (button), "pressed",
				   GTK_SIGNAL_FUNC (e_calendar_on_prev_pressed),
				   GTK_OBJECT (cal));
	gtk_signal_connect_object (GTK_OBJECT (button), "released",
				   GTK_SIGNAL_FUNC (e_calendar_on_prev_released),
				   GTK_OBJECT (cal));
	arrow = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_container_add (GTK_CONTAINER (button), arrow);
	cal->prev_item = gnome_canvas_item_new (canvas_group,
						gnome_canvas_widget_get_type (),
						"widget", button,
						NULL);

	button = gtk_button_new ();
	/*gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);*/
	gtk_widget_show (button);
	gtk_signal_connect_object (GTK_OBJECT (button), "pressed",
				   GTK_SIGNAL_FUNC (e_calendar_on_next_pressed),
				   GTK_OBJECT (cal));
	gtk_signal_connect_object (GTK_OBJECT (button), "released",
				   GTK_SIGNAL_FUNC (e_calendar_on_next_released),
				   GTK_OBJECT (cal));
	arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_container_add (GTK_CONTAINER (button), arrow);
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
}


static void
e_calendar_realize (GtkWidget *widget)
{
	(*GTK_WIDGET_CLASS (parent_class)->realize) (widget);
}


static void
e_calendar_style_set		(GtkWidget	*widget,
				 GtkStyle	*previous_style)
{

	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(*GTK_WIDGET_CLASS (parent_class)->style_set) (widget,
							       previous_style);
}


static void
e_calendar_size_request		(GtkWidget      *widget,
				 GtkRequisition *requisition)
{
	ECalendar *cal;
	GtkStyle *style;
	gint col_width, row_height, width, height;
	gboolean today_button_shown, none_button_shown;
	gint buttons_shown = 0, button_height, button_width, buttons_width;
	GtkRequisition today_button_requisition = { 0 };
	GtkRequisition none_button_requisition = { 0 };
	gint today_button_width = 0, today_button_height = 0;
	gint none_button_width = 0, none_button_height = 0;

	cal = E_CALENDAR (widget);
	style = GTK_WIDGET (cal)->style;

	gtk_object_get (GTK_OBJECT (cal->calitem),
			"row_height", &row_height,
			"column_width", &col_width,
			NULL);

	height = row_height * cal->min_rows;
	width = col_width * cal->min_cols;

	/* Add on space for line & button if shown. */
	today_button_shown = GTK_WIDGET_VISIBLE (cal->today_button);
	none_button_shown = GTK_WIDGET_VISIBLE (cal->none_button);

	if (today_button_shown || none_button_shown) {
		/* Note that we use the buttons' requisition fields directly
		   since we want the sizes without any usize modification. */
		if (today_button_shown) {
			gtk_widget_size_request (cal->today_button,
						 &today_button_requisition);
			today_button_width = cal->today_button->requisition.width;
			today_button_height = cal->today_button->requisition.height;
			buttons_shown++;
		}
		if (none_button_shown) {
			gtk_widget_size_request (cal->none_button,
						 &none_button_requisition);
			none_button_width = cal->none_button->requisition.width;
			none_button_height = cal->none_button->requisition.height;
			buttons_shown++;
		}

		button_height = MAX (today_button_height, none_button_height);
		height += E_CALENDAR_YPAD_ABOVE_LOWER_BUTTONS + button_height
			+ E_CALENDAR_YPAD_BELOW_LOWER_BUTTONS;

		button_width = MAX (today_button_width, none_button_width);
		button_width += E_CALENDAR_IXPAD_BUTTONS;
		buttons_width = buttons_shown * button_width
			+ (buttons_shown - 1) * E_CALENDAR_XPAD_BUTTONS;

		width = MAX (width, buttons_width);
	}

	requisition->width = width + style->klass->xthickness * 2;
	requisition->height = height + style->klass->ythickness * 2;
}


static void
e_calendar_size_allocate	(GtkWidget	*widget,
				 GtkAllocation	*allocation)
{
	ECalendar *cal;
	GdkFont *font;
	gdouble old_x2, old_y2, new_x2, new_y2;
	gdouble buttons_x1, buttons_y1;
	gdouble button_width, button_height;
	gdouble buttons_width = 0, buttons_height = 0;
	gdouble xthickness, ythickness, arrow_button_size;
	gboolean today_button_shown, none_button_shown;
	gint buttons_shown = 0;
	GtkRequisition today_button_requisition = { 0 };
	GtkRequisition none_button_requisition = { 0 };
	gint today_button_width = 0, today_button_height = 0;
	gint none_button_width = 0, none_button_height = 0;

	cal = E_CALENDAR (widget);
	font = widget->style->font;
	xthickness = widget->style->klass->xthickness;
	ythickness = widget->style->klass->ythickness;

	(*GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	/* Set the scroll region to its allocated size, if changed. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (cal),
					NULL, NULL, &old_x2, &old_y2);
	new_x2 = widget->allocation.width - 1;
	new_y2 = widget->allocation.height - 1;
	if (old_x2 != new_x2 || old_y2 != new_y2)
		gnome_canvas_set_scroll_region (GNOME_CANVAS (cal),
						0, 0, new_x2, new_y2);

	/* Set the positions of the Today & None buttons if shown. */
	today_button_shown = GTK_WIDGET_VISIBLE (cal->today_button);
	none_button_shown = GTK_WIDGET_VISIBLE (cal->none_button);

	if (today_button_shown || none_button_shown) {
		if (today_button_shown) {
			gtk_widget_size_request (cal->today_button,
						 &today_button_requisition);
			today_button_width = cal->today_button->requisition.width;
			today_button_height = cal->today_button->requisition.height;
			buttons_shown++;
		}
		if (none_button_shown) {
			gtk_widget_size_request (cal->none_button,
						 &none_button_requisition);
			none_button_width = cal->none_button->requisition.width;
			none_button_height = cal->none_button->requisition.height;
			buttons_shown++;
		}

		button_height = MAX (today_button_height, none_button_height);
		buttons_height = E_CALENDAR_YPAD_ABOVE_LOWER_BUTTONS
			+ button_height	+ E_CALENDAR_YPAD_BELOW_LOWER_BUTTONS;

		button_width = MAX (today_button_width, none_button_width);
		button_width += E_CALENDAR_IXPAD_BUTTONS;
		buttons_width = buttons_shown * button_width
			+ (buttons_shown - 1) * E_CALENDAR_XPAD_BUTTONS;

		buttons_x1 = (new_x2 + 1 - buttons_width) / 2;
		buttons_x1 = MAX (0, buttons_x1);
			
		buttons_y1 = new_y2 + 1 - E_CALENDAR_YPAD_BELOW_LOWER_BUTTONS
			- button_height - ythickness;
		buttons_y1 = MAX (0, buttons_y1);

		gnome_canvas_item_set (cal->today_button_item,
				       "x", buttons_x1,
				       "y", buttons_y1,
				       "width", (gdouble) button_width,
				       "height", (gdouble) button_height,
				       NULL);

		if (today_button_shown)
			buttons_x1 += button_width + E_CALENDAR_XPAD_BUTTONS;

		gnome_canvas_item_set (cal->none_button_item,
				       "x", buttons_x1,
				       "y", buttons_y1,
				       "width", (gdouble) button_width,
				       "height", (gdouble) button_height,
				       NULL);
	}

	/* Take off space for line & buttons if shown. */
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (cal->calitem),
			       "x1", 0.0,
			       "y1", 0.0,
			       "x2", new_x2,
			       "y2", new_y2,
			       "buttons_space", buttons_height,
			       NULL);


	/* Position the arrow buttons. */
	arrow_button_size = font->ascent + font->descent
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
}


static void
e_calendar_draw			(GtkWidget	*widget,
				 GdkRectangle	*area)
{
	ECalendar *cal;

	cal = E_CALENDAR (widget);

	(*GTK_WIDGET_CLASS (parent_class)->draw) (widget, area);

	/* GnomeCanvas bug workaround to draw the GnomeCanvasWidgets. */
#if 0
	(*GTK_WIDGET_CLASS (grandparent_class)->draw) (widget, area);
#endif
}


static gint
e_calendar_focus_in		(GtkWidget	*widget,
				 GdkEventFocus	*event)
{
	return FALSE;
}


static gint
e_calendar_focus_out		(GtkWidget	*widget,
				 GdkEventFocus	*event)
{
	return FALSE;
}


static gint
e_calendar_key_press		(GtkWidget	*widget,
				 GdkEventKey	*event)
{
	return FALSE;
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
		*top    = style->klass->ythickness;
		*bottom = style->klass->ythickness
			+ cal->calitem->buttons_space;
		*left   = style->klass->xthickness;
		*right  = style->klass->xthickness;
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


/* Set which, if any, of the buttons to show beneath the month displays.
   You must connect to the button's clicked signal and perform the
   required action yourself. */
void
e_calendar_set_buttons	(ECalendar	*cal,
			 gboolean	 show_today_button,
			 gboolean	 show_none_button)
{
	if (show_today_button) {
		gtk_widget_show (cal->today_button);
		gnome_canvas_item_show (cal->today_button_item);
	} else {
		gtk_widget_hide (cal->today_button);
		gnome_canvas_item_hide (cal->today_button_item);
	}

	if (show_none_button) {
		gtk_widget_show (cal->none_button);
		gnome_canvas_item_show (cal->none_button_item);
	} else {
		gtk_widget_hide (cal->none_button);
		gnome_canvas_item_hide (cal->none_button_item);
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

