/* Go to date dialog for Evolution
 *
 * Copyright (C) 1998 Red Hat, Inc.
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena <federico@ximian.com>
 *          JP Rosevear <jpr@ximian.com>
 */

#include <config.h>
#include <gnome.h>
#include <glade/glade.h>
#include "calendar-commands.h"
#include "tag-calendar.h"
#include "goto.h"

typedef struct 
{
	GladeXML *xml;
	GtkWidget *dialog;	

	GtkWidget *month;
	GtkWidget *year;
	ECalendar *ecal;
	GtkWidget *vbox;

	GnomeCalendar *gcal;
	gint year_val;
	gint month_val;
	gint day_val;
	
} GoToDialog;

GoToDialog *dlg = NULL;

/* Callback used when the year adjustment is changed */
static void
year_changed (GtkAdjustment *adj, gpointer data)
{
	GoToDialog *dlg = data;

	dlg->year_val = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (dlg->year));
	e_calendar_item_set_first_month (dlg->ecal->calitem, dlg->year_val, dlg->month_val);
}

/* Callback used when a month button is toggled */
static void
month_changed (GtkToggleButton *toggle, gpointer data)
{
	GoToDialog *dlg = data;
	GtkWidget *menu, *active;
	
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (dlg->month));
	active = gtk_menu_get_active (GTK_MENU (menu));
	dlg->month_val = g_list_index (GTK_MENU_SHELL (menu)->children, active);

	e_calendar_item_set_first_month (dlg->ecal->calitem, dlg->year_val, dlg->month_val);
}

static void
ecal_date_range_changed (ECalendarItem *calitem, gpointer user_data)
{
	GoToDialog *dlg = user_data;
	CalClient *client;
	
	client = gnome_calendar_get_cal_client (dlg->gcal);
	if (client)
		tag_calendar_by_client (dlg->ecal, client);
}

/* Event handler for day groups in the month item.  A button press makes the calendar jump to the
 * selected day and destroys the Go-to dialog box.
 */
static void
ecal_event (ECalendarItem *calitem, gpointer user_data)
{
	GoToDialog *dlg = user_data;
	GDate start_date, end_date;
	struct tm tm;
	time_t et;
	
	e_calendar_item_get_selection (calitem, &start_date, &end_date);
	g_date_to_struct_tm (&start_date, &tm);
	et = mktime (&tm);
	
	gnome_calendar_goto (dlg->gcal, et);

	gnome_dialog_close (GNOME_DIALOG (dlg->dialog));
}

/* Creates the ecalendar */
static void
create_ecal (GoToDialog *dlg)
{
	ECalendarItem *calitem;
	
	dlg->ecal = E_CALENDAR (e_calendar_new ());
	calitem = dlg->ecal->calitem;
	
	e_calendar_item_set_display_popup (calitem, FALSE);
	gtk_widget_show (GTK_WIDGET (dlg->ecal));
	gtk_box_pack_start (GTK_BOX (dlg->vbox), GTK_WIDGET (dlg->ecal), TRUE, TRUE, 0);

	e_calendar_item_set_first_month (calitem, dlg->year_val, dlg->month_val);
	
	ecal_date_range_changed (calitem, dlg);
}

static void
goto_today (GoToDialog *dlg)
{
	gnome_calendar_goto_today (dlg->gcal);
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (GoToDialog *dlg)
{
#define GW(name) glade_xml_get_widget (dlg->xml, name)

	dlg->dialog = GW ("goto-dialog");

	dlg->month = GW ("month");
	dlg->year = GW ("year");
	dlg->vbox = GW ("vbox");

#undef GW

	return (dlg->dialog
		&& dlg->month
		&& dlg->year
		&& dlg->vbox);
}

static void
goto_dialog_init_widgets (GoToDialog *dlg) 
{
	GtkWidget *menu;
	GtkAdjustment *adj;
	GList *l;
	
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (dlg->month));
	for (l = GTK_MENU_SHELL (menu)->children; l != NULL; l = l->next)
		gtk_signal_connect (GTK_OBJECT (menu), "selection_done",
				    GTK_SIGNAL_FUNC (month_changed), dlg);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (dlg->year));
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    (GtkSignalFunc) year_changed, dlg);

	gtk_signal_connect (GTK_OBJECT (dlg->ecal->calitem),
			    "date_range_changed",
			    GTK_SIGNAL_FUNC (ecal_date_range_changed),
			    dlg);
	gtk_signal_connect (GTK_OBJECT (dlg->ecal->calitem),
			    "selection_changed",
			    (GtkSignalFunc) ecal_event,
			    dlg);
}

/* Creates a "goto date" dialog and runs it */
void
goto_dialog (GnomeCalendar *gcal)
{
	GtkWidget *menu;
	time_t start_time;
	struct tm tm;
	int b;

	if (dlg) {
		return;
	}
		
	dlg = g_new0 (GoToDialog, 1);
	
	/* Load the content widgets */
	dlg->xml = glade_xml_new (EVOLUTION_GLADEDIR "/goto-dialog.glade", NULL);
	if (!dlg->xml) {
		g_message ("goto_dialog(): Could not load the Glade XML file!");
		return;
	}

	if (!get_widgets (dlg)) {
		g_message ("goto_dialog(): Could not find all widgets in the XML file!");
		return;
	}
	dlg->gcal = gcal;

	gnome_calendar_get_selected_time_range (dlg->gcal, &start_time, NULL);
	tm = *localtime (&start_time);
	dlg->year_val = tm.tm_year + 1900;
	dlg->month_val = tm.tm_mon;
	dlg->day_val = tm.tm_mday;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (dlg->month));
	gtk_option_menu_set_history (GTK_OPTION_MENU (dlg->month), dlg->month_val);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dlg->year), dlg->year_val);
	
	create_ecal (dlg);

	goto_dialog_init_widgets (dlg);

	gnome_dialog_set_parent (GNOME_DIALOG (dlg->dialog),
				 GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));

	b = gnome_dialog_run_and_close (GNOME_DIALOG (dlg->dialog));
	if (b == 0)
		goto_today (dlg);

	gtk_object_unref (GTK_OBJECT (dlg->xml));
	g_free (dlg);
	dlg = NULL;
}
