/* Day view widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena   <quartic@gimp.org>
 *          Miguel de Icaza <miguel@kernel.org>
 */

#include <gnome.h>
#include "gncal-day-view.h"
#include "timeutil.h"
#include "view-utils.h"
#include "main.h"
#include "eventedit.h"
#include "popup-menu.h"


#define TEXT_BORDER 2
#define MIN_INFO_WIDTH 50


static void gncal_day_view_class_init   (GncalDayViewClass *class);
static void gncal_day_view_init         (GncalDayView      *dview);
static void gncal_day_view_destroy      (GtkObject         *object);
static void gncal_day_view_realize      (GtkWidget         *widget);
static void gncal_day_view_size_request (GtkWidget         *widget,
					 GtkRequisition    *requisition);
static gint gncal_day_view_expose       (GtkWidget         *widget,
					 GdkEventExpose    *event);
static gint gncal_day_view_button_press (GtkWidget         *widget,
					 GdkEventButton    *event);


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

	object_class->destroy = gncal_day_view_destroy;

	widget_class->realize = gncal_day_view_realize;
	widget_class->size_request = gncal_day_view_size_request;
	widget_class->expose_event = gncal_day_view_expose;
	widget_class->button_press_event = gncal_day_view_button_press;
}

static void
gncal_day_view_init (GncalDayView *dview)
{
	GTK_WIDGET_UNSET_FLAGS (dview, GTK_NO_WINDOW);

	dview->calendar = NULL;

	dview->lower = 0;
	dview->upper = 0;

	dview->shadow_type = GTK_SHADOW_ETCHED_IN;
}

static void
gncal_day_view_destroy (GtkObject *object)
{
	GncalDayView *dview;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNCAL_IS_DAY_VIEW (object));

	dview = GNCAL_DAY_VIEW (object);

	if (dview->day_str)
		g_free (dview->day_str);
	if (dview->events)
		calendar_destroy_event_list (dview->events);
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static int
new_appointment (GtkWidget *widget, gpointer data)
{
	GncalDayView *dayview = GNCAL_DAY_VIEW (data);
	iCalObject *ico;
	GtkWidget *ee;
	struct tm tm;
	
	ico = ical_new ("", user_name, "");
	ico->new = 1;

	tm = *localtime (&dayview->lower);
	tm.tm_hour = day_begin;
	ico->dtstart = mktime (&tm);
	tm.tm_hour++;
	ico->dtend   = mktime (&tm);
	ee = event_editor_new (dayview->calendar, ico);
	gtk_widget_show (ee);
	return 1;
}

static void
context_menu (GncalDayView *dayview, GdkEventButton *event)
{
	static struct menu_item main_items[] = {
		{ N_("New appointment..."), (GtkSignalFunc) new_appointment, NULL, TRUE }
	};

	main_items [0].data = dayview;
	popup_menu (main_items, 1, event);
}

static gint
gncal_day_view_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GncalDayView *dayview;

	dayview = GNCAL_DAY_VIEW (widget);

	if (event->button == 3)
		context_menu (dayview, event);

	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
		gnome_calendar_dayjump (dayview->calendar, dayview->lower);

	return TRUE;
}

GtkWidget *
gncal_day_view_new (GnomeCalendar *calendar, time_t lower, time_t upper)
{
	GncalDayView *dview;

	g_return_val_if_fail (calendar != NULL, NULL);

	dview = gtk_type_new (gncal_day_view_get_type ());

	dview->calendar = calendar;
	dview->lower    = lower;
	dview->upper    = upper;
	dview->events   = 0;

	gncal_day_view_update (dview, NULL, 0);

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

	gdk_window_set_background (widget->window, &widget->style->bg[GTK_STATE_PRELIGHT]);
}

static void
gncal_day_view_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GncalDayView *dview;
	int str_width, width;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_DAY_VIEW (widget));
	g_return_if_fail (requisition != NULL);

	dview = GNCAL_DAY_VIEW (widget);

	/* border and min width */

	str_width = gdk_string_width (widget->style->font, dview->day_str);

	width = MAX (MIN_INFO_WIDTH, str_width);

	requisition->width = 2 * (widget->style->klass->xthickness + TEXT_BORDER) + width;
	requisition->height = 2 * (widget->style->klass->ythickness + TEXT_BORDER);

	/* division line */

	requisition->height += 2 * TEXT_BORDER + widget->style->klass->ythickness;

	/* title and at least one line of text */

	requisition->height += 2 * (widget->style->font->ascent + widget->style->font->descent);
}

static gint
gncal_day_view_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GncalDayView *dview;
	int x1, y1, width, height;
	GdkRectangle rect, dest;
	GdkFont *font;
	int str_width;
	GdkGC *gc;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (!GTK_WIDGET_DRAWABLE (widget))
		return FALSE;

	gc = widget->style->fg_gc [GTK_STATE_NORMAL];
	dview = GNCAL_DAY_VIEW (widget);

	x1 = widget->style->klass->xthickness;
	y1 = widget->style->klass->ythickness;
	width = widget->allocation.width - 2 * x1;
	height = widget->allocation.height - 2 * y1;

	/* Clear and paint frame shadow */

	gdk_window_clear_area (widget->window,
			       event->area.x, event->area.y,
			       event->area.width, event->area.height);

	gtk_draw_shadow (widget->style, widget->window,
			 GTK_STATE_NORMAL, dview->shadow_type,
			 0, 0,
			 widget->allocation.width,
			 widget->allocation.height);

	/* Clear and paint title */

	font = widget->style->font;

	rect.x = x1;
	rect.y = y1;
	rect.width = width;
	rect.height = 2 * TEXT_BORDER + font->ascent + font->descent;

	if (gdk_rectangle_intersect (&rect, &event->area, &dest)) {
		gdk_draw_rectangle (widget->window,
				    widget->style->bg_gc[GTK_STATE_NORMAL],
				    TRUE,
				    dest.x, dest.y,
				    dest.width, dest.height);

		dest = rect;

		dest.x += TEXT_BORDER;
		dest.y += TEXT_BORDER;
		dest.width -= 2 * TEXT_BORDER;
		dest.height -= 2 * TEXT_BORDER;

		gdk_gc_set_clip_rectangle (gc, &dest);

		str_width = gdk_string_width (font, dview->day_str);
		
		gdk_draw_string (widget->window, font, gc,
				 dest.x + (dest.width - str_width) / 2,
				 dest.y + font->ascent,
				 dview->day_str);

		gdk_gc_set_clip_rectangle (gc, NULL);
	}

	/* Division line */

	gtk_draw_hline (widget->style,
			widget->window,
			GTK_STATE_NORMAL,
			rect.x,
			rect.x + rect.width - 1,
			rect.y + rect.height);

	/* Text */

	if (dview->events != NULL){
		rect.x = x1 + TEXT_BORDER;
		rect.y = y1 + 3 * TEXT_BORDER +
			font->ascent + font->descent +
			widget->style->klass->ythickness;
		rect.width = width - 2 * TEXT_BORDER;
		rect.height = height - (rect.y - y1) - TEXT_BORDER;
		
		if (gdk_rectangle_intersect (&rect, &event->area, &dest))
			view_utils_draw_events (
				widget, widget->window, gc,
				&rect,
				VIEW_UTILS_DRAW_END | VIEW_UTILS_DRAW_SPLIT,
				dview->events,
				dview->lower,
				dview->upper);
	}

	return FALSE;
}

void
gncal_day_view_update (GncalDayView *dview, iCalObject *ico, int flags)
{
	struct tm tm;
	char buf[256];

	g_return_if_fail (dview != NULL);
	g_return_if_fail (GNCAL_IS_DAY_VIEW (dview));
	g_return_if_fail (dview->calendar->cal);

	if (dview->day_str)
		g_free (dview->day_str);

	tm = *localtime (&dview->lower);
	strftime (buf, sizeof (buf)-1, "%A %d", &tm);
	dview->day_str = g_strdup (buf);

	if (dview->events)
		calendar_destroy_event_list (dview->events);

	dview->events = calendar_get_events_in_range (dview->calendar->cal,
						      dview->lower,
						      dview->upper);

	gtk_widget_draw (GTK_WIDGET (dview), NULL);
}

void
gncal_day_view_set_bounds (GncalDayView *dview, time_t lower, time_t upper)
{
	g_return_if_fail (dview != NULL);
	g_return_if_fail (GNCAL_IS_DAY_VIEW (dview));

	if ((lower != dview->lower) || (upper != dview->upper)) {
		dview->lower = lower;
		dview->upper = upper;

		gncal_day_view_update (dview, NULL, 0);
	}
}

void
gncal_day_view_set_shadow (GncalDayView *dview, GtkShadowType shadow_type)
{
	g_return_if_fail (dview != NULL);
	g_return_if_fail (GNCAL_IS_DAY_VIEW (dview));

	if (shadow_type != dview->shadow_type) {
		dview->shadow_type = shadow_type;

		gtk_widget_draw (GTK_WIDGET (dview), NULL);
	}
}
