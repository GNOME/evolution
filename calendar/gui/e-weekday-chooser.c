/*
 * e-weekday-chooser.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include "e-weekday-chooser.h"

#include <config.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include <libgnomecanvas/libgnomecanvas.h>
#include <e-util/e-util.h>

#define PADDING 2

#define E_WEEKDAY_CHOOSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEEKDAY_CHOOSER, EWeekdayChooserPrivate))

/* Private part of the EWeekdayChooser structure */
struct _EWeekdayChooserPrivate {
	/* Selected days; see weekday_chooser_set_days() */
	guint8 day_mask;

	/* Blocked days; these cannot be modified */
	guint8 blocked_day_mask;

	/* Day that defines the start of the week; 0 = Sunday, ..., 6 = Saturday */
	gint week_start_day;

	/* Current keyboard focus day */
	gint focus_day;

	/* Metrics */
	gint font_ascent, font_descent;
	gint max_letter_width;

	/* Components */
	GnomeCanvasItem *boxes[7];
	GnomeCanvasItem *labels[7];
};

enum {
	PROP_0,
	PROP_WEEK_START_DAY
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint chooser_signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (
	EWeekdayChooser,
	e_weekday_chooser,
	GNOME_TYPE_CANVAS,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static gchar *
get_day_text (gint day_index)
{
	GDateWeekday weekday;

	/* Convert from tm_wday to GDateWeekday. */
	weekday = (day_index == 0) ? G_DATE_SUNDAY : day_index;

	return g_strdup (e_get_weekday_name (weekday, TRUE));
}

static void
colorize_items (EWeekdayChooser *chooser)
{
	EWeekdayChooserPrivate *priv;
	GdkColor *outline, *focus_outline;
	GdkColor *fill, *sel_fill;
	GdkColor *text_fill, *sel_text_fill;
	GtkStateType state;
	GtkStyle *style;
	gint i;

	priv = chooser->priv;

	state = gtk_widget_get_state (GTK_WIDGET (chooser));
	style = gtk_widget_get_style (GTK_WIDGET (chooser));

	outline = &style->fg[state];
	focus_outline = &style->bg[state];

	fill = &style->base[state];
	text_fill = &style->fg[state];

	sel_fill = &style->bg[GTK_STATE_SELECTED];
	sel_text_fill = &style->fg[GTK_STATE_SELECTED];

	for (i = 0; i < 7; i++) {
		gint day;
		GdkColor *f, *t, *o;

		day = i + priv->week_start_day;
		if (day >= 7)
			day -= 7;

		if (priv->day_mask & (0x1 << day)) {
			f = sel_fill;
			t = sel_text_fill;
		} else {
			f = fill;
			t = text_fill;
		}

		if (day == priv->focus_day)
			o = focus_outline;
		else
			o = outline;

		gnome_canvas_item_set (
			priv->boxes[i],
			"fill_color_gdk", f,
			"outline_color_gdk", o,
			NULL);

		gnome_canvas_item_set (
			priv->labels[i],
			"fill_color_gdk", t,
			NULL);
	}
}

static void
configure_items (EWeekdayChooser *chooser)
{
	EWeekdayChooserPrivate *priv;
	GtkAllocation allocation;
	gint width, height;
	gint box_width;
	gint i;

	priv = chooser->priv;

	gtk_widget_get_allocation (GTK_WIDGET (chooser), &allocation);

	width = allocation.width;
	height = allocation.height;

	box_width = (width - 1) / 7;

	for (i = 0; i < 7; i++) {
		gchar *c;
		gint day;

		day = i + priv->week_start_day;
		if (day >= 7)
			day -= 7;

		gnome_canvas_item_set (
			priv->boxes[i],
			"x1", (gdouble) (i * box_width),
			"y1", (gdouble) 0,
			"x2", (gdouble) ((i + 1) * box_width),
			"y2", (gdouble) (height - 1),
			"line_width", 0.0,
			NULL);

		c = get_day_text (day);
		gnome_canvas_item_set (
			priv->labels[i],
			"text", c,
			"x", (gdouble) (i * box_width) + PADDING,
			"y", (gdouble) (1 + PADDING),
			NULL);
		g_free (c);
	}

	colorize_items (chooser);
}

static void
weekday_chooser_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEEK_START_DAY:
			e_weekday_chooser_set_week_start_day (
				E_WEEKDAY_CHOOSER (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
weekday_chooser_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEEK_START_DAY:
			g_value_set_int (
				value,
				e_weekday_chooser_get_week_start_day (
				E_WEEKDAY_CHOOSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
weekday_chooser_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_weekday_chooser_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
weekday_chooser_realize (GtkWidget *widget)
{
	EWeekdayChooser *chooser;

	chooser = E_WEEKDAY_CHOOSER (widget);

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_weekday_chooser_parent_class)->realize (widget);

	configure_items (chooser);
}

static void
weekday_chooser_size_allocate (GtkWidget *widget,
                               GtkAllocation *allocation)
{
	GtkWidgetClass *widget_class;
	EWeekdayChooser *chooser;

	chooser = E_WEEKDAY_CHOOSER (widget);

	/* Chain up to parent's size_allocate() method. */
	widget_class = GTK_WIDGET_CLASS (e_weekday_chooser_parent_class);
	widget_class->size_allocate (widget, allocation);

	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (chooser), 0, 0,
		allocation->width, allocation->height);

	configure_items (chooser);
}

static void
weekday_chooser_style_set (GtkWidget *widget,
                           GtkStyle *previous_style)
{
	GtkWidgetClass *widget_class;
	EWeekdayChooser *chooser;
	EWeekdayChooserPrivate *priv;
	gint max_width;
	gint i;
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;

	chooser = E_WEEKDAY_CHOOSER (widget);
	priv = chooser->priv;

	/* Set up Pango prerequisites */
	font_desc = gtk_widget_get_style (widget)->font_desc;
	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (
		pango_context, font_desc,
		pango_context_get_language (pango_context));
	layout = pango_layout_new (pango_context);

	priv->font_ascent =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics));
	priv->font_descent =
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));

	max_width = 0;

	for (i = 0; i < 7; i++) {
		gchar *c;
		gint w;

		c = get_day_text (i);
		pango_layout_set_text (layout, c, strlen (c));
		pango_layout_get_pixel_size (layout, &w, NULL);
		g_free (c);

		if (w > max_width)
			max_width = w;
	}

	priv->max_letter_width = max_width;

	configure_items (chooser);
	g_object_unref (layout);
	pango_font_metrics_unref (font_metrics);

	/* Chain up to parent's style_set() method. */
	widget_class = GTK_WIDGET_CLASS (e_weekday_chooser_parent_class);
	widget_class->style_set (widget, previous_style);
}

static void
weekday_chooser_get_preferred_height (GtkWidget *widget,
                                      gint *minimum_height,
                                      gint *natural_height)
{
	EWeekdayChooser *chooser;
	EWeekdayChooserPrivate *priv;

	chooser = E_WEEKDAY_CHOOSER (widget);
	priv = chooser->priv;

	*minimum_height = *natural_height =
		(priv->font_ascent + priv->font_descent + 2 * PADDING + 2);
}

static void
weekday_chooser_get_preferred_width (GtkWidget *widget,
                                     gint *minimum_width,
                                     gint *natural_width)
{
	EWeekdayChooser *chooser;
	EWeekdayChooserPrivate *priv;

	chooser = E_WEEKDAY_CHOOSER (widget);
	priv = chooser->priv;

	*minimum_width = *natural_width =
		(priv->max_letter_width + 2 * PADDING + 1) * 7 + 1;
}

static gboolean
weekday_chooser_focus (GtkWidget *widget,
                       GtkDirectionType direction)
{
	EWeekdayChooser *chooser;
	EWeekdayChooserPrivate *priv;

	g_return_val_if_fail (E_IS_WEEKDAY_CHOOSER (widget), FALSE);
	chooser = E_WEEKDAY_CHOOSER (widget);
	priv = chooser->priv;

	if (!gtk_widget_get_can_focus (widget))
		return FALSE;

	if (gtk_widget_has_focus (widget)) {
		priv->focus_day = -1;
		colorize_items (chooser);
		return FALSE;
	}

	priv->focus_day = priv->week_start_day;
	gnome_canvas_item_grab_focus (priv->boxes[priv->focus_day]);
	colorize_items (chooser);

	return TRUE;
}

static void
e_weekday_chooser_class_init (EWeekdayChooserClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EWeekdayChooserPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = weekday_chooser_set_property;
	object_class->get_property = weekday_chooser_get_property;
	object_class->constructed = weekday_chooser_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = weekday_chooser_realize;
	widget_class->size_allocate = weekday_chooser_size_allocate;
	widget_class->style_set = weekday_chooser_style_set;
	widget_class->get_preferred_height = weekday_chooser_get_preferred_height;
	widget_class->get_preferred_width = weekday_chooser_get_preferred_width;
	widget_class->focus = weekday_chooser_focus;

	g_object_class_install_property (
		object_class,
		PROP_WEEK_START_DAY,
		g_param_spec_int (
			"week-start-day",
			"Week Start Day",
			NULL,
			0,  /* Monday */
			6,  /* Sunday */
			0,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	chooser_signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EWeekdayChooserClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
day_clicked (EWeekdayChooser *chooser,
             gint index)
{
	EWeekdayChooserPrivate *priv = chooser->priv;
	guint8 day_mask;

	if (priv->blocked_day_mask & (0x1 << index))
		return;

	if (priv->day_mask & (0x1 << index))
		day_mask = priv->day_mask & ~(0x1 << index);
	else
		day_mask = priv->day_mask | (0x1 << index);

	e_weekday_chooser_set_days (chooser, day_mask);
}

static gint
handle_key_press_event (EWeekdayChooser *chooser,
                        GdkEvent *event)
{
	EWeekdayChooserPrivate *priv = chooser->priv;
	guint keyval = event->key.keyval;

	if (priv->focus_day == -1)
		priv->focus_day = priv->week_start_day;

	switch (keyval) {
		case GDK_KEY_Up:
		case GDK_KEY_Right:
			priv->focus_day += 1;
			break;
		case GDK_KEY_Down:
		case GDK_KEY_Left:
			priv->focus_day -= 1;
			break;
		case GDK_KEY_space:
		case GDK_KEY_Return:
			day_clicked (chooser, priv->focus_day);
			return TRUE;
		default:
			return FALSE;
	}

	if (priv->focus_day > 6)
		priv->focus_day = 0;
	if (priv->focus_day < 0)
		priv->focus_day = 6;

	colorize_items (chooser);
	gnome_canvas_item_grab_focus (priv->boxes[priv->focus_day]);
	return TRUE;
}

/* Event handler for the day items */
static gint
day_event_cb (GnomeCanvasItem *item,
              GdkEvent *event,
              gpointer data)
{
	EWeekdayChooser *chooser;
	EWeekdayChooserPrivate *priv;
	gint i;

	chooser = E_WEEKDAY_CHOOSER (data);
	priv = chooser->priv;

	if (event->type == GDK_KEY_PRESS)
		return handle_key_press_event (chooser, event);

	if (!(event->type == GDK_BUTTON_PRESS && event->button.button == 1))
		return FALSE;

	/* Find which box was clicked */

	for (i = 0; i < 7; i++)
		if (priv->boxes[i] == item || priv->labels[i] == item)
			break;

	g_return_val_if_fail (i != 7, TRUE);

	i += priv->week_start_day;
	if (i >= 7)
		i -= 7;

	priv->focus_day = i;
	gnome_canvas_item_grab_focus (priv->boxes[i]);
	day_clicked (chooser, i);
	return TRUE;
}

/* Creates the canvas items for the weekday chooser.
 * The items are empty until they are configured elsewhere. */
static void
create_items (EWeekdayChooser *chooser)
{
	EWeekdayChooserPrivate *priv;
	GnomeCanvasGroup *parent;
	gint i;

	priv = chooser->priv;

	parent = gnome_canvas_root (GNOME_CANVAS (chooser));

	for (i = 0; i < 7; i++) {
		priv->boxes[i] = gnome_canvas_item_new (
			parent,
			GNOME_TYPE_CANVAS_RECT,
			NULL);
		g_signal_connect (
			priv->boxes[i], "event",
			G_CALLBACK (day_event_cb), chooser);

		priv->labels[i] = gnome_canvas_item_new (
			parent,
			GNOME_TYPE_CANVAS_TEXT,
			NULL);
		g_signal_connect (
			priv->labels[i], "event",
			G_CALLBACK (day_event_cb), chooser);
	}
}

static void
e_weekday_chooser_init (EWeekdayChooser *chooser)
{
	chooser->priv = E_WEEKDAY_CHOOSER_GET_PRIVATE (chooser);

	create_items (chooser);
	chooser->priv->focus_day = -1;
}

/**
 * e_weekday_chooser_new:
 *
 * Creates a new #EWeekdayChooser.
 *
 * Returns: an #EWeekdayChooser
 **/
GtkWidget *
e_weekday_chooser_new (void)
{
	return g_object_new (E_TYPE_WEEKDAY_CHOOSER, NULL);
}

/**
 * e_weekday_chooser_get_days:
 * @chooser: an #EWeekdayChooser
 *
 * Queries the days that are selected in @chooser.
 *
 * Return value: Bit mask of selected days.  Sunday is bit 0, Monday is bit 1,
 * etc.
 **/
guint8
e_weekday_chooser_get_days (EWeekdayChooser *chooser)
{
	g_return_val_if_fail (E_IS_WEEKDAY_CHOOSER (chooser), 0);

	return chooser->priv->day_mask;
}

/**
 * e_weekday_chooser_set_days:
 * @chooser: an #EWeekdayChooser
 * @day_mask: Bitmask with the days to be selected.
 *
 * Sets the days that are selected in @chooser.  In the @day_mask,
 * Sunday is bit 0, Monday is bit 1, etc.
 **/
void
e_weekday_chooser_set_days (EWeekdayChooser *chooser,
                            guint8 day_mask)
{
	g_return_if_fail (E_IS_WEEKDAY_CHOOSER (chooser));

	chooser->priv->day_mask = day_mask;
	colorize_items (chooser);

	g_signal_emit (chooser, chooser_signals[CHANGED], 0);
}

/**
 * e_weekday_chooser_get_blocked_days:
 * @chooser: an #EWeekdayChooser
 *
 * Queries the set of days that the @chooser prevents from being modified
 * by the user.
 *
 * Return value: Bit mask of blocked days, with the same format as that
 * returned by e_weekday_chooser_get_days().
 **/
guint
e_weekday_chooser_get_blocked_days (EWeekdayChooser *chooser)
{
	g_return_val_if_fail (E_IS_WEEKDAY_CHOOSER (chooser), 0);

	return chooser->priv->blocked_day_mask;
}

/**
 * e_weekday_chooser_set_blocked_days:
 * @chooser: an #EWeekdayChooser
 * @blocked_day_mask: Bitmask with the days to be blocked.
 *
 * Sets the days that the @chooser will prevent from being modified by
 * the user.  The @blocked_day_mask is specified in the same way as in
 * e_weekday_chooser_set_days().
 **/
void
e_weekday_chooser_set_blocked_days (EWeekdayChooser *chooser,
                                    guint8 blocked_day_mask)
{
	g_return_if_fail (E_IS_WEEKDAY_CHOOSER (chooser));

	chooser->priv->blocked_day_mask = blocked_day_mask;
}

/**
 * e_weekday_chooser_get_week_start_day:
 * @chooser: an #EWeekdayChooser
 *
 * Queries the day that defines the start of the week in @chooser.
 *
 * Return value: Index of the day that defines the start of the week.  See
 * weekday_chooser_set_week_start_day() to see how this is represented.
 **/
gint
e_weekday_chooser_get_week_start_day (EWeekdayChooser *chooser)
{
	g_return_val_if_fail (E_IS_WEEKDAY_CHOOSER (chooser), -1);

	return chooser->priv->week_start_day;
}

/**
 * e_weekday_chooser_set_week_start_day:
 * @chooser: an #EWeekdayChooser
 * @week_start_day: Index of the day that defines the start of the week; 0 is
 * Sunday, 1 is Monday, etc.
 *
 * Sets the day that defines the start of the week for @chooser.
 **/
void
e_weekday_chooser_set_week_start_day (EWeekdayChooser *chooser,
                                      gint week_start_day)
{
	g_return_if_fail (E_IS_WEEKDAY_CHOOSER (chooser));
	g_return_if_fail (week_start_day >= 0 && week_start_day < 7);

	if (week_start_day == chooser->priv->week_start_day)
		return;

	chooser->priv->week_start_day = week_start_day;

	configure_items (chooser);

	g_object_notify (G_OBJECT (chooser), "week-start-day");
}

