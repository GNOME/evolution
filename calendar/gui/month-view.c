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


/* This is a child in the month view.  Each child has a number of canvas items associated to it --
 * it can be more than one box because an event may span several weeks.
 */
struct child {
	iCalObject *ico;	/* The calendar object this child refers to */
	time_t start, end;	/* Start and end times for the instance of the event */
	int slot_start;		/* The first slot this child uses */
	int slots_used;		/* The number of slots occupied by this child */
	GList *segments;	/* The list of segments needed to display this child */
};

/* Each child is composed of one or more segments.  Each segment can be considered to be
 * the entire child clipped to a particular week, as events may span several weeks in the
 * month view.
 */
struct segment {
	time_t start, end;	/* Start/end times for this segment */
	GnomeCanvasItem *item;	/* Canvas item used to display this segment */
};


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

/* Adjusts a single segment from a child.  Takes the dimensions of the day the segment is on and
 * resizes the segment's canvas item to the apporpriate size and position.
 */
static void
adjust_segment (MonthView *mv, struct child *child, struct segment *seg)
{
	GnomeMonthItem *mitem;
	struct tm tm;
	int day_index;
	GnomeCanvasItem *item;
	double day_width, day_height;
	double x1, y1, x2, y2;
	double y;
	double slot_width;
	time_t day_begin_time, day_end_time;
	double start_factor, end_factor;

	mitem = GNOME_MONTH_ITEM (mv->mitem);

	/* Find out the dimensions of the day item for the segment */

	tm = *localtime (&seg->start);
	day_index = gnome_month_item_day2index (mitem, tm.tm_mday);
	g_assert (day_index != -1);

	item = gnome_month_item_num2child (mitem, day_index + GNOME_MONTH_ITEM_DAY_GROUP);
	gnome_canvas_item_get_bounds (item, &x1, &y1, &x2, &y2);

	/* Get the bottom coordinate of the day label */

	item = gnome_month_item_num2child (mitem, day_index + GNOME_MONTH_ITEM_DAY_LABEL);
	gnome_canvas_item_get_bounds (item, NULL, NULL, NULL, &y);

	/* Calculate usable area */

	y1 += y;
	day_width = x2 - x1;
	day_height = y2 - y1;

	slot_width = day_width / mv->num_slots;

	/* Set the coordinates of the segment's item */

	day_begin_time = time_day_begin (seg->start);
	day_end_time = time_day_end (seg->end);

	start_factor = (double) (seg->start - day_begin_time) / (day_end_time - day_begin_time);
	end_factor = (double) (seg->end - day_begin_time) / (day_end_time - day_begin_time);

	gnome_canvas_item_set (seg->item,
			       "x1", x1 + slot_width * child->slot_start,
			       "y1", y1 + day_height * start_factor,
			       "x2", x1 + slot_width * (child->slot_start + child->slots_used),
			       "y2", y1 + day_height * end_factor,
			       NULL);
}

/* Adjusts the child events of the month view to the appropriate size and position */
static void
adjust_children (MonthView *mv)
{
	GList *children;
	struct child *child;
	GList *segments;
	struct segment *seg;

	for (children = mv->children; children; children = children->next) {
		child = children->data;

		for (segments = child->segments; segments; segments = segments->next) {
			seg = segments->data;

			adjust_segment (mv, child, seg);
		}
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

/* Destroys a child structure and its associated canvas items */
static void
child_destroy (MonthView *mv, struct child *child)
{
	GList *list;
	struct segment *seg;

	/* Destroy the segments */

	for (list = child->segments; list; list = list->next) {
		seg = list->data;

		gtk_object_destroy (GTK_OBJECT (seg->item));
		g_free (seg);
	}

	g_list_free (child->segments);

	/* Destroy the child */

	g_free (child);
}

/* Creates the list of segments that are used to display a child.  Each child may have several
 * segments, because it may span several days in the month view.  This function only creates the
 * segment structures and the associated canvas items, but it does not set their final position in
 * the month view canvas -- that is done by the adjust_children() function.
 */
static void
child_create_segments (MonthView *mv, struct child *child)
{
	time_t t;
	time_t month_begin, month_end;
	time_t day_begin_time, day_end_time;
	struct segment *seg;

	/* Get the month's extents */

	t = time_from_day (mv->year, mv->month, 1);
	month_begin = time_month_begin (t);
	month_end = time_month_end (t);

	/* Get the first day of the event */

	t = MAX (child->start, month_begin);
	day_begin_time = time_day_begin (t);
	day_end_time = time_day_end (day_begin_time);

	/* Loop until the event ends or the month ends -- the segments list is created in reverse
	 * order.
	 */

	do {
		seg = g_new (struct segment, 1);

		/* Clip the child to this day */

		seg->start = MAX (child->start, day_begin_time);
		seg->end = MIN (child->end, day_end_time);

		seg->item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (mv->mitem),
						   gnome_canvas_rect_get_type (),
						   "fill_color", color_spec_from_prop (COLOR_PROP_MARK_DAY_BG),
						   "outline_color", "black",
						   "width_pixels", 0,
						   NULL);

		child->segments = g_list_prepend (child->segments, seg);

		/* Next day */

		day_begin_time = time_add_day (day_begin_time, 1);
		day_end_time = time_day_end (day_begin_time);
	} while ((child->end > day_begin_time) && (day_begin_time < month_end));

	/* Reverse the list to put it in increasing order */

	child->segments = g_list_reverse (child->segments);
}

/* Comparison function used to create the sorted list of children.  Sorts first by increasing start
 * time and then by decreasing end time, so that "longer" events are first in the list.
 */
static gint
child_compare (gconstpointer a, gconstpointer b)
{
	const struct child *ca, *cb;
	time_t diff;

	ca = a;
	cb = b;

	diff = ca->start - cb->start;

	if (diff == 0)
		diff = cb->end - ca->end;

	return (diff < 0) ? -1 : ((diff > 0) ? 1 : 0);
}

/* This is the callback function used from the calendar iterator.  It adds events to the list of
 * children in the month view.
 */
static int
add_event (iCalObject *ico, time_t start, time_t end, void *data)
{
	MonthView *mv;
	struct child *child;

	mv = MONTH_VIEW (data);

	child = g_new (struct child, 1);
	child->ico = ico;
	child->start = start;
	child->end = end;
	child->segments = NULL;

	child_create_segments (mv, child);

	/* Add it to the list of children */

	mv->children = g_list_insert_sorted (mv->children, child, child_compare);

	return TRUE; /* means "we are not yet finished" */
}

/* Time query function for the layout engine */
static void
child_query_func (GList *list, time_t *start, time_t *end)
{
	struct child *child;

	child = list->data;

	*start = child->start;
	*end = child->end;
}

/* Uses the generic event layout engine to set the children's layout information */
static void
layout_children (MonthView *mv)
{
	GList *list;
	struct child *child;
	int *allocations;
	int *slots;
	int i;

	layout_events (mv->children, child_query_func, &mv->num_slots, &allocations, &slots);

	if (mv->num_slots == 0)
		return;

	for (list = mv->children, i = 0; list; list = list->next, i++) {
		child = list->data;
		child->slot_start = allocations[i];
		child->slots_used = slots[i];
	}

	g_free (allocations);
	g_free (slots);
}

void
month_view_update (MonthView *mv, iCalObject *object, int flags)
{
	GList *list;
	time_t t;
	time_t month_begin, month_end;

	g_return_if_fail (mv != NULL);
	g_return_if_fail (IS_MONTH_VIEW (mv));

	/* Destroy the old list of children */

	for (list = mv->children; list; list = list->next)
		child_destroy (mv, list->data);

	g_list_free (mv->children);
	mv->children = NULL;

	/* Create a new list of children and lay them out */

	t = time_from_day (mv->year, mv->month, 1);
	month_begin = time_month_begin (t);
	month_end = time_month_end (t);

	calendar_iterate (mv->calendar->cal, month_begin, month_end, add_event, mv);
	layout_children (mv);
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
