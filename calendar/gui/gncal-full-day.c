/* Full day widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <gtk/gtkdrawingarea.h>
#include "gncal-full-day.h"


#define TEXT_BORDER 2
#define MIN_WIDTH 200


static void gncal_full_day_class_init    (GncalFullDayClass *class);
static void gncal_full_day_init          (GncalFullDay      *fullday);
static void gncal_full_day_realize       (GtkWidget         *widget);
static void gncal_full_day_size_request  (GtkWidget         *widget,
					  GtkRequisition    *requisition);
static void gncal_full_day_size_allocate (GtkWidget         *widget,
					  GtkAllocation     *allocation);
static gint gncal_full_day_expose        (GtkWidget         *widget,
					  GdkEventExpose    *event);


static GtkContainerClass *parent_class;


guint
gncal_full_day_get_type (void)
{
	static guint full_day_type = 0;

	if (!full_day_type) {
		GtkTypeInfo full_day_info = {
			"GncalFullDay",
			sizeof (GncalFullDay),
			sizeof (GncalFullDayClass),
			(GtkClassInitFunc) gncal_full_day_class_init,
			(GtkObjectInitFunc) gncal_full_day_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		full_day_type = gtk_type_unique (gtk_container_get_type (), &full_day_info);
	}

	return full_day_type;
}

static void
gncal_full_day_class_init (GncalFullDayClass *class)
{
	GtkWidgetClass    *widget_class;
	GtkContainerClass *container_class;

	widget_class = (GtkWidgetClass *) class;
	container_class = (GtkContainerClass *) class;

	parent_class = gtk_type_class (gtk_container_get_type ());

	widget_class->realize = gncal_full_day_realize;
	widget_class->size_request = gncal_full_day_size_request;
	widget_class->size_allocate = gncal_full_day_size_allocate;
	widget_class->expose_event = gncal_full_day_expose;
}

static void
gncal_full_day_init (GncalFullDay *fullday)
{
	GTK_WIDGET_UNSET_FLAGS (fullday, GTK_NO_WINDOW);

	fullday->calendar = NULL;

	fullday->lower = 0;
	fullday->upper = 0;
	fullday->interval = 30; /* 30 minutes by default */
}

GtkWidget *
gncal_full_day_new (GnomeCalendar *calendar, time_t lower, time_t upper)
{
	GncalFullDay *fullday;

	g_return_val_if_fail (calendar != NULL, NULL);

	fullday = gtk_type_new (gncal_full_day_get_type ());

	fullday->calendar = calendar;

	gncal_full_day_set_bounds (fullday, lower, upper);

	return GTK_WIDGET (fullday);
}

static void
gncal_full_day_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = (gtk_widget_get_events (widget)
				 | GDK_EXPOSURE_MASK);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gdk_window_set_background (widget->window, &widget->style->bg[GTK_STATE_PRELIGHT]);
}

static int
get_tm_bounds (GncalFullDay *fullday, struct tm *lower, struct tm *upper)
{
	struct tm tm_lower, tm_upper;
	int lmin, umin;

	/* Lower */

	tm_lower = *localtime (&fullday->lower);

	if ((tm_lower.tm_min % fullday->interval) != 0) {
		tm_lower.tm_min -= tm_lower.tm_min % fullday->interval; /* round down */
		mktime (&tm_lower);
	}

	/* Upper */

	tm_upper = *localtime (&fullday->upper);

	if ((tm_upper.tm_min % fullday->interval) != 0) {
		tm_upper.tm_min += fullday->interval - (tm_upper.tm_min % fullday->interval); /* round up */
		mktime (&tm_upper);
	}

	if (lower)
		*lower = tm_lower;

	if (upper)
		*upper = tm_upper;

	lmin = 60 * tm_lower.tm_hour + tm_lower.tm_min;
	umin = 60 * tm_upper.tm_hour + tm_upper.tm_min;

	if (umin == 0) /* midnight of next day? */
		umin = 60 * 24;

	return (umin - lmin) / fullday->interval; /* number of rows in view */
}

static int
calc_labels_width (GncalFullDay *fullday)
{
	struct tm cur, upper;
	time_t tim, time_upper;
	int width, max_w;
	char buf[256];

	get_tm_bounds (fullday, &cur, &upper);

	max_w = 0;

	tim = mktime (&cur);
	time_upper = mktime (&upper);

	while (tim < time_upper) {
		strftime (buf, 256, "%X", &cur);

		width = gdk_string_width (GTK_WIDGET (fullday)->style->font, buf);

		if (width > max_w)
			max_w = width;

		cur.tm_min += fullday->interval;
		tim = mktime (&cur);
	}

	return max_w;
}

static void
gncal_full_day_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GncalFullDay *fullday;
	int labels_width;
	int rows;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));
	g_return_if_fail (requisition != NULL);

	fullday = GNCAL_FULL_DAY (widget);

	/* Border and min width */

	labels_width = calc_labels_width (fullday);

	requisition->width = 2 * widget->style->klass->xthickness + 4 * TEXT_BORDER + labels_width + MIN_WIDTH;
	requisition->height = 2 * widget->style->klass->ythickness;

	/* Rows */

	rows = get_tm_bounds (fullday, NULL, NULL);

	requisition->height += (rows * (2 * TEXT_BORDER + widget->style->font->ascent + widget->style->font->descent)
				+ (rows - 1)); /* division lines */
}

static void
gncal_full_day_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));
	g_return_if_fail (allocation != NULL);

	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (widget->window,
					allocation->x, allocation->y,
					allocation->width, allocation->height);

	/* FIXME: adjust children */
}

static void
paint_back (GncalFullDay *fullday, GdkRectangle *area)
{
	GtkWidget *widget;
	GdkRectangle rect, dest;
	int x1, y1, width, height;
	int labels_width, division_x;
	int rows, row_height;
	int i, y;
	struct tm tm;
	char buf[256];

	widget = GTK_WIDGET (fullday);

	x1 = widget->style->klass->xthickness;
	y1 = widget->style->klass->ythickness;
	width = widget->allocation.width - 2 * x1;
	height = widget->allocation.height - 2 * y1;

	/* Clear and paint frame shadow */

	gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

	gtk_draw_shadow (widget->style, widget->window,
			 GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_IN,
			 0, 0,
			 widget->allocation.width,
			 widget->allocation.height);

	/* Clear space for labels */

	labels_width = calc_labels_width (fullday);

	rect.x = x1;
	rect.y = y1;
	rect.width = 2 * TEXT_BORDER + labels_width;
	rect.height = height;

	if (gdk_rectangle_intersect (&rect, area, &dest))
		gdk_draw_rectangle (widget->window,
				    widget->style->bg_gc[GTK_STATE_NORMAL],
				    TRUE,
				    dest.x, dest.y,
				    dest.width, dest.height);

	/* Vertical division */

	division_x = x1 + 2 * TEXT_BORDER + labels_width;

	gtk_draw_vline (widget->style, widget->window,
			GTK_STATE_NORMAL,
			y1,
			y1 + height - 1,
			division_x);

	/* Horizontal divisions */

	rows = get_tm_bounds (fullday, &tm, NULL);

	row_height = height / rows; /* includes division line */

	y = row_height;

	for (i = 1; i < rows; i++) {
		gdk_draw_line (widget->window,
			       widget->style->black_gc,
			       x1, y,
			       x1 + width - 1, y);

		y += row_height;
	}

	/* Labels */

	y = y1 + ((row_height - 1) - (widget->style->font->ascent + widget->style->font->descent)) / 2;

	for (i = 0; i < rows; i++) {
		mktime (&tm);

		if (gdk_rectangle_intersect (&rect, area, &dest)) {
			strftime (buf, 256, "%X", &tm);

			gdk_draw_string (widget->window,
					 widget->style->font,
					 widget->style->fg_gc[GTK_STATE_NORMAL],
					 x1 + TEXT_BORDER,
					 y + widget->style->font->ascent,
					 buf);
		}

		rect.y += row_height;
		y += row_height;

		tm.tm_min += fullday->interval;
	}
}

static gint
gncal_full_day_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GncalFullDay *fullday;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (!GTK_WIDGET_DRAWABLE (widget))
		return FALSE;

	fullday = GNCAL_FULL_DAY (widget);

	if (event->window == widget->window)
		paint_back (fullday, &event->area);

	/* FIXME: paint handles in windows if event->window == blah blah */

	return FALSE;
}

void
gncal_full_day_update (GncalFullDay *fullday)
{
	g_return_if_fail (fullday != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (fullday));

	/* FIXME */
}

void
gncal_full_day_set_bounds (GncalFullDay *fullday, time_t lower, time_t upper)
{
	g_return_if_fail (fullday != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (fullday));

	if ((lower != fullday->lower) || (upper != fullday->upper)) {
		fullday->lower = lower;
		fullday->upper = upper;

		gncal_full_day_update (fullday);
	}
}
