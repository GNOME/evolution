/* Full day widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtktext.h>
#include "gncal-full-day.h"
#include "view-utils.h"


#define TEXT_BORDER 2
#define HANDLE_SIZE 3
#define MIN_WIDTH 200
#define XOR_RECT_WIDTH 2
#define UNSELECT_TIMEOUT 150 /* ms */


typedef struct {
	iCalObject *ico;
	GtkWidget  *widget;
	GdkWindow  *window;
	int         lower_row; /* zero is first displayed row */
	int         rows_used;
	int         x;         /* coords of child's window */
	int         y;
	int         width;
	int         height;
} Child;

struct layout_row {
	int  intersections;
	int *slots;
};

struct drag_info {
	enum {
		DRAG_NONE,
		DRAG_SELECT,	/* selecting a range in the main window */
		DRAG_MOVE,	/* moving a child */
		DRAG_SIZE	/* resizing a child */
	} drag_mode;

	Child *child;
	int start_row;
	int rows_used;

	int sel_click_row;
	int sel_start_row;
	int sel_rows_used;
	guint32 click_time;
};


enum {
	RANGE_ACTIVATED,
	LAST_SIGNAL
};


static void gncal_full_day_class_init     (GncalFullDayClass *class);
static void gncal_full_day_init           (GncalFullDay      *fullday);
static void gncal_full_day_destroy        (GtkObject         *object);
static void gncal_full_day_map            (GtkWidget         *widget);
static void gncal_full_day_unmap          (GtkWidget         *widget);
static void gncal_full_day_realize        (GtkWidget         *widget);
static void gncal_full_day_unrealize      (GtkWidget         *widget);
static void gncal_full_day_draw           (GtkWidget         *widget,
					   GdkRectangle      *area);
static void gncal_full_day_draw_focus     (GtkWidget         *widget);
static void gncal_full_day_size_request   (GtkWidget         *widget,
					   GtkRequisition    *requisition);
static void gncal_full_day_size_allocate  (GtkWidget         *widget,
					   GtkAllocation     *allocation);
static gint gncal_full_day_button_press   (GtkWidget         *widget,
					   GdkEventButton    *event);
static gint gncal_full_day_button_release (GtkWidget         *widget,
					   GdkEventButton    *event);
static gint gncal_full_day_motion         (GtkWidget         *widget,
					   GdkEventMotion    *event);
static gint gncal_full_day_expose         (GtkWidget         *widget,
					   GdkEventExpose    *event);
static gint gncal_full_day_key_press      (GtkWidget         *widget,
					   GdkEventKey       *event);
static gint gncal_full_day_focus_in       (GtkWidget         *widget,
					   GdkEventFocus     *event);
static gint gncal_full_day_focus_out      (GtkWidget         *widget,
					   GdkEventFocus     *event);
static void gncal_full_day_foreach        (GtkContainer      *container,
					   GtkCallback        callback,
					   gpointer           callback_data);

static void range_activated (GncalFullDay *fullday);


static GtkContainerClass *parent_class;

static fullday_signals[LAST_SIGNAL] = { 0 };


static void
get_tm_range (GncalFullDay *fullday,
	      time_t time_lower, time_t time_upper,
	      struct tm *lower, struct tm *upper,
	      int *lower_row, int *rows_used)
{
	struct tm tm_lower, tm_upper;
	int lmin, umin;
	int lrow;

	/* Lower */

	tm_lower = *localtime (&time_lower);

	if ((tm_lower.tm_min % fullday->interval) != 0) {
		tm_lower.tm_min -= tm_lower.tm_min % fullday->interval; /* round down */
		mktime (&tm_lower);
	}

	/* Upper */

	tm_upper = *localtime (&time_upper);

	if ((tm_upper.tm_min % fullday->interval) != 0) {
		tm_upper.tm_min += fullday->interval - (tm_upper.tm_min % fullday->interval); /* round up */
		mktime (&tm_upper);
	}

	if (lower)
		*lower = tm_lower;

	if (upper)
		*upper = tm_upper;

	lmin = 60 * tm_lower.tm_hour + tm_lower.tm_min;
	umin = 60 * tm_upper.tm_hour + tm_upper.tm_min;

	if (umin == 0) /* midnight of next day? */
		umin = 60 * 24;

	lrow = lmin / fullday->interval;

	if (lower_row)
		*lower_row = lrow;

	if (rows_used)
		*rows_used = (umin - lmin) / fullday->interval;
}

static void
child_map (GncalFullDay *fullday, Child *child)
{
	gdk_window_show (child->window);
	gtk_widget_show (child->widget); /* OK, not just a map... */
}

static void
child_unmap (GncalFullDay *fullday, Child *child)
{
	gdk_window_hide (child->window);

	if (GTK_WIDGET_MAPPED (child->widget))
		gtk_widget_unmap (child->widget);
}

static void
child_set_text_pos (Child *child)
{
	GtkAllocation allocation;

	allocation.x = 0;
	allocation.y = HANDLE_SIZE;
	allocation.width = child->width;
	allocation.height = child->height - 2 * HANDLE_SIZE;

	gtk_widget_size_request (child->widget, &child->widget->requisition); /* FIXME: is this needed? */
	gtk_widget_size_allocate (child->widget, &allocation);
}

static void
child_realize (GncalFullDay *fullday, Child *child)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	GtkWidget *widget;

	widget = GTK_WIDGET (fullday);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = child->x;
	attributes.y = child->y;
	attributes.width = child->width;
	attributes.height = child->height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.cursor = fullday->up_down_cursor;
	attributes.event_mask = (GDK_EXPOSURE_MASK
				 | GDK_BUTTON_PRESS_MASK
				 | GDK_BUTTON_RELEASE_MASK
				 | GDK_BUTTON_MOTION_MASK
				 | GDK_POINTER_MOTION_HINT_MASK
				 | GDK_KEY_PRESS_MASK);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP | GDK_WA_CURSOR;

	child->window = gdk_window_new (widget->window, &attributes, attributes_mask);
	gdk_window_set_user_data (child->window, widget);

	gtk_style_set_background (widget->style, child->window, GTK_STATE_NORMAL);

	gtk_widget_set_parent_window (child->widget, child->window);

	child_set_text_pos (child);
}

static void
child_unrealize (GncalFullDay *fullday, Child *child)
{
	gdk_window_set_user_data (child->window, NULL);
	gdk_window_destroy (child->window);
	child->window = NULL;
}

static void
child_draw (GncalFullDay *fullday, Child *child, GdkRectangle *area, int draw_child)
{
	GdkRectangle arect, rect, dest;

	if (!area) {
		arect.x = 0;
		arect.y = 0;

		arect.width = child->width;
		arect.height = child->height;

		area = &arect;
	}

	/* Top handle */

	rect.x = 0;
	rect.y = 0;
	rect.width = child->width;
	rect.height = HANDLE_SIZE;

	if (gdk_rectangle_intersect (&rect, area, &dest))
		view_utils_draw_textured_frame (GTK_WIDGET (fullday), child->window, &rect, GTK_SHADOW_OUT);

	/* Bottom handle */

	rect.y = child->height - HANDLE_SIZE;

	if (gdk_rectangle_intersect (&rect, area, &dest))
		view_utils_draw_textured_frame (GTK_WIDGET (fullday), child->window, &rect, GTK_SHADOW_OUT);

	if (draw_child) {
		area->y -= HANDLE_SIZE;
		gtk_widget_draw (child->widget, area);
	}
}

static void
child_range_changed (GncalFullDay *fullday, Child *child)
{
	struct tm start, end;
	int lower_row, rows_used;
	int f_lower_row;

	/* Calc display range for event */

	get_tm_range (fullday, child->ico->dtstart, child->ico->dtend, &start, &end, &lower_row, &rows_used);
	get_tm_range (fullday, fullday->lower, fullday->upper, NULL, NULL, &f_lower_row, NULL);

	child->lower_row = lower_row - f_lower_row;
	child->rows_used = rows_used;
}

static void
child_realized_setup (GtkWidget *widget, gpointer data)
{
	Child *child;
	GncalFullDay *fullday;

	child = data;
	fullday = GNCAL_FULL_DAY (widget->parent);

	gdk_window_set_cursor (widget->window, fullday->beam_cursor);

	gtk_text_insert (GTK_TEXT (widget), NULL, NULL, NULL,
			 child->ico->summary,
			 strlen (child->ico->summary));
}

static gint
child_focus_out (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	Child *child;

	child = data;

	if (child->ico->summary)
		g_free (child->ico->summary);

	child->ico->summary = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);

	/* FIXME: need notify calendar of change? */

	return FALSE;
}

static Child *
child_new (GncalFullDay *fullday, iCalObject *ico)
{
	Child *child;

	child = g_new (Child, 1);

	child->ico = ico;
	child->widget = gtk_text_new (NULL, NULL);
	child->window = NULL;
	child->x = 0;
	child->y = 0;
	child->width = 0;
	child->height = 0;

	child_range_changed (fullday, child);

	/* We set the i-beam cursor and the initial summary text upon realization */

	gtk_signal_connect (GTK_OBJECT (child->widget), "realize",
			    (GtkSignalFunc) child_realized_setup,
			    child);

	/* Update the iCalObject summary when the text widget loses focus */

	gtk_signal_connect (GTK_OBJECT (child->widget), "focus_out_event",
			    (GtkSignalFunc) child_focus_out,
			    child);

	/* Finish setup */

	gtk_text_set_editable (GTK_TEXT (child->widget), TRUE);
	gtk_text_set_word_wrap (GTK_TEXT (child->widget), TRUE);

	gtk_widget_set_parent (child->widget, GTK_WIDGET (fullday));

	return child;
}

static void
child_destroy (GncalFullDay *fullday, Child *child)
{
	/* Unparent the child widget manually as we don't have a remove method */

	gtk_widget_ref (child->widget);

	gtk_widget_unparent (child->widget);

	if (GTK_WIDGET_MAPPED (fullday))
		child_unmap (fullday, child);

	if (GTK_WIDGET_REALIZED (fullday))
		child_unrealize (fullday, child);

	gtk_widget_unref (child->widget);

	g_free (child);
}

static void
child_set_pos (GncalFullDay *fullday, Child *child, int x, int y, int width, int height)
{
	child->x = x;
	child->y = y;
	child->width = width;
	child->height = height;

	if (!child->window) /* realized? */
		return;

	child_set_text_pos (child);
	gdk_window_move_resize (child->window, x, y, width, height);
}

static struct layout_row *
layout_get_rows (GncalFullDay *fullday)
{
	struct layout_row *rows;
	int max_i;
	int f_rows;
	GList *children;
	Child *child;
	int i, n;

	get_tm_range (fullday, fullday->lower, fullday->upper, NULL, NULL, NULL, &f_rows);

	rows = g_new0 (struct layout_row, f_rows);
	max_i = 0;

	for (children = fullday->children; children; children = children->next) {
		child = children->data;

		for (i = 0; i < child->rows_used; i++) {
			n = child->lower_row + i;

			rows[n].intersections++;
			
			if (rows[n].intersections > max_i)
				max_i = rows[n].intersections;
		}
	}

	for (i = 0; i < f_rows; i++)
		rows[i].slots = g_new0 (int, max_i);

	return rows;
}

static void
layout_get_child_intersections (Child *child, struct layout_row *rows, int *min, int *max)
{
	int i, n;
	int imin, imax;

	imax = 0;

	for (i = 0; i < child->rows_used; i++) {
		n = child->lower_row + i;

		if (rows[n].intersections > imax)
			imax = rows[n].intersections;
	}

	imin = imax;

	for (i = 0; i < child->rows_used; i++) {
		n = child->lower_row + i;

		if (rows[n].intersections < imin)
			imin = rows[n].intersections;
	}

	if (min)
		*min = imin;

	if (max)
		*max = imax;
}

static int
calc_labels_width (GncalFullDay *fullday)
{
	struct tm cur, upper;
	time_t tim, time_upper;
	int width, max_w;
	char buf[256];

	get_tm_range (fullday, fullday->lower, fullday->upper, &cur, &upper, NULL, NULL);

	max_w = 0;

	tim = mktime (&cur);
	time_upper = mktime (&upper);

	while (tim < time_upper) {
		strftime (buf, 256, "%X", &cur);

		width = gdk_string_width (GTK_WIDGET (fullday)->style->font, buf);

		if (width > max_w)
			max_w = width;

		cur.tm_min += fullday->interval;
		tim = mktime (&cur);
	}

	return max_w;
}

static int
calc_row_height (GncalFullDay *fullday)
{
	int f_rows;
	GtkWidget *widget;

	get_tm_range (fullday, fullday->lower, fullday->upper, NULL, NULL, NULL, &f_rows);

	widget = GTK_WIDGET (fullday);

	return (widget->allocation.height - 2 * widget->style->klass->ythickness) / f_rows;
}

static void
layout_child (GncalFullDay *fullday, Child *child, struct layout_row *rows, int left_x)
{
	int c_y, c_width, c_height;
	GtkWidget *widget;
	int row_height;

	/* Calculate child position */

	widget = GTK_WIDGET (fullday);

	row_height = calc_row_height (fullday);

	c_y = widget->style->klass->ythickness;

	/* FIXME: for now, the children overlap.  Make it layout them nicely. */

	c_width = widget->allocation.width - (widget->style->klass->xthickness + left_x);

	c_y += child->lower_row * row_height;
	c_height = child->rows_used * row_height;

	/* Position child */

	child_set_pos (fullday, child, left_x, c_y, c_width, c_height);
}

static void
layout_children (GncalFullDay *fullday)
{
	struct layout_row *rows;
	GList *children;
	GtkWidget *widget;
	int left_x;

	rows = layout_get_rows (fullday);

	widget = GTK_WIDGET (fullday);

	left_x = 2 * (widget->style->klass->xthickness + TEXT_BORDER) + calc_labels_width (fullday);

	for (children = fullday->children; children; children = children->next)
		layout_child (fullday, children->data, rows, left_x);

	g_free (rows);
}

guint
gncal_full_day_get_type (void)
{
	static guint full_day_type = 0;

	if (!full_day_type) {
		GtkTypeInfo full_day_info = {
			"GncalFullDay",
			sizeof (GncalFullDay),
			sizeof (GncalFullDayClass),
			(GtkClassInitFunc) gncal_full_day_class_init,
			(GtkObjectInitFunc) gncal_full_day_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		full_day_type = gtk_type_unique (gtk_container_get_type (), &full_day_info);
	}

	return full_day_type;
}

static void
gncal_full_day_class_init (GncalFullDayClass *class)
{
	GtkObjectClass    *object_class;
	GtkWidgetClass    *widget_class;
	GtkContainerClass *container_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	container_class = (GtkContainerClass *) class;

	parent_class = gtk_type_class (gtk_container_get_type ());

	fullday_signals[RANGE_ACTIVATED] =
		gtk_signal_new ("range_activated",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GncalFullDayClass, range_activated),
				gtk_signal_default_marshaller,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, fullday_signals, LAST_SIGNAL);

	object_class->destroy = gncal_full_day_destroy;

	widget_class->map = gncal_full_day_map;
	widget_class->unmap = gncal_full_day_unmap;
	widget_class->realize = gncal_full_day_realize;
	widget_class->unrealize = gncal_full_day_unrealize;
	widget_class->draw = gncal_full_day_draw;
	widget_class->draw_focus = gncal_full_day_draw_focus;
	widget_class->size_request = gncal_full_day_size_request;
	widget_class->size_allocate = gncal_full_day_size_allocate;
	widget_class->button_press_event = gncal_full_day_button_press;
	widget_class->button_release_event = gncal_full_day_button_release;
	widget_class->motion_notify_event = gncal_full_day_motion;
	widget_class->expose_event = gncal_full_day_expose;
	widget_class->key_press_event = gncal_full_day_key_press;
	widget_class->focus_in_event = gncal_full_day_focus_in;
	widget_class->focus_out_event = gncal_full_day_focus_out;

	container_class->foreach = gncal_full_day_foreach;

	class->range_activated = range_activated;
}

static void
gncal_full_day_init (GncalFullDay *fullday)
{
	GTK_WIDGET_UNSET_FLAGS (fullday, GTK_NO_WINDOW);
	GTK_WIDGET_SET_FLAGS (fullday, GTK_CAN_FOCUS);

	fullday->calendar = NULL;

	fullday->lower = 0;
	fullday->upper = 0;
	fullday->interval = 30; /* 30 minutes by default */

	fullday->children = NULL;
	fullday->drag_info = g_new0 (struct drag_info, 1);

	fullday->up_down_cursor = NULL;
	fullday->beam_cursor = NULL;
}

static void
gncal_full_day_destroy (GtkObject *object)
{
	GncalFullDay *fullday;
	GList *children;
	Child *child;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (object));

	fullday = GNCAL_FULL_DAY (object);

	/* Unparent the children manually as we don't have a remove method */

	for (children = fullday->children; children; children = children->next) {
		child = children->data;

		gtk_widget_unparent (child->widget);
	}

	g_list_free (fullday->children);
	g_free (fullday->drag_info);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

GtkWidget *
gncal_full_day_new (GnomeCalendar *calendar, time_t lower, time_t upper)
{
	GncalFullDay *fullday;

	g_return_val_if_fail (calendar != NULL, NULL);

	fullday = gtk_type_new (gncal_full_day_get_type ());

	fullday->calendar = calendar;

	gncal_full_day_set_bounds (fullday, lower, upper);

	return GTK_WIDGET (fullday);
}

static void
gncal_full_day_map (GtkWidget *widget)
{
	GncalFullDay *fullday;
	GList *children;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));

	GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

	fullday = GNCAL_FULL_DAY (widget);

	gdk_window_show (widget->window);

	for (children = fullday->children; children; children = children->next)
		child_map (fullday, children->data);
}

static void
gncal_full_day_unmap (GtkWidget *widget)
{
	GncalFullDay *fullday;
	GList *children;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	fullday = GNCAL_FULL_DAY (widget);

	gdk_window_hide (widget->window);

	for (children = fullday->children; children; children = children->next)
		child_unmap (fullday, children->data);
}

static void
gncal_full_day_realize (GtkWidget *widget)
{
	GncalFullDay *fullday;
	GdkWindowAttr attributes;
	gint attributes_mask;
	GList *children;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	fullday = GNCAL_FULL_DAY (widget);

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
				 | GDK_BUTTON_PRESS_MASK
				 | GDK_BUTTON_RELEASE_MASK
				 | GDK_BUTTON_MOTION_MASK
				 | GDK_POINTER_MOTION_HINT_MASK);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gdk_window_set_background (widget->window, &widget->style->bg[GTK_STATE_PRELIGHT]);

	fullday->up_down_cursor = gdk_cursor_new (GDK_DOUBLE_ARROW);
	fullday->beam_cursor = gdk_cursor_new (GDK_XTERM);

	for (children = fullday->children; children; children = children->next)
		child_realize (fullday, children->data);
}

static void
gncal_full_day_unrealize (GtkWidget *widget)
{
	GncalFullDay *fullday;
	GList *children;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));

	fullday = GNCAL_FULL_DAY (widget);

	for (children = fullday->children; children; children = children->next)
		child_unrealize (fullday, children->data);

	gdk_cursor_destroy (fullday->up_down_cursor);
	fullday->up_down_cursor = NULL;

	gdk_cursor_destroy (fullday->beam_cursor);
	fullday->beam_cursor = NULL;

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
paint_back (GncalFullDay *fullday, GdkRectangle *area)
{
	GtkWidget *widget;
	GdkRectangle rect, dest, aarea;
	struct drag_info *di;
	int x1, y1, width, height;
	int labels_width, division_x;
	int rows, row_height;
	int i, y;
	struct tm tm;
	char buf[256];

	widget = GTK_WIDGET (fullday);

	if (!area) {
		area = &aarea;

		area->x = 0;
		area->y = 0;
		area->width = widget->allocation.width;
		area->height = widget->allocation.height;
	}

	x1 = widget->style->klass->xthickness;
	y1 = widget->style->klass->ythickness;
	width = widget->allocation.width - 2 * x1;
	height = widget->allocation.height - 2 * y1;

	/* Clear and paint frame shadow */

	gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

	gtk_widget_draw_focus (widget);

	/* Clear space for labels */

	labels_width = calc_labels_width (fullday);

	rect.x = x1;
	rect.y = y1;
	rect.width = 2 * TEXT_BORDER + labels_width;
	rect.height = height;

	if (gdk_rectangle_intersect (&rect, area, &dest))
		gdk_draw_rectangle (widget->window,
				    widget->style->bg_gc[GTK_STATE_NORMAL],
				    TRUE,
				    dest.x, dest.y,
				    dest.width, dest.height);

	/* Selected region */

	di = fullday->drag_info;

	get_tm_range (fullday, fullday->lower, fullday->upper, &tm, NULL, NULL, &rows);

	row_height = calc_row_height (fullday);

	if (di->sel_rows_used != 0) {
		rect.x = x1;
		rect.y = y1 + row_height * di->sel_start_row;
		rect.width = width;
		rect.height = row_height * di->sel_rows_used;

		if (gdk_rectangle_intersect (&rect, area, &dest))
			gdk_draw_rectangle (widget->window,
					    widget->style->bg_gc[GTK_STATE_SELECTED],
					    TRUE,
					    dest.x, dest.y,
					    dest.width, dest.height); 
	}

	/* Vertical division */

	division_x = x1 + 2 * TEXT_BORDER + labels_width;

	gtk_draw_vline (widget->style, widget->window,
			GTK_STATE_NORMAL,
			y1,
			y1 + height - 1,
			division_x);

	/* Horizontal divisions */

	y = y1 + row_height - 1;

	for (i = 1; i < rows; i++) {
		gdk_draw_line (widget->window,
			       widget->style->black_gc,
			       x1, y,
			       x1 + width - 1, y);

		y += row_height;
	}

	/* Labels */

	y = y1 + ((row_height - 1) - (widget->style->font->ascent + widget->style->font->descent)) / 2;

	rect.x = x1;
	rect.y = y1;
	rect.width = 2 * TEXT_BORDER + labels_width;
	rect.height = row_height - 1;

	for (i = 0; i < rows; i++) {
		mktime (&tm);

		if (gdk_rectangle_intersect (&rect, area, &dest)) {
			strftime (buf, 256, "%X", &tm);

			gdk_draw_string (widget->window,
					 widget->style->font,
					 widget->style->fg_gc[GTK_STATE_NORMAL],
					 x1 + TEXT_BORDER,
					 y + widget->style->font->ascent,
					 buf);
		}

		rect.y += row_height;
		y += row_height;

		tm.tm_min += fullday->interval;
	}
}

static void
gncal_full_day_draw (GtkWidget *widget, GdkRectangle *area)
{
	GncalFullDay *fullday;
	GList *children;
	Child *child;
	GdkRectangle rect, dest;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));
	g_return_if_fail (area != NULL);

	if (!GTK_WIDGET_DRAWABLE (widget))
		return;

	fullday = GNCAL_FULL_DAY (widget);

	paint_back (fullday, area);

	for (children = fullday->children; children; children = children->next) {
		child = children->data;

		rect.x = child->x;
		rect.y = child->y;
		rect.width = child->width;
		rect.height = child->height;

		if (gdk_rectangle_intersect (&rect, area, &dest)) {
			dest.x -= child->x;
			dest.y -= child->y;

			child_draw (fullday, child, &dest, TRUE);
		}
	}
}

static void
gncal_full_day_draw_focus (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));

	if (!GTK_WIDGET_DRAWABLE (widget))
		return;

	gtk_draw_shadow (widget->style, widget->window,
			 GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_IN,
			 0, 0,
			 widget->allocation.width,
			 widget->allocation.height);

	if (GTK_WIDGET_HAS_FOCUS (widget))
		gdk_draw_rectangle (widget->window,
				    widget->style->black_gc,
				    FALSE,
				    0, 0,
				    widget->allocation.width - 1,
				    widget->allocation.height - 1);
}

static void
gncal_full_day_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GncalFullDay *fullday;
	int labels_width;
	int rows;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));
	g_return_if_fail (requisition != NULL);

	fullday = GNCAL_FULL_DAY (widget);

	/* Border and min width */

	labels_width = calc_labels_width (fullday);

	requisition->width = 2 * widget->style->klass->xthickness + 4 * TEXT_BORDER + labels_width + MIN_WIDTH;
	requisition->height = 2 * widget->style->klass->ythickness;

	/* Rows */

	get_tm_range (fullday, fullday->lower, fullday->upper, NULL, NULL, NULL, &rows);

	requisition->height += (rows * (2 * TEXT_BORDER + widget->style->font->ascent + widget->style->font->descent)
				+ (rows - 1)); /* division lines */
}

static void
gncal_full_day_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GncalFullDay *fullday;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));
	g_return_if_fail (allocation != NULL);

	widget->allocation = *allocation;

	fullday = GNCAL_FULL_DAY (widget);

	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (widget->window,
					allocation->x, allocation->y,
					allocation->width, allocation->height);

	layout_children (fullday);
}

static Child *
find_child_by_window (GncalFullDay *fullday, GdkWindow *window)
{
	GList *children;
	Child *child;

	for (children = fullday->children; children; children = children->next) {
		child = children->data;

		if (child->window == window)
			return child;
	}

	return NULL;
}

static void
draw_xor_rect (GncalFullDay *fullday)
{
	GtkWidget *widget;
	struct drag_info *di;
	int i;
	int row_height;
	int ythickness;

	widget = GTK_WIDGET (fullday);

	gdk_gc_set_function (widget->style->white_gc, GDK_INVERT);
	gdk_gc_set_subwindow (widget->style->white_gc, GDK_INCLUDE_INFERIORS);

	ythickness = widget->style->klass->ythickness;

	di = fullday->drag_info;

	row_height = calc_row_height (fullday);

	for (i = 0; i < XOR_RECT_WIDTH; i++)
		gdk_draw_rectangle (widget->window,
				    widget->style->white_gc,
				    FALSE,
				    di->child->x + i,
				    di->start_row * row_height + ythickness + i,
				    di->child->width - 2 * i - 1,
				    di->rows_used * row_height - 2 * i - 2);

	gdk_gc_set_function (widget->style->white_gc, GDK_COPY);
	gdk_gc_set_subwindow (widget->style->white_gc, GDK_CLIP_BY_CHILDREN);
}

static int
get_row_from_y (GncalFullDay *fullday, int y, int round)
{
	GtkWidget *widget;
	int row_height;
	int f_rows;
	int ythickness;

	get_tm_range (fullday, fullday->lower, fullday->upper, NULL, NULL, NULL, &f_rows);

	row_height = calc_row_height (fullday);

	widget = GTK_WIDGET (fullday);

	ythickness = widget->style->klass->ythickness;

	y -= ythickness;

	if (y < 0)
		y = 0;
	else if (y >= (f_rows * row_height))
		y = f_rows * row_height - 1;

	if (round)
		y += row_height / 2;

	y /= row_height;

	if (y > f_rows)
		y = f_rows; /* note that this is 1 more than the last row's index */

	return y;
}

static gint
gncal_full_day_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GncalFullDay *fullday;
	Child *child;
	struct drag_info *di;
	gint y;
	int row_height;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	fullday = GNCAL_FULL_DAY (widget);

	if (event->window == widget->window) {
		/* Clicked on main window */

		if (!GTK_WIDGET_HAS_FOCUS (widget))
			gtk_widget_grab_focus (widget);

		/* Prepare for drag */

		di = fullday->drag_info;

		di->drag_mode = DRAG_SELECT;

		di->sel_click_row = get_row_from_y (fullday, event->y, FALSE);
		di->sel_start_row = di->sel_click_row;
		di->sel_rows_used = 1;

		di->click_time = event->time;

		gdk_pointer_grab (widget->window, FALSE,
				  (GDK_BUTTON_MOTION_MASK
				   | GDK_POINTER_MOTION_HINT_MASK
				   | GDK_BUTTON_RELEASE_MASK),
				  NULL,
				  NULL,
				  event->time);

		paint_back (fullday, NULL);
	} else {
		/* Clicked on a child? */

		child = find_child_by_window (fullday, event->window);

		if (!child)
			return FALSE;

		/* Prepare for drag */

		di = fullday->drag_info;

		gtk_widget_get_pointer (widget, NULL, &y);

		if (event->y < HANDLE_SIZE)
			di->drag_mode = DRAG_MOVE;
		else
			di->drag_mode = DRAG_SIZE;

		row_height = calc_row_height (fullday);

		di->child = child;

		di->start_row = get_row_from_y (fullday, child->y, FALSE);
		di->rows_used = child->height / row_height;

		gdk_pointer_grab (child->window, FALSE,
				  (GDK_BUTTON_MOTION_MASK
				   | GDK_POINTER_MOTION_HINT_MASK
				   | GDK_BUTTON_RELEASE_MASK),
				  NULL,
				  NULL,
				  event->time);

		draw_xor_rect (fullday);
	}

	return FALSE;
}

static void
recompute_motion (GncalFullDay *fullday, int y)
{
	struct drag_info *di;
	int f_rows;
	int row;

	di = fullday->drag_info;

	get_tm_range (fullday, fullday->lower, fullday->upper, NULL, NULL, NULL, &f_rows);

	switch (di->drag_mode) {
	case DRAG_SELECT:
		row = get_row_from_y (fullday, y, FALSE);

		if (row >= f_rows)
			row = f_rows - 1;

		if (row < di->sel_click_row) {
			di->sel_start_row = row;
			di->sel_rows_used = di->sel_click_row - row + 1;
		} else {
			di->sel_start_row = di->sel_click_row;
			di->sel_rows_used = row - di->sel_start_row + 1;
		}

		break;

	case DRAG_MOVE:
		row = get_row_from_y (fullday, y, FALSE);

		if (row > (f_rows - di->rows_used))
			row = f_rows - di->rows_used;

		di->start_row = row;

		break;

	case DRAG_SIZE:
		row = get_row_from_y (fullday, y, TRUE);

		if (row <= di->start_row)
			row = di->start_row + 1;
		else if (row > f_rows)
			row = f_rows;

		di->rows_used = row - di->start_row;

		break;

	default:
		g_assert_not_reached ();
	}
}

static void
get_time_from_rows (GncalFullDay *fullday, int start_row, int rows_used, time_t *t_lower, time_t *t_upper)
{
	struct tm tm;
	int row_height;

	get_tm_range (fullday, fullday->lower, fullday->upper, &tm, NULL, NULL, NULL);

	row_height = calc_row_height (fullday);

	tm.tm_min += fullday->interval * start_row;
	*t_lower = mktime (&tm);

	tm.tm_min += fullday->interval * rows_used;
	*t_upper = mktime (&tm);
}

static void
update_from_drag_info (GncalFullDay *fullday)
{
	struct drag_info *di;
	GtkWidget *widget;

	di = fullday->drag_info;

	widget = GTK_WIDGET (fullday);

	get_time_from_rows (fullday, di->start_row, di->rows_used,
			    &di->child->ico->dtstart,
			    &di->child->ico->dtend);

	child_range_changed (fullday, di->child);

	/* Notify calendar of change */

	gnome_calendar_object_changed (fullday->calendar, di->child->ico);
}

static gint
gncal_full_day_button_release (GtkWidget *widget, GdkEventButton *event)
{
	GncalFullDay *fullday;
	struct drag_info *di;
	gint y;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	fullday = GNCAL_FULL_DAY (widget);

	di = fullday->drag_info;

	gtk_widget_get_pointer (widget, NULL, &y);

	switch (di->drag_mode) {
	case DRAG_NONE:
		return FALSE;

	case DRAG_SELECT:
		if ((event->time - di->click_time) < UNSELECT_TIMEOUT)
			di->sel_rows_used = 0;
		else
			recompute_motion (fullday, y);

		gdk_pointer_ungrab (event->time);

		paint_back (fullday, NULL);

		break;

	case DRAG_MOVE:
	case DRAG_SIZE:
		draw_xor_rect (fullday);
		recompute_motion (fullday, y);
		gdk_pointer_ungrab (event->time);

		update_from_drag_info (fullday);

		di->rows_used = 0;

		break;

	default:
		g_assert_not_reached ();
	}

	di->drag_mode = DRAG_NONE;
	di->child = NULL;

	return FALSE;
}

static gint
gncal_full_day_motion (GtkWidget *widget, GdkEventMotion *event)
{
	GncalFullDay *fullday;
	struct drag_info *di;
	gint y;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	fullday = GNCAL_FULL_DAY (widget);
	di = fullday->drag_info;

	gtk_widget_get_pointer (widget, NULL, &y);
	
	switch (di->drag_mode) {
	case DRAG_NONE:
		break;

	case DRAG_SELECT:
		recompute_motion (fullday, y);
		paint_back (fullday, NULL);

		break;

	case DRAG_MOVE:
	case DRAG_SIZE:
		draw_xor_rect (fullday);
		recompute_motion (fullday, y);
		draw_xor_rect (fullday);

		break;

	default:
		g_assert_not_reached ();
	}

	return FALSE;
}

static gint
gncal_full_day_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GncalFullDay *fullday;
	GList *children;
	Child *child;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (!GTK_WIDGET_DRAWABLE (widget))
		return FALSE;

	fullday = GNCAL_FULL_DAY (widget);

	if (event->window == widget->window)
		paint_back (fullday, &event->area);
	else
		for (children = fullday->children; children; children = children->next) {
			child = children->data;

			if (event->window == child->window) {
				child_draw (fullday, child, &event->area, FALSE);
				break;
			}
		}

	return FALSE;
}

static gint
gncal_full_day_key_press (GtkWidget *widget, GdkEventKey *event)
{
	GncalFullDay *fullday;
	struct drag_info *di;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	fullday = GNCAL_FULL_DAY (widget);

	di = fullday->drag_info;

	if (di->sel_rows_used == 0)
		return FALSE;

	if (event->keyval == GDK_Return) {
		gtk_signal_emit (GTK_OBJECT (fullday), fullday_signals [RANGE_ACTIVATED]);
		return TRUE;
	}

	return FALSE;
}

static gint
gncal_full_day_focus_in (GtkWidget *widget, GdkEventFocus *event)
{
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
	gtk_widget_draw_focus (widget);

	return FALSE;
}

static gint
gncal_full_day_focus_out (GtkWidget *widget, GdkEventFocus *event)
{
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
	gtk_widget_draw_focus (widget);

	return FALSE;
}

static void
gncal_full_day_foreach (GtkContainer *container, GtkCallback callback, gpointer callback_data)
{
	GncalFullDay *fullday;
	GList *children;
	Child *child;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (container));
	g_return_if_fail (callback != NULL);

	fullday = GNCAL_FULL_DAY (container);

	for (children = fullday->children; children; children = children->next) {
		child = children->data;

		(*callback) (child->widget, callback_data);
	}
}

void
gncal_full_day_update (GncalFullDay *fullday)
{
	GList *children;
	GList *l_events, *events;
	Child *child;

	g_return_if_fail (fullday != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (fullday));

	if (!fullday->calendar->cal)
		return;

	for (children = fullday->children; children; children = children->next)
		child_destroy (fullday, children->data);

	g_list_free (fullday->children);

	children = NULL;

	l_events = calendar_get_events_in_range (fullday->calendar->cal,
						 fullday->lower,
						 fullday->upper,
						 calendar_compare_by_dtstart);

	for (events = l_events; events; events = events->next) {
		child = child_new (fullday, events->data);
		children = g_list_append (children, child);
	}

	g_list_free (l_events);

	fullday->children = g_list_first (children);

	layout_children (fullday);

	/* Realize and map children */

	for (children = fullday->children; children; children = children->next) {
		if (GTK_WIDGET_REALIZED (fullday))
			child_realize (fullday, children->data);

		if (GTK_WIDGET_MAPPED (fullday))
			child_map (fullday, children->data);
	}

	/* FIXME: paint or something */

	gtk_widget_draw (GTK_WIDGET (fullday), NULL);
}

void
gncal_full_day_set_bounds (GncalFullDay *fullday, time_t lower, time_t upper)
{
	g_return_if_fail (fullday != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (fullday));

	if ((lower != fullday->lower) || (upper != fullday->upper)) {
		fullday->lower = lower;
		fullday->upper = upper;

		gncal_full_day_update (fullday);
	}
}

int
gncal_full_day_selection_range (GncalFullDay *fullday, time_t *lower, time_t *upper)
{
	struct drag_info *di;
	time_t alower, aupper;

	g_return_val_if_fail (fullday != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (fullday), FALSE);

	di = fullday->drag_info;

	if (di->sel_rows_used == 0)
		return FALSE;

	get_time_from_rows (fullday, di->sel_start_row, di->sel_rows_used, &alower, &aupper);

	if (lower)
		*lower = alower;

	if (upper)
		*upper= aupper;

	return TRUE;
}

static void
range_activated (GncalFullDay *fullday)
{
	struct drag_info *di;

	g_return_if_fail (fullday != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (fullday));

	di = fullday->drag_info;

	/* Remove selection; at this point someone should already have added an appointment */

	di->sel_rows_used = 0;

	paint_back (fullday, NULL);
}
