/* Full day widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <gtk/gtkdrawingarea.h>
#include <gtk/gtktext.h>
#include "gncal-full-day.h"
#include "view-utils.h"


#define TEXT_BORDER 2
#define HANDLE_SIZE 3
#define MIN_WIDTH 200
#define XOR_RECT_WIDTH 2


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
	Child *child;
	enum {
		DRAG_MOVE,
		DRAG_SIZE
	} drag_mode;
	int new_y;
	int new_height;
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
static void gncal_full_day_foreach        (GtkContainer      *container,
					   GtkCallback        callback,
					   gpointer           callback_data);


static GtkContainerClass *parent_class;


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

	if (GTK_WIDGET_VISIBLE (child->widget) && !GTK_WIDGET_MAPPED (child->widget))
		gtk_widget_map (child->widget);
}

static void
child_unmap (GncalFullDay *fullday, Child *child)
{
	gdk_window_hide (child->window);

	if (GTK_WIDGET_VISIBLE (child->widget) && GTK_WIDGET_MAPPED (child->widget))
		gtk_widget_unmap (child->widget);
}

static void
child_move_text (Child *child)
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
				 | GDK_POINTER_MOTION_HINT_MASK);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP | GDK_WA_CURSOR;

	child->window = gdk_window_new (widget->window, &attributes, attributes_mask);
	gdk_window_set_user_data (child->window, widget);

	gtk_style_set_background (widget->style, child->window, GTK_STATE_NORMAL);

	gtk_widget_set_parent_window (child->widget, child->window);

	child_move_text (child);
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
	gint w, h;

	gdk_window_get_size (child->window, &w, &h);

	if (!area) {
		arect.x = 0;
		arect.y = 0;

		arect.width = w;
		arect.height = h;

		area = &arect;
	}

	/* Top handle */

	rect.x = 0;
	rect.y = 0;
	rect.width = w;
	rect.height = HANDLE_SIZE;

	if (gdk_rectangle_intersect (&rect, area, &dest))
		view_utils_draw_textured_frame (GTK_WIDGET (fullday), child->window, &rect, GTK_SHADOW_OUT);

	/* Bottom handle */

	rect.y = h - HANDLE_SIZE;

	if (gdk_rectangle_intersect (&rect, area, &dest))
		view_utils_draw_textured_frame (GTK_WIDGET (fullday), child->window, &rect, GTK_SHADOW_OUT);

	if (draw_child) {
		area->y -= HANDLE_SIZE;
		gtk_widget_draw (child->widget, area);
	}
}

static void
child_set_beam_cursor (GtkWidget *widget, gpointer data)
{
	GncalFullDay *fullday = data;

	gdk_window_set_cursor (widget->window, fullday->beam_cursor);
}

static Child *
child_new (GncalFullDay *fullday, iCalObject *ico)
{
	Child *child;
	struct tm start, end;
	int lower_row, rows_used;
	int f_lower_row;

	child = g_new (Child, 1);

	child->ico = ico;
	child->widget = gtk_text_new (NULL, NULL);
	child->window = NULL;
	child->x = 0;
	child->y = 0;
	child->width = 0;
	child->height = 0;

	/* Calc display range for event */

	get_tm_range (fullday, child->ico->dtstart, child->ico->dtend, &start, &end, &lower_row, &rows_used);
	get_tm_range (fullday, fullday->lower, fullday->upper, NULL, NULL, &f_lower_row, NULL);

	child->lower_row = lower_row - f_lower_row;
	child->rows_used = rows_used;

	/* Finish setup */

	gtk_signal_connect (GTK_OBJECT (child->widget), "realize",
			    (GtkSignalFunc) child_set_beam_cursor,
			    fullday);

	gtk_text_set_editable (GTK_TEXT (child->widget), TRUE);
	gtk_text_set_word_wrap (GTK_TEXT (child->widget), TRUE);

	gtk_widget_set_parent (child->widget, GTK_WIDGET (fullday));
	gtk_widget_show (child->widget);

	return child;
}

static void
child_destroy (GncalFullDay *fullday, Child *child)
{
	/* FIXME */
}

static void
child_set_pos (GncalFullDay *fullday, Child *child, int x, int y, int width, int height)
{
	child->x = x;
	child->y = y;
	child->width = width;
	child->height = height;

	if (!GTK_WIDGET_REALIZED (fullday))
		return;

	child_move_text (child);
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

static void
layout_child (GncalFullDay *fullday, Child *child, struct layout_row *rows)
{
	int c_x, c_y, c_width, c_height;
	GtkWidget *widget;
	int labels_width;
	int height, f_rows;
	int row_height;

	/* Calculate child position */

	widget = GTK_WIDGET (fullday);

	labels_width = calc_labels_width (fullday); /* FIXME: this is expensive to do for each child */

	get_tm_range (fullday, fullday->lower, fullday->upper, NULL, NULL, NULL, &f_rows);

	height = widget->allocation.height - 2 * widget->style->klass->ythickness;
	row_height = height / f_rows;

	c_x = 2 * (widget->style->klass->xthickness + TEXT_BORDER) + labels_width;
	c_y = widget->style->klass->ythickness;

	/* FIXME: for now, the children overlap.  Make it layout them nicely. */

	c_width = widget->allocation.width - (widget->style->klass->xthickness + c_x);

	c_y += child->lower_row * row_height;
	c_height = child->rows_used * row_height;

	/* Position child */

	child_set_pos (fullday, child, c_x, c_y, c_width, c_height);
}

static void
layout_children (GncalFullDay *fullday)
{
	struct layout_row *rows;
	GList *children;

	rows = layout_get_rows (fullday);

	for (children = fullday->children; children; children = children->next)
		layout_child (fullday, children->data, rows);

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

	object_class->destroy = gncal_full_day_destroy;

	widget_class->map = gncal_full_day_map;
	widget_class->unmap = gncal_full_day_unmap;
	widget_class->realize = gncal_full_day_realize;
	widget_class->unrealize = gncal_full_day_unrealize;
	widget_class->draw = gncal_full_day_draw;
	widget_class->size_request = gncal_full_day_size_request;
	widget_class->size_allocate = gncal_full_day_size_allocate;
	widget_class->button_press_event = gncal_full_day_button_press;
	widget_class->button_release_event = gncal_full_day_button_release;
	widget_class->motion_notify_event = gncal_full_day_motion;
	widget_class->expose_event = gncal_full_day_expose;

	container_class->foreach = gncal_full_day_foreach;
}

static void
gncal_full_day_init (GncalFullDay *fullday)
{
	GTK_WIDGET_UNSET_FLAGS (fullday, GTK_NO_WINDOW);

	fullday->calendar = NULL;

	fullday->lower = 0;
	fullday->upper = 0;
	fullday->interval = 30; /* 30 minutes by default */

	fullday->children = NULL;
	fullday->drag_info = g_new (struct drag_info, 1);

	fullday->up_down_cursor = NULL;
	fullday->beam_cursor = NULL;
}

static void
gncal_full_day_destroy (GtkObject *object)
{
	GncalFullDay *fullday;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (object));

	fullday = GNCAL_FULL_DAY (object);

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
				 | GDK_EXPOSURE_MASK);

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
gncal_full_day_draw (GtkWidget *widget, GdkRectangle *area)
{
	GncalFullDay *fullday;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (widget));
	g_return_if_fail (area != NULL);

	if (!GTK_WIDGET_DRAWABLE (widget))
		return;

	fullday = GNCAL_FULL_DAY (widget);

	/* FIXME */
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

	widget = GTK_WIDGET (fullday);

	gdk_gc_set_function (widget->style->white_gc, GDK_INVERT);
	gdk_gc_set_subwindow (widget->style->white_gc, GDK_INCLUDE_INFERIORS);

	di = fullday->drag_info;

	for (i = 0; i < XOR_RECT_WIDTH; i++)
		gdk_draw_rectangle (widget->window,
				    widget->style->white_gc,
				    FALSE,
				    di->child->x + i,
				    di->new_y + i,
				    di->child->width - 2 * i - 1,
				    di->new_height - 2 * i - 1);

	gdk_gc_set_function (widget->style->white_gc, GDK_COPY);
	gdk_gc_set_subwindow (widget->style->white_gc, GDK_CLIP_BY_CHILDREN);
}

static gint
gncal_full_day_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GncalFullDay *fullday;
	Child *child;
	struct drag_info *di;
	gint y;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	fullday = GNCAL_FULL_DAY (widget);

	if (event->window == widget->window)
		return FALSE; /* FIXME: do range selection thing */
	else {
		child = find_child_by_window (fullday, event->window);

		if (!child)
			return FALSE;

		di = fullday->drag_info;

		di->child = child;

		gtk_widget_get_pointer (widget, NULL, &y);

		if (event->y < HANDLE_SIZE)
			di->drag_mode = DRAG_MOVE;
		else
			di->drag_mode = DRAG_SIZE;

		di->new_y = child->y;
		di->new_height = child->height;

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
	GtkWidget *widget;
	struct drag_info *di;
	int rows, row_height;
	int ythickness;

	widget = GTK_WIDGET (fullday);

	get_tm_range (fullday, fullday->lower, fullday->upper, NULL, NULL, NULL, &rows);

	ythickness = widget->style->klass->ythickness;

	row_height = (widget->allocation.height - 2 * ythickness) / rows;

	y -= ythickness;
	y = (y + row_height / 2) / row_height; /* round to nearest bound */
	y = y * row_height + ythickness;

	di = fullday->drag_info;

	switch (di->drag_mode) {
	case DRAG_MOVE:
		if (y < ythickness)
			y = ythickness;
		else if (y >= (ythickness + rows * row_height - di->new_height))
			y = ythickness + rows * row_height - di->new_height;

		di->new_y = y;

		break;
		
	case DRAG_SIZE:
		if (y <= di->child->y)
			y = di->child->y + row_height;
		else if (y >= (ythickness + rows * row_height))
			y = ythickness + rows * row_height;

		di->new_height = y - di->new_y;

		break;

	default:
		g_assert_not_reached ();
	}
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

	if (!di->child || (event->window != di->child->window))
		return FALSE;

	gtk_widget_get_pointer (widget, NULL, &y);

	draw_xor_rect (fullday);
	recompute_motion (fullday, y);
	gdk_pointer_ungrab (event->time);

	/* FIXME: update child, notify, etc. */

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

	if (!di->child || (event->window != di->child->window))
		return FALSE;

	gtk_widget_get_pointer (widget, NULL, &y);
	
	draw_xor_rect (fullday);
	recompute_motion (fullday, y);
	draw_xor_rect (fullday);

	return FALSE;
}

static void
paint_back (GncalFullDay *fullday, GdkRectangle *area)
{
	GtkWidget *widget;
	GdkRectangle rect, dest;
	int x1, y1, width, height;
	int labels_width, division_x;
	int rows, row_height;
	int i, y;
	struct tm tm;
	char buf[256];

	widget = GTK_WIDGET (fullday);

	x1 = widget->style->klass->xthickness;
	y1 = widget->style->klass->ythickness;
	width = widget->allocation.width - 2 * x1;
	height = widget->allocation.height - 2 * y1;

	/* Clear and paint frame shadow */

	gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

	gtk_draw_shadow (widget->style, widget->window,
			 GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_IN,
			 0, 0,
			 widget->allocation.width,
			 widget->allocation.height);

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

	/* Vertical division */

	division_x = x1 + 2 * TEXT_BORDER + labels_width;

	gtk_draw_vline (widget->style, widget->window,
			GTK_STATE_NORMAL,
			y1,
			y1 + height - 1,
			division_x);

	/* Horizontal divisions */

	get_tm_range (fullday, fullday->lower, fullday->upper, &tm, NULL, NULL, &rows);

	row_height = height / rows; /* includes division line at bottom of row */

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

	l_events = calendar_get_events_in_range (fullday->calendar->cal,
						 fullday->lower,
						 fullday->upper,
						 calendar_compare_by_dtstart);

	/* FIXME: this is expensive and looks ugly -- use some form of cache? */

	for (children = fullday->children; children; children = children->next)
		child_destroy (fullday, children->data);

	g_list_free (fullday->children);

	children = NULL;

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
