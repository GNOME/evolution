/* Week view composite widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <string.h>
#include "gncal-week-view.h"


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

		week_view_type = gtk_type_unique (gtk_table_get_type (), &week_view_info);
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

GtkWidget *
gncal_week_view_new (GnomeCalendar *calendar, time_t start_of_week)
{
	GncalWeekView *wview;
	int i;

	g_return_val_if_fail (calendar != NULL, NULL);

	wview = gtk_type_new (gncal_week_view_get_type ());

	gtk_table_set_homogeneous (GTK_TABLE (wview), TRUE);

	wview->calendar = calendar;

	for (i = 0; i < 7; i++) {
		wview->days[i] = GNCAL_DAY_VIEW (gncal_day_view_new (calendar, 0, 0));

		if (i < 5)
			gtk_table_attach (GTK_TABLE (wview), GTK_WIDGET (wview->days[i]),
					  i, i + 1,
					  0, 1,
					  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
					  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
					  0, 0);
		else
			gtk_table_attach (GTK_TABLE (wview), GTK_WIDGET (wview->days[i]),
					  i - 2, i - 1,
					  1, 2,
					  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
					  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
					  0, 0);

		gtk_widget_show (GTK_WIDGET (wview->days[i]));
	}

	/* FIXME: for now this is a plain calendar (for not having anything better to put
	 * there).  In the final version it should be a nice days/hours matrix with
	 * "event density" display as in Sun's "cm" program.
	 */

	wview->gtk_calendar = GTK_CALENDAR (gtk_calendar_new ());
	gtk_calendar_display_options (wview->gtk_calendar,
				      GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES);
	gtk_table_attach (GTK_TABLE (wview), GTK_WIDGET (wview->gtk_calendar),
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
	time_t day_start, day_end;
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

	day_start = mktime (&tm);

	/* Calendar */

	gtk_calendar_select_month (wview->gtk_calendar, tm.tm_mon, tm.tm_year + 1900);

	/* Day views */

	for (i = 0; i < 7; i++) { /* rest of days */
		tm.tm_mday++;
		day_end = mktime (&tm);

		printf ("Boundary: ");
		print_time_t (day_start);
		gncal_day_view_set_bounds (wview->days[i], day_start, day_end - 1);

		day_start = day_end;
	}

	update (wview, FALSE, NULL, 0);
}
