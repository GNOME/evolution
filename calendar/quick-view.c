/* Quick view widget for Gnomecal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx
 */

#include <config.h>
#include "quick-view.h"
#include "main.h"


static void quick_view_class_init (QuickViewClass *class);
static void quick_view_init       (QuickView      *qv);


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
}

static void
quick_view_init (QuickView *qv)
{
	GTK_WINDOW (qv)->type = GTK_WINDOW_POPUP;
	gtk_window_position (GTK_WINDOW (qv), GTK_WIN_POS_MOUSE);
}

/* Handles button release events from the canvas in the quick view.  When a button release is
 * received, it pops down the quick view and calls gtk_main_quit().
 */
static gint
button_release (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	QuickView *qv;

	qv = data;

	if (event->button != qv->button)
		return FALSE;

	gdk_pointer_ungrab (event->time);
	gtk_grab_remove (GTK_WIDGET (qv));
	gtk_widget_hide (GTK_WIDGET (qv));

	gtk_main_quit (); /* End modality */
	return TRUE;
}


/* Creates the items corresponding to a single calendar object.  Takes in the y position of the
 * items to create and returns the y position of the next item to create.
 */
double
create_items_for_event (QuickView *qv, CalendarObject *co, double y)
{
	GnomeCanvas *canvas;
	char start[100], end[100];
	struct tm start_tm, end_tm;
	char *str;

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

	str = g_copy_strings (start, " - ", end, " ", co->ico->summary, NULL);

	gnome_canvas_item_new (gnome_canvas_root (canvas),
			       gnome_canvas_text_get_type (),
			       "x", 0.0,
			       "y", y,
			       "anchor", GTK_ANCHOR_NW,
			       "text", str,
			       NULL);

	g_free (str);

	return (y + 16); /* FIXME */
}

/* Creates the canvas items corresponding to the events in the list */
static void
setup_event_list (QuickView *qv, GList *event_list)
{
	CalendarObject *co;
	double y;

	/* If there are no events, then just put a simple label */

	if (!event_list) {
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (qv->canvas)),
				       gnome_canvas_text_get_type (),
				       "x", 0.0,
				       "y", 0.0,
				       "anchor", GTK_ANCHOR_NW,
				       "text", _("No appointments scheduled for this day"),
				       NULL);
		return;
	}

	/* Create the items for all the events in the list */

	y = 0.0;

	for (; event_list; event_list = event_list->next) {
		co = event_list->data;
		y = create_items_for_event (qv, co, y);
	}

	/* Set the scrolling region to fit all the items */

	gnome_canvas_set_scroll_region (GNOME_CANVAS (qv->canvas),
					0.0, 0.0,
					300.0, y); /* FIXME: figure out reasonable sizes */

	gnome_canvas_set_size (GNOME_CANVAS (qv->canvas), 300, y);
}

GtkWidget *
quick_view_new (GnomeCalendar *calendar, char *title, GList *event_list)
{
	QuickView *qv;
	GtkWidget *w;

	g_return_val_if_fail (calendar != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (calendar), NULL);

	qv = gtk_type_new (quick_view_get_type ());
	qv->calendar = calendar;

	/* Create base widgets for the popup window */

	w = gtk_frame_new (title);
	gtk_container_add (GTK_CONTAINER (qv), w);
	gtk_widget_show (w);

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());

	qv->canvas = gnome_canvas_new ();

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_signal_connect (GTK_OBJECT (qv->canvas), "button_release_event",
			    (GtkSignalFunc) button_release,
			    qv);

	gtk_container_add (GTK_CONTAINER (w), qv->canvas);
	gtk_widget_show (qv->canvas);

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

	gtk_widget_show (GTK_WIDGET (qv));
	gtk_grab_add (GTK_WIDGET (qv));

	gdk_pointer_grab (GTK_WIDGET (qv)->window,
			  TRUE,
			  GDK_BUTTON_RELEASE_MASK,
			  NULL,
			  NULL,
			  event->time);

	qv->button = event->button;

	gtk_main (); /* Begin modality */

	/* The button release event handler will call gtk_main_quit() */
}
