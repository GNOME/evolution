/* Week view composite widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <quartic@gimp.org>
 *          Miguel de Icaza <miguel@kernel.org>
 */

#include <config.h>
#include <gnome.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gncal-week-view.h"
#include "main.h"
#include "timeutil.h"

static void gncal_week_view_init (GncalWeekView *wview);


guint
gncal_week_view_get_type (void)
{
	static guint week_view_type = 0;

	if (!week_view_type) {
		GtkTypeInfo week_view_info = {
			"GncalWeekView",
			sizeof (GncalWeekView),
			sizeof (GncalWeekViewClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) gncal_week_view_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		week_view_type = gtk_type_unique (gtk_vbox_get_type (), &week_view_info);
	}

	return week_view_type;
}

static void
gncal_week_view_init (GncalWeekView *wview)
{
	int i;

	wview->calendar = NULL;
	memset (&wview->start_of_week, 0, sizeof (wview->start_of_week));

	for (i = 0; i < 7; i++)
		wview->days[i] = NULL;

	wview->gtk_calendar = NULL;
}

static void
jump_to_day (GtkCalendar *cal, GncalWeekView *wview, int day)
{
	struct tm tm;
	time_t t;
	static int inside;
	
	if (inside)
		return;
	inside = 1;
	tm.tm_mday = day;
	tm.tm_mon  = cal->month;
	tm.tm_year = cal->year - 1900;
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_isdst = -1;
	t = mktime (&tm);

	gncal_week_view_set (wview, t);
	inside = 0;
}

static void
jump_to_day_click (GtkCalendar *cal, GncalWeekView *wview)
{
	jump_to_day (cal, wview, cal->selected_day);
}

static void
sync_week (GtkCalendar *cal, GncalWeekView *wview)
{
	jump_to_day (cal, wview, wview->start_of_week.tm_mday + 7);
	gnome_calendar_tag_calendar (wview->calendar, wview->gtk_calendar);
}

static void
double_click_on_weekday (GtkWidget *widget, GdkEvent *e, GncalWeekView *wview)
{
}

GtkWidget *
gncal_week_view_new (GnomeCalendar *calendar, time_t start_of_week)
{
	GncalWeekView *wview;
	GtkWidget *table;
	int i;

	g_return_val_if_fail (calendar != NULL, NULL);

	wview = gtk_type_new (gncal_week_view_get_type ());

	table = gtk_table_new (0, 0, 0);
	gtk_table_set_homogeneous (GTK_TABLE (table), TRUE);
	wview->label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (wview), wview->label, 0, 0, 0);
	gtk_box_pack_start (GTK_BOX (wview), table, 1, 1, 0);
	wview->calendar = calendar;
	for (i = 0; i < 7; i++) {
		wview->days[i] = GNCAL_DAY_VIEW (gncal_day_view_new (calendar, 0, 0));
		gtk_signal_connect (GTK_OBJECT (wview->days [i]), "button_press_event",
				    GTK_SIGNAL_FUNC(double_click_on_weekday), wview);
		
		if (i < 5)
			gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (wview->days[i]),
					  i, i + 1,
					  0, 1,
					  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
					  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
					  0, 0);
		else
			gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (wview->days[i]),
					  i - 2, i - 1,
					  1, 2,
					  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
					  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
					  0, 0);

		gtk_widget_show (GTK_WIDGET (wview->days[i]));
	}

	wview->gtk_calendar = GTK_CALENDAR (gtk_calendar_new ());

	gtk_signal_connect (GTK_OBJECT (wview->gtk_calendar), "day_selected_double_click",
			    GTK_SIGNAL_FUNC(jump_to_day_click), wview);
	gtk_signal_connect (GTK_OBJECT (wview->gtk_calendar), "month_changed",
			    GTK_SIGNAL_FUNC(sync_week), wview);
	
	gtk_calendar_display_options (wview->gtk_calendar,
				      (GTK_CALENDAR_SHOW_HEADING
				       | GTK_CALENDAR_SHOW_DAY_NAMES
				       | (week_starts_on_monday
					  ? GTK_CALENDAR_WEEK_START_MONDAY : 0)));

	gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (wview->gtk_calendar),
			  0, 3,
			  1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  4, 4);
	gtk_widget_show (GTK_WIDGET (wview->gtk_calendar));

	gncal_week_view_set (wview, start_of_week);

	return GTK_WIDGET (wview);
}

static void
update (GncalWeekView *wview, int update_days, iCalObject *object, int flags)
{
	int i;

	if (update_days)
		for (i = 0; i < 7; i++)
			gncal_day_view_update (wview->days[i], object, flags);

	/* FIXME: update extra widgets */
}

void
gncal_week_view_update (GncalWeekView *wview, iCalObject *ico, int flags)
{
	g_return_if_fail (wview != NULL);
	g_return_if_fail (GNCAL_IS_WEEK_VIEW (wview));

	update (wview, TRUE, ico, flags);
}

void
gncal_week_view_set (GncalWeekView *wview, time_t start_of_week)
{
	struct tm tm;
	time_t day_start, day_end, week_start, week_end;
	int i;

	g_return_if_fail (wview != NULL);
	g_return_if_fail (GNCAL_IS_WEEK_VIEW (wview));

	tm = *localtime (&start_of_week);
	
	/* back up to start of week (Monday) */

	tm.tm_mday -= (tm.tm_wday == 0) ? 6 : (tm.tm_wday - 1);
	
	/* Start of day */

	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;

	day_start = week_start = mktime (&tm);
	
	/* Calendar */

	gtk_calendar_select_month (wview->gtk_calendar, tm.tm_mon, tm.tm_year + 1900);

	/* Day views */

	for (i = 0; i < 7; i++) { /* rest of days */
		tm.tm_mday++;
		day_end = mktime (&tm);

		gncal_day_view_set_bounds (wview->days[i], day_start, day_end - 1);

		day_start = day_end;
	}
	
	update (wview, FALSE, NULL, 0);

	/* The label */
	{
		char buf [3][100];
		
		week_end = time_add_day (week_start, 6);

		strftime (buf[0], sizeof (buf[0]), _("%a %b %d %Y"), 
			localtime(&week_start));

		strftime (buf[1], sizeof (buf[1]), _("%a %b %d %Y"),
			localtime(&week_end));

		g_snprintf(buf[2], sizeof(buf[2]), "%s - %s", buf[0], buf[1]);
		gtk_label_set (GTK_LABEL (wview->label), buf[2]);
		
	}
}

void
gncal_week_view_time_format_changed (GncalWeekView *wview)
{
	g_return_if_fail (wview != NULL);
	g_return_if_fail (GNCAL_IS_WEEK_VIEW (wview));

	gtk_calendar_display_options (wview->gtk_calendar,
				      (week_starts_on_monday
				       ? (wview->gtk_calendar->display_flags
					  | GTK_CALENDAR_WEEK_START_MONDAY)
				       : (wview->gtk_calendar->display_flags
					  & ~GTK_CALENDAR_WEEK_START_MONDAY)));
}
