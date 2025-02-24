/*
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
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECalendar - displays a table of monthly calendars, allowing highlighting
 * and selection of one or more days. Like GtkCalendar with more features.
 * Most of the functionality is in the ECalendarItem canvas item, though
 * we also add GnomeCanvasWidget buttons to go to the previous/next month and
 * to got to the current day.
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

#include <libgnomecanvas/gnome-canvas-widget.h>

#include "e-misc-utils.h"
#include "e-calendar.h"

#define E_CALENDAR_SMALL_FONT_PTSIZE 6

#define E_CALENDAR_SMALL_FONT \
	"-adobe-utopia-regular-r-normal-*-*-100-*-*-p-*-iso8859-*"
#define E_CALENDAR_SMALL_FONT_FALLBACK \
	"-adobe-helvetica-medium-r-normal-*-*-80-*-*-p-*-iso8859-*"

/* The space between the arrow buttons and the edge of the widget. */
#define E_CALENDAR_ARROW_BUTTON_X_PAD	2
#define E_CALENDAR_ARROW_BUTTON_Y_PAD	0

/* Vertical padding. The padding above the button includes the space for the
 * horizontal line. */
#define	E_CALENDAR_YPAD_ABOVE_LOWER_BUTTONS		4
#define	E_CALENDAR_YPAD_BELOW_LOWER_BUTTONS		3

/* Horizontal padding inside & between buttons. */
#define E_CALENDAR_IXPAD_BUTTONS			4
#define E_CALENDAR_XPAD_BUTTONS				8

/* The time between steps when the prev/next buttons is pressed, in 1/1000ths
 * of a second, and the number of timeouts we skip before we start
 * automatically moving back/forward. */
#define E_CALENDAR_AUTO_MOVE_TIMEOUT		150
#define E_CALENDAR_AUTO_MOVE_TIMEOUT_DELAY	2

struct _ECalendarPrivate {
	ECalendarItem *calitem;

	GnomeCanvasItem *prev_item;
	GnomeCanvasItem *next_item;
	GnomeCanvasItem *prev_item_year;
	GnomeCanvasItem *next_item_year;

	gint min_rows;
	gint min_cols;

	gint max_rows;
	gint max_cols;

	/* These are all used when the prev/next buttons are held down.
	 * moving_forward is TRUE if we are moving forward in time, i.e. the
	 * next button is pressed. */
	gint timeout_id;
	gint timeout_delay;
	gboolean moving_forward;

	gint reposition_timeout_id;
};

static void e_calendar_dispose		(GObject	*object);
static void e_calendar_realize		(GtkWidget	*widget);
static void e_calendar_style_updated	(GtkWidget	*widget);
static void e_calendar_get_preferred_width (GtkWidget *widget,
					    gint      *minimal_width,
					    gint      *natural_width);
static void e_calendar_get_preferred_height (GtkWidget *widget,
					     gint      *minimal_height,
					     gint      *natural_height);
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
static gboolean e_calendar_button_has_focus (ECalendar	*cal);
static gboolean e_calendar_focus (GtkWidget *widget,
				  GtkDirectionType direction);

static void e_calendar_on_prev_pressed	(ECalendar	*cal);
static void e_calendar_on_prev_released	(ECalendar	*cal);
static void e_calendar_on_prev_clicked  (ECalendar      *cal);
static void e_calendar_on_next_pressed	(ECalendar	*cal);
static void e_calendar_on_next_released	(ECalendar	*cal);
static void e_calendar_on_next_clicked  (ECalendar      *cal);
static void e_calendar_on_prev_year_pressed  (ECalendar *cal);
static void e_calendar_on_prev_year_released (ECalendar *cal);
static void e_calendar_on_prev_year_clicked  (ECalendar *cal);
static void e_calendar_on_next_year_pressed  (ECalendar *cal);
static void e_calendar_on_next_year_released (ECalendar *cal);
static void e_calendar_on_next_year_clicked  (ECalendar *cal);

static void	e_calendar_start_auto_move	(ECalendar *cal,
						 gboolean moving_forward);
static gboolean e_calendar_auto_move_handler	(gpointer data);
static void e_calendar_start_auto_move_year	(ECalendar *cal,
						 gboolean moving_forward);
static gboolean e_calendar_auto_move_year_handler (gpointer data);
static void e_calendar_stop_auto_move		(ECalendar *cal);

G_DEFINE_TYPE_WITH_PRIVATE (ECalendar, e_calendar, E_TYPE_CANVAS)

static void
calitem_month_width_changed_cb (ECalendarItem *item,
				ECalendar *cal)
{
	g_return_if_fail (E_IS_CALENDAR (cal));

	gtk_widget_queue_resize (GTK_WIDGET (cal));
}

static GtkWidget *
e_calendar_create_button (gboolean start_arrow)
{
	GtkWidget *button;
	GtkCssProvider *css_provider;
	GtkStyleContext *style_context;
	GError *error = NULL;
	const gchar *icon_name;

	if (start_arrow)
		icon_name = "pan-start-symbolic";
	else
		icon_name = "pan-end-symbolic";

	button = gtk_button_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (button);

	css_provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (css_provider,
		"button.ecalendar {"
		" min-height: 0px;"
		" min-width: 0px;"
		" padding: 0px;"
		"}", -1, &error);
	style_context = gtk_widget_get_style_context (button);
	gtk_style_context_add_class (style_context, "flat");
	if (error == NULL) {
		gtk_style_context_add_class (style_context, "ecalendar");
		gtk_style_context_add_provider (
			style_context,
			GTK_STYLE_PROVIDER (css_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	} else {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	}
	g_object_unref (css_provider);

	return button;
}

static gint
e_calendar_calc_min_column_width (ECalendar *cal)
{
	GtkWidget *widget;
	GtkStyleContext *style_context;
	GtkBorder padding;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	gdouble xthickness, arrow_button_size;

	g_return_val_if_fail (E_IS_CALENDAR (cal), 0);

	widget = GTK_WIDGET (cal);
	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_padding (style_context, gtk_style_context_get_state (style_context), &padding);
	xthickness = padding.left;

	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (
		pango_context, NULL,
		pango_context_get_language (pango_context));

	arrow_button_size =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics))
		+ PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics))
		+ E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME
		+ E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME
		- E_CALENDAR_ARROW_BUTTON_Y_PAD * 2 - 2;

	pango_font_metrics_unref (font_metrics);

	return E_CALENDAR_ITEM_MIN_CELL_XPAD +
		E_CALENDAR_ARROW_BUTTON_X_PAD +
		(5 * E_CALENDAR_XPAD_BUTTONS) +
		(4 * arrow_button_size) +
		(4 * xthickness) +
		(5 * cal->priv->calitem->max_digit_width) +
		cal->priv->calitem->max_month_name_width;
}

static void
e_calendar_class_init (ECalendarClass *class)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) class;
	object_class->dispose = e_calendar_dispose;

	widget_class = (GtkWidgetClass *) class;
	widget_class->realize = e_calendar_realize;
	widget_class->style_updated = e_calendar_style_updated;
	widget_class->get_preferred_width = e_calendar_get_preferred_width;
	widget_class->get_preferred_height = e_calendar_get_preferred_height;
	widget_class->size_allocate = e_calendar_size_allocate;
	widget_class->drag_motion = e_calendar_drag_motion;
	widget_class->drag_leave = e_calendar_drag_leave;
	widget_class->focus = e_calendar_focus;
}

static void
e_calendar_init (ECalendar *cal)
{
	GnomeCanvasGroup *canvas_group;
	PangoFontDescription *small_font_desc;
	PangoContext *pango_context;
	GtkWidget *button;
	AtkObject *a11y;

	cal->priv = e_calendar_get_instance_private (cal);

	pango_context = gtk_widget_create_pango_context (GTK_WIDGET (cal));
	g_warn_if_fail (pango_context != NULL);

	/* Create the small font. */

	small_font_desc = pango_font_description_copy (
		pango_context_get_font_description (pango_context));
	pango_font_description_set_size (
		small_font_desc,
		E_CALENDAR_SMALL_FONT_PTSIZE * PANGO_SCALE);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (cal)->root);

	cal->priv->calitem = E_CALENDAR_ITEM (
		gnome_canvas_item_new (
			canvas_group,
			e_calendar_item_get_type (),
			"week_number_font_desc", small_font_desc,
			NULL));

	pango_font_description_free (small_font_desc);
	g_object_unref (pango_context);

	g_signal_connect (cal->priv->calitem, "month-width-changed",
		G_CALLBACK (calitem_month_width_changed_cb), cal);

	g_signal_connect_swapped (cal->priv->calitem, "calc-min-column-width",
		G_CALLBACK (e_calendar_calc_min_column_width), cal);

	/* Create the arrow buttons to move to the previous/next month. */
	button = e_calendar_create_button (TRUE);
	g_signal_connect_swapped (
		button, "pressed",
		G_CALLBACK (e_calendar_on_prev_pressed), cal);
	g_signal_connect_swapped (
		button, "released",
		G_CALLBACK (e_calendar_on_prev_released), cal);
	g_signal_connect_swapped (
		button, "clicked",
		G_CALLBACK (e_calendar_on_prev_clicked), cal);

	cal->priv->prev_item = gnome_canvas_item_new (
		canvas_group,
		gnome_canvas_widget_get_type (),
		"widget", button,
		NULL);
	a11y = gtk_widget_get_accessible (button);
	atk_object_set_name (a11y, _("Previous month"));

	button = e_calendar_create_button (FALSE);
	g_signal_connect_swapped (
		button, "pressed",
		G_CALLBACK (e_calendar_on_next_pressed), cal);
	g_signal_connect_swapped (
		button, "released",
		G_CALLBACK (e_calendar_on_next_released), cal);
	g_signal_connect_swapped (
		button, "clicked",
		G_CALLBACK (e_calendar_on_next_clicked), cal);

	cal->priv->next_item = gnome_canvas_item_new (
		canvas_group,
		gnome_canvas_widget_get_type (),
		"widget", button,
		NULL);
	a11y = gtk_widget_get_accessible (button);
	atk_object_set_name (a11y, _("Next month"));

	/* Create the arrow buttons to move to the previous/next year. */
	button = e_calendar_create_button (TRUE);
	g_signal_connect_swapped (
		button, "pressed",
		G_CALLBACK (e_calendar_on_prev_year_pressed), cal);
	g_signal_connect_swapped (
		button, "released",
		G_CALLBACK (e_calendar_on_prev_year_released), cal);
	g_signal_connect_swapped (
		button, "clicked",
		G_CALLBACK (e_calendar_on_prev_year_clicked), cal);

	cal->priv->prev_item_year = gnome_canvas_item_new (
		canvas_group,
		gnome_canvas_widget_get_type (),
		"widget", button,
		NULL);
	a11y = gtk_widget_get_accessible (button);
	atk_object_set_name (a11y, _("Previous year"));

	button = e_calendar_create_button (FALSE);
	g_signal_connect_swapped (
		button, "pressed",
		G_CALLBACK (e_calendar_on_next_year_pressed), cal);
	g_signal_connect_swapped (
		button, "released",
		G_CALLBACK (e_calendar_on_next_year_released), cal);
	g_signal_connect_swapped (
		button, "clicked",
		G_CALLBACK (e_calendar_on_next_year_clicked), cal);

	cal->priv->next_item_year = gnome_canvas_item_new (
		canvas_group,
		gnome_canvas_widget_get_type (),
		"widget", button,
		NULL);
	a11y = gtk_widget_get_accessible (button);
	atk_object_set_name (a11y, _("Next year"));

	cal->priv->min_rows = 1;
	cal->priv->min_cols = 1;
	cal->priv->max_rows = -1;
	cal->priv->max_cols = -1;

	cal->priv->timeout_id = 0;
}

/**
 * e_calendar_new:
 * @Returns: a new #ECalendar.
 *
 * Creates a new #ECalendar.
 **/
GtkWidget *
e_calendar_new (void)
{
	GtkWidget *cal;
	AtkObject *a11y;

	cal = g_object_new (e_calendar_get_type (), NULL);
	a11y = gtk_widget_get_accessible (cal);
	atk_object_set_name (a11y, _("Month Calendar"));

	return cal;
}

ECalendarItem *
e_calendar_get_item (ECalendar *cal)
{
	g_return_val_if_fail (E_IS_CALENDAR (cal), NULL);

	return cal->priv->calitem;
}

static void
e_calendar_dispose (GObject *object)
{
	ECalendar *cal;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_CALENDAR (object));

	cal = E_CALENDAR (object);

	if (cal->priv->timeout_id != 0) {
		g_source_remove (cal->priv->timeout_id);
		cal->priv->timeout_id = 0;
	}

	if (cal->priv->reposition_timeout_id) {
		g_source_remove (cal->priv->reposition_timeout_id);
		cal->priv->reposition_timeout_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_calendar_parent_class)->dispose (object);
}

static void
e_calendar_update_window_background (GtkWidget *widget)
{
	GdkWindow *window;
	GdkRGBA bg_bg;

	e_utils_get_theme_color (widget, "theme_bg_color", E_UTILS_DEFAULT_THEME_BG_COLOR, &bg_bg);

	/* Set the background of the canvas window to the normal color,
	 * or the arrow buttons are not displayed properly. */
	window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
	gdk_window_set_background_rgba (window, &bg_bg);
}

static void
e_calendar_realize (GtkWidget *widget)
{
	(*GTK_WIDGET_CLASS (e_calendar_parent_class)->realize) (widget);

	e_calendar_update_window_background (widget);
}

static void
e_calendar_style_updated (GtkWidget *widget)
{
	ECalendar *e_calendar;

	e_calendar = E_CALENDAR (widget);
	if (GTK_WIDGET_CLASS (e_calendar_parent_class)->style_updated)
		(*GTK_WIDGET_CLASS (e_calendar_parent_class)->style_updated) (widget);

	/* Set the background of the canvas window to the normal color,
	 * or the arrow buttons are not displayed properly. */
	if (gtk_widget_get_realized (widget))
		e_calendar_update_window_background (widget);

	e_calendar_item_style_updated (widget, e_calendar->priv->calitem);
}

static void
e_calendar_get_preferred_width (GtkWidget *widget,
                                gint *minimum,
                                gint *natural)
{
	ECalendar *cal;
	GtkStyleContext *style_context;
	GtkBorder padding;
	gint col_width;

	cal = E_CALENDAR (widget);

	g_object_get ((cal->priv->calitem), "column_width", &col_width, NULL);
	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_padding (style_context, gtk_style_context_get_state (style_context), &padding);

	*minimum = *natural = col_width * cal->priv->min_cols + padding.left * 2;
}

static void
e_calendar_get_preferred_height (GtkWidget *widget,
                                 gint *minimum,
                                 gint *natural)
{
	ECalendar *cal;
	GtkStyleContext *style_context;
	GtkBorder padding;
	gint row_height;

	cal = E_CALENDAR (widget);

	g_object_get ((cal->priv->calitem), "row_height", &row_height, NULL);
	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_padding (style_context, gtk_style_context_get_state (style_context), &padding);

	*minimum = *natural = row_height * cal->priv->min_rows + padding.top * 2;
}

static gboolean
e_calendar_reposition_timeout_cb (gpointer user_data)
{
	ECalendar *cal = user_data;
	GtkWidget *widget;
	GtkStyleContext *style_context;
	GtkBorder padding;
	GtkAllocation old_allocation;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	gdouble old_x2, old_y2, new_x2, new_y2;
	gdouble xthickness, ythickness, arrow_button_size, current_x, month_width;
	gboolean is_rtl;

	g_return_val_if_fail (E_IS_CALENDAR (cal), FALSE);

	cal->priv->reposition_timeout_id = 0;

	widget = GTK_WIDGET (cal);
	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_padding (style_context, gtk_style_context_get_state (style_context), &padding);
	xthickness = padding.left;
	ythickness = padding.top;

	/* Set up Pango prerequisites */
	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (
		pango_context, NULL,
		pango_context_get_language (pango_context));

	/* Set the scroll region to its allocated size, if changed. */
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (cal),
		NULL, NULL, &old_x2, &old_y2);
	gtk_widget_get_allocation (widget, &old_allocation);
	new_x2 = old_allocation.width - 1;
	new_y2 = old_allocation.height - 1;
	if (new_x2 < cal->priv->calitem->min_month_width)
		new_x2 = cal->priv->calitem->min_month_width;
	if (new_y2 < cal->priv->calitem->min_month_height)
		new_y2 = cal->priv->calitem->min_month_height;
	if (old_x2 != new_x2 || old_y2 != new_y2)
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (cal),
			0, 0, new_x2, new_y2);

	/* Take off space for line & buttons if shown. */
	gnome_canvas_item_set (
		GNOME_CANVAS_ITEM (cal->priv->calitem),
		"x1", 0.0,
		"y1", 0.0,
		"x2", new_x2,
		"y2", new_y2,
		NULL);

	if (cal->priv->calitem->month_width > 0)
		month_width = cal->priv->calitem->month_width;
	else
		month_width = new_x2;
	month_width -= E_CALENDAR_ITEM_MIN_CELL_XPAD + E_CALENDAR_ARROW_BUTTON_X_PAD;

	/* Position the arrow buttons. */
	arrow_button_size =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics))
		+ PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics))
		+ E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME
		+ E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME
		- E_CALENDAR_ARROW_BUTTON_Y_PAD * 2 - 2;

	is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
	current_x = is_rtl ?
		(month_width - 2 * xthickness - E_CALENDAR_ARROW_BUTTON_X_PAD - arrow_button_size) :
		(xthickness);

	gnome_canvas_item_set (
		cal->priv->prev_item,
		"x", current_x,
		"y", ythickness + E_CALENDAR_ARROW_BUTTON_Y_PAD,
		"width", arrow_button_size,
		"height", arrow_button_size,
		NULL);

	current_x += (is_rtl ? -1.0 : +1.0) * (cal->priv->calitem->max_month_name_width - xthickness + 2 * arrow_button_size);

	gnome_canvas_item_set (
		cal->priv->next_item,
		"x", current_x,
		"y", ythickness + E_CALENDAR_ARROW_BUTTON_Y_PAD,
		"width", arrow_button_size,
		"height", arrow_button_size,
		NULL);

	current_x = is_rtl ?
		(xthickness) :
		(month_width - 2 * xthickness - E_CALENDAR_ARROW_BUTTON_X_PAD - arrow_button_size);

	gnome_canvas_item_set (
		cal->priv->next_item_year,
		"x", current_x,
		"y", ythickness + E_CALENDAR_ARROW_BUTTON_Y_PAD,
		"width", arrow_button_size,
		"height", arrow_button_size,
		NULL);

	current_x += (is_rtl ? +1.0 : -1.0) * (cal->priv->calitem->max_digit_width * 5 - xthickness + 2 * arrow_button_size);

	gnome_canvas_item_set (
		cal->priv->prev_item_year,
		"x", current_x,
		"y", ythickness + E_CALENDAR_ARROW_BUTTON_Y_PAD,
		"width", arrow_button_size,
		"height", arrow_button_size,
		NULL);

	pango_font_metrics_unref (font_metrics);

	return FALSE;
}

static void
e_calendar_size_allocate (GtkWidget *widget,
                          GtkAllocation *allocation)
{
	ECalendar *cal;

	(*GTK_WIDGET_CLASS (e_calendar_parent_class)->size_allocate) (widget, allocation);

	cal = E_CALENDAR (widget);

	if (cal->priv->reposition_timeout_id) {
		g_source_remove (cal->priv->reposition_timeout_id);
		cal->priv->reposition_timeout_id = 0;
	}

	cal->priv->reposition_timeout_id =
		g_timeout_add (1, e_calendar_reposition_timeout_cb, widget);
}

void
e_calendar_set_minimum_size (ECalendar *cal,
                             gint rows,
                             gint cols)
{
	g_return_if_fail (E_IS_CALENDAR (cal));

	cal->priv->min_rows = rows;
	cal->priv->min_cols = cols;

	gnome_canvas_item_set (
		GNOME_CANVAS_ITEM (cal->priv->calitem),
		"minimum_rows", rows,
		"minimum_columns", cols,
		NULL);

	gtk_widget_queue_resize (GTK_WIDGET (cal));
}

static void
e_calendar_on_prev_pressed (ECalendar *cal)
{
	e_calendar_start_auto_move (cal, FALSE);
}

static void
e_calendar_on_next_pressed (ECalendar *cal)
{
	e_calendar_start_auto_move (cal, TRUE);
}

static void
e_calendar_on_prev_year_pressed (ECalendar *cal)
{
	e_calendar_start_auto_move_year (cal, FALSE);
}

static void
e_calendar_on_next_year_pressed (ECalendar *cal)
{
	e_calendar_start_auto_move_year (cal, TRUE);
}

static void
e_calendar_start_auto_move (ECalendar *cal,
                            gboolean moving_forward)
{
	if (cal->priv->timeout_id == 0) {
		cal->priv->timeout_id = e_named_timeout_add (
			E_CALENDAR_AUTO_MOVE_TIMEOUT,
			e_calendar_auto_move_handler, cal);
	}

	cal->priv->timeout_delay = E_CALENDAR_AUTO_MOVE_TIMEOUT_DELAY;
	cal->priv->moving_forward = moving_forward;

}

static void
e_calendar_start_auto_move_year (ECalendar *cal,
                                 gboolean moving_forward)
{
	if (cal->priv->timeout_id == 0) {
		cal->priv->timeout_id = e_named_timeout_add (
			E_CALENDAR_AUTO_MOVE_TIMEOUT,
			e_calendar_auto_move_year_handler, cal);
	}

	cal->priv->timeout_delay = E_CALENDAR_AUTO_MOVE_TIMEOUT_DELAY;
	cal->priv->moving_forward = moving_forward;
}

static gboolean
e_calendar_auto_move_year_handler (gpointer data)
{
	ECalendar *cal;
	ECalendarItem *calitem;
	gint offset;

	g_return_val_if_fail (E_IS_CALENDAR (data), FALSE);

	cal = E_CALENDAR (data);
	calitem = cal->priv->calitem;

	if (cal->priv->timeout_delay > 0) {
		cal->priv->timeout_delay--;
	} else {
		offset = cal->priv->moving_forward ? 12 : -12;
		e_calendar_item_set_first_month (
			calitem, calitem->year,
			calitem->month + offset);
	}

	return TRUE;
}

static gboolean
e_calendar_auto_move_handler (gpointer data)
{
	ECalendar *cal;
	ECalendarItem *calitem;
	gint offset;

	g_return_val_if_fail (E_IS_CALENDAR (data), FALSE);

	cal = E_CALENDAR (data);
	calitem = cal->priv->calitem;

	if (cal->priv->timeout_delay > 0) {
		cal->priv->timeout_delay--;
	} else {
		offset = cal->priv->moving_forward ? 1 : -1;
		e_calendar_item_set_first_month (
			calitem, calitem->year,
			calitem->month + offset);
	}

	return TRUE;
}

static void
e_calendar_on_prev_released (ECalendar *cal)
{
	e_calendar_stop_auto_move (cal);
}

static void
e_calendar_on_next_released (ECalendar *cal)
{
	e_calendar_stop_auto_move (cal);
}

static void
e_calendar_on_prev_year_released (ECalendar *cal)
{
	e_calendar_stop_auto_move (cal);
}

static void
e_calendar_on_next_year_released (ECalendar *cal)
{
	e_calendar_stop_auto_move (cal);
}

static void
e_calendar_stop_auto_move (ECalendar *cal)
{
	if (cal->priv->timeout_id != 0) {
		g_source_remove (cal->priv->timeout_id);
		cal->priv->timeout_id = 0;
	}
}

static void
e_calendar_on_prev_clicked (ECalendar *cal)
{
	e_calendar_item_set_first_month (
		cal->priv->calitem, cal->priv->calitem->year,
		cal->priv->calitem->month - 1);
}

static void
e_calendar_on_next_clicked (ECalendar *cal)
{
	e_calendar_item_set_first_month (
		cal->priv->calitem, cal->priv->calitem->year,
		cal->priv->calitem->month + 1);
}

static void
e_calendar_on_prev_year_clicked (ECalendar *cal)
{
	e_calendar_item_set_first_month (
		cal->priv->calitem, cal->priv->calitem->year,
		cal->priv->calitem->month - 12);
}

static void
e_calendar_on_next_year_clicked (ECalendar *cal)
{
	e_calendar_item_set_first_month (
		cal->priv->calitem, cal->priv->calitem->year,
		cal->priv->calitem->month + 12);
}

static gint
e_calendar_drag_motion (GtkWidget *widget,
                        GdkDragContext *context,
                        gint x,
                        gint y,
                        guint time)
{
	return FALSE;
}

static void
e_calendar_drag_leave (GtkWidget *widget,
                       GdkDragContext *context,
                       guint time)
{
}

static gboolean
e_calendar_button_has_focus (ECalendar *cal)
{
	GtkWidget *prev_widget, *next_widget;
	gboolean ret_val;

	g_return_val_if_fail (E_IS_CALENDAR (cal), FALSE);

	prev_widget = GNOME_CANVAS_WIDGET (cal->priv->prev_item)->widget;
	next_widget = GNOME_CANVAS_WIDGET (cal->priv->next_item)->widget;
	ret_val = gtk_widget_has_focus (prev_widget) ||
		gtk_widget_has_focus (next_widget);
	return ret_val;
}

static gboolean
e_calendar_focus (GtkWidget *widget,
                  GtkDirectionType direction)
{
#define E_CALENDAR_FOCUS_CHILDREN_NUM 5
	ECalendar *cal;
	GnomeCanvas *canvas;
	GnomeCanvasItem *children[E_CALENDAR_FOCUS_CHILDREN_NUM];
	gint focused_index = -1;
	gint index;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_CALENDAR (widget), FALSE);
	cal = E_CALENDAR (widget);
	canvas = GNOME_CANVAS (widget);

	if (!gtk_widget_get_can_focus (widget))
		return FALSE;

	children[0] = GNOME_CANVAS_ITEM (cal->priv->calitem);
	children[1] = cal->priv->prev_item;
	children[2] = cal->priv->next_item;
	children[3] = cal->priv->prev_item_year;
	children[4] = cal->priv->next_item_year;

	/* get current focused item, if e-calendar has had focus */
	if (gtk_widget_has_focus (widget) || e_calendar_button_has_focus (cal))
		for (index = 0; index < E_CALENDAR_FOCUS_CHILDREN_NUM; ++index) {
			if (canvas->focused_item == NULL)
				break;

			if (children[index] == canvas->focused_item) {
				focused_index = index;
				break;
			}
		}

	if (focused_index == -1)
		if (direction == GTK_DIR_TAB_FORWARD)
			focused_index = 0;
		else
			focused_index = E_CALENDAR_FOCUS_CHILDREN_NUM - 1;
	else
		if (direction == GTK_DIR_TAB_FORWARD)
			++focused_index;
		else
			--focused_index;

	if (focused_index < 0 ||
	    focused_index >= E_CALENDAR_FOCUS_CHILDREN_NUM)
		/* move out of e-calendar */
		return FALSE;
	gnome_canvas_item_grab_focus (children[focused_index]);
	if (GNOME_IS_CANVAS_WIDGET (children[focused_index])) {
		widget = GNOME_CANVAS_WIDGET (children[focused_index])->widget;
		gtk_widget_grab_focus (widget);
	}
	return TRUE;
}
