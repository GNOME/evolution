/* Evolution calendar - Week day picker widget
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <libgnomeui/gnome-canvas-text.h>
#include <libgnome/gnome-i18n.h>
#include "weekday-picker.h"



#define PADDING 2

/* Private part of the WeekdayPicker structure */
struct _WeekdayPickerPrivate {
	/* Selected days; see weekday_picker_set_days() */
	guint8 day_mask;

	/* Metrics */
	int font_ascent, font_descent;
	int max_letter_width;

	/* Components */
	GnomeCanvasItem *boxes[7];
	GnomeCanvasItem *labels[7];

	/* Whether the week starts on Monday or Sunday */
	guint week_starts_on_monday : 1;
};



/* Signal IDs */
enum {
	CHANGED,
	LAST_SIGNAL
};

static void weekday_picker_class_init (WeekdayPickerClass *class);
static void weekday_picker_init (WeekdayPicker *wp);
static void weekday_picker_finalize (GtkObject *object);

static void weekday_picker_realize (GtkWidget *widget);
static void weekday_picker_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void weekday_picker_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void weekday_picker_style_set (GtkWidget *widget, GtkStyle *previous_style);

static GnomeCanvasClass *parent_class;

static guint wp_signals[LAST_SIGNAL];



/**
 * weekday_picker_get_type:
 * 
 * Registers the #WeekdayPicker class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #WeekdayPicker class.
 **/
GtkType
weekday_picker_get_type (void)
{
	static GtkType weekday_picker_type = 0;

	if (!weekday_picker_type) {
		static const GtkTypeInfo weekday_picker_info = {
			"WeekdayPicker",
			sizeof (WeekdayPicker),
			sizeof (WeekdayPickerClass),
			(GtkClassInitFunc) weekday_picker_class_init,
			(GtkObjectInitFunc) weekday_picker_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		weekday_picker_type = gtk_type_unique (GNOME_TYPE_CANVAS, &weekday_picker_info);
	}

	return weekday_picker_type;
}

/* Class initialization function for the weekday picker */
static void
weekday_picker_class_init (WeekdayPickerClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (GNOME_TYPE_CANVAS);

	wp_signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (WeekdayPickerClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, wp_signals, LAST_SIGNAL);

	object_class->finalize = weekday_picker_finalize;

	widget_class->realize = weekday_picker_realize;
	widget_class->size_request = weekday_picker_size_request;
	widget_class->size_allocate = weekday_picker_size_allocate;
	widget_class->style_set = weekday_picker_style_set;

	class->changed = NULL;
}

/* Event handler for the day items */
static gint
day_event_cb (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;
	int i;
	guint8 day_mask;

	wp = WEEKDAY_PICKER (data);
	priv = wp->priv;

	if (!(event->type == GDK_BUTTON_PRESS && event->button.button == 1))
		return FALSE;

	/* Find which box was clicked */

	for (i = 0; i < 7; i++)
		if (priv->boxes[i] == item || priv->labels[i] == item)
			break;

	g_assert (i != 7);

	/* Turn on that day */

	if (priv->week_starts_on_monday) {
		if (i == 6)
			i = 0;
		else
			i++;
	}

	if (priv->day_mask & (0x1 << i))
		day_mask = priv->day_mask & ~(0x1 << i);
	else
		day_mask = priv->day_mask | (0x1 << i);

	weekday_picker_set_days (wp, day_mask);

	return TRUE;
}


/* Creates the canvas items for the weekday picker.  The items are empty until
 * they are configured elsewhere.
 */
static void
create_items (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;
	GnomeCanvasGroup *parent;
	int i;

	priv = wp->priv;

	parent = gnome_canvas_root (GNOME_CANVAS (wp));

	for (i = 0; i < 7; i++) {
		priv->boxes[i] = gnome_canvas_item_new (parent,
							GNOME_TYPE_CANVAS_RECT,
							NULL);
		gtk_signal_connect (GTK_OBJECT (priv->boxes[i]), "event",
				    GTK_SIGNAL_FUNC (day_event_cb),
				    wp);

		priv->labels[i] = gnome_canvas_item_new (parent,
							 GNOME_TYPE_CANVAS_TEXT,
							 NULL);
		gtk_signal_connect (GTK_OBJECT (priv->labels[i]), "event",
				    GTK_SIGNAL_FUNC (day_event_cb),
				    wp);
	}
}

/* Object initialization function for the weekday picker */
static void
weekday_picker_init (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	priv = g_new0 (WeekdayPickerPrivate, 1);

	wp->priv = priv;

	create_items (wp);
}

/* Finalize handler for the weekday picker */
static void
weekday_picker_finalize (GtkObject *object)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (object));

	wp = WEEKDAY_PICKER (object);
	priv = wp->priv;

	g_free (priv);
	wp->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
colorize_items (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;
	GdkColor *outline;
	GdkColor *fill, *sel_fill;
	GdkColor *text_fill, *sel_text_fill;
	int i;

	priv = wp->priv;

	outline = &GTK_WIDGET (wp)->style->fg[GTK_WIDGET_STATE (wp)];

	fill = &GTK_WIDGET (wp)->style->base[GTK_WIDGET_STATE (wp)];
	text_fill = &GTK_WIDGET (wp)->style->fg[GTK_WIDGET_STATE (wp)];

	sel_fill = &GTK_WIDGET (wp)->style->bg[GTK_STATE_SELECTED];
	sel_text_fill = &GTK_WIDGET (wp)->style->fg[GTK_STATE_SELECTED];

	if (priv->week_starts_on_monday) {
		GdkColor *f, *t;

		for (i = 1; i < 7; i++) {
			if (priv->day_mask & (0x1 << i)) {
				f = sel_fill;
				t = sel_text_fill;
			} else {
				f = fill;
				t = text_fill;
			}

			gnome_canvas_item_set (priv->boxes[i - 1],
					       "fill_color_gdk", f,
					       "outline_color_gdk", outline,
					       NULL);

			gnome_canvas_item_set (priv->labels[i - 1],
					       "fill_color_gdk", t,
					       NULL);
		}

		if (priv->day_mask & (0x1 << 0)) {
			f = sel_fill;
			t = sel_text_fill;
		} else {
			f = fill;
			t = text_fill;
		}
		
		gnome_canvas_item_set (priv->boxes[6],
				       "fill_color_gdk", f,
				       "outline_color_gdk", outline,
				       NULL);

		gnome_canvas_item_set (priv->labels[6],
				       "fill_color_gdk", t,
				       NULL);
	} else {
		GdkColor *f, *t;

		for (i = 0; i < 7; i++) {
			if (priv->day_mask & (0x1 << i)) {
				f = sel_fill;
				t = sel_text_fill;
			} else {
				f = fill;
				t = text_fill;
			}

			gnome_canvas_item_set (priv->boxes[i],
					       "fill_color_gdk", f,
					       "outline_color_gdk", outline,
					       NULL);

			gnome_canvas_item_set (priv->labels[i],
					       "fill_color_gdk", t,
					       NULL);
		}
	}
}

/* Configures the items in the weekday picker by setting their attributes. */
static void
configure_items (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;
	int width, height;
	int box_width;
	const char *str;
	int i;

	priv = wp->priv;

	width = GTK_WIDGET (wp)->allocation.width;
	height = GTK_WIDGET (wp)->allocation.height;

	box_width = (width - 1) / 7;

	if (priv->week_starts_on_monday)
		str = _("MTWTFSS");
	else
		str = _("SMTWTFS");

	for (i = 0; i < 7; i++) {
		char *c;

		gnome_canvas_item_set (priv->boxes[i],
				       "x1", (double) (i * box_width),
				       "y1", (double) 0,
				       "x2", (double) ((i + 1) * box_width),
				       "y2", (double) (height - 1),
				       "width_pixels", 0,
				       NULL);

		c = g_strndup (str + i, 1);
		gnome_canvas_item_set (priv->labels[i],
				       "text", c,
				       "font_gdk", GTK_WIDGET (wp)->style->font,
				       "x", (double) (i * box_width) + box_width / 2.0,
				       "y", (double) (1 + PADDING),
				       "anchor", GTK_ANCHOR_N,
				       NULL);
		g_free (c);
	}

	colorize_items (wp);
}

/* Realize handler for the weekday picker */
static void
weekday_picker_realize (GtkWidget *widget)
{
	WeekdayPicker *wp;

	wp = WEEKDAY_PICKER (widget);

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	configure_items (wp);
}

/* Size_request handler for the weekday picker */
static void
weekday_picker_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;

	wp = WEEKDAY_PICKER (widget);
	priv = wp->priv;

	requisition->width = (priv->max_letter_width + 2 * PADDING + 1) * 7 + 1;
	requisition->height = (priv->font_ascent + priv->font_descent + 2 * PADDING + 2);
}

/* Size_allocate handler for the weekday picker */
static void
weekday_picker_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	WeekdayPicker *wp;

	wp = WEEKDAY_PICKER (widget);

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (wp),
					0, 0, allocation->width, allocation->height);

	configure_items (wp);
}

/* Style_set handler for the weekday picker */
static void
weekday_picker_style_set (GtkWidget *widget, GtkStyle *previous_style)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;
	int max_width;
	const char *str;
	int i, len;

	wp = WEEKDAY_PICKER (widget);
	priv = wp->priv;

	priv->font_ascent = widget->style->font->ascent;
	priv->font_descent = widget->style->font->descent;

	max_width = 0;

	str = _("SMTWTFS");
	len = strlen (str);

	for (i = 0; i < len; i++) {
		int w;

		w = gdk_char_measure (widget->style->font, str[i]);
		if (w > max_width)
			max_width = w;
	}

	priv->max_letter_width = max_width;

	configure_items (wp);

	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(* GTK_WIDGET_CLASS (parent_class)->style_set) (widget, previous_style);
}



/**
 * weekday_picker_new:
 * @void: 
 * 
 * Creates a new weekday picker widget.
 * 
 * Return value: A newly-created weekday picker.
 **/
GtkWidget *
weekday_picker_new (void)
{
	return gtk_type_new (TYPE_WEEKDAY_PICKER);
}

/**
 * weekday_picker_set_days:
 * @wp: A weekday picker.
 * @day_mask: Bitmask with the days to be selected.
 * 
 * Sets the days that are selected in a weekday picker.  In the @day_mask,
 * Sunday is bit 0, Monday is bit 1, etc.
 **/
void
weekday_picker_set_days (WeekdayPicker *wp, guint8 day_mask)
{
	WeekdayPickerPrivate *priv;

	g_return_if_fail (wp != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (wp));

	priv = wp->priv;

	priv->day_mask = day_mask;
	colorize_items (wp);

	gtk_signal_emit (GTK_OBJECT (wp), wp_signals[CHANGED]);
}

/**
 * weekday_picker_get_days:
 * @wp: A weekday picker.
 * 
 * Queries the days that are selected in a weekday picker.
 * 
 * Return value: Bit mask of selected days.  Sunday is bit 0, Monday is bit 1,
 * etc.
 **/
guint8
weekday_picker_get_days (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	g_return_val_if_fail (wp != NULL, 0);
	g_return_val_if_fail (IS_WEEKDAY_PICKER (wp), 0);

	priv = wp->priv;
	return priv->day_mask;
}

/**
 * weekday_picker_set_week_starts_on_monday:
 * @wp: A weekday picker.
 * @on_monday: Whether weeks start on Monday.
 * 
 * Sets whether a weekday picker should display weeks as starting on monday.
 * The default setting is to make Sunday the first day of the week.
 **/
void
weekday_picker_set_week_starts_on_monday (WeekdayPicker *wp, gboolean on_monday)
{
	WeekdayPickerPrivate *priv;

	g_return_if_fail (wp != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (wp));

	priv = wp->priv;
	priv->week_starts_on_monday = on_monday ? TRUE : FALSE;

	configure_items (wp);
}

/**
 * weekday_picker_get_week_starts_on_monday:
 * @wp: A weekday picker.
 * 
 * Queries whether a weekday picker is set to show weeks as starting on Monday.
 * 
 * Return value: TRUE if weeks start on monday, FALSE if on Sunday.
 **/
gboolean
weekday_picker_get_week_starts_on_monday (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	g_return_val_if_fail (wp != NULL, FALSE);
	g_return_val_if_fail (IS_WEEKDAY_PICKER (wp), FALSE);

	priv = wp->priv;
	return priv->week_starts_on_monday;
}
