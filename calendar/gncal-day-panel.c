/* Day view notebook panel for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
 */

#include <config.h>
#include <gnome.h>
#include <gtk/gtkhseparator.h>
#include "gncal-day-panel.h"
#include "main.h"
#include "timeutil.h"


guint
gncal_day_panel_get_type (void)
{
	static guint day_panel_type = 0;

	if (!day_panel_type) {
		GtkTypeInfo day_panel_info = {
			"GncalDayPanel",
			sizeof (GncalDayPanel),
			sizeof (GncalDayPanelClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) NULL,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		day_panel_type = gtk_type_unique (gtk_table_get_type (), &day_panel_info);
	}

	return day_panel_type;
}

static void
day_view_range_activated (GncalFullDay *fullday, GncalDayPanel *dpanel)
{
	iCalObject *ical;

	ical = ical_new ("", user_name, "");
	ical->new = 1;

	gncal_full_day_selection_range (fullday, &ical->dtstart, &ical->dtend);

	gnome_calendar_add_object (dpanel->calendar, ical);
	save_default_calendar (dpanel->calendar);
	gncal_full_day_focus_child (fullday, ical);
}

static void
full_day_size_allocated (GtkWidget *widget, GtkAllocation *allocation, GncalDayPanel *dpanel)
{
	GtkAdjustment *adj;
	int yoffset;
	gfloat newval;

	adj = gtk_scrolled_window_get_vadjustment (dpanel->fullday_sw);

	yoffset = gncal_full_day_get_day_start_yoffset (GNCAL_FULL_DAY (widget));

	newval = adj->lower + (adj->upper - adj->lower) * (double) yoffset / allocation->height;
	if (newval != adj->value)
		gtk_signal_emit_by_name (GTK_OBJECT (adj), "value_changed");
}

static void
calendar_day_selected (GtkCalendar *calendar, GncalDayPanel *dpanel)
{
	gint y, m, d;
	struct tm tm;

	gtk_calendar_get_date (calendar, &y, &m, &d);

	tm.tm_year = y - 1900;
	tm.tm_mon  = m;
	tm.tm_mday = d;
	tm.tm_hour = 5; /* for daylight savings time fix */
	tm.tm_min  = 0;
	tm.tm_sec  = 0;

	gnome_calendar_goto (dpanel->calendar, mktime (&tm));
}

static void
retag_calendar (GtkCalendar *calendar, GncalDayPanel *dpanel)
{
	gnome_calendar_tag_calendar (dpanel->calendar, GTK_CALENDAR (dpanel->gtk_calendar));
}

GtkWidget *
gncal_day_panel_new (GnomeCalendar *calendar, time_t start_of_day)
{
	GncalDayPanel *dpanel;
	GtkWidget *w;
	GtkWidget *hpane, *vpane;
	gint start_pos = 265;
	struct tm tm;

	g_return_val_if_fail (calendar != NULL, NULL);

	dpanel = gtk_type_new (gncal_day_panel_get_type ());

	gtk_container_border_width (GTK_CONTAINER (dpanel), 4);
	gtk_table_set_row_spacings (GTK_TABLE (dpanel), 4);
	gtk_table_set_col_spacings (GTK_TABLE (dpanel), 4);

	dpanel->calendar = calendar;

	/* Date label */

	w = gtk_label_new ("");
	dpanel->date_label = GTK_LABEL (w);
	gtk_table_attach (GTK_TABLE (dpanel), w,
			  0, 1, 0, 1,
			  GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (w);

	/* Create horizontal pane */
	
	hpane = gtk_hpaned_new ();
	gtk_table_attach (GTK_TABLE (dpanel), hpane,
			  0, 1, 2, 4,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);
	
	/* Full day */

	w = gtk_scrolled_window_new (NULL, NULL);
	dpanel->fullday_sw = GTK_SCROLLED_WINDOW (w);
	gtk_scrolled_window_set_policy (dpanel->fullday_sw,
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_paned_pack1 (GTK_PANED (hpane), w, FALSE, TRUE);
	/*gtk_paned_add1 (GTK_PANED (hpane), w);*/
	gtk_widget_show (w);

	w = gncal_full_day_new (calendar, time_day_begin (start_of_day), time_day_end (start_of_day));
	dpanel->fullday = GNCAL_FULL_DAY (w);
	gtk_signal_connect (GTK_OBJECT (dpanel->fullday), "range_activated",
			    (GtkSignalFunc) day_view_range_activated,
			    dpanel);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (dpanel->fullday_sw), w);
	gtk_widget_show (w);

	/* We'll scroll the list to the proper initial position */

	gtk_signal_connect (GTK_OBJECT (dpanel->fullday), "size_allocate",
			    (GtkSignalFunc) full_day_size_allocated,
			    dpanel);

	/* Create vertical pane */
	
	vpane = gtk_vpaned_new ();
	gtk_paned_pack2 (GTK_PANED (hpane), GTK_WIDGET (vpane), TRUE, TRUE);
	/*gtk_paned_add2 (GTK_PANED (hpane), GTK_WIDGET (vpane));*/
		
	/* Gtk calendar */

	tm = *localtime (&start_of_day);

	w = gtk_calendar_new ();
	dpanel->gtk_calendar = GTK_CALENDAR (w);
	gtk_calendar_display_options (dpanel->gtk_calendar,
				      (GTK_CALENDAR_SHOW_HEADING
				       | GTK_CALENDAR_SHOW_DAY_NAMES
				       | (week_starts_on_monday
					  ? GTK_CALENDAR_WEEK_START_MONDAY : 0)));
	gtk_calendar_select_month (dpanel->gtk_calendar, tm.tm_mon, tm.tm_year + 1900);
	gtk_calendar_select_day (dpanel->gtk_calendar, tm.tm_mday);
	dpanel->day_selected_id = gtk_signal_connect (GTK_OBJECT (dpanel->gtk_calendar),
						      "day_selected_double_click",
						      (GtkSignalFunc) calendar_day_selected,
						      dpanel);
	gtk_signal_connect (GTK_OBJECT (dpanel->gtk_calendar), "month_changed",
			    GTK_SIGNAL_FUNC (retag_calendar), dpanel);
	gtk_paned_add1 (GTK_PANED (vpane), w);
	gtk_widget_show (w);

	/* To-do */

	w = gncal_todo_new (calendar);
	dpanel->todo = GNCAL_TODO (w);
	gtk_paned_add2 (GTK_PANED (vpane), w);
	gtk_widget_show (w);

	/* Done */

	gncal_day_panel_set (dpanel, start_of_day);

	gtk_paned_set_position (GTK_PANED (hpane), start_pos);

	return GTK_WIDGET (dpanel);
}

static void
update (GncalDayPanel *dpanel, int update_fullday, iCalObject *ico, int flags)
{
	char buf [80];
	
	if (update_fullday){
		gncal_full_day_update (dpanel->fullday, ico, flags);
		retag_calendar (dpanel->gtk_calendar, dpanel);
	}
	gncal_todo_update (dpanel->todo, ico, flags);
	
	strftime (buf, sizeof (buf), _("%a %b %d %Y"), localtime (&dpanel->start_of_day));
	gtk_label_set (GTK_LABEL (dpanel->date_label), buf);
}

void
gncal_day_panel_update (GncalDayPanel *dpanel, iCalObject *ico, int flags)
{
	g_return_if_fail (dpanel != NULL);
	g_return_if_fail (GNCAL_IS_DAY_PANEL (dpanel));

	update (dpanel, TRUE, ico, flags);
}

void
gncal_day_panel_set (GncalDayPanel *dpanel, time_t start_of_day)
{
	char buf[80];
	struct tm tm;

	g_return_if_fail (dpanel != NULL);
	g_return_if_fail (GNCAL_IS_DAY_PANEL (dpanel));

	dpanel->start_of_day = time_day_begin(start_of_day);
	if (dpanel->fullday->lower == dpanel->start_of_day)
		return;

	tm = *localtime (&dpanel->start_of_day);
	strftime (buf, sizeof (buf), _("%a %b %d %Y"), &tm);
	gtk_label_set (GTK_LABEL (dpanel->date_label), buf);

	gncal_full_day_set_bounds (dpanel->fullday, dpanel->start_of_day, time_day_end (dpanel->start_of_day));

	gtk_calendar_select_month (dpanel->gtk_calendar, tm.tm_mon, tm.tm_year + 1900);

	gtk_signal_handler_block (GTK_OBJECT (dpanel->gtk_calendar), dpanel->day_selected_id);
	gtk_calendar_select_day (dpanel->gtk_calendar, tm.tm_mday);
	gtk_signal_handler_unblock (GTK_OBJECT (dpanel->gtk_calendar), dpanel->day_selected_id);

	update (dpanel, FALSE, NULL, 0);
}

void
gncal_day_panel_time_format_changed (GncalDayPanel *dpanel)
{
	g_return_if_fail (dpanel != NULL);
	g_return_if_fail (GNCAL_IS_DAY_PANEL (dpanel));

	gtk_calendar_display_options (dpanel->gtk_calendar,
				      (week_starts_on_monday
				       ? (dpanel->gtk_calendar->display_flags
					  | GTK_CALENDAR_WEEK_START_MONDAY)
				       : (dpanel->gtk_calendar->display_flags
					  & ~GTK_CALENDAR_WEEK_START_MONDAY)));
}

void
todo_list_properties_changed (GncalDayPanel *dpanel)
{
	g_return_if_fail (dpanel != NULL);
	g_return_if_fail (GNCAL_IS_DAY_PANEL (dpanel));

	gncal_todo_update (dpanel->todo, NULL, 0);
}



