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
#include "timeutil.h"


static void
highlight_current_day (GnomeMonthItem *mitem)
{
	struct tm *tm;
	time_t t;
	GnomeCanvasItem *label;
	int i;

	t = time (NULL);
	tm = localtime (&t);

	/* First clear all the days to normal */

	for (i = 0; i < 42; i++) {
		label = gnome_month_item_num2child (mitem, i + GNOME_MONTH_ITEM_DAY_LABEL);
		gnome_canvas_item_set (label,
				       "fill_color", "black",
				       NULL);
	}

	/* Highlight the current day, if appropriate */

	if ((mitem->year == (tm->tm_year + 1900)) && (mitem->month == tm->tm_mon)) {
		i = gnome_month_item_day2index (mitem, tm->tm_mday);
		label = gnome_month_item_num2child (mitem, i + GNOME_MONTH_ITEM_DAY_LABEL);
		gnome_canvas_item_set (label,
				       "fill_color", "blue",
				       NULL);
	}
}

/* Callback used when the year adjustment is changed */
static void
year_changed (GtkAdjustment *adj, GtkWidget *dialog)
{
	GnomeCanvasItem *mitem;

	mitem = gtk_object_get_data (GTK_OBJECT (dialog), "month_item");
	gnome_canvas_item_set (mitem,
			       "year", (int) adj->value,
			       NULL);
	highlight_current_day (GNOME_MONTH_ITEM (mitem));
}

/* Creates the year control with its adjustment */
static GtkWidget *
create_year (GtkWidget *dialog, GnomeCalendar *gcal, int year)
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
			    dialog);

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
	GtkWidget *dialog;
	GnomeCanvasItem *mitem;

	if (!toggle->active)
		return;

	dialog = gtk_object_get_user_data (GTK_OBJECT (toggle));
	mitem = gtk_object_get_data (GTK_OBJECT (dialog), "month_item");
	gnome_canvas_item_set (mitem,
			       "month", GPOINTER_TO_INT (data),
			       NULL);
	highlight_current_day (GNOME_MONTH_ITEM (mitem));
}

/* Creates the months control */
static GtkWidget *
create_months (GtkWidget *dialog, GnomeCalendar *gcal, int month)
{
	GtkWidget *table;
	GtkWidget *w;
	GSList *group;
	int i, row, col;
	struct tm tm;
	char buf[100];

	tm = *localtime (&gcal->current_display);

	table = gtk_table_new (2, 6, TRUE);

	group = NULL;

	for (i = 0; i < 12; i++) {
		row = i / 6;
		col = i % 6;

		tm.tm_mon = i;
		strftime (buf, 100, "%b", &tm);

		w = gtk_radio_button_new (group);
		group = gtk_radio_button_group (GTK_RADIO_BUTTON (w));
		gtk_object_set_user_data (GTK_OBJECT (w), dialog);
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
set_scroll_region (GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	gnome_canvas_item_set (data,
			       "width", (double) (allocation->width - 1),
			       "height", (double) (allocation->height - 1),
			       NULL);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (widget),
					0, 0,
					allocation->width, allocation->height);
}

/* Event handler for day groups in the month item.  A button press makes the calendar jump to the
 * selected day and destroys the Go-to dialog box.  Days are prelighted as appropriate on
 * enter_notify and leave_notify events.
 */
static gint
day_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	GtkWidget *dialog;
	GnomeCalendar *gcal;
	GnomeMonthItem *mitem;
	GnomeCanvasItem *box;
	int child_num, day;

	dialog = GTK_WIDGET (data);
	gcal = gtk_object_get_data (GTK_OBJECT (dialog), "gnome_calendar");
	mitem = gtk_object_get_data (GTK_OBJECT (dialog), "month_item");

	child_num = gnome_month_item_child2num (mitem, item);
	day = gnome_month_item_num2day (mitem, child_num);

	if (day == 0)
		return FALSE;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if ((event->button.button == 1) && (day != 0)) {
			gnome_calendar_goto (gcal, time_from_day (mitem->year, mitem->month, day));
			gtk_widget_destroy (dialog);
		}
		break;

	case GDK_ENTER_NOTIFY:
		box = gnome_month_item_num2child (mitem, child_num - GNOME_MONTH_ITEM_DAY_GROUP + GNOME_MONTH_ITEM_DAY_BOX);
		gnome_canvas_item_set (box,
				       "fill_color", "#ea60ea60ea60",
				       NULL);
		break;

	case GDK_LEAVE_NOTIFY:
		box = gnome_month_item_num2child (mitem, child_num - GNOME_MONTH_ITEM_DAY_GROUP + GNOME_MONTH_ITEM_DAY_BOX);
		gnome_canvas_item_set (box,
				       "fill_color", "#d6d6d6d6d6d6",
				       NULL);
		break;

	default:
		break;
	}

	return FALSE;
}

/* Creates the canvas with the month item for selecting days */
static GtkWidget *
create_days (GtkWidget *dialog, GnomeCalendar *gcal, int day, int month, int year)
{
	GtkWidget *canvas;
	GnomeCanvasItem *mitem;
	int i;
	GnomeCanvasItem *day_group;

	canvas = gnome_canvas_new ();
	gnome_canvas_set_size (GNOME_CANVAS (canvas), 150, 120);

	mitem = gnome_month_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (canvas)->root));
	gnome_canvas_item_set (mitem,
			       "month", month,
			       "year", year,
			       "start_on_monday", week_starts_on_monday,
			       NULL);
	highlight_current_day (GNOME_MONTH_ITEM (mitem));

	gtk_object_set_data (GTK_OBJECT (dialog), "month_item", mitem);

	/* Connect to size_allocate so that we can change the size of the month item and the
	 * scrolling region appropriately.
	 */

	gtk_signal_connect (GTK_OBJECT (canvas), "size_allocate",
			    (GtkSignalFunc) set_scroll_region,
			    mitem);

	/* Bind the day groups to our event handler */

	for (i = 0; i < 42; i++) {
		day_group = gnome_month_item_num2child (GNOME_MONTH_ITEM (mitem), i + GNOME_MONTH_ITEM_DAY_GROUP);
		gtk_signal_connect (GTK_OBJECT (day_group), "event",
				    (GtkSignalFunc) day_event,
				    dialog);
	}

	return canvas;
}

static void
goto_today (GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;
	GnomeCalendar *gcal;

	dialog = GTK_WIDGET (data);
	gcal = gtk_object_get_data (GTK_OBJECT (dialog), "gnome_calendar");

	gnome_calendar_goto_today (gcal);
	gtk_widget_destroy (dialog);
}

/* Creates a "goto date" dialog and runs it */
void
goto_dialog (GnomeCalendar *gcal)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *w;
	struct tm *tm;

	tm = localtime (&gcal->current_display);

	dialog = gnome_dialog_new (_("Go to date"),
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);
	gtk_object_set_data (GTK_OBJECT (dialog), "gnome_calendar", gcal);

	vbox = GNOME_DIALOG (dialog)->vbox;

	/* Instructions */

	w = gtk_label_new (_("Please select the date you want to go to.\n"
			     "When you click on a day, you will be taken\n"
			     "to that date."));
	gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Year */

	w = create_year (dialog, gcal, tm->tm_year + 1900);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Month */

	w = create_months (dialog, gcal, tm->tm_mon);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Days (canvas with month item) */

	w = create_days (dialog, gcal, tm->tm_mday, tm->tm_mon, tm->tm_year + 1900);
	gtk_box_pack_start (GTK_BOX (vbox), w, TRUE, TRUE, 0);
	gtk_widget_show (w);

	/* Today button */

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	w = gtk_button_new_with_label (_("Go to today"));
	gtk_signal_connect (GTK_OBJECT (w), "clicked",
			    (GtkSignalFunc) goto_today,
			    dialog);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Run! */

	gnome_dialog_set_modal (GNOME_DIALOG (dialog));
	gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
	gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (gcal));
	gtk_widget_show (dialog);
}
