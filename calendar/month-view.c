/* Month view display for gncal
 *
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <libgnomeui/gnome-canvas-text.h>
#include "month-view.h"
#include "main.h"


#define SPACING 4		/* Spacing between title and calendar */


static void month_view_class_init    (MonthViewClass *class);
static void month_view_init          (MonthView      *mv);
static void month_view_size_request  (GtkWidget      *widget,
				      GtkRequisition *requisition);
static void month_view_size_allocate (GtkWidget      *widget,
				      GtkAllocation  *allocation);


static GnomeCanvasClass *parent_class;


GtkType
month_view_get_type (void)
{
	static GtkType month_view_type = 0;

	if (!month_view_type) {
		GtkTypeInfo month_view_info = {
			"MonthView",
			sizeof (MonthView),
			sizeof (MonthViewClass),
			(GtkClassInitFunc) month_view_class_init,
			(GtkObjectInitFunc) month_view_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		month_view_type = gtk_type_unique (gnome_canvas_get_type (), &month_view_info);
	}

	return month_view_type;
}

static void
month_view_class_init (MonthViewClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gnome_canvas_get_type ());

	widget_class->size_request = month_view_size_request;
	widget_class->size_allocate = month_view_size_allocate;
}

static void
month_view_init (MonthView *mv)
{
	/* Title */

	mv->title = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (mv)),
					   gnome_canvas_text_get_type (),
					   "anchor", GTK_ANCHOR_N,
					   "font", "-*-helvetica-bold-r-normal--18-*-*-*-p-*-iso8859-1",
					   "fill_color", "black",
					   NULL);

	/* Month item */

	mv->mitem = gnome_month_item_new (gnome_canvas_root (GNOME_CANVAS (mv)));
	gnome_canvas_item_set (mv->mitem,
			       "x", 0.0,
			       "anchor", GTK_ANCHOR_NW,
			       "day_anchor", GTK_ANCHOR_NE,
			       "start_on_monday", week_starts_on_monday,
			       "heading_height", 18.0,
			       "heading_font", "-*-helvetica-bold-r-normal--12-*-*-*-*-*-iso8859-1",
			       "day_font", "-*-helvetica-bold-r-normal--14-*-*-*-*-*-iso8859-1",
			       NULL);
}

GtkWidget *
month_view_new (GnomeCalendar *calendar, time_t month)
{
	MonthView *mv;

	g_return_val_if_fail (calendar != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (calendar), NULL);

	mv = gtk_type_new (month_view_get_type ());
	mv->calendar = calendar;

	month_view_set (mv, month);
	return GTK_WIDGET (mv);
}

static void
month_view_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_MONTH_VIEW (widget));
	g_return_if_fail (requisition != NULL);

	if (GTK_WIDGET_CLASS (parent_class)->size_request)
		(* GTK_WIDGET_CLASS (parent_class)->size_request) (widget, requisition);

	requisition->width = 200;
	requisition->height = 150;
}

static void
month_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	MonthView *mv;
	GdkFont *font;
	GtkArg arg;
	int y;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_MONTH_VIEW (widget));
	g_return_if_fail (allocation != NULL);

	mv = MONTH_VIEW (widget);

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (mv), 0, 0, allocation->width, allocation->height);

	/* Adjust items to new size */

	arg.name = "font_gdk";
	gtk_object_getv (GTK_OBJECT (mv->title), 1, &arg);
	font = GTK_VALUE_BOXED (arg);

	gnome_canvas_item_set (mv->title,
			       "x", (double) allocation->width / 2.0,
			       "y", (double) SPACING,
			       NULL);

	y = font->ascent + font->descent + 2 * SPACING;
	gnome_canvas_item_set (mv->mitem,
			       "y", (double) y,
			       "width", (double) (allocation->width - 1),
			       "height", (double) (allocation->height - y - 1),
			       NULL);

	/* FIXME: adjust events */
}

void
month_view_update (MonthView *mv, iCalObject *object, int flags)
{
	g_return_if_fail (mv != NULL);
	g_return_if_fail (IS_MONTH_VIEW (mv));

	/* FIXME */
}

void
month_view_set (MonthView *mv, time_t month)
{
	struct tm *tm;
	char buf[100];

	g_return_if_fail (mv != NULL);
	g_return_if_fail (IS_MONTH_VIEW (mv));

	/* Title */

	tm = localtime (&month);
	strftime (buf, 100, "%B %Y", tm);

	gnome_canvas_item_set (mv->title,
			       "text", buf,
			       NULL);

	/* Month item */

	gnome_canvas_item_set (mv->mitem,
			       "year", tm->tm_year + 1900,
			       "month", tm->tm_mon,
			       NULL);

	/* FIXME: update events */
}

void
month_view_time_format_changed (MonthView *mv)
{
	g_return_if_fail (mv != NULL);
	g_return_if_fail (IS_MONTH_VIEW (mv));

	gnome_canvas_item_set (mv->mitem,
			       "start_on_monday", week_starts_on_monday,
			       NULL);

	/* FIXME: update events */
}

void
month_view_colors_changed (MonthView *mv)
{
	g_return_if_fail (mv != NULL);
	g_return_if_fail (IS_MONTH_VIEW (mv));

	unmark_month_item (mv->mitem);
	/* FIXME */
}
