/* Quick view widget for Gnomecal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx
 */

#include <config.h>
#include "quick-view.h"
#include "main.h"


#define QUICK_VIEW_FONTSET "-adobe-helvetica-medium-r-normal--10-*-*-*-p-*-*-*,-cronyx-helvetica-medium-r-normal-*-11-*-*-*-p-*-koi8-r,-*-*-medium-r-normal--10-*-*-*-*-*-ksc5601.1987-0,*"


static void quick_view_class_init (QuickViewClass *class);
static void quick_view_init       (QuickView      *qv);

static gint quick_view_button_release (GtkWidget *widget, GdkEventButton *event);
static gint quick_view_map_event (GtkWidget *widget, GdkEventAny *event);


static GtkWindowClass *parent_class;


GtkType
quick_view_get_type (void)
{
	static GtkType quick_view_type = 0;

	if (!quick_view_type) {
		GtkTypeInfo quick_view_info = {
			"QuickView",
			sizeof (QuickView),
			sizeof (QuickViewClass),
			(GtkClassInitFunc) quick_view_class_init,
			(GtkObjectInitFunc) quick_view_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		quick_view_type = gtk_type_unique (gtk_window_get_type (), &quick_view_info);
	}

	return quick_view_type;
}

static void
quick_view_class_init (QuickViewClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gtk_window_get_type ());

	widget_class->button_release_event = quick_view_button_release;
	widget_class->map_event = quick_view_map_event;
}

static void
quick_view_init (QuickView *qv)
{
	GTK_WINDOW (qv)->type = GTK_WINDOW_POPUP;
	gtk_window_set_position (GTK_WINDOW (qv), GTK_WIN_POS_MOUSE);
}

static gint
quick_view_button_release (GtkWidget *widget, GdkEventButton *event)
{
	QuickView *qv;

	qv = QUICK_VIEW (widget);

	if (event->button != qv->button)
		return FALSE;

	gdk_pointer_ungrab (event->time);
	gtk_grab_remove (GTK_WIDGET (qv));
	gtk_widget_hide (GTK_WIDGET (qv));

	gtk_main_quit (); /* End modality of the quick view */
	return TRUE;
}

static gint
quick_view_map_event (GtkWidget *widget, GdkEventAny *event)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new (GDK_ARROW);
	gdk_pointer_grab (widget->window,
			  TRUE,
			  GDK_BUTTON_RELEASE_MASK,
			  NULL,
			  cursor,
			  GDK_CURRENT_TIME);
	gdk_cursor_destroy (cursor);

	return FALSE;
}

/* Creates the items corresponding to a single calendar object.  Takes in the y position of the
 * items to create and returns the y position of the next item to create.  Also takes in the current
 * maximum width for items and returns the new maximum width.
 */
void
create_items_for_event (QuickView *qv, CalendarObject *co, double *y, double *max_width)
{
	GnomeCanvas *canvas;
	GnomeCanvasItem *item;
	char start[100], end[100];
	struct tm start_tm, end_tm;
	char *str;
	GtkArg args[2];

	/* FIXME: make this nice */

	canvas = GNOME_CANVAS (qv->canvas);

	start_tm = *localtime (&co->ev_start);
	end_tm = *localtime (&co->ev_end);

	if (am_pm_flag) {
		strftime (start, sizeof (start), "%I:%M%p", &start_tm);
		strftime (end, sizeof (end), "%I:%M%p", &end_tm);
	} else {
		strftime (start, sizeof (start), "%H:%M", &start_tm);
		strftime (end, sizeof (end), "%H:%M", &end_tm);
	}

	str = g_strconcat (start, " - ", end, " ", co->ico->summary, NULL);

	item = gnome_canvas_item_new (gnome_canvas_root (canvas),
				      gnome_canvas_text_get_type (),
				      "x", 0.0,
				      "y", *y,
				      "anchor", GTK_ANCHOR_NW,
				      "text", str,
				      "fontset", QUICK_VIEW_FONTSET,
				      NULL);

	g_free (str);

	/* Measure the text and return the proper size values */

	args[0].name = "text_width";
	args[1].name = "text_height";
	gtk_object_getv (GTK_OBJECT (item), 2, args);

	if (GTK_VALUE_DOUBLE (args[0]) > *max_width)
		*max_width = GTK_VALUE_DOUBLE (args[0]);

	*y += GTK_VALUE_DOUBLE (args[1]);
}

/* Creates the canvas items corresponding to the events in the list */
static void
setup_event_list (QuickView *qv, GList *event_list)
{
	CalendarObject *co;
	GnomeCanvasItem *item;
	GtkArg args[2];
	double y, max_width;

	/* If there are no events, then just put a simple label */

	if (!event_list) {
		item = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (qv->canvas)),
					      gnome_canvas_text_get_type (),
					      "x", 0.0,
					      "y", 0.0,
					      "anchor", GTK_ANCHOR_NW,
					      "text", _("No appointments for this day"),
					      "fontset", QUICK_VIEW_FONTSET,
					      NULL);

		/* Measure the text and set the proper sizes */

		args[0].name = "text_width";
		args[1].name = "text_height";
		gtk_object_getv (GTK_OBJECT (item), 2, args);

		y = GTK_VALUE_DOUBLE (args[1]);
		max_width = GTK_VALUE_DOUBLE (args[0]);
	} else {
		/* Create the items for all the events in the list */

		y = 0.0;
		max_width = 0.0;

		for (; event_list; event_list = event_list->next) {
			co = event_list->data;
			create_items_for_event (qv, co, &y, &max_width);
		}
	}

	/* Set the scrolling region to fit all the items */

	gnome_canvas_set_scroll_region (GNOME_CANVAS (qv->canvas),
					0.0, 0.0,
					max_width, y);

	gtk_widget_set_usize (qv->canvas, max_width, y);
}

GtkWidget *
quick_view_new (GnomeCalendar *calendar, char *title, GList *event_list)
{
	QuickView *qv;
	GtkWidget *vbox;
	GtkWidget *w;

	g_return_val_if_fail (calendar != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (calendar), NULL);

	qv = gtk_type_new (quick_view_get_type ());
	qv->calendar = calendar;

	/* Create base widgets for the popup window */

	w = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (w), GTK_SHADOW_ETCHED_OUT);
	gtk_container_add (GTK_CONTAINER (qv), w);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (w), vbox);

	w = gtk_label_new (title);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	w = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	w = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_container_set_border_width (GTK_CONTAINER (w), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), w, TRUE, TRUE, 0);

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());

	qv->canvas = gnome_canvas_new ();

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_container_add (GTK_CONTAINER (w), qv->canvas);

	/* Set up the event list */

	setup_event_list (qv, event_list);

	return GTK_WIDGET (qv);
}

void
quick_view_do_popup (QuickView *qv, GdkEventButton *event)
{
	g_return_if_fail (qv != NULL);
	g_return_if_fail (IS_QUICK_VIEW (qv));
	g_return_if_fail (event != NULL);

	/* Pop up the window */

	gtk_widget_show_all (GTK_WIDGET (qv));
	gtk_grab_add (GTK_WIDGET (qv));

	qv->button = event->button;

	gtk_main (); /* Begin modality */

	/* The button release event handler will call gtk_main_quit() */
}
