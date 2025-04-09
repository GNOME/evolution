/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CAL_DAY_COLUMN_H
#define E_CAL_DAY_COLUMN_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <e-util/e-util.h>
#include <calendar/gui/e-cal-range-model.h>

G_BEGIN_DECLS

#define E_TYPE_CAL_DAY_COLUMN e_cal_day_column_get_type ()

G_DECLARE_FINAL_TYPE (ECalDayColumn, e_cal_day_column, E, CAL_DAY_COLUMN, GtkFixed)

ECalDayColumn *	e_cal_day_column_new		(EClientCache *client_cache,
						 EAlertSink *alert_sink,
						 ECalRangeModelSourceFilterFunc source_filter_func,
						 gpointer source_filter_user_data);
void		e_cal_day_column_set_time_division_minutes
						(ECalDayColumn *self,
						 guint minutes);
guint		e_cal_day_column_get_time_division_minutes
						(ECalDayColumn *self);
void		e_cal_day_column_set_timezone	(ECalDayColumn *self,
						 ICalTimezone *zone);
ICalTimezone *	e_cal_day_column_get_timezone	(ECalDayColumn *self);
void		e_cal_day_column_set_use_24hour_format
						(ECalDayColumn *self,
						 gboolean value);
gboolean	e_cal_day_column_get_use_24hour_format
						(ECalDayColumn *self);
void		e_cal_day_column_set_show_time	(ECalDayColumn *self,
						 gboolean value);
gboolean	e_cal_day_column_get_show_time	(ECalDayColumn *self);
void		e_cal_day_column_set_range	(ECalDayColumn *self,
						 time_t start,
						 time_t end);
void		e_cal_day_column_get_range	(ECalDayColumn *self,
						 time_t *out_start,
						 time_t *out_end);
void		e_cal_day_column_layout_for_width
						(ECalDayColumn *self,
						 guint min_width);
gint		e_cal_day_column_time_to_y	(ECalDayColumn *self,
						 guint hour,
						 guint minute);
gboolean	e_cal_day_column_y_to_time	(ECalDayColumn *self,
						 gint yy,
						 guint *out_hour,
						 guint *out_minute);
void		e_cal_day_column_highlight_time	(ECalDayColumn *self,
						 ECalClient *client,
						 const gchar *uid,
						 guint hour_start,
						 guint minute_start,
						 guint hour_end,
						 guint minute_end,
						 const GdkRGBA *bg_rgba_freetime,
						 const GdkRGBA *bg_rgba_clash);

G_END_DECLS

#endif /* E_CAL_DAY_COLUMN_H */
