/* Year view display for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Arturo Espinosa <arturo@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <gtk/gtkmain.h>
#include <libgnomeui/gnome-canvas-text.h>
#include "year-view.h"
#include "main.h"
#include "mark.h"
#include "timeutil.h"


#define HEAD_SPACING 4		/* Spacing between year heading and months */
#define TITLE_SPACING 2		/* Spacing between title and calendar */
#define SPACING 4		/* Spacing between months */


static void year_view_class_init    (YearViewClass  *class);
static void year_view_init          (YearView       *yv);
static void year_view_destroy       (GtkObject      *object);
static void year_view_size_request  (GtkWidget      *widget,
				     GtkRequisition *requisition);
static void year_view_size_allocate (GtkWidget      *widget,
				     GtkAllocation  *allocation);


static GnomeCanvas *parent_class;


GtkType
year_view_get_type (void)
{
	static GtkType year_view_type = 0;

	if (!year_view_type) {
		GtkTypeInfo year_view_info = {
			"YearView",
			sizeof (YearView),
			sizeof (YearViewClass),
			(GtkClassInitFunc) year_view_class_init,
			(GtkObjectInitFunc) year_view_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		year_view_type = gtk_type_unique (gnome_canvas_get_type (), &year_view_info);
	}

	return year_view_type;
}

static void
year_view_class_init (YearViewClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gnome_canvas_get_type ());

	object_class->destroy = year_view_destroy;

	widget_class->size_request = year_view_size_request;
	widget_class->size_allocate = year_view_size_allocate;
}

/* Resizes the year view's child items.  This is done in the idle loop for performance (we avoid
 * resizing on every size allocation).
 */
static gint
idle_handler (gpointer data)
{
	YearView *yv;
	double width, height;
	double mwidth, mheight;
	double h_yofs, m_yofs;
	double x, y;
	GtkArg arg;
	GdkFont *head_font, *title_font;
	int i;

	yv = data;

	/* Get the fonts to get their size later */

	arg.name = "font_gdk";
	gtk_object_getv (GTK_OBJECT (yv->heading), 1, &arg);
	head_font = GTK_VALUE_BOXED (arg);

	arg.name = "font_gdk";
	gtk_object_getv (GTK_OBJECT (yv->titles[0]), 1, &arg);
	title_font = GTK_VALUE_BOXED (arg);

	/* Adjust heading */

	gnome_canvas_item_set (yv->heading,
			       "x", (double) yv->canvas.width / 2.0,
			       "y", (double) HEAD_SPACING,
			       NULL);

	/* Adjust months */

	h_yofs = 2 * HEAD_SPACING + head_font->ascent + head_font->descent;
	m_yofs = SPACING + title_font->ascent + title_font->descent;

	width = (yv->canvas.width + SPACING) / 3.0;
	height = (yv->canvas.height - h_yofs + SPACING) / 4.0;

	mwidth = (yv->canvas.width - 2 * SPACING) / 3.0;
	mheight = (yv->canvas.height - h_yofs - 3 * SPACING - 4 * m_yofs) / 4.0;

	for (i = 0; i < 12; i++) {
		x = (i % 3) * width;
		y = (i / 3) * height + h_yofs;

		/* Title */

		gnome_canvas_item_set (yv->titles[i],
				       "x", x + width / 2.0,
				       "y", y,
				       NULL);

		/* Month item */

		gnome_canvas_item_set (yv->mitems[i],
				       "x", x,
				       "y", y + m_yofs,
				       "width", mwidth,
				       "height", mheight,
				       NULL);
	}

	/* Done */

	yv->need_resize = FALSE;
	return FALSE;
}

/* Marks the year view as needing a resize, which will be performed during the idle loop */
static void
need_resize (YearView *yv)
{
	if (yv->need_resize)
		return;

	yv->need_resize = TRUE;
	yv->idle_id = gtk_idle_add (idle_handler, yv);
}

/* Event handler for days in the year's month items */
static gint
day_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	YearView *yv;
	GnomeCanvasItem *mitem;
	int child_num, day;

	mitem = data;
	child_num = gnome_month_item_child2num (GNOME_MONTH_ITEM (mitem), item);
	day = gnome_month_item_num2day (GNOME_MONTH_ITEM (mitem), child_num);

	yv = gtk_object_get_user_data (GTK_OBJECT (mitem));

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if ((event->button.button == 1) && (day != 0))
			gnome_calendar_dayjump (yv->calendar,
						time_from_day (GNOME_MONTH_ITEM (mitem)->year,
							       GNOME_MONTH_ITEM (mitem)->month,
							       day));
		break;

	default:
		break;
	}

	return FALSE;
}

/* Sets up the month item with the specified index -- connects signals for handling events, etc. */
static void
setup_month_item (YearView *yv, int n)
{
	GnomeCanvasItem *mitem;
	GnomeCanvasItem *item;
	int i;

	mitem = yv->mitems[n];

	/* Connect signals */

	for (i = 0; i < 42; i++) {
		item = gnome_month_item_num2child (GNOME_MONTH_ITEM (mitem), GNOME_MONTH_ITEM_DAY_GROUP + i);
		gtk_signal_connect (GTK_OBJECT (item), "event",
				    (GtkSignalFunc) day_event,
				    mitem);
	}
}

static void
year_view_init (YearView *yv)
{
	int i;
	char buf[100];
	struct tm tm;

	memset (&tm, 0, sizeof (tm));

	/* Heading */

	yv->heading = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (yv)),
					     gnome_canvas_text_get_type (),
					     "anchor", GTK_ANCHOR_N,
					     "font", "-*-helvetica-bold-r-normal--14-*-*-*-*-*-iso8859-1",
					     "fill_color", "black",
					     NULL);

	/* Months */

	for (i = 0; i < 12; i++) {
		/* Title */

		strftime (buf, 100, "%B", &tm);
		tm.tm_mon++;

		yv->titles[i] = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (yv)),
						       gnome_canvas_text_get_type (),
						       "text", buf,
						       "anchor", GTK_ANCHOR_N,
						       "font", "-*-helvetica-bold-r-normal--12-*-*-*-*-*-iso8859-1",
						       "fill_color", "black",
						       NULL);

		/* Month item */

		yv->mitems[i] = gnome_month_item_new (gnome_canvas_root (GNOME_CANVAS (yv)));
		gtk_object_set_user_data (GTK_OBJECT (yv->mitems[i]), yv);
		gnome_canvas_item_set (yv->mitems[i],
				       "anchor", GTK_ANCHOR_NW,
				       "start_on_monday", week_starts_on_monday,
				       NULL);
		setup_month_item (yv, i);
	}

	/* We will need to resize the items when we paint for the first time */

	yv->idle_id = -1;
	need_resize (yv);
}

static void
year_view_destroy (GtkObject *object)
{
	YearView *yv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_YEAR_VIEW (object));

	yv = YEAR_VIEW (object);

	if (yv->need_resize) {
		yv->need_resize = FALSE;
		gtk_idle_remove (yv->idle_id);
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

GtkWidget *
year_view_new (GnomeCalendar *calendar, time_t year)
{
	YearView *yv;

	g_return_val_if_fail (calendar != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (calendar), NULL);

	yv = gtk_type_new (year_view_get_type ());
	yv->calendar = calendar;

	year_view_set (yv, year);
	return GTK_WIDGET (yv);
}

static void
year_view_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	YearView *yv;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_YEAR_VIEW (widget));
	g_return_if_fail (requisition != NULL);

	yv = YEAR_VIEW (widget);

	if (GTK_WIDGET_CLASS (parent_class)->size_request)
		(* GTK_WIDGET_CLASS (parent_class)->size_request) (widget, requisition);

	requisition->width = 200;
	requisition->height = 150;
}

static void
year_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	YearView *yv;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_YEAR_VIEW (widget));
	g_return_if_fail (allocation != NULL);

	yv = YEAR_VIEW (widget);

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (yv), 0, 0, allocation->width, allocation->height);
	need_resize (yv);
}

void
year_view_update (YearView *yv, iCalObject *object, int flags)
{
	g_return_if_fail (yv != NULL);
	g_return_if_fail (IS_YEAR_VIEW (yv));

	/* If only the summary changed, we don't care */

	if (object && ((flags & CHANGE_SUMMARY) == flags))
		return;

	year_view_set (yv, time_year_begin (time_from_day (yv->year, 0, 1)));
}

void
year_view_set (YearView *yv, time_t year)
{
	struct tm *tm;
	char buf[100];
	int i;

	g_return_if_fail (yv != NULL);
	g_return_if_fail (IS_YEAR_VIEW (yv));

	tm = localtime (&year);
	yv->year = tm->tm_year + 1900;

	/* Heading */

	sprintf (buf, "%d", yv->year);
	gnome_canvas_item_set (yv->heading,
			       "text", buf,
			       NULL);

	/* Months */

	for (i = 0; i < 12; i++)
		gnome_canvas_item_set (yv->mitems[i],
				       "year", yv->year,
				       "month", i,
				       NULL);

	/* Unmark and re-mark all the months */

	year_view_colors_changed (yv);
}

void
year_view_time_format_changed (YearView *yv)
{
	int i;

	g_return_if_fail (yv != NULL);
	g_return_if_fail (IS_YEAR_VIEW (yv));

	for (i = 0; i < 12; i++)
		gnome_canvas_item_set (yv->mitems[i],
				       "start_on_monday", week_starts_on_monday,
				       NULL);

	year_view_set (yv, time_year_begin (time_from_day (yv->year, 0, 1)));
}

void
year_view_colors_changed (YearView *yv)
{
	int i;

	g_return_if_fail (yv != NULL);
	g_return_if_fail (IS_YEAR_VIEW (yv));

	for (i = 0; i < 12; i++) {
		unmark_month_item (GNOME_MONTH_ITEM (yv->mitems[i]));
		mark_month_item (GNOME_MONTH_ITEM (yv->mitems[i]), yv->calendar->cal);
		month_item_prepare_prelight (GNOME_MONTH_ITEM (yv->mitems[i]), default_prelight_func, NULL);
	}
}
