/* Day view widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include "gncal-day-view.h"
#include "timeutil.h"


#define TEXT_BORDER 2
#define MIN_INFO_WIDTH 50


static void gncal_day_view_class_init   (GncalDayViewClass *class);
static void gncal_day_view_init         (GncalDayView      *dview);
static void gncal_day_view_realize      (GtkWidget         *widget);
static void gncal_day_view_size_request (GtkWidget         *widget,
					 GtkRequisition    *requisition);
static gint gncal_day_view_expose       (GtkWidget         *widget,
					 GdkEventExpose    *event);


static GtkWidgetClass *parent_class;


guint
gncal_day_view_get_type (void)
{
	static guint day_view_type = 0;

	if (!day_view_type) {
		GtkTypeInfo day_view_info = {
			"GncalDayView",
			sizeof (GncalDayView),
			sizeof (GncalDayViewClass),
			(GtkClassInitFunc) gncal_day_view_class_init,
			(GtkObjectInitFunc) gncal_day_view_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		day_view_type = gtk_type_unique (gtk_widget_get_type (), &day_view_info);
	}

	return day_view_type;
}

static void
gncal_day_view_class_init (GncalDayViewClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gtk_widget_get_type ());

	widget_class->realize = gncal_day_view_realize;
	widget_class->size_request = gncal_day_view_size_request;
	widget_class->expose_event = gncal_day_view_expose;
}

static void
gncal_day_view_init (GncalDayView *dview)
{
	GTK_WIDGET_UNSET_FLAGS (dview, GTK_NO_WINDOW);

	dview->calendar = NULL;

	dview->lower = 0;
	dview->upper = 24;
	dview->use_am_pm = TRUE;
}

GtkWidget *
gncal_day_view_new (Calendar *calendar)
{
	GncalDayView *dview;

#if 0
	g_assert (calendar != NULL);
#endif

	dview = gtk_type_new (gncal_day_view_get_type ());

	dview->calendar = calendar;

	return GTK_WIDGET (dview);
}

static void
gncal_day_view_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_DAY_VIEW (widget));

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
				 | GDK_EXPOSURE_MASK
				 | GDK_BUTTON_PRESS_MASK);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);

	gtk_style_set_background (widget->style, widget->window, GTK_STATE_SELECTED);
}

static int
calc_labels_width (GncalDayView *dview)
{
	int width, max_width;
	GdkFont *font;
	char *buf;
	int i;

	font = GTK_WIDGET (dview)->style->font;

	max_width = 0;

	for (i = 0; i < 24; i++) {
		buf = format_simple_hour (i, dview->use_am_pm);
		width = gdk_string_width (font, buf);
		if (width > max_width)
			max_width = width;
	}

	return width;
}

static void
gncal_day_view_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GncalDayView *dview;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_DAY_VIEW (widget));
	g_return_if_fail (requisition != NULL);

	dview = GNCAL_DAY_VIEW (widget);

	requisition->width = 2 * widget->style->klass->xthickness + 4 * TEXT_BORDER + MIN_INFO_WIDTH;
	requisition->height = 2 * widget->style->klass->xthickness;

	requisition->width += calc_labels_width (dview);
	requisition->height += ((dview->upper - dview->lower)
				* (widget->style->font->ascent + widget->style->font->descent
				   + 2 * TEXT_BORDER));
}

static gint
gncal_day_view_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GncalDayView *dview;
	int x1, y1, width, height;
	int division_x;
	int row_height;
	int i;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (!GTK_WIDGET_DRAWABLE (widget))
		return FALSE;

	dview = GNCAL_DAY_VIEW (widget);

	x1 = widget->style->klass->xthickness;
	y1 = widget->style->klass->ythickness;
	width = widget->allocation.width - 2 * x1;
	height = widget->allocation.width - 2 * y1;

	/* Clear and paint frame shadow */

	gdk_window_clear_area (widget->window, event->area.x, event->area.y, event->area.width, event->area.height);

	gtk_draw_shadow (widget->style, widget->window,
			 GTK_STATE_NORMAL, GTK_SHADOW_IN,
			 0, 0,
			 widget->allocation.width,
			 widget->allocation.height);

	/* Divisions */

	division_x = x1 + 2 * TEXT_BORDER + calc_labels_width (dview);

	gdk_draw_line (widget->window,
		       widget->style->black_gc,
		       division_x, y1,
		       division_x, y1 + height - 1);

	row_height = height / (dview->upper - dview->lower);

	for (i = 0; i < (dview->upper - dview->lower - 1); i++)
		gdk_draw_line (widget->window,
			       widget->style->black_gc,
			       x1,
			       y1 + (i + 1) * row_height,
			       x1 + width - 1,
			       y1 + (i + 1) * row_height);

	return FALSE;
}

void
gncal_day_view_set_bounds (GncalDayView *dview, int lower, int upper)
{
	g_return_if_fail (dview != NULL);
	g_return_if_fail (GNCAL_IS_DAY_VIEW (dview));

	lower = CLAMP (lower, 0, 23);
	upper = CLAMP (upper, lower + 1, 24);

	if ((lower != dview->lower) || (upper != dview->upper)) {
		dview->lower = lower;
		dview->upper = upper;

		gtk_widget_queue_resize (GTK_WIDGET (dview));
	}
}

void
gncal_day_view_set_format (GncalDayView *dview, int use_am_pm)
{
	g_return_if_fail (dview != NULL);
	g_return_if_fail (GNCAL_IS_DAY_VIEW (dview));

	use_am_pm = use_am_pm ? TRUE : FALSE;

	if (use_am_pm != dview->use_am_pm) {
		dview->use_am_pm = use_am_pm;

		gtk_widget_queue_resize (GTK_WIDGET (dview));
	}
}
