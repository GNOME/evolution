/*
 * Calendar properties dialog box
 * (C) 1998 the Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@kernel.org>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */
#include <config.h>
#include <langinfo.h>
#include <gnome.h>
#include "gnome-cal.h"
#include "main.h"

static GtkWidget *prop_win;		/* The preferences dialog */
static GtkWidget *time_format_12;	/* Radio button for 12-hour format */
static GtkWidget *time_format_24;	/* Radio button for 24-hour format */
static GtkWidget *start_on_sunday;	/* Check button for weeks starting on Sunday */
static GtkWidget *start_on_monday;	/* Check button for weeks starting on Monday */
static GtkWidget *start_omenu;		/* Option menu for start of day */
static GtkWidget *end_omenu;		/* Option menu for end of day */
static GtkWidget *start_items[24];	/* Menu items for start of day menu */
static GtkWidget *end_items[24];	/* Menu items for end of day menu */

/* Callback used when the property box is closed -- just sets the prop_win variable to null. */
static int
prop_cancel (void)
{
	prop_win = NULL;
	return FALSE;
}

/* Returns the index of the active item in a menu */
static int
get_active_index (GtkWidget *menu)
{
	GtkWidget *active;

	active = gtk_menu_get_active (GTK_MENU (menu));
	return GPOINTER_TO_INT (gtk_object_get_user_data (GTK_OBJECT (active)));
}

/* Callback used when the Apply button is clicked. */
static void
prop_apply (GtkWidget *w, int page)
{
	if (page != -1)
		return;

	/* Day begin/end */

	day_begin = get_active_index (gtk_option_menu_get_menu (GTK_OPTION_MENU (start_omenu)));
	day_end   = get_active_index (gtk_option_menu_get_menu (GTK_OPTION_MENU (end_omenu)));
	gnome_config_set_int ("/calendar/Calendar/Day start", day_begin);
	gnome_config_set_int ("/calendar/Calendar/Day end", day_end);

	/* Time format */

	am_pm_flag = GTK_TOGGLE_BUTTON (time_format_12)->active;
	gnome_config_set_bool ("/calendar/Calendar/AM PM flag", am_pm_flag);

	/* Week start */

	week_starts_on_monday = GTK_TOGGLE_BUTTON (start_on_monday)->active;
	gnome_config_set_bool ("/calendar/Calendar/Week starts on Monday", week_starts_on_monday);

	gnome_config_sync ();
	time_format_changed ();
}

/* Notifies the property box that the data has changed */
static void
toggled (GtkWidget *widget, gpointer data)
{
	gnome_property_box_changed (GNOME_PROPERTY_BOX (prop_win));
}

/* Builds and returns a two-element radio button group surrounded by a frame.  The radio buttons are
 * stored in the specified variables, and the first radio button's state is set according to the
 * specified flag value.  The buttons are connected to the toggled() function to update the property
 * box's dirty state.
 */
static GtkWidget *
build_two_radio_group (char *title,
		       char *radio_1_title, GtkWidget **radio_1_widget,
		       char *radio_2_title, GtkWidget **radio_2_widget,
		       int radio_1_value)
{
	GtkWidget *frame;
	GtkWidget *vbox;

	frame = gtk_frame_new (title);

	vbox = gtk_vbox_new (TRUE, 0);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	*radio_1_widget = gtk_radio_button_new_with_label (NULL, radio_1_title);
	gtk_box_pack_start (GTK_BOX (vbox), *radio_1_widget, FALSE, FALSE, 0);
	
	*radio_2_widget = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (*radio_1_widget),
								       radio_2_title);
	gtk_box_pack_start (GTK_BOX (vbox), *radio_2_widget, FALSE, FALSE, 0);

	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (*radio_1_widget), radio_1_value);
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (*radio_2_widget), !radio_1_value);

	gtk_signal_connect (GTK_OBJECT (*radio_1_widget), "toggled",
			    (GtkSignalFunc) toggled,
			    NULL);

	return frame;
}

/* Callback invoked when a menu item from the start/end time option menus is selected.  It adjusts
 * the other menu to the proper time, if needed.
 */
static void
hour_activated (GtkWidget *widget, gpointer data)
{
	int start, end;

	if (data == start_omenu) {
		/* Adjust the end menu */

		start = GPOINTER_TO_INT (gtk_object_get_user_data (GTK_OBJECT (widget)));
		end = get_active_index (gtk_option_menu_get_menu (GTK_OPTION_MENU (end_omenu)));

		if (end < start)
			gtk_option_menu_set_history (GTK_OPTION_MENU (end_omenu), start);
	} else if (data == end_omenu) {
		/* Adjust the start menu */

		end = GPOINTER_TO_INT (gtk_object_get_user_data (GTK_OBJECT (widget)));
		start = get_active_index (gtk_option_menu_get_menu (GTK_OPTION_MENU (start_omenu)));

		if (start > end)
			gtk_option_menu_set_history (GTK_OPTION_MENU (start_omenu), end);
	} else
		g_assert_not_reached ();

	gnome_property_box_changed (GNOME_PROPERTY_BOX (prop_win));
}

/* Builds an option menu of 24 hours */
static GtkWidget *
build_hours_menu (GtkWidget **items, int active)
{
	GtkWidget *omenu;
	GtkWidget *menu;
	int i;
	char buf[100];
	struct tm tm;

	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();

	memset (&tm, 0, sizeof (tm));

	for (i = 0; i < 24; i++) {
		tm.tm_hour = i;
		strftime (buf, 100, "%I:%M %p", &tm);

		items[i] = gtk_menu_item_new_with_label (buf);
		gtk_object_set_user_data (GTK_OBJECT (items[i]), GINT_TO_POINTER (i));
		gtk_signal_connect (GTK_OBJECT (items[i]), "activate",
				    (GtkSignalFunc) hour_activated,
				    omenu);

		gtk_menu_append (GTK_MENU (menu), items[i]);
		gtk_widget_show (items[i]);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), active);
	return omenu;
}

/* Creates and displays the preferences dialog for the whole application */
void
properties (void)
{
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *hbox2;
	GtkWidget *hbox3;
	GtkWidget *w;

	if (prop_win)
		return;

	/* Main window and hbox for property page */
	
	prop_win = gnome_property_box_new ();
	gtk_window_set_title (GTK_WINDOW (prop_win), _("Preferences"));

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_border_width (GTK_CONTAINER (hbox), GNOME_PAD_SMALL);
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (prop_win), hbox,
					gtk_label_new (_("Time display")));

	/* Time format */

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	w = build_two_radio_group (_("Time format"),
				   _("12-hour (AM/PM)"), &time_format_12,
				   _("24-hour"), &time_format_24,
				   am_pm_flag);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	/* Weeks start on */

	w = build_two_radio_group (_("Weeks start on"),
				   _("Sunday"), &start_on_sunday,
				   _("Monday"), &start_on_monday,
				   !week_starts_on_monday);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	/* Day range */

	frame = gtk_frame_new (_("Day range"));
	gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	w = gtk_label_new (_("Please select the start and end hours you want\n"
			     "to be displayed in the day view and week view.\n"
			     "Times outside this range will not be displayed\n"
			     "by default."));
	gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	hbox2 = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);

	/* Day start */

	hbox3 = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox2), hbox3, FALSE, FALSE, 0);

	w = gtk_label_new (_("Day start:"));
	gtk_box_pack_start (GTK_BOX (hbox3), w, FALSE, FALSE, 0);

	start_omenu = build_hours_menu (start_items, day_begin);
	gtk_box_pack_start (GTK_BOX (hbox3), start_omenu, FALSE, FALSE, 0);

	/* Day end */

	hbox3 = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox2), hbox3, FALSE, FALSE, 0);

	w = gtk_label_new (_("Day end:"));
	gtk_box_pack_start (GTK_BOX (hbox3), w, FALSE, FALSE, 0);

	end_omenu = build_hours_menu (end_items, day_end);
	gtk_box_pack_start (GTK_BOX (hbox3), end_omenu, FALSE, FALSE, 0);

	/* Done! */
	
	gtk_signal_connect (GTK_OBJECT (prop_win), "destroy",
			    (GtkSignalFunc) prop_cancel, NULL);

	gtk_signal_connect (GTK_OBJECT (prop_win), "delete_event",
			    (GtkSignalFunc) prop_cancel, NULL);

	gtk_signal_connect (GTK_OBJECT (prop_win), "apply",
			    (GtkSignalFunc) prop_apply, NULL);

	gtk_widget_show_all (prop_win);
}
