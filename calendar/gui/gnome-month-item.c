/* General-purpose monthly calendar canvas item for GNOME
 *
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <math.h>
#include <time.h>
#include <gnome.h>
#include "gnome-month-item.h"


#define DEFAULT_FONT "-*-helvetica-medium-r-normal--10-*-*-*-p-*-*-*"


/* Number of days in a month, for normal and leap years */
static const int days_in_month[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

/* The weird month of September 1752, where 3 Sep through 13 Sep were eliminated due to the
 * Gregorian reformation.
 */
static const int sept_1752[42] = {
	 0,  0,  1,  2, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30,
	 0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0
};

#define REFORMATION_DAY 639787		/* First day of the reformation, counted from 1 Jan 1 */
#define MISSING_DAYS 11			/* They corrected out 11 days */
#define THURSDAY 4			/* First day of reformation */
#define SATURDAY 6			/* Offset value; 1 Jan 1 was a Saturday */
#define SEPT_1752_START 2		/* Start day within month */
#define SEPT_1752_END 20		/* End day within month */


enum {
	ARG_0,
	ARG_YEAR,
	ARG_MONTH,
	ARG_X,
	ARG_Y,
	ARG_WIDTH,
	ARG_HEIGHT,
	ARG_ANCHOR,
	ARG_HEAD_PADDING,
	ARG_DAY_PADDING,
	ARG_DAY_NAMES,
	ARG_HEADING_HEIGHT,
	ARG_HEADING_ANCHOR,
	ARG_DAY_ANCHOR,
	ARG_START_ON_MONDAY,
	ARG_HEAD_FONT,
	ARG_HEAD_FONTSET,
	ARG_HEAD_FONT_GDK,
	ARG_DAY_FONT,
	ARG_DAY_FONTSET,
	ARG_DAY_FONT_GDK,
	ARG_HEAD_COLOR,
	ARG_HEAD_COLOR_GDK,
	ARG_OUTLINE_COLOR,
	ARG_OUTLINE_COLOR_GDK,
	ARG_DAY_BOX_COLOR,
	ARG_DAY_BOX_COLOR_GDK,
	ARG_DAY_COLOR,
	ARG_DAY_COLOR_GDK
};


static void gnome_month_item_class_init (GnomeMonthItemClass *class);
static void gnome_month_item_init       (GnomeMonthItem      *mitem);
static void gnome_month_item_destroy    (GtkObject           *object);
static void gnome_month_item_set_arg    (GtkObject           *object,
					 GtkArg              *arg,
					 guint                arg_id);
static void gnome_month_item_get_arg    (GtkObject           *object,
					 GtkArg              *arg,
					 guint                arg_id);



static GnomeCanvasGroupClass *parent_class;


GtkType
gnome_month_item_get_type (void)
{
	static GtkType month_item_type = 0;

	if (!month_item_type) {
		GtkTypeInfo month_item_info = {
			"GnomeMonthItem",
			sizeof (GnomeMonthItem),
			sizeof (GnomeMonthItemClass),
			(GtkClassInitFunc) gnome_month_item_class_init,
			(GtkObjectInitFunc) gnome_month_item_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		month_item_type = gtk_type_unique (gnome_canvas_group_get_type (), &month_item_info);
	}

	return month_item_type;
}

static void
gnome_month_item_class_init (GnomeMonthItemClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_group_get_type ());

	gtk_object_add_arg_type ("GnomeMonthItem::year", GTK_TYPE_UINT, GTK_ARG_READWRITE, ARG_YEAR);
	gtk_object_add_arg_type ("GnomeMonthItem::month", GTK_TYPE_UINT, GTK_ARG_READWRITE, ARG_MONTH);
	gtk_object_add_arg_type ("GnomeMonthItem::x", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X);
	gtk_object_add_arg_type ("GnomeMonthItem::y", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y);
	gtk_object_add_arg_type ("GnomeMonthItem::width", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_WIDTH);
	gtk_object_add_arg_type ("GnomeMonthItem::height", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_HEIGHT);
	gtk_object_add_arg_type ("GnomeMonthItem::anchor", GTK_TYPE_ANCHOR_TYPE, GTK_ARG_READWRITE, ARG_ANCHOR);
	gtk_object_add_arg_type ("GnomeMonthItem::heading_padding", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_HEAD_PADDING);
	gtk_object_add_arg_type ("GnomeMonthItem::day_padding", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_DAY_PADDING);
	gtk_object_add_arg_type ("GnomeMonthItem::day_names", GTK_TYPE_POINTER, GTK_ARG_WRITABLE, ARG_DAY_NAMES);
	gtk_object_add_arg_type ("GnomeMonthItem::heading_height", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_HEADING_HEIGHT);
	gtk_object_add_arg_type ("GnomeMonthItem::heading_anchor", GTK_TYPE_ANCHOR_TYPE, GTK_ARG_READWRITE, ARG_HEADING_ANCHOR);
	gtk_object_add_arg_type ("GnomeMonthItem::day_anchor", GTK_TYPE_ANCHOR_TYPE, GTK_ARG_READWRITE, ARG_DAY_ANCHOR);
	gtk_object_add_arg_type ("GnomeMonthItem::start_on_monday", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_START_ON_MONDAY);
	gtk_object_add_arg_type ("GnomeMonthItem::heading_font", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_HEAD_FONT);
	gtk_object_add_arg_type ("GnomeMonthItem::heading_fontset", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_HEAD_FONTSET);
	gtk_object_add_arg_type ("GnomeMonthItem::heading_font_gdk", GTK_TYPE_GDK_FONT, GTK_ARG_READWRITE, ARG_HEAD_FONT_GDK);
	gtk_object_add_arg_type ("GnomeMonthItem::day_font", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_DAY_FONT);
	gtk_object_add_arg_type ("GnomeMonthItem::day_fontset", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_DAY_FONTSET);
	gtk_object_add_arg_type ("GnomeMonthItem::day_font_gdk", GTK_TYPE_GDK_FONT, GTK_ARG_READWRITE, ARG_DAY_FONT_GDK);
	gtk_object_add_arg_type ("GnomeMonthItem::heading_color", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_HEAD_COLOR);
	gtk_object_add_arg_type ("GnomeMonthItem::heading_color_gdk", GTK_TYPE_GDK_COLOR, GTK_ARG_READWRITE, ARG_HEAD_COLOR_GDK);
	gtk_object_add_arg_type ("GnomeMonthItem::outline_color", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_OUTLINE_COLOR);
	gtk_object_add_arg_type ("GnomeMonthItem::outline_color_gdk", GTK_TYPE_GDK_COLOR, GTK_ARG_READWRITE, ARG_OUTLINE_COLOR_GDK);
	gtk_object_add_arg_type ("GnomeMonthItem::day_box_color", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_DAY_BOX_COLOR);
	gtk_object_add_arg_type ("GnomeMonthItem::day_box_color_gdk", GTK_TYPE_GDK_COLOR, GTK_ARG_READWRITE, ARG_DAY_BOX_COLOR_GDK);
	gtk_object_add_arg_type ("GnomeMonthItem::day_color", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_DAY_COLOR);
	gtk_object_add_arg_type ("GnomeMonthItem::day_color_gdk", GTK_TYPE_GDK_COLOR, GTK_ARG_READWRITE, ARG_DAY_COLOR_GDK);

	object_class->destroy = gnome_month_item_destroy;
	object_class->set_arg = gnome_month_item_set_arg;
	object_class->get_arg = gnome_month_item_get_arg;
}

/* Calculates the minimum heading height based on the heading font size and padding.  It also
 * calculates the minimum width of the month item based on the width of the headings.
 */
static void
check_heading_sizes (GnomeMonthItem *mitem)
{
	double m_height;
	double m_width;
	int width;
	int max_width;
	int i;

	/* Calculate minimum height */

	m_height = mitem->head_font->ascent + mitem->head_font->descent + 2 * mitem->head_padding;

	if (mitem->head_height < m_height)
		mitem->head_height = m_height;
	
	/* Go through each heading and remember the widest one */

	max_width = 0;

	for (i = 0; i < 7; i++) {
		width = gdk_string_width (mitem->head_font, mitem->day_names[i]);
		if (max_width < width)
			max_width = width;
	}

	m_width = 7 * (max_width + 2 * mitem->head_padding);

	if (mitem->width < m_width)
		mitem->width = m_width;
}

/* Calculates the minimum width and height of the month item based on the day font size and padding.
 * Assumes that the minimum heading height has already been computed.
 */
static void
check_day_sizes (GnomeMonthItem *mitem)
{
	double m_height;
	double m_width;
	int width;
	int max_width;
	char buf[100];
	int i;

	/* Calculate minimum height */

	m_height = mitem->head_height + 6 * (mitem->day_font->ascent + mitem->day_font->descent + 2 * mitem->day_padding);

	if (mitem->height < m_height)
		mitem->height = m_height;

	/* Calculate minimum width */

	max_width = 0;

	for (i = 1; i < 32; i++) {
		sprintf (buf, "%d", i);
		width = gdk_string_width (mitem->day_font, buf);
		if (max_width < width)
			max_width = width;
	}

	m_width = 7 * (max_width + 2 * mitem->day_padding);

	if (mitem->width < m_width)
		mitem->width = m_width;
}

/* Calculates the minimum size of the month item based on the font sizes and paddings.  If the
 * current size of the month item is smaller than the required minimum size, this function will
 * change the size to the appropriate values.
 */
static void
check_sizes (GnomeMonthItem *mitem)
{
	check_heading_sizes (mitem);
	check_day_sizes (mitem);
}

/* Recalculates the position of the toplevel calendar group based on the logical position and anchor */
static void
reanchor (GnomeMonthItem *mitem)
{
	double x, y;

	x = mitem->x;
	y = mitem->y;

	switch (mitem->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		x -= mitem->width / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		x -= mitem->width;
		break;
	}

	switch (mitem->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		y -= mitem->height / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		y -= mitem->height;
		break;
	}

	/* Explicitly use the canvas group class prefix since the month item class has x and y
	 * arguments as well.
	 */

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mitem),
			       "GnomeCanvasGroup::x", x,
			       "GnomeCanvasGroup::y", y,
			       NULL);
}

/* Takes an anchor specification and the corners of a rectangle, and returns an anchored point with
 * respect to that rectangle.
 */
static void
get_label_anchor (GtkAnchorType anchor, double x1, double y1, double x2, double y2, double *x, double *y)
{
	switch (anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		*x = x1;
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		*x = (x1 + x2) / 2.0;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		*x = x2;
		break;
	}

	switch (anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		*y = y1;
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		*y = (y1 + y2) / 2.0;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		*y = y2;
		break;
	}
}

/* Resets the position of the day name headings in the calendar */
static void
reshape_headings (GnomeMonthItem *mitem)
{
	double width;
	int i;
	double x, y;

	width = mitem->width / 7;

	for (i = 0; i < 7; i++) {
		/* Group */
		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_HEAD_GROUP + i],
				       "x", width * i,
				       "y", 0.0,
				       NULL);

		/* Box */
		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_HEAD_BOX + i],
				       "x1", 0.0,
				       "y1", 0.0,
				       "x2", width,
				       "y2", mitem->head_height,
				       NULL);

		/* Label */
		get_label_anchor (mitem->head_anchor,
				  mitem->head_padding,
				  mitem->head_padding,
				  width - mitem->head_padding,
				  mitem->head_height - mitem->head_padding,
				  &x, &y);

		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_HEAD_LABEL + i],
				       "x", x,
				       "y", y,
				       "anchor", mitem->head_anchor,
				       NULL);
	}
}

/* Resets the position of the days in the calendar */
static void
reshape_days (GnomeMonthItem *mitem)
{
	double width, height;
	double x, y;
	int row, col;
	int i;

	width = mitem->width / 7;
	height = (mitem->height - mitem->head_height) / 6;

	i = 0;

	for (row = 0; row < 6; row++)
		for (col = 0; col < 7; col++) {
			/* Group */
			gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_DAY_GROUP + i],
					       "x", width * col,
					       "y", mitem->head_height + height * row,
					       NULL);

			/* Box */
			gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_DAY_BOX + i],
					       "x1", 0.0,
					       "y1", 0.0,
					       "x2", width,
					       "y2", height,
					       NULL);

			/* Label */
			get_label_anchor (mitem->day_anchor,
					  mitem->day_padding,
					  mitem->day_padding,
					  width - mitem->day_padding,
					  height - mitem->day_padding,
					  &x, &y);

			gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_DAY_LABEL + i],
					       "x", x,
					       "y", y,
					       "anchor", mitem->day_anchor,
					       NULL);

			i++;
		}
}

/* Changes the positions and resizes the items in the calendar to match the new size of the
 * calendar.
 */
static void
reshape (GnomeMonthItem *mitem)
{
	check_sizes (mitem);
	reanchor (mitem);
	reshape_headings (mitem);
	reshape_days (mitem);
}

/* Sets the font for all the day headings */
static void
set_head_font (GnomeMonthItem *mitem)
{
	int i;

	for (i = 0; i < 7; i++)
		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_HEAD_LABEL + i],
				       "font_gdk", mitem->head_font,
				       NULL);
}

/* Sets the color for all the headings */
static void
set_head_color (GnomeMonthItem *mitem)
{
	int i;
	GdkColor outline;
	GdkColor head;

	outline.pixel = mitem->outline_pixel;
	head.pixel = mitem->head_pixel;

	for (i = 0; i < 7; i++) {
		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_HEAD_BOX + i],
				       "fill_color_gdk", &outline,
				       NULL);

		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_HEAD_LABEL + i],
				       "fill_color_gdk", &head,
				       NULL);
	}
}

/* Creates the items for the day name headings */
static void
create_headings (GnomeMonthItem *mitem)
{
	int i;

	/* Just create the items; they will be positioned and configured by a call to reshape() */

	for (i = 0; i < 7; i++) {
		/* Group */
		mitem->items[GNOME_MONTH_ITEM_HEAD_GROUP + i] =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (mitem),
					       gnome_canvas_group_get_type (),
					       NULL);

		/* Box */
		mitem->items[GNOME_MONTH_ITEM_HEAD_BOX + i] =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (mitem->items[GNOME_MONTH_ITEM_HEAD_GROUP + i]),
					       gnome_canvas_rect_get_type (),
					       NULL);

		/* Label */
		mitem->items[GNOME_MONTH_ITEM_HEAD_LABEL + i] =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (mitem->items[GNOME_MONTH_ITEM_HEAD_GROUP + i]),
					       gnome_canvas_text_get_type (),
					       NULL);
	}

	set_head_font (mitem);
	set_head_color (mitem);
}

/* Returns the number of leap years since year 1 up to (but not including) the specified year */
static int
leap_years_up_to (int year)
{
	return (year / 4					/* trivial leapness */
		- ((year > 1700) ? (year / 100 - 17) : 0)	/* minus centuries since 1700 */
		+ ((year > 1600) ? ((year - 1600) / 400) : 0));	/* plus centuries since 1700 divisible by 400 */
}

/* Returns whether the specified year is a leap year */
static int
is_leap_year (int year)
{
	if (year <= 1752)
		return !(year % 4);
	else
		return (!(year % 4) && (year % 100)) || !(year % 400);
}

/* Returns the 1-based day number within the year of the specified date */
static int
day_in_year (int day, int month, int year)
{
	int is_leap, i;

	is_leap = is_leap_year (year);

	for (i = 0; i < month; i++)
		day += days_in_month [is_leap][i];

	return day;
}

/* Returns the day of the week (zero-based, zero is Sunday) for the specified date.  For the days
 * that were removed on the Gregorian reformation, it returns Thursday.
 */
static int
day_in_week (int day, int month, int year)
{
	int n;

	n = (year - 1) * 365 + leap_years_up_to (year - 1) + day_in_year (day, month, year);

	if (n < REFORMATION_DAY)
		return (n - 1 + SATURDAY) % 7;

	if (n >= (REFORMATION_DAY + MISSING_DAYS))
		return (n - 1 + SATURDAY - MISSING_DAYS) % 7;

	return THURSDAY;
}

/* Fills the 42-element days array with the day numbers for the specified month.  Slots outside the
 * bounds of the month are filled with zeros.  The starting and ending indexes of the days are
 * returned in the start and end arguments.
 */
static void
build_month (int month, int year, int start_on_monday, int *days, int *start, int *end)
{
	int i;
	int d_month, d_week;

	/* Note that months are zero-based, so September is month 8 */

	if ((year == 1752) && (month == 8)) {
		memcpy (days, sept_1752, 42 * sizeof (int));

		if (start)
			*start = SEPT_1752_START;

		if (end)
			*end = SEPT_1752_END;

		return;
	}

	for (i = 0; i < 42; i++)
		days[i] = 0;

	d_month = days_in_month[is_leap_year (year)][month];
	d_week = day_in_week (1, month, year);

	if (start_on_monday)
		d_week = (d_week + 6) % 7;

	for (i = 0; i < d_month; i++)
		days[d_week + i] = i + 1;

	if (start)
		*start = d_week;

	if (end)
		*end = d_week + d_month - 1;
}

/* Set the day numbers in the monthly calendar */
static void
set_days (GnomeMonthItem *mitem)
{
	int i;
	int start, end;
	char buf[100];

	build_month (mitem->month, mitem->year, mitem->start_on_monday, mitem->day_numbers, &start, &end);

	/* Clear days before start of month */

	for (i = 0; i < start; i++)
		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_DAY_LABEL + i],
				       "text", NULL,
				       NULL);

	/* Set days of month */

	for (; start <= end; start++, i++) {
		sprintf (buf, "%d", mitem->day_numbers[start]);
		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_DAY_LABEL + i],
				       "text", buf,
				       NULL);
	}

	/* Clear days after end of month */

	for (; i < 42; i++)
		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_DAY_LABEL + i],
				       "text", NULL,
				       NULL);
}

/* Sets the font for all the day numbers */
static void
set_day_font (GnomeMonthItem *mitem)
{
	int i;

	for (i = 0; i < 42; i++)
		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_DAY_LABEL + i],
				       "font_gdk", mitem->day_font,
				       NULL);
}

/* Sets the color for all the day items */
static void
set_day_color (GnomeMonthItem *mitem)
{
	int i;
	GdkColor outline;
	GdkColor day_box;
	GdkColor day;

	outline.pixel = mitem->outline_pixel;
	day_box.pixel = mitem->day_box_pixel;
	day.pixel = mitem->day_pixel;

	for (i = 0; i < 42; i++) {
		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_DAY_BOX + i],
				       "outline_color_gdk", &outline,
				       "fill_color_gdk", &day_box,
				       NULL);

		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_DAY_LABEL + i],
				       "fill_color_gdk", &day,
				       NULL);
	}
}

/* Creates the items for the days */
static void
create_days (GnomeMonthItem *mitem)
{
	int i;

	/* Just create the items; they will be positioned and configured by a call to reshape() */

	for (i = 0; i < 42; i++) {
		/* Group */
		mitem->items[GNOME_MONTH_ITEM_DAY_GROUP + i] =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (mitem),
					       gnome_canvas_group_get_type (),
					       NULL);

		/* Box */
		mitem->items[GNOME_MONTH_ITEM_DAY_BOX + i] =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (mitem->items[GNOME_MONTH_ITEM_DAY_GROUP + i]),
					       gnome_canvas_rect_get_type (),
					       NULL);

		/* Label */
		mitem->items[GNOME_MONTH_ITEM_DAY_LABEL + i] =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (mitem->items[GNOME_MONTH_ITEM_DAY_GROUP + i]),
					       gnome_canvas_text_get_type (),
					       NULL);
	}

	set_day_font (mitem);
	set_day_color (mitem);
	set_days (mitem);
}

/* Resets the text of the day name headings */
static void
set_day_names (GnomeMonthItem *mitem)
{
	int i;

	for (i = 0; i < 7; i++)
		gnome_canvas_item_set (mitem->items[GNOME_MONTH_ITEM_HEAD_LABEL + i],
				       "text", mitem->day_names[mitem->start_on_monday ? ((i + 1) % 7) : i],
				       NULL);
}

/* Creates all the canvas items that make up the calendar */
static void
create_items (GnomeMonthItem *mitem)
{
	mitem->items = g_new (GnomeCanvasItem *, GNOME_MONTH_ITEM_LAST);

	create_headings (mitem);
	create_days (mitem);

	/* Initialize by default to three-letter day names */

	mitem->day_names[0] = g_strdup (_("Sun"));
	mitem->day_names[1] = g_strdup (_("Mon"));
	mitem->day_names[2] = g_strdup (_("Tue"));
	mitem->day_names[3] = g_strdup (_("Wed"));
	mitem->day_names[4] = g_strdup (_("Thu"));
	mitem->day_names[5] = g_strdup (_("Fri"));
	mitem->day_names[6] = g_strdup (_("Sat"));
	
	set_day_names (mitem);
	reshape (mitem);
}

static void
gnome_month_item_init (GnomeMonthItem *mitem)
{
	time_t t;
	struct tm tm;

	/* Initialize to the current month by default */

	t = time (NULL);
	tm = *localtime (&t);

	mitem->year = tm.tm_year + 1900;
	mitem->month = tm.tm_mon;

	mitem->x = 0.0;
	mitem->y = 0.0;
	mitem->width = 150.0; /* not unreasonable defaults, I hope */
	mitem->height = 100.0;
	mitem->anchor = GTK_ANCHOR_NW;
	mitem->head_padding = 0.0;
	mitem->day_padding = 2.0;
	mitem->head_height = 14.0;
	mitem->head_anchor = GTK_ANCHOR_CENTER;
	mitem->day_anchor = GTK_ANCHOR_CENTER;

	/* Load the default fonts */

	mitem->head_font = gdk_font_load (DEFAULT_FONT);
	if (!mitem->head_font) {
		mitem->head_font = gdk_font_load ("fixed");
		g_assert (mitem->head_font != NULL);
	}

	mitem->day_font = gdk_font_load (DEFAULT_FONT);
	if (!mitem->day_font) {
		mitem->day_font = gdk_font_load ("fixed");
		g_assert (mitem->day_font != NULL);
	}
}

GnomeCanvasItem *
gnome_month_item_new (GnomeCanvasGroup *parent)
{
	GnomeMonthItem *mitem;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_GROUP (parent), NULL);

	mitem = GNOME_MONTH_ITEM (gnome_canvas_item_new (parent,
							 gnome_month_item_get_type (),
							 NULL));

	gnome_month_item_construct (mitem);

	return GNOME_CANVAS_ITEM (mitem);
}

void
gnome_month_item_construct (GnomeMonthItem *mitem)
{
	GdkColor color;

	g_return_if_fail (mitem != NULL);
	g_return_if_fail (GNOME_IS_MONTH_ITEM (mitem));

	gnome_canvas_get_color (GNOME_CANVAS_ITEM (mitem)->canvas, "#d6d6d6d6d6d6", &color);
	mitem->head_pixel = color.pixel;
	mitem->day_box_pixel = color.pixel;

	gnome_canvas_get_color (GNOME_CANVAS_ITEM (mitem)->canvas, "black", &color);
	mitem->outline_pixel = color.pixel;
	mitem->day_pixel = color.pixel;

	create_items (mitem);
}

static void
free_day_names (GnomeMonthItem *mitem)
{
	int i;

	if (mitem->day_names[0])
		for (i = 0; i < 7; i++)
			g_free (mitem->day_names[i]);
}

static void
gnome_month_item_destroy (GtkObject *object)
{
	GnomeMonthItem *mitem;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_MONTH_ITEM (object));

	mitem = GNOME_MONTH_ITEM (object);

	free_day_names (mitem);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Sets the color of the specified pixel value to that of the specified argument, which must be in
 * GdkColor format if format is TRUE, otherwise it must be in string format.
 */
static void
set_color_arg (GnomeMonthItem *mitem, gulong *pixel, GtkArg *arg, int gdk_format, int set_head, int set_day)
{
	GdkColor color;

	if (gdk_format)
		*pixel = ((GdkColor *) GTK_VALUE_BOXED (*arg))->pixel;
	else {
		if (gnome_canvas_get_color (GNOME_CANVAS_ITEM (mitem)->canvas, GTK_VALUE_STRING (*arg), &color))
			*pixel = color.pixel;
		else
			*pixel = 0;
	}

	if (set_head)
		set_head_color (mitem);

	if (set_day)
		set_day_color (mitem);
}

static void
gnome_month_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeMonthItem *mitem;
	char **day_names;
	int i;

	mitem = GNOME_MONTH_ITEM (object);

	switch (arg_id) {
	case ARG_YEAR:
		mitem->year = GTK_VALUE_UINT (*arg);
		set_days (mitem);
		break;

	case ARG_MONTH:
		mitem->month = GTK_VALUE_UINT (*arg);
		set_days (mitem);
		break;

	case ARG_X:
		mitem->x = GTK_VALUE_DOUBLE (*arg);
		reanchor (mitem);
		break;

	case ARG_Y:
		mitem->y = GTK_VALUE_DOUBLE (*arg);
		reanchor (mitem);
		break;

	case ARG_WIDTH:
		mitem->width = fabs (GTK_VALUE_DOUBLE (*arg));
		reshape (mitem);
		break;

	case ARG_HEIGHT:
		mitem->height = fabs (GTK_VALUE_DOUBLE (*arg));
		reshape (mitem);
		break;

	case ARG_ANCHOR:
		mitem->anchor = GTK_VALUE_ENUM (*arg);
		reanchor (mitem);
		break;

	case ARG_HEAD_PADDING:
		mitem->head_padding = fabs (GTK_VALUE_DOUBLE (*arg));
		reshape (mitem);
		break;

	case ARG_DAY_PADDING:
		mitem->day_padding = fabs (GTK_VALUE_DOUBLE (*arg));
		reshape (mitem);
		break;

	case ARG_DAY_NAMES:
		day_names = GTK_VALUE_POINTER (*arg);

		/* First, check that none of the names is null */

		for (i = 0; i < 7; i++)
			if (!day_names[i]) {
				g_warning ("Day number %d was NULL; day names cannot be NULL!", i);
				return;
			}

		/* Set the new names */

		free_day_names (mitem);
		for (i = 0; i < 7; i++)
			mitem->day_names[i] = g_strdup (day_names[i]);

		set_day_names (mitem);
		reshape (mitem);
		break;

	case ARG_HEADING_HEIGHT:
		mitem->head_height = fabs (GTK_VALUE_DOUBLE (*arg));
		reshape (mitem);
		break;

	case ARG_HEADING_ANCHOR:
		mitem->head_anchor = GTK_VALUE_ENUM (*arg);
		reshape (mitem);
		break;

	case ARG_DAY_ANCHOR:
		mitem->day_anchor = GTK_VALUE_ENUM (*arg);
		reshape (mitem);
		break;

	case ARG_START_ON_MONDAY:
		mitem->start_on_monday = GTK_VALUE_BOOL (*arg);
		set_day_names (mitem);
		set_days (mitem);
		break;

	case ARG_HEAD_FONT:
		gdk_font_unref (mitem->head_font);

		mitem->head_font = gdk_font_load (GTK_VALUE_STRING (*arg));
		if (!mitem->head_font) {
			mitem->head_font = gdk_font_load ("fixed");
			g_assert (mitem->head_font != NULL);
		}

		set_head_font (mitem);
		reshape (mitem);
		break;

	case ARG_HEAD_FONTSET:
		gdk_font_unref (mitem->head_font);

		mitem->head_font = gdk_fontset_load (GTK_VALUE_STRING (*arg));
		if (!mitem->head_font) {
			mitem->head_font =
			  gdk_fontset_load ("-*-fixed-medium-r-semicondensed--13-120-75-75-c-60-*-*");
			g_assert (mitem->head_font != NULL);
		}

		set_head_font (mitem);
		reshape (mitem);
		break;

	case ARG_HEAD_FONT_GDK:
		gdk_font_unref (mitem->head_font);

		mitem->head_font = GTK_VALUE_BOXED (*arg);
		gdk_font_ref (mitem->head_font);
		set_head_font (mitem);
		reshape (mitem);
		break;

	case ARG_DAY_FONT:
		gdk_font_unref (mitem->day_font);

		mitem->day_font = gdk_font_load (GTK_VALUE_STRING (*arg));
		if (!mitem->day_font) {
			mitem->day_font = gdk_font_load ("fixed");
			g_assert (mitem->day_font != NULL);
		}

		set_day_font (mitem);
		reshape (mitem);
		break;

	case ARG_DAY_FONTSET:
		gdk_font_unref (mitem->day_font);

		mitem->day_font = gdk_fontset_load (GTK_VALUE_STRING (*arg));
		if (!mitem->day_font) {
			mitem->day_font =
			  gdk_fontset_load ("-*-fixed-medium-r-semicondensed--13-120-75-75-c-60-*-*");
			g_assert (mitem->day_font != NULL);
		}

		set_day_font (mitem);
		reshape (mitem);
		break;

	case ARG_DAY_FONT_GDK:
		gdk_font_unref (mitem->day_font);

		mitem->day_font = GTK_VALUE_BOXED (*arg);
		gdk_font_ref (mitem->day_font);
		set_day_font (mitem);
		reshape (mitem);
		break;

	case ARG_HEAD_COLOR:
		set_color_arg (mitem, &mitem->head_pixel, arg, FALSE, TRUE, FALSE);
		break;

	case ARG_HEAD_COLOR_GDK:
		set_color_arg (mitem, &mitem->head_pixel, arg, TRUE, TRUE, FALSE);
		break;

	case ARG_OUTLINE_COLOR:
		set_color_arg (mitem, &mitem->outline_pixel, arg, FALSE, TRUE, TRUE);
		break;

	case ARG_OUTLINE_COLOR_GDK:
		set_color_arg (mitem, &mitem->outline_pixel, arg, TRUE, TRUE, TRUE);
		break;

	case ARG_DAY_BOX_COLOR:
		set_color_arg (mitem, &mitem->day_box_pixel, arg, FALSE, FALSE, TRUE);
		break;

	case ARG_DAY_BOX_COLOR_GDK:
		set_color_arg (mitem, &mitem->day_box_pixel, arg, TRUE, FALSE, TRUE);
		break;

	case ARG_DAY_COLOR:
		set_color_arg (mitem, &mitem->day_pixel, arg, FALSE, FALSE, TRUE);
		break;

	case ARG_DAY_COLOR_GDK:
		set_color_arg (mitem, &mitem->day_pixel, arg, TRUE, FALSE, TRUE);
		break;

	default:
		break;
	}
}

/* Allocates a GdkColor structure filled with the specified pixel, and puts it into the specified
 * arg for returning it in the get_arg method.
 */
static void
get_color_arg (GnomeMonthItem *mitem, gulong pixel, GtkArg *arg)
{
	GdkColor *color;

	color = g_new (GdkColor, 1);
	color->pixel = pixel;
	gdk_color_context_query_color (GNOME_CANVAS_ITEM (mitem)->canvas->cc, color);
	GTK_VALUE_BOXED (*arg) = color;
}

static void
gnome_month_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeMonthItem *mitem;

	mitem = GNOME_MONTH_ITEM (object);

	switch (arg_id) {
	case ARG_YEAR:
		GTK_VALUE_UINT (*arg) = mitem->year;
		break;

	case ARG_MONTH:
		GTK_VALUE_UINT (*arg) = mitem->month;
		break;

	case ARG_X:
		GTK_VALUE_DOUBLE (*arg) = mitem->x;
		break;

	case ARG_Y:
		GTK_VALUE_DOUBLE (*arg) = mitem->y;
		break;

	case ARG_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = mitem->width;
		break;

	case ARG_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = mitem->height;
		break;

	case ARG_ANCHOR:
		GTK_VALUE_ENUM (*arg) = mitem->anchor;
		break;

	case ARG_HEAD_PADDING:
		GTK_VALUE_DOUBLE (*arg) = mitem->head_padding;
		break;

	case ARG_DAY_PADDING:
		GTK_VALUE_DOUBLE (*arg) = mitem->day_padding;
		break;

	case ARG_HEADING_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = mitem->head_height;
		break;

	case ARG_HEADING_ANCHOR:
		GTK_VALUE_ENUM (*arg) = mitem->head_anchor;
		break;

	case ARG_DAY_ANCHOR:
		GTK_VALUE_ENUM (*arg) = mitem->day_anchor;
		break;

	case ARG_START_ON_MONDAY:
		GTK_VALUE_BOOL (*arg) = mitem->start_on_monday;
		break;

	case ARG_HEAD_FONT_GDK:
		GTK_VALUE_BOXED (*arg) = mitem->head_font;
		break;

	case ARG_DAY_FONT_GDK:
		GTK_VALUE_BOXED (*arg) = mitem->day_font;
		break;

	case ARG_HEAD_COLOR_GDK:
		get_color_arg (mitem, mitem->head_pixel, arg);
		break;

	case ARG_OUTLINE_COLOR_GDK:
		get_color_arg (mitem, mitem->outline_pixel, arg);
		break;

	case ARG_DAY_BOX_COLOR_GDK:
		get_color_arg (mitem, mitem->day_box_pixel, arg);
		break;

	case ARG_DAY_COLOR_GDK:
		get_color_arg (mitem, mitem->day_pixel, arg);
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

GnomeCanvasItem *
gnome_month_item_num2child (GnomeMonthItem *mitem, int child_num)
{
	g_return_val_if_fail (mitem != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_MONTH_ITEM (mitem), NULL);

	return mitem->items[child_num];
}

int
gnome_month_item_child2num (GnomeMonthItem *mitem, GnomeCanvasItem *child)
{
	int i;

	g_return_val_if_fail (mitem != NULL, -1);
	g_return_val_if_fail (GNOME_IS_MONTH_ITEM (mitem), -1);
	g_return_val_if_fail (child != NULL, -1);
	g_return_val_if_fail (GNOME_IS_CANVAS_ITEM (child), -1);

	for (i = 0; i < GNOME_MONTH_ITEM_LAST; i++)
		if (mitem->items[i] == child)
			return i;

	return -1;
}

int
gnome_month_item_num2day (GnomeMonthItem *mitem, int child_num)
{
	g_return_val_if_fail (mitem != NULL, 0);
	g_return_val_if_fail (GNOME_IS_MONTH_ITEM (mitem), 0);

	if ((child_num >= GNOME_MONTH_ITEM_DAY_GROUP) && (child_num < GNOME_MONTH_ITEM_LAST)) {
		child_num = (child_num - GNOME_MONTH_ITEM_DAY_GROUP) % 42;
		return mitem->day_numbers[child_num];
	} else
		return 0;
}

int
gnome_month_item_day2index (GnomeMonthItem *mitem, int day_num)
{
	int i;

	g_return_val_if_fail (mitem != NULL, -1);
	g_return_val_if_fail (GNOME_IS_MONTH_ITEM (mitem), -1);
	g_return_val_if_fail (day_num >= 1, -1);

	/* Find first day of month */

	for (i = 0; mitem->day_numbers[i] == 0; i++)
		;

	/* Find the specified day */

	for (; (mitem->day_numbers[i] != 0) && (i < 42); i++)
		if (mitem->day_numbers[i] == day_num)
			return i;

	/* Bail out */

	return -1;
}
