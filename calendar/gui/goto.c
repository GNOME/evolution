/* Go to date dialog for gnomecal
 *
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <gnome.h>
#include "gnome-cal.h"
#include "gnome-month-item.h"
#include "main.h"
#include "mark.h"
#include "cal-util/timeutil.h"


static GtkWidget *goto_win;		/* The goto dialog window */
static GnomeMonthItem *month_item;	/* The month item in the dialog */
static GnomeCalendar *gnome_calendar;	/* The gnome calendar the dialog refers to */
static int current_index;		/* The index of the day marked as current, or -1 if none */


/* Updates the specified month item by marking it appropriately from the calendar the dialog refers
 * to.  Also marks the current day if appropriate.
 */
static void
update (void)
{
	GnomeCanvasItem *item;
	time_t t;
	struct tm tm;

	unmark_month_item (month_item);
	mark_month_item (month_item, gnome_calendar);

	if (current_index != -1) {
		item = gnome_month_item_num2child (month_item,
						   GNOME_MONTH_ITEM_DAY_LABEL + current_index);
		gnome_canvas_item_set (item,
				       "fill_color", color_spec_from_prop (COLOR_PROP_DAY_FG),
				       "fontset", NORMAL_DAY_FONTSET,
				       NULL);
		current_index = -1;
	}

	t = time (NULL);
	tm = *localtime (&t);

	if (((tm.tm_year + 1900) == month_item->year) && (tm.tm_mon == month_item->month)) {
		current_index = gnome_month_item_day2index (month_item, tm.tm_mday);
		g_assert (current_index != -1);

		item = gnome_month_item_num2child (month_item,
						   GNOME_MONTH_ITEM_DAY_LABEL + current_index);
		gnome_canvas_item_set (item,
				       "fill_color", color_spec_from_prop (COLOR_PROP_CURRENT_DAY_FG),
				       "fontset", CURRENT_DAY_FONTSET,
				       NULL);
	}
}

/* Callback used when the year adjustment is changed */
static void
year_changed (GtkAdjustment *adj, gpointer data)
{
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (month_item),
			       "year", (int) adj->value,
			       NULL);
	update ();
}

/* Creates the year control with its adjustment */
static GtkWidget *
create_year (int year)
{
	GtkWidget *hbox;
	GtkAdjustment *adj;
	GtkWidget *w;

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);

	w = gtk_label_new (_("Year:"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (year, 1900, 9999, 1, 10, 10));
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    (GtkSignalFunc) year_changed,
			    NULL);

	w = gtk_spin_button_new (adj, 1.0, 0);
	gtk_widget_set_usize (w, 60, 0);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	return hbox;
}

/* Callback used when a month button is toggled */
static void
month_toggled (GtkToggleButton *toggle, gpointer data)
{
	if (!toggle->active)
		return;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (month_item),
			       "month", GPOINTER_TO_INT (data),
			       NULL);
	update ();
}

/* Creates the months control */
static GtkWidget *
create_months (int month)
{
	GtkWidget *table;
	GtkWidget *w;
	GSList *group;
	int i, row, col;
	struct tm tm;
	char buf[100];

	tm = *localtime (&gnome_calendar->current_display);

	table = gtk_table_new (2, 6, TRUE);

	group = NULL;

	for (i = 0; i < 12; i++) {
		row = i / 6;
		col = i % 6;

		tm.tm_mon = i;
		strftime (buf, 100, "%b", &tm);

		w = gtk_radio_button_new (group);
		group = gtk_radio_button_group (GTK_RADIO_BUTTON (w));
		gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (w), FALSE);

		gtk_container_add (GTK_CONTAINER (w), gtk_label_new (buf));

		if (i == month)
			gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w), TRUE);

		gtk_signal_connect (GTK_OBJECT (w), "toggled",
				    (GtkSignalFunc) month_toggled,
				    GINT_TO_POINTER (i));
		gtk_table_attach (GTK_TABLE (table), w,
				  col, col + 1,
				  row, row + 1,
				  GTK_EXPAND | GTK_FILL,
				  GTK_EXPAND | GTK_FILL,
				  0, 0);
		gtk_widget_show_all (w);
	}

	return table;
}

/* Sets the scrolling region of the canvas to the allocation size */
static void
set_scroll_region (GtkWidget *widget, GtkAllocation *allocation)
{
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (month_item),
			       "width", (double) (allocation->width - 1),
			       "height", (double) (allocation->height - 1),
			       NULL);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (widget),
					0, 0,
					allocation->width, allocation->height);
}

/* Event handler for day groups in the month item.  A button press makes the calendar jump to the
 * selected day and destroys the Go-to dialog box.
 */
static gint
day_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	int child_num, day;

	child_num = gnome_month_item_child2num (month_item, item);
	day = gnome_month_item_num2day (month_item, child_num);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if ((event->button.button == 1) && (day != 0)) {
			gnome_calendar_goto (gnome_calendar,
					     time_from_day (month_item->year, month_item->month, day));
			gtk_widget_destroy (goto_win);
		}
		break;

	default:
		break;
	}

	return FALSE;
}

/* Creates the canvas with the month item for selecting days */
static GtkWidget *
create_days (int day, int month, int year)
{
	GtkWidget *canvas;
	int i;
	GnomeCanvasItem *day_group;

	canvas = gnome_canvas_new ();
	gtk_widget_set_usize (canvas, 150, 120);

	month_item = GNOME_MONTH_ITEM (gnome_month_item_new (gnome_canvas_root (GNOME_CANVAS (canvas))));
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (month_item),
			       "month", month,
			       "year", year,
			       "start_on_monday", week_starts_on_monday,
			       NULL);
	colorify_month_item (month_item, default_color_func, NULL);
	month_item_prepare_prelight (month_item, default_color_func, NULL);
	update ();

	/* Connect to size_allocate so that we can change the size of the month item and the
	 * scrolling region appropriately.
	 */

	gtk_signal_connect (GTK_OBJECT (canvas), "size_allocate",
			    (GtkSignalFunc) set_scroll_region,
			    NULL);

	/* Bind the day groups to our event handler */

	for (i = 0; i < 42; i++) {
		day_group = gnome_month_item_num2child (month_item, i + GNOME_MONTH_ITEM_DAY_GROUP);
		gtk_signal_connect (GTK_OBJECT (day_group), "event",
				    (GtkSignalFunc) day_event,
				    NULL);
	}

	return canvas;
}

static void
goto_today (GtkWidget *widget, gpointer data)
{
	gnome_calendar_goto_today (gnome_calendar);
	gtk_widget_destroy (goto_win);
}

/* Creates a "goto date" dialog and runs it */
void
goto_dialog (GnomeCalendar *gcal)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *w;
	GtkWidget *days;
	struct tm tm;

	gnome_calendar = gcal;
	current_index = -1;

	tm = *localtime (&gnome_calendar->current_display);

	goto_win = gnome_dialog_new (_("Go to date"),
				     GNOME_STOCK_BUTTON_CANCEL,
				     NULL);
	gnome_dialog_set_parent (GNOME_DIALOG (goto_win), GTK_WINDOW (gcal));
	
	vbox = GNOME_DIALOG (goto_win)->vbox;

	/* Instructions */

	w = gtk_label_new (_("Please select the date you want to go to.\n"
			     "When you click on a day, you will be taken\n"
			     "to that date."));
	gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Create month item before creating the year controls, since the latter ones need the
	 * month_item to be created.
	 */

	days = create_days (tm.tm_mday, tm.tm_mon, tm.tm_year + 1900);

	/* Year */

	w = create_year (tm.tm_year + 1900);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Month */

	w = create_months (tm.tm_mon);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Days (canvas with month item) */

	gtk_box_pack_start (GTK_BOX (vbox), days, TRUE, TRUE, 0);
	gtk_widget_show (days);

	/* Today button */

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	w = gtk_button_new_with_label (_("Go to today"));
	gtk_signal_connect (GTK_OBJECT (w), "clicked",
			    (GtkSignalFunc) goto_today,
			    NULL);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Run! */

	gtk_window_set_modal (GTK_WINDOW (goto_win), TRUE);
	gnome_dialog_set_close (GNOME_DIALOG (goto_win), TRUE);
	gnome_dialog_set_parent (GNOME_DIALOG (goto_win), GTK_WINDOW (gnome_calendar));
	gtk_widget_show (goto_win);
}
