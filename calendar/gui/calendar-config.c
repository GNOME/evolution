/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2000, Helix Code, Inc.
 * Copyright 2000, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * calendar-config.c - functions to load/save/get/set user settings.
 */

#include <config.h>
#include "component-factory.h"
#include "calendar-config.h"


typedef struct
{
	CalWeekdays	working_days;
	gboolean	use_24_hour_format;
	gint		week_start_day;
	gint		day_start_hour;
	gint		day_start_minute;
	gint		day_end_hour;
	gint		day_end_minute;
	gint		time_divisions;
	gboolean	dnav_show_week_no;
	gint		view;
	gfloat		hpane_pos;
	gfloat		vpane_pos;
	gfloat		month_hpane_pos;
	gfloat		month_vpane_pos;
	gboolean	compress_weekend;
	gboolean	show_event_end;
} CalendarConfig;


static CalendarConfig *config = NULL;

static void config_read			(void);


void
calendar_config_init			(void)
{
	if (config)
		return;
	
	config = g_new0 (CalendarConfig, 1);

	config_read ();
}


static void
config_read				(void)
{
	gchar *prefix;
	gboolean is_default;

	/* 'Display' settings. */
	prefix = g_strdup_printf ("=%s/config/Calendar=/Display/",
				  evolution_dir);
	gnome_config_push_prefix (prefix);
	g_free (prefix);

	config->working_days = gnome_config_get_int_with_default ("WorkingDays", &is_default);
	if (is_default) {
		config->working_days = CAL_MONDAY | CAL_TUESDAY
			| CAL_WEDNESDAY | CAL_THURSDAY | CAL_FRIDAY;
	}
	config->use_24_hour_format = gnome_config_get_bool ("Use24HourFormat=0");
	config->week_start_day = gnome_config_get_int ("WeekStartDay=1");
	config->day_start_hour = gnome_config_get_int ("DayStartHour=9");
	config->day_start_minute = gnome_config_get_int ("DayStartMinute=0");
	config->day_end_hour = gnome_config_get_int ("DayEndHour=17");
	config->day_end_minute = gnome_config_get_int ("DayEndMinute=0");
	config->time_divisions = gnome_config_get_int ("TimeDivisions=30");
	config->view = gnome_config_get_int ("View=0");
	config->hpane_pos = gnome_config_get_float ("HPanePosition=1");
	config->vpane_pos = gnome_config_get_float ("VPanePosition=1");
	config->month_hpane_pos = gnome_config_get_float ("MonthHPanePosition=0");
	config->month_vpane_pos = gnome_config_get_float ("MonthVPanePosition=1");
	config->compress_weekend = gnome_config_get_bool ("CompressWeekend=1");
	config->show_event_end = gnome_config_get_bool ("ShowEventEndTime=1");

	gnome_config_pop_prefix ();


	/* 'DateNavigator' settings. */
	prefix = g_strdup_printf ("=%s/config/Calendar=/DateNavigator/",
				  evolution_dir);
	gnome_config_push_prefix (prefix);
	g_free (prefix);

	config->dnav_show_week_no = gnome_config_get_bool ("ShowWeekNumbers=0");

	gnome_config_pop_prefix ();


	gnome_config_sync ();
}


void
calendar_config_write			(void)
{
	gchar *prefix;
	
	/* 'Display' settings. */
	prefix = g_strdup_printf ("=%s/config/Calendar=/Display/",
				  evolution_dir);
	gnome_config_push_prefix (prefix);
	g_free (prefix);

	gnome_config_set_int ("WorkingDays", config->working_days);
	gnome_config_set_bool ("Use24HourFormat", config->use_24_hour_format);
	gnome_config_set_int ("WeekStartDay", config->week_start_day);
	gnome_config_set_int ("DayStartHour", config->day_start_hour);
	gnome_config_set_int ("DayStartMinute", config->day_start_minute);
	gnome_config_set_int ("DayEndHour", config->day_end_hour);
	gnome_config_set_int ("DayEndMinute", config->day_end_minute);
	gnome_config_set_bool ("CompressWeekend", config->compress_weekend);
	gnome_config_set_bool ("ShowEventEndTime", config->show_event_end);

	gnome_config_pop_prefix ();


	/* 'DateNavigator' settings. */
	prefix = g_strdup_printf ("=%s/config/Calendar=/DateNavigator/",
				  evolution_dir);
	gnome_config_push_prefix (prefix);
	g_free (prefix);

	gnome_config_set_bool ("ShowWeekNumbers", config->dnav_show_week_no);

	gnome_config_pop_prefix ();


	gnome_config_sync ();
}


void
calendar_config_write_on_exit		(void)
{
	gchar *prefix;
	
	/* 'Display' settings. */
	prefix = g_strdup_printf ("=%s/config/Calendar=/Display/",
				  evolution_dir);
	gnome_config_push_prefix (prefix);
	g_free (prefix);

	gnome_config_set_int ("View", config->view);
	gnome_config_set_int ("TimeDivisions", config->time_divisions);
	gnome_config_set_float ("HPanePosition", config->hpane_pos);
	gnome_config_set_float ("VPanePosition", config->vpane_pos);
	gnome_config_set_float ("MonthHPanePosition", config->month_hpane_pos);
	gnome_config_set_float ("MonthVPanePosition", config->month_vpane_pos);

	gnome_config_pop_prefix ();


	gnome_config_sync ();
}


/*
 * Calendar Settings.
 */

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean
calendar_config_get_24_hour_format	(void)
{
	return config->use_24_hour_format;
}


void
calendar_config_set_24_hour_format	(gboolean     use_24_hour)
{
	config->use_24_hour_format = use_24_hour;
}


/* The start day of the week (0 = Sun to 6 = Mon). */
gint
calendar_config_get_week_start_day	(void)
{
	return config->week_start_day;
}


void
calendar_config_set_week_start_day	(gint	      week_start_day)
{
	config->week_start_day = week_start_day;
}


/* The start and end times of the work-day. */
gint
calendar_config_get_day_start_hour	(void)
{
	return config->day_start_hour;
}


void
calendar_config_set_day_start_hour	(gint	      day_start_hour)
{
	config->day_start_hour = day_start_hour;
}


gint
calendar_config_get_day_start_minute	(void)
{
	return config->day_start_minute;
}


void
calendar_config_set_day_start_minute	(gint	      day_start_min)
{
	config->day_start_minute = day_start_min;
}


gint
calendar_config_get_day_end_hour	(void)
{
	return config->day_end_hour;
}


void
calendar_config_set_day_end_hour	(gint	      day_end_hour)
{
	config->day_end_hour = day_end_hour;
}


gint
calendar_config_get_day_end_minute	(void)
{
	return config->day_end_minute;
}


void
calendar_config_set_day_end_minute	(gint	      day_end_min)
{
	config->day_end_minute = day_end_min;
}


/* The time divisions in the Day/Work-Week view in minutes (5/10/15/30/60). */
gint
calendar_config_get_time_divisions	(void)
{
	return config->time_divisions;
}


void
calendar_config_set_time_divisions	(gint	      divisions)
{
	config->time_divisions = divisions;
}


/* Whether we show week numbers in the Date Navigator. */
gboolean
calendar_config_get_dnav_show_week_no	(void)
{
	return config->dnav_show_week_no;
}


void
calendar_config_set_dnav_show_week_no	(gboolean     show_week_no)
{
	config->dnav_show_week_no = show_week_no;
}


/* The view to show on start-up, 0 = Day, 1 = WorkWeek, 2 = Week, 3 = Month. */
gint
calendar_config_get_default_view	(void)
{
	return config->view;
}


void
calendar_config_set_default_view	(gint	      view)
{
	config->view = view;
}


/* The positions of the panes in the normal and month views. */
gfloat
calendar_config_get_hpane_pos		(void)
{
	return config->hpane_pos;
}


void
calendar_config_set_hpane_pos		(gfloat	      hpane_pos)
{
	config->hpane_pos = hpane_pos;
}


gfloat
calendar_config_get_vpane_pos		(void)
{
	return config->vpane_pos;
}


void
calendar_config_set_vpane_pos		(gfloat	      vpane_pos)
{
	config->vpane_pos = vpane_pos;
}


gfloat
calendar_config_get_month_hpane_pos	(void)
{
	return config->month_hpane_pos;
}


void
calendar_config_set_month_hpane_pos	(gfloat	      hpane_pos)
{
	config->month_hpane_pos = hpane_pos;
}


gfloat
calendar_config_get_month_vpane_pos	(void)
{
	return config->month_vpane_pos;
}


void
calendar_config_set_month_vpane_pos	(gfloat	      vpane_pos)
{
	config->month_vpane_pos = vpane_pos;
}


/* Whether we compress the weekend in the week/month views. */
gboolean
calendar_config_get_compress_weekend	(void)
{
	return config->compress_weekend;
}


void
calendar_config_set_compress_weekend	(gboolean     compress)
{
	config->compress_weekend = compress;
}


/* Whether we show event end times. */
gboolean
calendar_config_get_show_event_end	(void)
{
	return config->show_event_end;
}


void
calendar_config_set_show_event_end	(gboolean     show_end)
{
	config->show_event_end = show_end;
}


/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays
calendar_config_get_working_days	(void)
{
	return config->working_days;
}


void
calendar_config_set_working_days	(CalWeekdays  days)
{
	config->working_days = days;
}


/* This sets all the common config settings for an ECalendar widget.
   These are the week start day, and whether we show week numbers. */
void
calendar_config_configure_e_calendar	(ECalendar	*cal)
{
	gboolean dnav_show_week_no;
	gint week_start_day;

	g_return_if_fail (E_IS_CALENDAR (cal));

	dnav_show_week_no = calendar_config_get_dnav_show_week_no ();

	/* Note that this is 0 (Sun) to 6 (Sat). */
	week_start_day = calendar_config_get_week_start_day ();

	/* Convert it to 0 (Mon) to 6 (Sun), which is what we use. */
	week_start_day = (week_start_day + 6) % 7;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (cal->calitem),
			       "show_week_numbers", dnav_show_week_no,
			       "week_start_day", week_start_day,
			       NULL);
}


/* This sets all the common config settings for an EDateEdit widget.
   These are the week start day, whether we show week numbers, whether we
   use 24 hour format, and the hours of the working day to use in the time
   popup. */
void
calendar_config_configure_e_date_edit	(EDateEdit	*dedit)
{
	gboolean dnav_show_week_no, use_24_hour;
	gint week_start_day, start_hour, end_hour;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	dnav_show_week_no = calendar_config_get_dnav_show_week_no ();

	/* Note that this is 0 (Sun) to 6 (Sat). */
	week_start_day = calendar_config_get_week_start_day ();

	/* Convert it to 0 (Mon) to 6 (Sun), which is what we use. */
	week_start_day = (week_start_day + 6) % 7;

	use_24_hour = calendar_config_get_24_hour_format ();

	start_hour = calendar_config_get_day_start_hour ();
	end_hour = calendar_config_get_day_end_hour ();
	/* Round up the end hour. */
	if (calendar_config_get_day_end_minute () != 0)
		end_hour = end_hour + 1 % 24;

	e_date_edit_set_week_start_day (dedit, week_start_day);
	e_date_edit_set_show_week_numbers (dedit, dnav_show_week_no);
	e_date_edit_set_use_24_hour_format (dedit, use_24_hour);
	e_date_edit_set_time_popup_range (dedit, start_hour, end_hour);
}


/* This sets all the common config settings for an ECellDateEdit ETable item.
   These are the settings for the ECalendar popup and the time list (if we use
   24 hour format, and the hours of the working day). */
void
calendar_config_configure_e_cell_date_edit	(ECellDateEdit	*ecde)
{
	gboolean use_24_hour;
	gint start_hour, end_hour;

	g_return_if_fail (E_IS_CELL_DATE_EDIT (ecde));

	calendar_config_configure_e_calendar (E_CALENDAR (ecde->calendar));

	use_24_hour = calendar_config_get_24_hour_format ();

	start_hour = calendar_config_get_day_start_hour ();
	end_hour = calendar_config_get_day_end_hour ();
	/* Round up the end hour. */
	if (calendar_config_get_day_end_minute () != 0)
		end_hour = end_hour + 1 % 24;

	e_cell_date_edit_freeze (ecde);
	gtk_object_set (GTK_OBJECT (ecde),
			"use_24_hour_format", use_24_hour,
			"lower_hour", start_hour,
			"upper_hour", end_hour,
			NULL);
	e_cell_date_edit_thaw (ecde);
}


/* This sets all the common config settings for an ECalendarTable widget.
   These are the settings for the ECalendar popup and the time list (if we use
   24 hour format, and the hours of the working day). */
void
calendar_config_configure_e_calendar_table	(ECalendarTable	*cal_table)
{
	CalendarModel *model;
	gboolean use_24_hour;

	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	use_24_hour = calendar_config_get_24_hour_format ();

	model = e_calendar_table_get_model (cal_table);
	calendar_model_set_use_24_hour_format (model, use_24_hour);

	calendar_config_configure_e_cell_date_edit (cal_table->dates_cell);
}
