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


/* These are indices into the items array of a GnomeMonthItem structure */
enum {
	ITEM_HEAD_GROUP = 0,
	ITEM_HEAD_BOX   = 7,
	ITEM_HEAD_LABEL = 14
};

enum {
	ARG_0,
	ARG_YEAR,
	ARG_MONTH,
	ARG_X,
	ARG_Y,
	ARG_WIDTH,
	ARG_HEIGHT,
	ARG_ANCHOR,
	ARG_PADDING,
	ARG_DAY_NAMES,
	ARG_HEADING_HEIGHT,
	ARG_HEADING_ANCHOR,
	ARG_START_ON_MONDAY
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
	gtk_object_add_arg_type ("GnomeMonthItem::padding", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_PADDING);
	gtk_object_add_arg_type ("GnomeMonthItem::day_names", GTK_TYPE_POINTER, GTK_ARG_READABLE, ARG_DAY_NAMES);
	gtk_object_add_arg_type ("GnomeMonthItem::heading_height", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_HEADING_HEIGHT);
	gtk_object_add_arg_type ("GnomeMonthItem::heading_anchor", GTK_TYPE_ANCHOR_TYPE, GTK_ARG_READWRITE, ARG_HEADING_ANCHOR);
	gtk_object_add_arg_type ("GnomeMonthItem::start_on_monday", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_START_ON_MONDAY);

	object_class->destroy = gnome_month_item_destroy;
	object_class->set_arg = gnome_month_item_set_arg;
	object_class->get_arg = gnome_month_item_get_arg;
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
		gnome_canvas_item_set (mitem->items[ITEM_HEAD_GROUP + i],
				       "x", width * i,
				       "y", 0.0,
				       NULL);

		/* Box */
		gnome_canvas_item_set (mitem->items[ITEM_HEAD_BOX + i],
				       "x1", 0.0,
				       "y1", 0.0,
				       "x2", width,
				       "y2", mitem->head_height,
				       NULL);

		/* Label */
		get_label_anchor (mitem->head_anchor,
				  mitem->padding,
				  mitem->padding,
				  width - mitem->padding,
				  mitem->head_height - mitem->padding,
				  &x, &y);

		gnome_canvas_item_set (mitem->items[ITEM_HEAD_LABEL + i],
				       "x", x,
				       "y", y,
				       "anchor", mitem->head_anchor,
				       NULL);
	}
}

/* Changes the positions and resizes the items in the calendar to match the new size of the
 * calendar.
 */
static void
reshape (GnomeMonthItem *mitem)
{
	reshape_headings (mitem);
}

/* Creates the items for the day name headings */
static void
create_headings (GnomeMonthItem *mitem)
{
	int i;

	/* Just create the items; they will be positioned and configured by a call to reshape() */

	for (i = 0; i < 7; i++) {
		/* Group */
		mitem->items[ITEM_HEAD_GROUP + i] =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (mitem),
					       gnome_canvas_group_get_type (),
					       NULL);

		/* Box */
		mitem->items[ITEM_HEAD_BOX + i] =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (mitem->items[ITEM_HEAD_GROUP + i]),
					       gnome_canvas_rect_get_type (),
					       "fill_color", "black",
					       NULL);

		/* Label */
		mitem->items[ITEM_HEAD_LABEL + i] =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (mitem->items[ITEM_HEAD_GROUP + i]),
					       gnome_canvas_text_get_type (),
					       "fill_color", "white",
					       "font", "-adobe-helvetica-medium-r-normal--12-*-72-72-p-*-iso8859-1",
					       NULL);
	}
}

/* Returns a normalized day index (as in sunday to saturday) based on a visible day index */
static int
get_day_index (GnomeMonthItem *mitem, int draw_index)
{
	if (mitem->start_on_monday)
		return (draw_index + 1) % 7;
	else
		return draw_index;
}

/* Resets the text of the day name headings */
static void
set_day_names (GnomeMonthItem *mitem)
{
	int i;

	for (i = 0; i < 7; i++)
		gnome_canvas_item_set (mitem->items[ITEM_HEAD_LABEL + i],
				       "text", mitem->day_names[get_day_index (mitem, i)],
				       NULL);
}

/* Creates all the canvas items that make up the calendar */
static void
create_items (GnomeMonthItem *mitem)
{
	/*  7 heading groups
	 *  7 heading boxes
	 *  7 heading labels
	 * ------------------
	 * 21 items total
	 */

	mitem->items = g_new (GnomeCanvasItem *, 21);

	create_headings (mitem);
	/* FIXME */

	/* Initialize by default to three-letter day names */

	mitem->day_names[0] = _("Sun");
	mitem->day_names[1] = _("Mon");
	mitem->day_names[2] = _("Tue");
	mitem->day_names[3] = _("Wed");
	mitem->day_names[4] = _("Thu");
	mitem->day_names[5] = _("Fri");
	mitem->day_names[6] = _("Sat");
	
	set_day_names (mitem);
	reshape (mitem);
}

static void
gnome_month_item_init (GnomeMonthItem *mitem)
{
	time_t t;
	struct tm *tm;

	/* Initialize to the current month by default */

	t = time (NULL);
	tm = localtime (&t);

	mitem->year = tm->tm_year + 1900;
	mitem->month = tm->tm_mon;

	mitem->x = 0.0;
	mitem->y = 0.0;
	mitem->width = 150.0; /* not unreasonable defaults, I hope */
	mitem->height = 100.0;
	mitem->anchor = GTK_ANCHOR_NW;
	mitem->padding = 0.0;
	mitem->head_height = 14.0;
	mitem->head_anchor = GTK_ANCHOR_CENTER;
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
	g_return_if_fail (mitem != NULL);
	g_return_if_fail (GNOME_IS_MONTH_ITEM (mitem));

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

static void
recalc_month (GnomeMonthItem *mitem)
{
	/* FIXME */
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
		recalc_month (mitem);
		break;

	case ARG_MONTH:
		mitem->month = GTK_VALUE_UINT (*arg);
		recalc_month (mitem);
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
		reanchor (mitem);
		reshape (mitem);
		break;

	case ARG_HEIGHT:
		mitem->height = fabs (GTK_VALUE_DOUBLE (*arg));
		reanchor (mitem);
		reshape (mitem);
		break;

	case ARG_ANCHOR:
		mitem->anchor = GTK_VALUE_ENUM (*arg);
		reanchor (mitem);
		break;

	case ARG_PADDING:
		mitem->padding = fabs (GTK_VALUE_DOUBLE (*arg));
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
		break;

	case ARG_HEADING_HEIGHT:
		mitem->head_height = fabs (GTK_VALUE_DOUBLE (*arg));
		reshape (mitem);
		break;

	case ARG_HEADING_ANCHOR:
		mitem->head_anchor = GTK_VALUE_ENUM (*arg);
		reshape (mitem);
		break;

	case ARG_START_ON_MONDAY:
		mitem->start_on_monday = GTK_VALUE_BOOL (*arg);
		set_day_names (mitem);
		break;

	default:
		break;
	}
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

	case ARG_PADDING:
		GTK_VALUE_DOUBLE (*arg) = mitem->padding;
		break;

	case ARG_HEADING_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = mitem->head_height;
		break;

	case ARG_HEADING_ANCHOR:
		GTK_VALUE_ENUM (*arg) = mitem->head_anchor;
		break;

	case ARG_START_ON_MONDAY:
		GTK_VALUE_BOOL (*arg) = mitem->start_on_monday;
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
