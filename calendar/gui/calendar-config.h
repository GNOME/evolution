/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
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
 * calendar-config.h - functions to load/save/get/set user settings.
 */

#ifndef _CALENDAR_CONFIG_H_
#define _CALENDAR_CONFIG_H_


/* These are used to get/set the working days in the week. The bit-flags are
   combined together. The bits must be from 0 (Sun) to 6 (Sat) to match the
   day values used by localtime etc. */
typedef enum
{
	CAL_SUNDAY	= 1 << 0,
	CAL_MONDAY	= 1 << 1,
	CAL_TUESDAY	= 1 << 2,
	CAL_WEDNESDAY	= 1 << 3,
	CAL_THURSDAY	= 1 << 4,
	CAL_FRIDAY	= 1 << 5,
	CAL_SATURDAY	= 1 << 6
} CalWeekdays;



void	  calendar_config_init			(void);
void	  calendar_config_write			(void);
void	  calendar_config_write_on_exit		(void);


/*
 * Calendar Settings.
 */

/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays calendar_config_get_working_days	(void);
void	  calendar_config_set_working_days	(CalWeekdays  days);

/* The start day of the week (0 = Sun to 6 = Sat). */
gint	  calendar_config_get_week_start_day	(void);
void	  calendar_config_set_week_start_day	(gint	      week_start_day);

/* The start and end times of the work-day. */
gint	  calendar_config_get_day_start_hour	(void);
void	  calendar_config_set_day_start_hour	(gint	      day_start_hour);

gint	  calendar_config_get_day_start_minute	(void);
void	  calendar_config_set_day_start_minute	(gint	      day_start_min);

gint	  calendar_config_get_day_end_hour	(void);
void	  calendar_config_set_day_end_hour	(gint	      day_end_hour);

gint	  calendar_config_get_day_end_minute	(void);
void	  calendar_config_set_day_end_minute	(gint	      day_end_min);

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean  calendar_config_get_24_hour_format	(void);
void	  calendar_config_set_24_hour_format	(gboolean     use_24_hour);

/* The time divisions in the Day/Work-Week view in minutes (5/10/15/30/60). */
gint	  calendar_config_get_time_divisions	(void);
void	  calendar_config_set_time_divisions	(gint	      divisions);

/* Whether we show event end times. */
gboolean  calendar_config_get_show_event_end	(void);
void	  calendar_config_set_show_event_end	(gboolean     show_end);

/* Whether we compress the weekend in the week/month views. */
gboolean  calendar_config_get_compress_weekend	(void);
void	  calendar_config_set_compress_weekend	(gboolean     compress);

/* Whether we show week numbers in the Date Navigator. */
gboolean  calendar_config_get_dnav_show_week_no	(void);
void	  calendar_config_set_dnav_show_week_no	(gboolean     show_week_no);

/* The positions of the panes in the normal and month views. */
gfloat    calendar_config_get_hpane_pos		(void);
void	  calendar_config_set_hpane_pos		(gfloat	      hpane_pos);

gfloat    calendar_config_get_vpane_pos		(void);
void	  calendar_config_set_vpane_pos		(gfloat	      vpane_pos);

gfloat    calendar_config_get_month_hpane_pos	(void);
void	  calendar_config_set_month_hpane_pos	(gfloat	      hpane_pos);

gfloat    calendar_config_get_month_vpane_pos	(void);
void	  calendar_config_set_month_vpane_pos	(gfloat	      vpane_pos);


#endif /* _CALENDAR_CONFIG_H_ */
