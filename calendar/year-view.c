/* Year view display for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Arturo Espinosa <arturo@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <gtk/gtkmain.h>
#include <gnome.h>
#include "eventedit.h"
#include "year-view.h"
#include "main.h"
#include "mark.h"
#include "quick-view.h"
#include "timeutil.h"


#define HEAD_SPACING 4		/* Spacing between year heading and months */
#define TITLE_SPACING 1		/* Spacing between title and calendar */
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

/* Resizes the year view's child items.  This is done in the idle loop for
 * performance (we avoid resizing on every size allocation).
 */
static gint
idle_handler (gpointer data)
{
	YearView *yv;
	GtkArg arg;
	double head_height;
	double title_height;
	double width, height;
	double month_width;
	double month_height;
	double month_yofs;
	double xofs, yofs;
	double x, y;
	int i;

	yv = data;

	/* Compute the size we can use */

	width = MAX (GTK_WIDGET (yv)->allocation.width, yv->min_width);
	height = MAX (GTK_WIDGET (yv)->allocation.height, yv->min_height);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (yv), 0, 0, width, height);

	width--;
	height--;

	/* Get the heights of the heading and the titles */

	arg.name = "text_height";
	gtk_object_getv (GTK_OBJECT (yv->heading), 1, &arg);
	head_height = GTK_VALUE_DOUBLE (arg) + 2 * HEAD_SPACING;

	arg.name = "text_height";
	gtk_object_getv (GTK_OBJECT (yv->titles[0]), 1, &arg);
	title_height = GTK_VALUE_DOUBLE (arg);

	/* Offsets */

	xofs = (width + SPACING) / 3.0;
	yofs = (height - head_height + SPACING) / 4.0;

	/* Month item vertical offset */

	month_yofs = title_height + TITLE_SPACING;

	/* Month item dimensions */

	month_width = (width - 2 * SPACING) / 3.0;
	month_height = (yofs - SPACING) - month_yofs;

	/* Adjust the year heading */

	gnome_canvas_item_set (yv->heading,
			       "x", width / 2.0,
			       "y", (double) HEAD_SPACING,
			       NULL);

	/* Adjust titles and months */

	for (i = 0; i < 12; i++) {
		x = (i % 3) * xofs;
		y = head_height + (i / 3) * yofs;

		gnome_canvas_item_set (yv->titles[i],
				       "x", x + month_width / 2.0,
				       "y", y,
				       NULL);

		gnome_canvas_item_set (yv->mitems[i],
				       "x", x,
				       "y", y + month_yofs,
				       "width", month_width,
				       "height", month_height,
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

/* Callback used to destroy the year view's popup menu when the year view itself is destroyed */
static void
destroy_menu (GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy (GTK_WIDGET (data));
}

/* Create a new appointment in the highlighted day from the year view's popup menu */
static void
new_appointment (GtkWidget *widget, gpointer data)
{
	YearView *yv;
	time_t *t;

	yv = YEAR_VIEW (data);
	t = gtk_object_get_data (GTK_OBJECT (widget), "time_data");

	event_editor_new_whole_day (yv->calendar, *t);
}

/* Convenience functions to jump to a view and set the time */
static void
do_jump (GtkWidget *widget, gpointer data, char *view_name)
{
	YearView *yv;
	time_t *t;

	yv = YEAR_VIEW (data);

	/* Get the time data from the menu item */

	t = gtk_object_get_data (GTK_OBJECT (widget), "time_data");

	/* Set the view and time */

	gnome_calendar_set_view (yv->calendar, view_name);
	gnome_calendar_goto (yv->calendar, *t);
}

/* The following three callbacks set the view in the calendar and change the time */

static void
jump_to_day (GtkWidget *widget, gpointer data)
{
	do_jump (widget, data, "dayview");
}

static void
jump_to_week (GtkWidget *widget, gpointer data)
{
	do_jump (widget, data, "weekview");
}

static void
jump_to_month (GtkWidget *widget, gpointer data)
{
	do_jump (widget, data, "monthview");
}

/* Information for the year view's popup menu */
static GnomeUIInfo yv_popup_menu[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("_New appointment in this day..."), NULL, new_appointment, GNOME_STOCK_MENU_NEW),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Jump to this _day"), NULL, jump_to_day, GNOME_STOCK_MENU_JUMP_TO),
	GNOMEUIINFO_ITEM_STOCK (N_("Jump to this _week"), NULL, jump_to_week, GNOME_STOCK_MENU_JUMP_TO),
	GNOMEUIINFO_ITEM_STOCK (N_("Jump to this _month"), NULL, jump_to_month, GNOME_STOCK_MENU_JUMP_TO),
	GNOMEUIINFO_END
};

/* Returns the popup menu cooresponding to the specified year view.  If the menu has not been
 * created yet, it creates it and attaches it to the year view.
 */
static GtkWidget *
get_popup_menu (YearView *yv)
{
	GtkWidget *menu;

	menu = gtk_object_get_data (GTK_OBJECT (yv), "popup_menu");

	if (!menu) {
		menu = gnome_popup_menu_new (yv_popup_menu);
		gtk_object_set_data (GTK_OBJECT (yv), "popup_menu", menu);
		gtk_signal_connect (GTK_OBJECT (yv), "destroy",
				    (GtkSignalFunc) destroy_menu,
				    menu);
	}

	return menu;
}

/* Executes the year view's popup menu.  It may disable/enable some menu items based on the
 * specified flags.  A pointer to a time_t value containing the specified time data is set in the
 * "time_data" object data key of the menu items.
 */
static void
do_popup_menu (YearView *yv, GdkEventButton *event, int allow_new, int allow_day, int allow_week, int allow_month,
	       int year, int month, int day)
{
	GtkWidget *menu;
	static time_t t;

	menu = get_popup_menu (yv);

	/* Enable/disable items as appropriate */

	gtk_widget_set_sensitive (yv_popup_menu[0].widget, allow_new);
	gtk_widget_set_sensitive (yv_popup_menu[2].widget, allow_day);
	gtk_widget_set_sensitive (yv_popup_menu[3].widget, allow_week);
	gtk_widget_set_sensitive (yv_popup_menu[4].widget, allow_month);

	/* Set the day item relevant to the context */

	t = time_from_day (year, month, day);

	gtk_object_set_data (GTK_OBJECT (yv_popup_menu[0].widget), "time_data", &t);
	gtk_object_set_data (GTK_OBJECT (yv_popup_menu[2].widget), "time_data", &t);
	gtk_object_set_data (GTK_OBJECT (yv_popup_menu[3].widget), "time_data", &t);
	gtk_object_set_data (GTK_OBJECT (yv_popup_menu[4].widget), "time_data", &t);

	gnome_popup_menu_do_popup (menu, NULL, NULL, event, yv);
}

/* Creates the quick view when the user clicks on a day */
static void
do_quick_view_popup (YearView *yv, GdkEventButton *event, int year, int month, int day)
{
	time_t day_start, day_end;
	GList *list;
	GtkWidget *qv;
	char date_str[256];

	day_start = time_from_day (year, month, day);
	day_end = time_day_end (day_start);

	list = calendar_get_events_in_range (yv->calendar->cal, day_start, day_end);

	strftime (date_str, sizeof (date_str), _("%a %b %d %Y"), localtime (&day_start));
	qv = quick_view_new (yv->calendar, date_str, list);

	quick_view_do_popup (QUICK_VIEW (qv), event);

	gtk_widget_destroy (qv);
	calendar_destroy_event_list (list);
}

/* Event handler for days in the year's month items */
static gint
day_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	YearView *yv;
	GnomeMonthItem *mitem;
	int child_num, day;

	mitem = GNOME_MONTH_ITEM (data);
	child_num = gnome_month_item_child2num (mitem, item);
	day = gnome_month_item_num2day (mitem, child_num);

	yv = YEAR_VIEW (item->canvas);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (day == 0)
			break;

		if (event->button.button == 1) {
			do_quick_view_popup (yv, (GdkEventButton *) event, mitem->year, mitem->month, day);
			return TRUE;
		} else if (event->button.button == 3) {
			do_popup_menu (yv, (GdkEventButton *) event, TRUE, TRUE, TRUE, TRUE,
				       mitem->year, mitem->month, day);

			/* We have to stop the signal emission because mark.c will grab it too and
			 * set the return value to FALSE.  Blargh.
			 */
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item), "event");
			return TRUE;
		}

		break;

	default:
		break;
	}

	return FALSE;
}

/* Event handler for whole month items */
static gint
month_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	YearView *yv;
	GnomeMonthItem *mitem;

	mitem = GNOME_MONTH_ITEM (item);

	yv = YEAR_VIEW (item->canvas);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button.button != 3)
			break;

		do_popup_menu (yv, (GdkEventButton *) event, FALSE, FALSE, FALSE, TRUE,
			       mitem->year, mitem->month, 1);

		/* We have to stop the signal emission because mark.c will grab it too and
		 * set the return value to FALSE.  Blargh.
		 */
		gtk_signal_emit_stop_by_name (GTK_OBJECT (item), "event");
		return TRUE;

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

	/* Connect the day signals */

	for (i = 0; i < 42; i++) {
		item = gnome_month_item_num2child (GNOME_MONTH_ITEM (mitem), GNOME_MONTH_ITEM_DAY_GROUP + i);
		gtk_signal_connect (GTK_OBJECT (item), "event",
				    (GtkSignalFunc) day_event,
				    mitem);
	}

	/* Connect the month signals */

	gtk_signal_connect (GTK_OBJECT (mitem), "event",
			    (GtkSignalFunc) month_event,
			    NULL);

	/* Prepare for prelighting */

	month_item_prepare_prelight (GNOME_MONTH_ITEM (mitem), default_color_func, NULL);
}

/* Computes the minimum size for the year view and stores it in its internal fields */
static void
compute_min_size (YearView *yv)
{
	GtkArg args[2];
	double m_width;
	double m_height;
	double max_width;
	double w;
	int i;

	/* Compute the minimum size of the year heading */

	args[0].name = "text_width";
	args[1].name = "text_height";
	gtk_object_getv (GTK_OBJECT (yv->heading), 2, args);

	m_width = GTK_VALUE_DOUBLE (args[0]);
	m_height = 2 * HEAD_SPACING + GTK_VALUE_DOUBLE (args[1]);

	/* Add height of month titles and their spacings */

	args[0].name = "text_height";
	gtk_object_getv (GTK_OBJECT (yv->titles[0]), 1, &args[0]);

	m_height += 4 * (GTK_VALUE_DOUBLE (args[0]) + TITLE_SPACING);

	/* Add width of month titles */

	max_width = 0.0;

	for (i = 0; i < 12; i++) {
		args[0].name = "text_width";
		gtk_object_getv (GTK_OBJECT (yv->titles[i]), 1, &args[0]);

		w = GTK_VALUE_DOUBLE (args[0]);
		if (max_width < w)
			max_width = w;
	}

	max_width = 3 * max_width + 2 * SPACING;

	if (m_width < max_width)
		m_width = max_width;

	/* Add width of month items */

	args[0].name = "width";
	args[1].name = "height";
	gtk_object_getv (GTK_OBJECT (yv->mitems[0]), 2, args);

	max_width = 3 * GTK_VALUE_DOUBLE (args[0]) + 2 * SPACING;

	if (m_width < max_width)
		m_width = max_width;

	/* Add height of month items */

	m_height += 4 * GTK_VALUE_DOUBLE (args[1]) + 3 * SPACING;

	/* Finally, set the minimum width and height in the year view */

	yv->min_width = (int) (m_width + 0.5);
	yv->min_height = (int) (m_height + 0.5);
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
					     "fontset", HEADING_FONTSET,
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
						       "fontset", TITLE_FONTSET,
						       "fill_color", "black",
						       NULL);

		/* Month item */

		yv->mitems[i] = gnome_month_item_new (gnome_canvas_root (GNOME_CANVAS (yv)));
		gnome_canvas_item_set (yv->mitems[i],
				       "anchor", GTK_ANCHOR_NW,
				       "start_on_monday", week_starts_on_monday,
				       "heading_fontset", DAY_HEADING_FONTSET,
				       "day_fontset", NORMAL_DAY_FONTSET,
				       NULL);
		setup_month_item (yv, i);
	}

	/* We will need to resize the items when we paint for the first time */

	yv->old_marked_day = -1;
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

	year_view_colors_changed (yv);
	year_view_set (yv, year);
	compute_min_size (yv);

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

	requisition->width = yv->min_width;
	requisition->height = yv->min_height;
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

/* Unmarks the old day that was marked as current and marks the current day if appropriate */
static void
mark_current_day (YearView *yv)
{
	time_t t;
	struct tm tm;
	int month_index, day_index;
	GnomeCanvasItem *item;

	/* Unmark the old day */

	if (yv->old_marked_day != -1) {
		month_index = yv->old_marked_day / 42;
		day_index = yv->old_marked_day % 42;

		item = gnome_month_item_num2child (GNOME_MONTH_ITEM (yv->mitems[month_index]),
						   GNOME_MONTH_ITEM_DAY_LABEL + day_index);
		gnome_canvas_item_set (item,
				       "fill_color", color_spec_from_prop (COLOR_PROP_DAY_FG),
				       "fontset", NORMAL_DAY_FONTSET,
				       NULL);

		yv->old_marked_day = -1;
	}

	/* Mark the new day */

	t = time (NULL);
	tm = *localtime (&t);

	if ((tm.tm_year + 1900) == yv->year) {
		month_index = tm.tm_mon;
		day_index = gnome_month_item_day2index (GNOME_MONTH_ITEM (yv->mitems[month_index]), tm.tm_mday);
		g_assert (day_index != -1);

		item = gnome_month_item_num2child (GNOME_MONTH_ITEM (yv->mitems[month_index]),
						   GNOME_MONTH_ITEM_DAY_LABEL + day_index);
		gnome_canvas_item_set (item,
				       "fill_color", color_spec_from_prop (COLOR_PROP_CURRENT_DAY_FG),
				       "fontset", CURRENT_DAY_FONTSET,
				       NULL);

		yv->old_marked_day = month_index * 42 + day_index;
	}
}

void
year_view_set (YearView *yv, time_t year)
{
	struct tm tm;
	char buf[100];
	int i;

	g_return_if_fail (yv != NULL);
	g_return_if_fail (IS_YEAR_VIEW (yv));

	tm = *localtime (&year);
	yv->year = tm.tm_year + 1900;

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

	for (i = 0; i < 12; i++) {
		unmark_month_item (GNOME_MONTH_ITEM (yv->mitems[i]));
		mark_month_item (GNOME_MONTH_ITEM (yv->mitems[i]), yv->calendar->cal);
	}

	mark_current_day (yv);
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
		colorify_month_item (GNOME_MONTH_ITEM (yv->mitems[i]), default_color_func, NULL);
		mark_month_item (GNOME_MONTH_ITEM (yv->mitems[i]), yv->calendar->cal);
	}

	mark_current_day (yv);
}
