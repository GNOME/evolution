/*
 * e-weekday-chooser.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include <libgnomecanvas/libgnomecanvas.h>
#include <e-util/e-util.h>

#include "e-weekday-chooser.h"

#define PADDING 2

#define E_WEEKDAY_CHOOSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEEKDAY_CHOOSER, EWeekdayChooserPrivate))

/* Private part of the EWeekdayChooser structure */
struct _EWeekdayChooserPrivate {
	gboolean blocked_weekdays[8];   /* indexed by GDateWeekday */
	gboolean selected_weekdays[8];  /* indexed by GDateWeekday */

	/* Day that defines the start of the week. */
	GDateWeekday week_start_day;

	/* Current keyboard focus day */
	GDateWeekday focus_day;

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

static void
colorize_items (EWeekdayChooser *chooser)
{
	GdkColor outline, focus_outline;
	GdkColor fill, sel_fill;
	GdkColor text_fill, sel_text_fill;
	GDateWeekday weekday;
	GtkWidget *widget;
	gint ii;

	widget = GTK_WIDGET (chooser);

	e_utils_get_theme_color_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &outline);
	e_utils_get_theme_color_color (widget, "theme_bg_color", E_UTILS_DEFAULT_THEME_BG_COLOR, &focus_outline);
	e_utils_get_theme_color_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &fill);
	e_utils_get_theme_color_color (widget, "theme_fg_color", E_UTILS_DEFAULT_THEME_FG_COLOR, &text_fill);
	e_utils_get_theme_color_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &sel_fill);
	e_utils_get_theme_color_color (widget, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &sel_text_fill);

	weekday = e_weekday_chooser_get_week_start_day (chooser);

	for (ii = 0; ii < 7; ii++) {
		GdkColor *f, *t, *o;

		if (chooser->priv->selected_weekdays[weekday]) {
			f = &sel_fill;
			t = &sel_text_fill;
		} else {
			f = &fill;
			t = &text_fill;
		}

		if (weekday == chooser->priv->focus_day)
			o = &focus_outline;
		else
			o = &outline;

		gnome_canvas_item_set (
			chooser->priv->boxes[ii],
			"fill_color_gdk", f,
			"outline_color_gdk", o,
			NULL);

		gnome_canvas_item_set (
			chooser->priv->labels[ii],
			"fill_color_gdk", t,
			NULL);

		weekday = e_weekday_get_next (weekday);
	}
}

static void
configure_items (EWeekdayChooser *chooser)
{
	GtkAllocation allocation;
	gint width, height;
	gint box_width;
	GDateWeekday weekday;
	gint ii;

	gtk_widget_get_allocation (GTK_WIDGET (chooser), &allocation);

	width = allocation.width;
	height = allocation.height;

	box_width = (width - 1) / 7;

	weekday = e_weekday_chooser_get_week_start_day (chooser);

	for (ii = 0; ii < 7; ii++) {
		gnome_canvas_item_set (
			chooser->priv->boxes[ii],
			"x1", (gdouble) (ii * box_width),
			"y1", (gdouble) 0,
			"x2", (gdouble) ((ii + 1) * box_width),
			"y2", (gdouble) (height - 1),
			"line_width", 0.0,
			NULL);

		gnome_canvas_item_set (
			chooser->priv->labels[ii],
			"text", e_get_weekday_name (weekday, TRUE),
			"x", (gdouble) (ii * box_width) + PADDING,
			"y", (gdouble) (1 + PADDING),
			NULL);

		weekday = e_weekday_get_next (weekday);
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
				g_value_get_enum (value));
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
			g_value_set_enum (
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
weekday_chooser_style_updated (GtkWidget *widget)
{
	GtkWidgetClass *widget_class;
	EWeekdayChooser *chooser;
	EWeekdayChooserPrivate *priv;
	gint max_width;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;
	GDateWeekday weekday;

	chooser = E_WEEKDAY_CHOOSER (widget);
	priv = chooser->priv;

	/* Set up Pango prerequisites */
	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (
		pango_context, NULL,
		pango_context_get_language (pango_context));
	layout = pango_layout_new (pango_context);

	priv->font_ascent =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics));
	priv->font_descent =
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));

	max_width = 0;

	for (weekday = G_DATE_MONDAY; weekday <= G_DATE_SUNDAY; weekday++) {
		const gchar *name;
		gint w;

		name = e_get_weekday_name (weekday, TRUE);
		pango_layout_set_text (layout, name, strlen (name));
		pango_layout_get_pixel_size (layout, &w, NULL);

		if (w > max_width)
			max_width = w;
	}

	priv->max_letter_width = max_width;

	configure_items (chooser);
	g_object_unref (layout);
	pango_font_metrics_unref (font_metrics);

	/* Chain up to parent's style_updated() method. */
	widget_class = GTK_WIDGET_CLASS (e_weekday_chooser_parent_class);
	widget_class->style_updated (widget);
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

	chooser = E_WEEKDAY_CHOOSER (widget);

	if (!gtk_widget_get_can_focus (widget))
		return FALSE;

	if (gtk_widget_has_focus (widget)) {
		chooser->priv->focus_day = G_DATE_BAD_WEEKDAY;
		colorize_items (chooser);
		return FALSE;
	}

	chooser->priv->focus_day = chooser->priv->week_start_day;
	gnome_canvas_item_grab_focus (chooser->priv->boxes[0]);

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
	widget_class->style_updated = weekday_chooser_style_updated;
	widget_class->get_preferred_height = weekday_chooser_get_preferred_height;
	widget_class->get_preferred_width = weekday_chooser_get_preferred_width;
	widget_class->focus = weekday_chooser_focus;

	g_object_class_install_property (
		object_class,
		PROP_WEEK_START_DAY,
		g_param_spec_enum (
			"week-start-day",
			"Week Start Day",
			NULL,
			E_TYPE_DATE_WEEKDAY,
			G_DATE_MONDAY,
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
             GDateWeekday weekday)
{
	gboolean selected;

	if (chooser->priv->blocked_weekdays[weekday])
		return;

	selected = e_weekday_chooser_get_selected (chooser, weekday);
	e_weekday_chooser_set_selected (chooser, weekday, !selected);
}

static gint
handle_key_press_event (EWeekdayChooser *chooser,
                        GdkEvent *event)
{
	EWeekdayChooserPrivate *priv = chooser->priv;
	guint keyval = event->key.keyval;
	guint index;

	if (chooser->priv->focus_day == G_DATE_BAD_WEEKDAY)
		chooser->priv->focus_day = chooser->priv->week_start_day;

	switch (keyval) {
		case GDK_KEY_Up:
		case GDK_KEY_Right:
			chooser->priv->focus_day =
				e_weekday_get_next (chooser->priv->focus_day);
			break;
		case GDK_KEY_Down:
		case GDK_KEY_Left:
			chooser->priv->focus_day =
				e_weekday_get_prev (chooser->priv->focus_day);
			break;
		case GDK_KEY_space:
		case GDK_KEY_Return:
			day_clicked (chooser, priv->focus_day);
			return TRUE;
		default:
			return FALSE;
	}

	colorize_items (chooser);

	index = e_weekday_get_days_between (
		chooser->priv->week_start_day,
		chooser->priv->focus_day);

	gnome_canvas_item_grab_focus (chooser->priv->boxes[index]);

	return TRUE;
}

/* Event handler for the day items */
static gboolean
day_event_cb (GnomeCanvasItem *item,
              GdkEvent *event,
              gpointer data)
{
	EWeekdayChooser *chooser;
	gint ii;

	chooser = E_WEEKDAY_CHOOSER (data);

	if (event->type == GDK_KEY_PRESS)
		return handle_key_press_event (chooser, event);

	if (!(event->type == GDK_BUTTON_PRESS && event->button.button == 1))
		return FALSE;

	/* Find which box was clicked */

	for (ii = 0; ii < 7; ii++) {
		if (chooser->priv->boxes[ii] == item)
			break;
		if (chooser->priv->labels[ii] == item)
			break;
	}

	if (ii >= 7) {
		g_warn_if_reached ();
		return FALSE;
	}

	chooser->priv->focus_day = e_weekday_add_days (
		chooser->priv->week_start_day, ii);

	gnome_canvas_item_grab_focus (chooser->priv->boxes[ii]);

	day_clicked (chooser, chooser->priv->focus_day);

	return TRUE;
}

/* Creates the canvas items for the weekday chooser.
 * The items are empty until they are configured elsewhere. */
static void
create_items (EWeekdayChooser *chooser)
{
	GnomeCanvasGroup *parent;
	gint ii;

	parent = gnome_canvas_root (GNOME_CANVAS (chooser));

	for (ii = 0; ii < 7; ii++) {
		chooser->priv->boxes[ii] = gnome_canvas_item_new (
			parent,
			GNOME_TYPE_CANVAS_RECT,
			NULL);
		g_signal_connect (
			chooser->priv->boxes[ii], "event",
			G_CALLBACK (day_event_cb), chooser);

		chooser->priv->labels[ii] = gnome_canvas_item_new (
			parent,
			GNOME_TYPE_CANVAS_TEXT,
			NULL);
		g_signal_connect (
			chooser->priv->labels[ii], "event",
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
 * @weekday: a #GDateWeekday
 *
 * Returns whether @weekday is selected.
 *
 * Returns: whether @weekday is selected
 **/
gboolean
e_weekday_chooser_get_selected (EWeekdayChooser *chooser,
                                GDateWeekday weekday)
{
	g_return_val_if_fail (E_IS_WEEKDAY_CHOOSER (chooser), FALSE);
	g_return_val_if_fail (g_date_valid_weekday (weekday), FALSE);

	return chooser->priv->selected_weekdays[weekday];
}

/**
 * e_weekday_chooser_set_selected:
 * @chooser: an #EWeekdayChooser
 * @weekday: a #GDateWeekday
 * @selected: selected flag
 *
 * Selects or deselects @weekday.
 **/
void
e_weekday_chooser_set_selected (EWeekdayChooser *chooser,
                                GDateWeekday weekday,
                                gboolean selected)
{
	g_return_if_fail (E_IS_WEEKDAY_CHOOSER (chooser));
	g_return_if_fail (g_date_valid_weekday (weekday));

	chooser->priv->selected_weekdays[weekday] = selected;

	colorize_items (chooser);

	g_signal_emit (chooser, chooser_signals[CHANGED], 0);
}

/**
 * e_weekday_chooser_get_blocked:
 * @chooser: an #EWeekdayChooser
 * @weekday: a #GDateWeekday
 *
 * Returns whether @weekday is blocked from being modified by the user.
 *
 * Returns: whether @weekday is blocked
 **/
gboolean
e_weekday_chooser_get_blocked (EWeekdayChooser *chooser,
                               GDateWeekday weekday)
{
	g_return_val_if_fail (E_IS_WEEKDAY_CHOOSER (chooser), FALSE);
	g_return_val_if_fail (g_date_valid_weekday (weekday), FALSE);

	return chooser->priv->blocked_weekdays[weekday];
}

/**
 * e_weekday_chooser_set_blocked:
 * @chooser: an #EWeekdayChooser
 * @weekday: a #GDateWeekday
 * @blocked: blocked flag
 *
 * Sets whether @weekday is blocked from being modified by the user.
 **/
void
e_weekday_chooser_set_blocked (EWeekdayChooser *chooser,
                               GDateWeekday weekday,
                               gboolean blocked)
{
	g_return_if_fail (E_IS_WEEKDAY_CHOOSER (chooser));
	g_return_if_fail (g_date_valid_weekday (weekday));

	chooser->priv->blocked_weekdays[weekday] = blocked;
}

/**
 * e_weekday_chooser_get_week_start_day:
 * @chooser: an #EWeekdayChooser
 *
 * Queries the day that defines the start of the week in @chooser.
 *
 * Returns: a #GDateWeekday
 **/
GDateWeekday
e_weekday_chooser_get_week_start_day (EWeekdayChooser *chooser)
{
	g_return_val_if_fail (E_IS_WEEKDAY_CHOOSER (chooser), G_DATE_BAD_WEEKDAY);

	return chooser->priv->week_start_day;
}

/**
 * e_weekday_chooser_set_week_start_day:
 * @chooser: an #EWeekdayChooser
 * @week_start_day: a #GDateWeekday
 *
 * Sets the day that defines the start of the week for @chooser.
 **/
void
e_weekday_chooser_set_week_start_day (EWeekdayChooser *chooser,
                                      GDateWeekday week_start_day)
{
	g_return_if_fail (E_IS_WEEKDAY_CHOOSER (chooser));
	g_return_if_fail (g_date_valid_weekday (week_start_day));

	if (week_start_day == chooser->priv->week_start_day)
		return;

	chooser->priv->week_start_day = week_start_day;

	configure_items (chooser);

	g_object_notify (G_OBJECT (chooser), "week-start-day");
}

