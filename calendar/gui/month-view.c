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


/* Spacing between title and calendar */
#define SPACING 4

/* Padding between day borders and event text */
#define EVENT_PADDING 3

/* Font for the month view events */
#define EVENT_FONT "-adobe-helvetica-medium-r-normal--10-*-*-*-p-*-*-*"


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

/* Returns the index of the specified arrow in the array of arrows */
static int
get_arrow_index (MonthView *mv, GnomeCanvasItem *arrow)
{
	int i;

	for (i = 0; i < 42; i++)
		if (mv->up[i] == arrow)
			return i;
		else if (mv->down[i] == arrow)
			return i + 42;

	g_warning ("Eeeek, arrow %p not found in month view %p", arrow, mv);
	return -1;
}

/* Checks whether arrows need to be displayed at the specified day index or not */
static void
check_arrow_visibility (MonthView *mv, int day_index)
{
	GtkArg args[3];
	double text_height;
	double clip_height;
	double y_offset;

	args[0].name = "text_height";
	args[1].name = "clip_height";
	args[2].name = "y_offset";
	gtk_object_getv (GTK_OBJECT (mv->text[day_index]), 3, args);

	text_height = GTK_VALUE_DOUBLE (args[0]);
	clip_height = GTK_VALUE_DOUBLE (args[1]);
	y_offset = GTK_VALUE_DOUBLE (args[2]);

	/* Check up arrow */

	if (y_offset < 0.0)
		gnome_canvas_item_show (mv->up[day_index]);
	else
		gnome_canvas_item_hide (mv->up[day_index]);

	if (y_offset > (clip_height - text_height))
		gnome_canvas_item_show (mv->down[day_index]);
	else
		gnome_canvas_item_hide (mv->down[day_index]);
}

/* Finds which arrow was clicked and scrolls the corresponding text item in the month view */
static void
do_arrow_click (MonthView *mv, GnomeCanvasItem *arrow)
{
	int arrow_index;
	int day_index;
	int up;
	GtkArg args[4];
	double text_height, clip_height;
	double y_offset;
	GdkFont *font;

	arrow_index = get_arrow_index (mv, arrow);
	up = (arrow_index < 42);
	day_index = up ? arrow_index : (arrow_index - 42);

	/* See how much we can scroll */

	args[0].name = "text_height";
	args[1].name = "clip_height";
	args[2].name = "y_offset";
	args[3].name = "font_gdk";
	gtk_object_getv (GTK_OBJECT (mv->text[day_index]), 4, args);

	text_height = GTK_VALUE_DOUBLE (args[0]);
	clip_height = GTK_VALUE_DOUBLE (args[1]);
	y_offset = GTK_VALUE_DOUBLE (args[2]);
	font = GTK_VALUE_BOXED (args[3]);

	if (up)
		y_offset += font->ascent + font->descent;
	else
		y_offset -= font->ascent + font->descent;

	if (y_offset > 0.0)
		y_offset = 0.0;
	else if (y_offset < (clip_height - text_height))
		y_offset = clip_height - text_height;

	/* Scroll */

	gnome_canvas_item_set (mv->text[day_index],
			       "y_offset", y_offset,
			       NULL);

	check_arrow_visibility (mv, day_index);
}

/* Event handler for the scroll arrows in the month view */
static gint
arrow_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	MonthView *mv;

	mv = MONTH_VIEW (data);

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		gnome_canvas_item_set (item,
				       "fill_color", color_spec_from_prop (COLOR_PROP_PRELIGHT_DAY_BG),
				       NULL);
		return TRUE;

	case GDK_LEAVE_NOTIFY:
		gnome_canvas_item_set (item,
				       "fill_color", color_spec_from_prop (COLOR_PROP_DAY_FG),
				       NULL);
		return TRUE;

	case GDK_BUTTON_PRESS:
		do_arrow_click (mv, item);
		return TRUE;

	default:
		break;
	}

	return FALSE;
}

/* Creates a new arrow out of the specified points and connects the proper signals to it */
static GnomeCanvasItem *
new_arrow (MonthView *mv, GnomeCanvasGroup *group, GnomeCanvasPoints *points)
{
	GnomeCanvasItem *item;
	char *color_spec;

	color_spec = color_spec_from_prop (COLOR_PROP_DAY_FG);

	item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (group),
				      gnome_canvas_polygon_get_type (),
				      "points", points,
				      "fill_color", color_spec,
				      "outline_color", color_spec,
				      NULL);

	gtk_signal_connect (GTK_OBJECT (item), "event",
			    (GtkSignalFunc) arrow_event,
			    mv);

	return item;
}

static void
month_view_init (MonthView *mv)
{
	int i;
	GnomeCanvasItem *day_group;
	GnomeCanvasPoints *points;

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

	/* Arrows and text items.  The arrows start hidden by default; they will be shown as
	 * appropriate by the item adjustment code.
	 */

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

		mv->up[i] = new_arrow (mv, GNOME_CANVAS_GROUP (day_group), points);

		/* Down arrow */

		points->coords[0] = 13;
		points->coords[1] = 3;
		points->coords[2] = 17;
		points->coords[3] = 10;
		points->coords[4] = 21;
		points->coords[5] = 3;

		mv->down[i] = new_arrow (mv, GNOME_CANVAS_GROUP (day_group), points);

		/* Text item */

		mv->text[i] = gnome_canvas_item_new (GNOME_CANVAS_GROUP (day_group),
						     gnome_canvas_text_get_type (),
						     "font", EVENT_FONT,
						     "anchor", GTK_ANCHOR_NW,
						     "fill_color", color_spec_from_prop (COLOR_PROP_DAY_FG),
						     "clip", TRUE,
						     NULL);
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

/* Adjusts the text items for events in the month view to the appropriate size.  It also makes the
 * corresponding arrows visible or invisible, as appropriate.
 */
static void
adjust_children (MonthView *mv)
{
	int i;
	GnomeCanvasItem *item;
	double x1, y1, x2, y2;
	GtkArg arg;

	for (i = 0; i < 42; i++) {
		/* Get dimensions of the day group */

		item = gnome_month_item_num2child (GNOME_MONTH_ITEM (mv->mitem), i + GNOME_MONTH_ITEM_DAY_GROUP);
		gnome_canvas_item_get_bounds (item, &x1, &y1, &x2, &y2);

		/* Normalize and add paddings */

		x2 -= x1 + EVENT_PADDING;
		x1 = EVENT_PADDING;
		y2 -= y1 + EVENT_PADDING;
		y1 = EVENT_PADDING;

		/* Add height of day label to y1 */

		item = gnome_month_item_num2child (GNOME_MONTH_ITEM (mv->mitem), i + GNOME_MONTH_ITEM_DAY_LABEL);

		arg.name = "text_height";
		gtk_object_getv (GTK_OBJECT (item), 1, &arg);
		y1 += GTK_VALUE_DOUBLE (arg);

		/* Set the position and clip size */

		gnome_canvas_item_set (mv->text[i],
				       "x", x1,
				       "y", y1,
				       "clip_width", x2 - x1,
				       "clip_height", y2 - y1,
				       "x_offset", 0.0,
				       "y_offset", 0.0,
				       NULL);

		/* See what visibility state the arrows should be set to */

		check_arrow_visibility (mv, i);
	}
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

	/* Adjust children */

	adjust_children (mv);
}

/* This defines the environment for the calendar iterator function that is used to populate the
 * month view with events.
 */
struct iter_info {
	MonthView *mv;			/* The month view we are creating children for */
	int first_day_index;		/* Index of the first day of the month within the month item */
	time_t month_begin, month_end;	/* Beginning and end of month */
	GString **strings;		/* Array of strings to populate */
};

/* This is the calendar iterator function used to populate the string array with event information.
 * For each event, it iterates through all the days that the event touches and appends the proper
 * information to the string array in the iter_info structure.
 */
static int
add_event (iCalObject *ico, time_t start, time_t end, void *data)
{
	struct iter_info *ii;
	struct tm *tm;
	time_t t;
	time_t day_begin_time, day_end_time;

	ii = data;

	/* Get the first day of the event */

	t = MAX (start, ii->month_begin);
	day_begin_time = time_day_begin (t);
	day_end_time = time_day_end (day_begin_time);

	/* Loop until the event ends or the month ends.  For each day touched, append the proper
	 * information to the corresponding string.
	 */

	do {
		tm = localtime (&day_begin_time);
		g_string_sprintfa (ii->strings[ii->first_day_index + tm->tm_mday - 1], "%s\n", ico->summary);

		/* Next day */

		day_begin_time = time_add_day (day_begin_time, 1);
		day_end_time = time_day_end (day_begin_time);
	} while ((end > day_begin_time) && (day_begin_time < ii->month_end));

	return TRUE; /* this means we are not finished yet with event generation */
}

void
month_view_update (MonthView *mv, iCalObject *object, int flags)
{
	struct iter_info ii;
	GString *strings[42];
	int i;
	time_t t;

	g_return_if_fail (mv != NULL);
	g_return_if_fail (IS_MONTH_VIEW (mv));

	ii.mv = mv;

	/* Create an array of empty GStrings */

	ii.strings = strings;

	for (i = 0; i < 42; i++)
		strings[i] = g_string_new (NULL);

	ii.first_day_index = gnome_month_item_day2index (GNOME_MONTH_ITEM (mv->mitem), 1);
	g_assert (ii.first_day_index != -1);

	/* Populate the array of strings with events */

	t = time_from_day (mv->year, mv->month, 1);
	ii.month_begin = time_month_begin (t);
	ii.month_end = time_month_end (t);

	calendar_iterate (mv->calendar->cal, ii.month_begin, ii.month_end, add_event, &ii);

	for (i = 0; i < 42; i++) {
		/* Delete the last character if it is a newline */

		if (strings[i]->str && strings[i]->len && (strings[i]->str[strings[i]->len - 1] == '\n'))
			g_string_truncate (strings[i], strings[i]->len - 1);
		
		gnome_canvas_item_set (mv->text[i],
				       "text", strings[i]->str,
				       NULL);
		g_string_free (strings[i], TRUE);
	}

	/* Adjust children for scrolling */

	adjust_children (mv);
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
