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


static void
highlight_current_day (GnomeMonthItem *mitem)
{
	struct tm *tm;
	time_t t;

	t = time (NULL);
	tm = localtime (&t);

	if ((mitem->year == (tm->tm_year + 1900)) && (mitem->month == tm->tm_mon)) {
		;
		/* FIXME */
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

/* Creates the canvas with the month item for selecting days */
static GtkWidget *
create_days (GtkWidget *dialog, GnomeCalendar *gcal, int day, int month, int year)
{
	GtkWidget *canvas;
	GnomeCanvasItem *mitem;

	canvas = gnome_canvas_new ();
	gnome_canvas_set_size (GNOME_CANVAS (canvas), 150, 120);

	mitem = gnome_month_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (canvas)->root));
	gnome_canvas_item_set (mitem,
			       "month", month,
			       "year", year,
			       NULL);
	highlight_current_day (GNOME_MONTH_ITEM (mitem));

	gtk_object_set_data (GTK_OBJECT (dialog), "month_item", mitem);

	/* Connect to size_allocate so that we can change the size of the month item and the
	 * scrolling region appropriately.
	 */

	gtk_signal_connect (GTK_OBJECT (canvas), "size_allocate",
			    (GtkSignalFunc) set_scroll_region,
			    mitem);

	return canvas;
}

/* Creates a "goto date" dialog and runs it */
void
goto_dialog (GnomeCalendar *gcal)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *w;
	struct tm *tm;

	tm = localtime (&gcal->current_display);

	dialog = gnome_dialog_new (_("Go to date"),
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);

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

	/* Run! */

	gnome_dialog_set_modal (GNOME_DIALOG (dialog));

	printf ("el dialogo regreso %d\n", gnome_dialog_run_and_destroy (GNOME_DIALOG (dialog)));
}
