/* Full day widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <quartic@gimp.org>
 *          Miguel de Icaza <miguel@kernel.org>
 */
#include <config.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gnome.h>
#include "eventedit.h"
#include "gncal-full-day.h"
#include "view-utils.h"
#include "layout.h"
#include "main.h"
#include "popup-menu.h"

/* Images */
#include "bell.xpm"
#include "recur.xpm"

#define TEXT_BORDER 2
#define HANDLE_SIZE 8
#define MIN_WIDTH 200
#define XOR_RECT_WIDTH 2
#define UNSELECT_TIMEOUT 0 /* ms */

/* Size of the pixmaps */
#define DECOR_WIDTH      16
#define DECOR_HEIGHT     16

typedef struct {
	iCalObject *ico;
	GtkWidget  *widget;
	GdkWindow  *window;
	GdkWindow  *decor_window;
	guint       focus_out_id;
	int         lower_row; /* zero is first displayed row */
	int         rows_used;
	int         x;         /* coords of child's window */
	int         y;
	int         width;
	int         height;
	int         decor_width;
	int         decor_height;
	int         items;	/* number of decoration bitmaps */
	time_t      start, end;
} Child;

struct drag_info {
	enum {
		DRAG_NONE,
		DRAG_SELECT,		/* selecting a range in the main window */
		DRAG_MOVE,		/* moving a child */
		DRAG_SIZE_TOP,		/* resizing a child */
		DRAG_SIZE_BOTTOM
	} drag_mode;

	Child *child;
	int child_click_y;
	int child_start_row;
	int child_rows_used;

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
static void gncal_full_day_forall         (GtkContainer      *container,
					   gboolean           include_internals,
					   GtkCallback        callback,
					   gpointer           callback_data);

static void range_activated (GncalFullDay *fullday);

static GtkContainerClass *parent_class;

static int fullday_signals[LAST_SIGNAL] = { 0 };

/* The little images */
static GdkPixmap *pixmap_bell, *pixmap_recur;

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
	if (child->decor_width)
		gdk_window_show (child->decor_window);
	gtk_widget_show (child->widget); /* OK, not just a map... */
}

static void
child_unmap (GncalFullDay *fullday, Child *child)
{
	gdk_window_hide (child->window);
	gdk_window_hide (child->decor_window);
	if (GTK_WIDGET_MAPPED (child->widget))
		gtk_widget_unmap (child->widget);
}

static void
child_set_text_pos (Child *child)
{
	GtkAllocation allocation;
	int has_focus;
	int handle_size;
	
	has_focus = GTK_WIDGET_HAS_FOCUS (child->widget);

	handle_size = (child->ico->recur) ? 0 : HANDLE_SIZE;
	
	allocation.x = handle_size;
	allocation.y = has_focus ? handle_size : 0;
	allocation.width = child->width - handle_size - child->decor_width;
	allocation.height = child->height - (has_focus ? (2 * handle_size) : 0);

	gtk_widget_size_request (child->widget, &child->widget->requisition); /* FIXME: is this needed? */
	gtk_widget_size_allocate (child->widget, &allocation);
}

static void
child_realize (GncalFullDay *fullday, Child *child)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	GtkWidget *widget;
	GdkColor c;
	
	widget = GTK_WIDGET (fullday);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = child->x;
	attributes.y = child->y;
	attributes.width = child->width - child->decor_width;;
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

	/* Create the decoration window */
	attributes.x = child->x + child->width - child->decor_width;
	attributes.width  = child->decor_width ? child->decor_width : 1;
	attributes.height = child->decor_height ? child->decor_height : 1;
	attributes.visual   = gdk_imlib_get_visual ();
	attributes.colormap = gdk_imlib_get_colormap ();
	attributes.event_mask = (GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	child->decor_window = gdk_window_new (widget->window, &attributes, attributes_mask);
	gdk_color_white (gdk_imlib_get_colormap (), &c);
	gdk_window_set_background (child->decor_window, &c);
	gdk_window_set_user_data (child->decor_window, widget);

	if (!pixmap_bell){
		GdkImlibImage *imlib_bell, *imlib_recur;
		GdkPixmap *mask;
		
		imlib_bell  = gdk_imlib_create_image_from_xpm_data (bell_xpm);
		gdk_imlib_render (imlib_bell, DECOR_WIDTH, DECOR_HEIGHT);
		pixmap_bell = gdk_imlib_move_image (imlib_bell);
		mask = gdk_imlib_move_mask  (imlib_bell);
		gdk_imlib_destroy_image (imlib_bell);
		fullday->bell_gc = gdk_gc_new (child->decor_window);
		if (mask)
			gdk_gc_set_clip_mask (fullday->bell_gc, mask);
			
		imlib_recur = gdk_imlib_create_image_from_xpm_data (recur_xpm);
		gdk_imlib_render (imlib_recur, DECOR_WIDTH, DECOR_HEIGHT);
		pixmap_recur = gdk_imlib_move_image (imlib_recur);
		mask   = gdk_imlib_move_mask (imlib_recur);
		gdk_imlib_destroy_image (imlib_recur);
		fullday->recur_gc = gdk_gc_new (child->decor_window);
		if (mask)
			gdk_gc_set_clip_mask (fullday->recur_gc, mask);
	}
	child_set_text_pos (child);
}

static void
child_unrealize (GncalFullDay *fullday, Child *child)
{
	if (GTK_WIDGET_REALIZED (child->widget))
		gtk_widget_unrealize (child->widget);

	gdk_window_set_user_data (child->window, NULL);
	gdk_window_destroy (child->window);
	child->window = NULL;
}

static void
child_draw_decor (GncalFullDay *fullday, Child *child)
{
	iCalObject *ico = child->ico;
	int ry = 0;

	if (ico->recur) {
		gdk_gc_set_clip_origin (fullday->recur_gc, 0, ry);
		gdk_draw_pixmap (child->decor_window,
				 fullday->recur_gc,
				 pixmap_recur,
				 0, 0,
				 0, ry,
				 DECOR_WIDTH, DECOR_HEIGHT);
		ry += DECOR_HEIGHT;
	}

	if (ico->dalarm.enabled || ico->malarm.enabled || ico->palarm.enabled || ico->aalarm.enabled) {
		gdk_gc_set_clip_origin (fullday->bell_gc, 0, ry);
		gdk_draw_pixmap (child->decor_window,
				 fullday->bell_gc,
				 pixmap_bell,
				 0, 0,
				 0, ry, 
				 DECOR_WIDTH, DECOR_HEIGHT);
		ry += DECOR_HEIGHT;
	}
}

static void
child_draw (GncalFullDay *fullday, Child *child, GdkRectangle *area, GdkWindow *window, int draw_child)
{
	GdkRectangle arect, rect, dest;
	int has_focus;

	has_focus = GTK_WIDGET_HAS_FOCUS (child->widget);

	if (!window || (window == child->window)) {
		if (!area) {
			arect.x = 0;
			arect.y = 0;
			arect.width = child->width;
			arect.height = child->height;

			area = &arect;
		}

		/* Left handle */

		rect.x = 0;
		rect.y = has_focus ? HANDLE_SIZE : 0;
		rect.width = HANDLE_SIZE;
		rect.height = has_focus ? (child->height - 2 * HANDLE_SIZE) : child->height;

		if (gdk_rectangle_intersect (&rect, area, &dest))
			view_utils_draw_textured_frame (GTK_WIDGET (fullday), child->window, &rect, GTK_SHADOW_OUT);

		if (has_focus) {
			/* Top handle */

			rect.x = 0;
			rect.y = 0;
			rect.width = child->width - child->decor_width;
			rect.height = HANDLE_SIZE;

			if (gdk_rectangle_intersect (&rect, area, &dest))
				view_utils_draw_textured_frame (GTK_WIDGET (fullday), child->window,
								&rect, GTK_SHADOW_OUT);

			/* Bottom handle */

			rect.y = child->height - HANDLE_SIZE;

			if (gdk_rectangle_intersect (&rect, area, &dest))
				view_utils_draw_textured_frame (GTK_WIDGET (fullday), child->window,
								&rect, GTK_SHADOW_OUT);
		}

	} else if (!window || (window == child->decor_window)) {
		if (!area) {
			arect.x = 0;
			arect.y = 0;
			arect.width = child->decor_width;
			arect.height = child->decor_height;

			area = &arect;
		}

		child_draw_decor (fullday, child);
	}

	if (draw_child)
		gtk_widget_draw (child->widget, NULL);
}

static void
child_range_changed (GncalFullDay *fullday, Child *child)
{
	struct tm start, end;
	int lower_row, rows_used;
	int f_lower_row;

	/* Calc display range for event */

	get_tm_range (fullday, child->start, child->end, &start, &end, &lower_row, &rows_used);
	get_tm_range (fullday, fullday->lower, fullday->upper, NULL, NULL, &f_lower_row, NULL);

	child->lower_row = lower_row - f_lower_row;
	child->rows_used = rows_used;
}

static void
new_appointment (GtkWidget *widget, gpointer data)
{
	GncalFullDay *fullday;
	GtkWidget *ee;
	iCalObject *ico;
	
	fullday = GNCAL_FULL_DAY (data);

	ico = ical_new ("", user_name, "");
	ico->new = 1;
	
	gncal_full_day_selection_range (fullday, &ico->dtstart, &ico->dtend);
	ee = event_editor_new (fullday->calendar, ico);
	gtk_widget_show (ee);
}

static void
edit_appointment (GtkWidget *widget, gpointer data)
{
	Child *child;
	GtkWidget *ee;

	child = data;

	ee = event_editor_new (GNCAL_FULL_DAY (child->widget->parent)->calendar, child->ico);
	gtk_widget_show (ee);
}

static void
delete_occurance (GtkWidget *widget, gpointer data)
{
	Child *child;
	iCalObject *ical;
	time_t *t;

	child = data;
	ical = child->ico;
	t = g_new(time_t, 1);
	*t = child->start;
	ical->exdate = g_list_prepend(ical->exdate, t);
	gnome_calendar_object_changed(GNCAL_FULL_DAY (child->widget->parent)->calendar, child->ico, CHANGE_DATES);
	
}

static void
delete_appointment (GtkWidget *widget, gpointer data)
{
	Child *child;
	GncalFullDay *fullday;

	child = data;

	fullday = GNCAL_FULL_DAY (child->widget->parent);

	gnome_calendar_remove_object (fullday->calendar, child->ico);
}

static void
unrecur_appointment (GtkWidget *widget, gpointer data)
{
	Child *child;
	GncalFullDay *fullday;
	iCalObject *new;

	child = data;
	fullday = GNCAL_FULL_DAY (child->widget->parent);
	
	/* New object */
	new = ical_object_duplicate (child->ico);
	g_free (new->recur);
	new->recur = 0;
	new->dtstart = child->start;
	new->dtend   = child->end;
	
	/* Duplicate, and eliminate the recurrency fields */
	ical_object_add_exdate (child->ico, child->start);
	gnome_calendar_object_changed (fullday->calendar, child->ico, CHANGE_ALL);

	gnome_calendar_add_object (fullday->calendar, new);
}

static void
child_popup_menu (GncalFullDay *fullday, Child *child, GdkEventButton *event)
{
	int sensitive, items;
	struct menu_item *context_menu;
	
	static struct menu_item child_items[] = {
		{ N_("Edit this appointment..."), (GtkSignalFunc) edit_appointment, NULL, TRUE },
		{ N_("Delete this appointment"), (GtkSignalFunc) delete_appointment, NULL, TRUE },
		{ NULL, NULL, NULL, TRUE },
		{ N_("New appointment..."), (GtkSignalFunc) new_appointment, NULL, TRUE }
	};

	static struct menu_item recur_child_items[] = {
		{ N_("Make this appointment movable"), (GtkSignalFunc) unrecur_appointment, NULL, TRUE },
		{ N_("Edit this appointment..."), (GtkSignalFunc) edit_appointment, NULL, TRUE },
		{ N_("Delete this occurance"), (GtkSignalFunc) delete_occurance, NULL, TRUE },
		{ N_("Delete all occurances"), (GtkSignalFunc) delete_appointment, NULL, TRUE },
		{ NULL, NULL, NULL, TRUE },
		{ N_("New appointment..."), (GtkSignalFunc) new_appointment, NULL, TRUE }
	};

	sensitive = (child->ico->user_data == NULL);

	if (child->ico->recur){
		items = 6;
		context_menu = &recur_child_items[0];
		context_menu[2].data = child;
		context_menu[3].data = child;
		context_menu[4].data = fullday;
		context_menu[3].sensitive = sensitive;
	} else {
		items = 4;
		context_menu = &child_items[0];
		context_menu[3].data = fullday;
	}
	/* These settings are common for each context sensitive menu */
	context_menu[0].data = child;
	context_menu[1].data = child;
	context_menu[0].sensitive = sensitive;
	context_menu[1].sensitive = sensitive;
	context_menu[2].sensitive = sensitive;

	popup_menu (context_menu, items, event);
}

static void
child_realized_setup (GtkWidget *widget, gpointer data)
{
	Child *child;
	GncalFullDay *fullday;

	child = data;
	fullday = GNCAL_FULL_DAY (widget->parent);

	gdk_window_set_cursor (widget->window, fullday->beam_cursor);
}

static void
child_set_pos (GncalFullDay *fullday, Child *child, int x, int y, int width, int height)
{
	const int decor_width = child->decor_width;

	child->x = x;
	child->y = y;
	child->width = width;
	child->height = height;

	if (!child->window) /* realized? */
		return;

	child_set_text_pos (child);
	gdk_window_move_resize (child->window, x, y, width - decor_width, height);

	if (decor_width){
		gdk_window_move_resize (child->decor_window, x + width - decor_width, y,
					decor_width, child->decor_height);
	}
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
child_set_size (Child *child)
{
	int row_height;
	int x, y, width, height;
	GncalFullDay *fullday;

	fullday = GNCAL_FULL_DAY (child->widget->parent);

	row_height = calc_row_height (fullday);

	x = child->x;
	y = child->lower_row * row_height + GTK_WIDGET (fullday)->style->klass->ythickness;
	width = child->width;
	height = child->rows_used * row_height;

	if (GTK_WIDGET_HAS_FOCUS (child->widget) && !child->ico->recur) {
		y -= HANDLE_SIZE;
		height += 2 * HANDLE_SIZE;
	}

	child_set_pos (fullday, child, x, y, width, height);
}

static gint
child_focus_in (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	child_set_size (data);
	return FALSE;
}

static gint
child_focus_out (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	Child *child;
	GncalFullDay *fullday;
	char *text;

	child = data;

	child_set_size (child);

	/* Update summary in calendar object */

	text = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);
	if (child->ico->summary && strcmp (text, child->ico->summary) == 0)
		return FALSE;

	if (child->ico->summary)
		g_free (child->ico->summary);

	child->ico->summary = text;

	/* Notify calendar of change */

	fullday = GNCAL_FULL_DAY (widget->parent);

	gnome_calendar_object_changed (fullday->calendar, child->ico, CHANGE_SUMMARY);

	return FALSE;
}

static gint
child_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval != GDK_Escape)
		return FALSE;

	/* If user pressed Esc, un-focus the child by focusing the fullday widget */

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "key_press_event");
	gtk_widget_grab_focus (widget->parent);

	return FALSE;
}

static gint
child_button_press (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	Child *child;
	GncalFullDay *fullday;

	if (event->button != 3)
		return FALSE;

	child = data;
	fullday = GNCAL_FULL_DAY (widget->parent);

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "button_press_event");
	gtk_widget_grab_focus (widget);
	child_popup_menu (fullday, child, event);

	return TRUE;
}

/*
 * compute the space required to display the decorations 
 */
static void
child_compute_decor (Child *child)
{
	iCalObject *ico = child->ico;
	int rows_used;

	child->items = 0;
	rows_used = (child->rows_used < 1) ? 1 : child->rows_used;
	if (ico->recur)
		child->items++;
	if (ico->dalarm.enabled || ico->aalarm.enabled || ico->palarm.enabled || ico->malarm.enabled)
		child->items++;

	if (child->items > rows_used){
		child->decor_width  = DECOR_WIDTH * 2;
		child->decor_height = DECOR_HEIGHT;
	} else {
		child->decor_width  = DECOR_WIDTH * (child->items ? 1 : 0);
		child->decor_height = DECOR_HEIGHT * child->items;
	}
}

static Child *
child_new (GncalFullDay *fullday, time_t start, time_t end, iCalObject *ico)
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
	child->start = start;
	child->end = end;
	child_range_changed (fullday, child);
	child_compute_decor (child);

	if (ico->summary)
		gtk_text_insert (GTK_TEXT (child->widget), NULL, NULL, NULL,
				 ico->summary,
				 strlen (ico->summary));

	/* We set the i-beam cursor of the text widget upon realization */

	gtk_signal_connect (GTK_OBJECT (child->widget), "realize",
			    (GtkSignalFunc) child_realized_setup,
			    child);

	gtk_signal_connect_after (GTK_OBJECT (child->widget), "focus_in_event",
				  (GtkSignalFunc) child_focus_in,
				  child);

	child->focus_out_id = gtk_signal_connect_after (GTK_OBJECT (child->widget), "focus_out_event",
							(GtkSignalFunc) child_focus_out,
							child);

	gtk_signal_connect (GTK_OBJECT (child->widget), "key_press_event",
			    (GtkSignalFunc) child_key_press,
			    child);

	gtk_signal_connect (GTK_OBJECT (child->widget), "button_press_event",
			    (GtkSignalFunc) child_button_press,
			    child);

	/* Finish setup */

	gtk_text_set_editable (GTK_TEXT (child->widget), TRUE);
	gtk_text_set_word_wrap (GTK_TEXT (child->widget), TRUE);

	gtk_widget_set_parent (child->widget, GTK_WIDGET (fullday));

	return child;
}

static void
squick (GtkWidget *widget, gpointer data)
{
	printf ("destroyed!\n");
}

static void
child_destroy (GncalFullDay *fullday, Child *child)
{
	/* Disconnect the focus_out_event signal since we will get such an event
	 * from the destroy call.
	 */
	gtk_signal_disconnect (GTK_OBJECT (child->widget), child->focus_out_id);

	if (GTK_WIDGET_MAPPED (fullday))
		child_unmap (fullday, child);

	if (GTK_WIDGET_REALIZED (fullday))
		child_unrealize (fullday, child);

	/* Unparent the child widget manually as we don't have a remove method */

	gtk_signal_connect (GTK_OBJECT (child->widget), "destroy",
			    (GtkSignalFunc) squick,
			    NULL);

	gtk_widget_unparent (child->widget);
	g_free (child);
}

static int
calc_labels_width (GncalFullDay *fullday)
{
	struct tm cur, upper;
	time_t tim, time_upper;
	int width, max_w;
	char buf[40];

	get_tm_range (fullday, fullday->lower, fullday->upper, &cur, &upper, NULL, NULL);

	max_w = 0;

	tim = mktime (&cur);
	time_upper = mktime (&upper);

	while (tim < time_upper) {
		if (am_pm_flag)
			strftime (buf, sizeof (buf), "%I:%M%p", &cur);
		else
			strftime (buf, sizeof (buf), "%H:%M", &cur);
			

		width = gdk_string_width (GTK_WIDGET (fullday)->style->font, buf);

		if (width > max_w)
			max_w = width;

		cur.tm_min += fullday->interval;
		tim = mktime (&cur);
	}

	return max_w;
}

/* Used with layout_events(), takes in a list element and returns the start and end times for the
 * event corresponding to that element.
 */
static void
child_layout_query_func (GList *event, time_t *start, time_t *end)
{
	Child *child;

	child = event->data;

	*start = child->start;
	*end = child->end;
}

/* Takes the list of children in the full day view and lays them out nicely without overlapping.
 * Basically it calls the layout_events() function in layout.c and resizes the fullday's children.
 */
static void
layout_children (GncalFullDay *fullday)
{
	GtkWidget *widget;
	GList *children;
	Child *child;
	int num_slots;
	int *allocations;
	int *slots;
	int left_x;
	int usable_pixels, pixels_per_col, extra_pixels;
	int i;

	if (!fullday->children)
		return;

	layout_events (fullday->children, child_layout_query_func, &num_slots, &allocations, &slots);

	/* Set the size and position of each child */

	widget = GTK_WIDGET (fullday);
	left_x = 2 * (widget->style->klass->xthickness + TEXT_BORDER) + calc_labels_width (fullday);

	usable_pixels = widget->allocation.width - left_x - widget->style->klass->xthickness;
	pixels_per_col = usable_pixels / num_slots;
	extra_pixels = usable_pixels % num_slots;

	for (children = fullday->children, i = 0; children; children = children->next, i++) {
		child = children->data;

		child->x = left_x + pixels_per_col * allocations[i];
		child->width = pixels_per_col * slots[i];

		if ((allocations[i] + slots[i]) == num_slots)
			child->width += extra_pixels;

		child_set_size (child);
	}

	g_free (allocations);
	g_free (slots);
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

	container_class->forall = gncal_full_day_forall;

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
	fullday->recur_gc = NULL;
	fullday->bell_gc = NULL;
}

/* Destroys all the children in the full day widget */
static void
destroy_children (GncalFullDay *fullday)
{
	GList *children;

	for (children = fullday->children; children; children = children->next)
		child_destroy (fullday, children->data);

	g_list_free (fullday->children);
	fullday->children = NULL;
}

static void
gncal_full_day_destroy (GtkObject *object)
{
	GncalFullDay *fullday;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (object));

	fullday = GNCAL_FULL_DAY (object);

	destroy_children (fullday);

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
	fullday->beam_cursor    = gdk_cursor_new (GDK_XTERM);

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

	if (fullday->bell_gc)
		gdk_gc_destroy (fullday->bell_gc);
	if (fullday->recur_gc)
		gdk_gc_destroy (fullday->recur_gc);

	if (pixmap_bell){
		gdk_pixmap_unref (pixmap_bell);
		pixmap_bell = NULL;
	}
	
	if (pixmap_recur){
		gdk_pixmap_unref (pixmap_recur);
		pixmap_recur = NULL;
	}
		
	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

struct paint_info {
	GtkWidget *widget;
	struct drag_info *di;
	GdkRectangle *area;
	int x1, y1, width, height;
	int labels_width;
	int row_height;
	struct tm start_tm;
};

static void
paint_row (GncalFullDay *fullday, int row, struct paint_info *p)
{
	GdkRectangle rect, dest;
	GdkGC *left_gc, *right_gc, *text_gc;
	int begin_row, end_row;
	struct tm tm;
	char buf[40];

	begin_row = (day_begin * 60) / fullday->interval;
	end_row = (day_end * 60) / fullday->interval;

	/* See which GCs we will use */

	if ((p->di->sel_rows_used != 0)
	    && (row >= p->di->sel_start_row)
	    && (row < (p->di->sel_start_row + p->di->sel_rows_used))) {
		left_gc  = p->widget->style->bg_gc[GTK_STATE_SELECTED];
		right_gc = left_gc;
		text_gc  = p->widget->style->fg_gc[GTK_STATE_SELECTED];
	} else if ((row < begin_row) || (row >= end_row)) {
		left_gc  = p->widget->style->bg_gc[GTK_STATE_NORMAL];
		right_gc = p->widget->style->bg_gc[GTK_STATE_ACTIVE];
		text_gc  = p->widget->style->fg_gc[GTK_STATE_NORMAL];
	} else {
		left_gc  = p->widget->style->bg_gc[GTK_STATE_NORMAL];
		right_gc = p->widget->style->bg_gc[GTK_STATE_PRELIGHT];
		text_gc  = p->widget->style->fg_gc[GTK_STATE_NORMAL];
	}

	/* Left background and text */

	rect.x = p->x1;
	rect.y = p->y1 + row * p->row_height;
	rect.width = 2 * TEXT_BORDER + p->labels_width;
	rect.height = p->row_height - 1;

	if (gdk_rectangle_intersect (&rect, p->area, &dest)) {
		gdk_draw_rectangle (p->widget->window,
				    left_gc,
				    TRUE,
				    dest.x, dest.y,
				    dest.width, dest.height);

		tm = p->start_tm;
		tm.tm_min += row * fullday->interval;
		mktime (&tm);

		if (am_pm_flag)
			strftime (buf, sizeof (buf), "%I:%M%p", &tm);
		else
			strftime (buf, sizeof (buf), "%H:%M", &tm);

		gdk_draw_string (p->widget->window,
				 p->widget->style->font,
				 text_gc,
				 rect.x + TEXT_BORDER,
				 rect.y + TEXT_BORDER + p->widget->style->font->ascent,
				 buf);
	}

	/* Right background */

	rect.x += rect.width + p->widget->style->klass->xthickness;
	rect.width = p->width - (rect.x - p->x1);

	if (gdk_rectangle_intersect (&rect, p->area, &dest))
		gdk_draw_rectangle (p->widget->window,
				    right_gc,
				    TRUE,
				    dest.x, dest.y,
				    dest.width, dest.height);

	/* Horizontal division at bottom of row */

	rect.x = p->x1;
	rect.y += rect.height;
	rect.width = p->width;
	rect.height = 1;

	if (gdk_rectangle_intersect (&rect, p->area, &dest))
		gdk_draw_line (p->widget->window,
			       p->widget->style->black_gc,
			       rect.x, rect.y,
			       rect.x + rect.width - 1, rect.y);
}

static void
paint_back (GncalFullDay *fullday, GdkRectangle *area)
{
	struct paint_info p;
	int start_row, end_row;
	int i;
	GdkRectangle rect, dest, aarea;
	int f_rows;
	int draw_focus;

	p.widget = GTK_WIDGET (fullday);
	p.di = fullday->drag_info;

	if (!area) {
		area = &aarea;

		area->x = 0;
		area->y = 0;
		area->width = p.widget->allocation.width;
		area->height = p.widget->allocation.height;
	}
	p.area = area;

	p.x1 = p.widget->style->klass->xthickness;
	p.y1 = p.widget->style->klass->ythickness;
	p.width = p.widget->allocation.width - 2 * p.x1;
	p.height = p.widget->allocation.height - 2 * p.y1;

	p.labels_width = calc_labels_width (fullday);
	p.row_height = calc_row_height (fullday);
	get_tm_range (fullday, fullday->lower, fullday->upper, &p.start_tm, NULL, NULL, &f_rows);

	/* Frame shadow */

	rect.x = 0;
	rect.y = 0;
	rect.width = p.widget->allocation.width;
	rect.height = p.widget->style->klass->ythickness;
	
	draw_focus = gdk_rectangle_intersect (&rect, area, &dest);

	if (!draw_focus) {
		rect.y = p.widget->allocation.height - rect.height;

		draw_focus = gdk_rectangle_intersect (&rect, area, &dest);
	}

	if (!draw_focus) {
		rect.y = p.widget->style->klass->ythickness;
		rect.width = p.widget->style->klass->xthickness;
		rect.height = p.widget->allocation.height - 2 * rect.y;

		draw_focus = gdk_rectangle_intersect (&rect, area, &dest);
	}

	if (!draw_focus) {
		rect.x = p.widget->allocation.width - rect.width;

		draw_focus = gdk_rectangle_intersect (&rect, area, &dest);
	}

	if (draw_focus)
		gtk_widget_draw_focus (p.widget);

	/* Rows */

	start_row = (area->y - p.y1) / p.row_height;
	end_row = (area->y + area->height - 1 - p.y1) / p.row_height;

	if (end_row >= f_rows)
		end_row = f_rows - 1;

	for (i = start_row; i <= end_row; i++)
		paint_row (fullday, i, &p);

	/* Slack area at bottom of widget */

	rect.x = p.x1;
	rect.y = p.y1 + f_rows * p.row_height;
	rect.width = p.width;
	rect.height = p.height - (rect.y - p.y1);

	if (gdk_rectangle_intersect (&rect, area, &dest))
		gdk_draw_rectangle (p.widget->window,
				    p.widget->style->bg_gc[GTK_STATE_NORMAL],
				    TRUE,
				    dest.x, dest.y,
				    dest.width, dest.height);

	/* Vertical division */

	rect.x = p.x1 + 2 * TEXT_BORDER + p.labels_width;
	rect.y = p.y1;
	rect.width = p.widget->style->klass->xthickness;
	rect.height = p.height;

	if (gdk_rectangle_intersect (&rect, area, &dest))
		gtk_draw_vline (p.widget->style, p.widget->window,
				GTK_STATE_NORMAL,
				rect.y,
				rect.y + rect.height - 1,
				rect.x);
}

static void
paint_back_rows (GncalFullDay *fullday, int start_row, int rows_used)
{
	int row_height;
	int xthickness, ythickness;
	GtkWidget *widget;
	GdkRectangle area;

	widget = GTK_WIDGET (fullday);

	row_height = calc_row_height (fullday);

	xthickness = widget->style->klass->xthickness;
	ythickness = widget->style->klass->ythickness;

	area.x = xthickness;
	area.y = ythickness + start_row * row_height;
	area.width = widget->allocation.width - 2 * xthickness;
	area.height = rows_used * row_height;

	paint_back (fullday, &area);
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
			child_draw (fullday, child, NULL, NULL, TRUE);
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
find_child_by_window (GncalFullDay *fullday, GdkWindow *window, int *on_text)
{
	GList *children;
	Child *child;
	GtkWidget *owner;

	*on_text = FALSE;

	gdk_window_get_user_data (window, (gpointer *) &owner);

	for (children = fullday->children; children; children = children->next) {
		child = children->data;

		if (child->window == window || child->decor_window == window)
			return child;

		if (child->widget == owner) {
			*on_text = TRUE;
			return child;
		}
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
				    di->child_start_row * row_height + ythickness + i,
				    di->child->width - 2 * i - 1,
				    di->child_rows_used * row_height - 2 - 2 * i);

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

static int
button_1 (GncalFullDay *fullday, GdkEventButton *event)
{
	GtkWidget *widget;
	Child *child;
	int on_text;
	struct drag_info *di;
	gint y;
	int row_height;
	int has_focus;
	int old_start_row, old_rows_used;
	int old_max;
	int paint_start_row, paint_rows_used;

	widget = GTK_WIDGET (fullday);

	if (event->window == widget->window) {
		/* Clicked on main window */

		if (!GTK_WIDGET_HAS_FOCUS (widget))
			gtk_widget_grab_focus (widget);

		/* Prepare for drag */

		di = fullday->drag_info;

		di->drag_mode = DRAG_SELECT;

		old_start_row = di->sel_start_row;
		old_rows_used = di->sel_rows_used;

		di->sel_click_row = get_row_from_y (fullday, event->y, FALSE);
		di->sel_start_row = di->sel_click_row;
		di->sel_rows_used = 1;

		di->click_time = event->time;

		gdk_pointer_grab (widget->window, FALSE,
				  (GDK_BUTTON_MOTION_MASK
				   | GDK_POINTER_MOTION_HINT_MASK
				   | GDK_BUTTON_RELEASE_MASK),
				  NULL,
				  fullday->up_down_cursor,
				  event->time);

		if (old_rows_used == 0) {
			paint_start_row = di->sel_start_row;
			paint_rows_used = di->sel_rows_used;
		} else {
			paint_start_row = MIN (old_start_row, di->sel_start_row);
			old_max = old_start_row + old_rows_used - 1;
			paint_rows_used = MAX (old_max, di->sel_start_row) - paint_start_row + 1;
		}

		paint_back_rows (fullday, paint_start_row, paint_rows_used);

		return TRUE;
	} else {
		/* Clicked on a child? */

		child = find_child_by_window (fullday, event->window, &on_text);

		if (!child || on_text || child->ico->recur)
			return FALSE;

		/* Prepare for drag */

		di = fullday->drag_info;

		gtk_widget_get_pointer (widget, NULL, &y);

		has_focus = GTK_WIDGET_HAS_FOCUS (child->widget);

		if (has_focus) {
			if (event->y < HANDLE_SIZE)
				di->drag_mode = DRAG_SIZE_TOP;
			else if (event->y >= (child->height - HANDLE_SIZE))
				di->drag_mode = DRAG_SIZE_BOTTOM;
			else
				di->drag_mode = DRAG_MOVE;
		} else
			di->drag_mode = DRAG_MOVE;

		row_height = calc_row_height (fullday);

		di->child = child;

		di->child_click_y = event->y;
		di->child_start_row = child->lower_row;
		di->child_rows_used = child->rows_used;

		gdk_pointer_grab (child->window, FALSE,
				  (GDK_BUTTON_MOTION_MASK
				   | GDK_POINTER_MOTION_HINT_MASK
				   | GDK_BUTTON_RELEASE_MASK),
				  NULL,
				  fullday->up_down_cursor,
				  event->time);

		draw_xor_rect (fullday);

		return TRUE;
	}

	return FALSE;
}

static int
button_3 (GncalFullDay *fullday, GdkEventButton *event)
{
	static struct menu_item main_items[] = {
		{ N_("New appointment..."), (GtkSignalFunc) new_appointment, NULL, TRUE }
	};

	GtkWidget *widget;
	Child *child;
	int on_text;

	widget = GTK_WIDGET (fullday);

	if (event->window == widget->window) {
		/* Clicked on main window */

		if (!GTK_WIDGET_HAS_FOCUS (widget))
			gtk_widget_grab_focus (widget);

		main_items[0].data = fullday;

		popup_menu (main_items, sizeof (main_items) / sizeof (main_items[0]), event);

		return TRUE;
	} else {
		child = find_child_by_window (fullday, event->window, &on_text);

		if (!child || on_text)
			return FALSE;

		child_popup_menu (fullday, child, event);

		return TRUE;
	}

	return FALSE;
}

static gint
gncal_full_day_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GncalFullDay *fullday;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	fullday = GNCAL_FULL_DAY (widget);

	switch (event->button) {
	case 1:
		return button_1 (fullday, event);

	case 3:
		return button_3 (fullday, event);

	default:
		break;
	}

	return FALSE;
}

static void
recompute_motion (GncalFullDay *fullday, int y)
{
	struct drag_info *di;
	int f_rows;
	int row;
	int has_focus;

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
		has_focus = GTK_WIDGET_HAS_FOCUS (di->child->widget);

		row = get_row_from_y (fullday, y - di->child_click_y + (has_focus ? HANDLE_SIZE : 0), TRUE);

		if (row > (f_rows - di->child_rows_used))
			row = f_rows - di->child_rows_used;

		di->child_start_row = row;

		break;

	case DRAG_SIZE_TOP:
		row = get_row_from_y (fullday, y + HANDLE_SIZE, TRUE);

		if (row > (di->child_start_row + di->child_rows_used - 1))
			row = di->child_start_row + di->child_rows_used - 1;

		di->child_rows_used = (di->child_start_row + di->child_rows_used) - row;
		di->child_start_row = row;

		break;

	case DRAG_SIZE_BOTTOM:
		row = get_row_from_y (fullday, y - HANDLE_SIZE, TRUE);

		if (row <= di->child_start_row)
			row = di->child_start_row + 1;
		else if (row > f_rows)
			row = f_rows;

		di->child_rows_used = row - di->child_start_row;

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

	get_time_from_rows (fullday, di->child_start_row, di->child_rows_used,
			    &di->child->ico->dtstart,
			    &di->child->ico->dtend);

	child_range_changed (fullday, di->child);

	/* Notify calendar of change */

	gnome_calendar_object_changed (fullday->calendar, di->child->ico, CHANGE_DATES);
}

static gint
gncal_full_day_button_release (GtkWidget *widget, GdkEventButton *event)
{
	GncalFullDay *fullday;
	struct drag_info *di;
	gint y;
	int retval;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	fullday = GNCAL_FULL_DAY (widget);

	di = fullday->drag_info;

	gtk_widget_get_pointer (widget, NULL, &y);

	retval = FALSE;

	switch (di->drag_mode) {
	case DRAG_NONE:
		break;

	case DRAG_SELECT:
	    if ((event->time - di->click_time) < UNSELECT_TIMEOUT)
		    di->sel_rows_used = 0;
	    else
		    recompute_motion (fullday, y);

		gdk_pointer_ungrab (event->time);

		paint_back_rows (fullday, di->sel_start_row, MAX (di->sel_rows_used, 1));

		retval = TRUE;
		break;

	case DRAG_MOVE:
	case DRAG_SIZE_TOP:
	case DRAG_SIZE_BOTTOM:
		draw_xor_rect (fullday);
		recompute_motion (fullday, y);
		gdk_pointer_ungrab (event->time);

		update_from_drag_info (fullday);

		di->child_rows_used = 0;

		retval = TRUE;
		break;

	default:
		g_assert_not_reached ();
	}

	di->drag_mode = DRAG_NONE;
	di->child = NULL;

	return retval;
}

static gint
gncal_full_day_motion (GtkWidget *widget, GdkEventMotion *event)
{
	GncalFullDay *fullday;
	struct drag_info *di;
	gint y;
	int old_min, old_max;
	int new_min, new_max;
	int new_start_row, new_rows_used;

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
		old_min = di->sel_start_row;
		old_max = di->sel_start_row + di->sel_rows_used - 1;

		recompute_motion (fullday, y);

		new_min = di->sel_start_row;
		new_max = di->sel_start_row + di->sel_rows_used - 1;

		new_start_row = MIN (old_min, new_min);
		new_rows_used = MAX (old_max, new_max) - new_start_row + 1;

		paint_back_rows (fullday, new_start_row, new_rows_used);

		return TRUE;

	case DRAG_MOVE:
	case DRAG_SIZE_TOP:
	case DRAG_SIZE_BOTTOM:
		draw_xor_rect (fullday);
		recompute_motion (fullday, y);
		draw_xor_rect (fullday);

		return TRUE;

	default:
		g_assert_not_reached ();
	}

	return FALSE;
}

static gint
gncal_full_day_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GncalFullDay *fullday;
	Child *child;
	int on_text;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (!GTK_WIDGET_DRAWABLE (widget))
		return FALSE;

	fullday = GNCAL_FULL_DAY (widget);

	if (event->window == widget->window)
		paint_back (fullday, &event->area);
	else {
		child = find_child_by_window (fullday, event->window, &on_text);

		if (child && !on_text)
			child_draw (fullday, child, &event->area, event->window, FALSE);
	}

	return FALSE;
}

static gint
gncal_full_day_key_press (GtkWidget *widget, GdkEventKey *event)
{
	GncalFullDay *fullday;
	struct drag_info *di;
	GList *children;
	Child *child;
	gint pos;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	fullday = GNCAL_FULL_DAY (widget);

	di = fullday->drag_info;

	if (di->sel_rows_used == 0)
		return FALSE;

	if (event->keyval == GDK_Return) {
		gtk_signal_emit (GTK_OBJECT (fullday), fullday_signals[RANGE_ACTIVATED]);
		return TRUE;
	}

	/*
	 * If a non-printable key was pressed, bail.  Otherwise, begin
	 * editing the appointment.
	 */
	if ((event->keyval < 0x20) || (event->keyval > 0xFF)
	    || (event->length == 0) || (event->state & GDK_CONTROL_MASK)
	    || (event->state & GDK_MOD1_MASK))
		return FALSE;

	gtk_signal_emit (GTK_OBJECT (fullday),
			 fullday_signals[RANGE_ACTIVATED]);
	
	/*
	 * Find the new child, which should hopefully be focused, and
	 * insert the keypress.
	 */
	for (children = fullday->children; children; children = children->next) {
		child = children->data;

		if (GTK_WIDGET_HAS_FOCUS (child->widget)) {
			pos = gtk_text_get_length (GTK_TEXT (child->widget));

			gtk_editable_insert_text (GTK_EDITABLE (child->widget),
						  event->string,
						  event->length,
						  &pos);

			return TRUE;
		}
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
gncal_full_day_forall (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data)
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

static gint
child_compare (gconstpointer a, gconstpointer b)
{
	const Child *ca = a;
	const Child *cb = b;
	time_t diff;
	
	diff = ca->start - cb->start;

	if (diff == 0)
		diff = cb->end - ca->end;

	return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
}

static int
fullday_add_children (iCalObject *obj, time_t start, time_t end, void *c)
{
	GncalFullDay *fullday = c;
	Child *child;
	
	child = child_new (fullday, start, end, obj);
	fullday->children = g_list_insert_sorted (fullday->children, child, child_compare);

	return 1;
}

void
gncal_full_day_update (GncalFullDay *fullday, iCalObject *ico, int flags)
{
	GList *children;
	Child *child;
	
	g_return_if_fail (fullday != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (fullday));

	if (!fullday->calendar->cal)
		return;

	/* Try to find child that changed */

	for (children = fullday->children; children; children = children->next) {
		child = children->data;

		if (child->ico == ico)
			break;
	}

	/* If child was found and nothing but the summary changed, we can just paint the child and return */

	if (children && !(flags & ~CHANGE_SUMMARY)) {
		child_draw (fullday, child, NULL, NULL, TRUE);
		return;
	}

	/* We have to regenerate and layout our list of children */

	destroy_children (fullday);

	calendar_iterate (fullday->calendar->cal,
			  fullday->lower,
			  fullday->upper,
			  fullday_add_children,
			  fullday);

	layout_children (fullday);

	/* Realize and map children */

	for (children = fullday->children; children; children = children->next) {
		if (GTK_WIDGET_REALIZED (fullday))
			child_realize (fullday, children->data);

		if (GTK_WIDGET_MAPPED (fullday))
			child_map (fullday, children->data);
	}

	gtk_widget_draw (GTK_WIDGET (fullday), NULL);
}

void
gncal_full_day_set_bounds (GncalFullDay *fullday, time_t lower, time_t upper)
{
	struct drag_info *di;

	g_return_if_fail (fullday != NULL);
	g_return_if_fail (GNCAL_IS_FULL_DAY (fullday));

	if ((lower != fullday->lower) || (upper != fullday->upper)) {
		fullday->lower = lower;
		fullday->upper = upper;

		di = fullday->drag_info;

		di->sel_rows_used = 0; /* clear selection */

		gncal_full_day_update (fullday, NULL, 0);
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

	if (di->sel_rows_used == 0){
		time_t now = time (NULL);
		struct tm tm    = *localtime (&now);
		struct tm thisd = *localtime (&fullday->lower);
		
		thisd.tm_hour = tm.tm_hour;
		thisd.tm_min  = tm.tm_min;
		thisd.tm_sec  = 0;
		*lower = mktime (&thisd);
		thisd.tm_hour++;
		*upper = mktime (&thisd);
		return FALSE;
	}

	get_time_from_rows (fullday, di->sel_start_row, di->sel_rows_used, &alower, &aupper);

	if (lower)
		*lower = alower;

	if (upper)
		*upper= aupper;

	return TRUE;
}

void
gncal_full_day_focus_child (GncalFullDay *fullday, iCalObject *ico)
{
	GList *children;
	Child *child;

	g_return_if_fail (fullday != NULL);
	g_return_if_fail (ico != NULL);

	for (children = fullday->children; children; children = children->next) {
		child = children->data;

		if (child->ico == ico) {
			gtk_widget_grab_focus (child->widget);
			break;
		}
	}
}

int
gncal_full_day_get_day_start_yoffset (GncalFullDay *fullday)
{
	GtkWidget *widget;
	int begin_row;

	g_return_val_if_fail (fullday != NULL, 0);
	g_return_val_if_fail (GNCAL_IS_FULL_DAY (fullday), 0);

	widget = GTK_WIDGET (fullday);

	begin_row = (day_begin * 60) / fullday->interval;

	return widget->style->klass->ythickness + begin_row * calc_row_height (fullday);
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
