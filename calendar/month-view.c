/* Month view display for gncal
 *
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <libgnomeui/gnome-canvas-text.h>
#include "layout.h"
#include "month-view.h"
#include "main.h"
#include "mark.h"
#include "timeutil.h"


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
	int i;
	GnomeCanvasItem *day_group;
	GnomeCanvasPoints *points;
	char *color_spec;

	/* Title */

	mv->title = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (mv)),
					   gnome_canvas_text_get_type (),
					   "anchor", GTK_ANCHOR_N,
					   "font", HEADING_FONT,
					   "fill_color", "black",
					   NULL);

	/* Month item */

	mv->mitem = gnome_month_item_new (gnome_canvas_root (GNOME_CANVAS (mv)));
	gnome_canvas_item_set (mv->mitem,
			       "x", 0.0,
			       "anchor", GTK_ANCHOR_NW,
			       "day_anchor", GTK_ANCHOR_NE,
			       "start_on_monday", week_starts_on_monday,
			       "heading_padding", 2.0,
			       "heading_font", BIG_DAY_HEADING_FONT,
			       "day_font", BIG_NORMAL_DAY_FONT,
			       NULL);

	/* Arrows and text items */

	color_spec = color_spec_from_prop (COLOR_PROP_DAY_FG);

	points = gnome_canvas_points_new (3);

	for (i = 0; i < 42; i++) {
		day_group = gnome_month_item_num2child (GNOME_MONTH_ITEM (mv->mitem),
							i + GNOME_MONTH_ITEM_DAY_GROUP);

		/* Up arrow */

		points->coords[0] = 3;
		points->coords[1] = 10;
		points->coords[2] = 11;
		points->coords[3] = 10;
		points->coords[4] = 7;
		points->coords[5] = 3;

		mv->up[i] = gnome_canvas_item_new (GNOME_CANVAS_GROUP (day_group),
						   gnome_canvas_polygon_get_type (),
						   "points", points,
						   "fill_color", color_spec,
						   "outline_color", color_spec,
						   NULL);
		gnome_canvas_item_hide (mv->up[i]);

		/* Down arrow */

		points->coords[0] = 13;
		points->coords[1] = 3;
		points->coords[2] = 17;
		points->coords[3] = 10;
		points->coords[4] = 21;
		points->coords[5] = 3;

		mv->down[i] = gnome_canvas_item_new (GNOME_CANVAS_GROUP (day_group),
						     gnome_canvas_polygon_get_type (),
						     "points", points,
						     "fill_color", color_spec,
						     "outline_color", color_spec,
						     NULL);
		gnome_canvas_item_hide (mv->down[i]);
	}

	mv->old_current_index = -1;
}

GtkWidget *
month_view_new (GnomeCalendar *calendar, time_t month)
{
	MonthView *mv;

	g_return_val_if_fail (calendar != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (calendar), NULL);

	mv = gtk_type_new (month_view_get_type ());
	mv->calendar = calendar;

	month_view_colors_changed (mv);
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
}

void
month_view_update (MonthView *mv, iCalObject *object, int flags)
{
	g_return_if_fail (mv != NULL);
	g_return_if_fail (IS_MONTH_VIEW (mv));

	/* FIXME */
}

/* Unmarks the old day that was marked as current and marks the current day if appropriate */
static void
mark_current_day (MonthView *mv)
{
	time_t t;
	struct tm *tm;
	GnomeCanvasItem *item;

	/* Unmark the old day */

	if (mv->old_current_index != -1) {
		item = gnome_month_item_num2child (GNOME_MONTH_ITEM (mv->mitem),
						   GNOME_MONTH_ITEM_DAY_LABEL + mv->old_current_index);
		gnome_canvas_item_set (item,
				       "fill_color", color_spec_from_prop (COLOR_PROP_DAY_FG),
				       "font", BIG_NORMAL_DAY_FONT,
				       NULL);

		mv->old_current_index = -1;
	}

	/* Mark the new day */

	t = time (NULL);
	tm = localtime (&t);

	if (((tm->tm_year + 1900) == mv->year) && (tm->tm_mon == mv->month)) {
		mv->old_current_index = gnome_month_item_day2index (GNOME_MONTH_ITEM (mv->mitem), tm->tm_mday);
		g_assert (mv->old_current_index != -1);

		item = gnome_month_item_num2child (GNOME_MONTH_ITEM (mv->mitem),
						   GNOME_MONTH_ITEM_DAY_LABEL + mv->old_current_index);
		gnome_canvas_item_set (item,
				       "fill_color", color_spec_from_prop (COLOR_PROP_CURRENT_DAY_FG),
				       "font", BIG_CURRENT_DAY_FONT,
				       NULL);
	}
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

	mv->year = tm->tm_year + 1900;
	mv->month = tm->tm_mon;
	
	strftime (buf, 100, "%B %Y", tm);

	gnome_canvas_item_set (mv->title,
			       "text", buf,
			       NULL);

	/* Month item */

	gnome_canvas_item_set (mv->mitem,
			       "year", mv->year,
			       "month", mv->month,
			       NULL);

	/* Update events */

	month_view_update (mv, NULL, 0);
	mark_current_day (mv);
}

void
month_view_time_format_changed (MonthView *mv)
{
	g_return_if_fail (mv != NULL);
	g_return_if_fail (IS_MONTH_VIEW (mv));

	gnome_canvas_item_set (mv->mitem,
			       "start_on_monday", week_starts_on_monday,
			       NULL);

	month_view_set (mv, time_month_begin (time_from_day (mv->year, mv->month, 1)));
}

void
month_view_colors_changed (MonthView *mv)
{
	g_return_if_fail (mv != NULL);
	g_return_if_fail (IS_MONTH_VIEW (mv));

	colorify_month_item (GNOME_MONTH_ITEM (mv->mitem), default_color_func, NULL);
	mark_current_day (mv);

	/* FIXME: set children to the marked color */
}
