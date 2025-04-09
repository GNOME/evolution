/*
 * Evolution calendar - Utilities for manipulating ECalComponent objects
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef COMP_UTIL_H
#define COMP_UTIL_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>

#include <e-util/e-util.h>
#include <calendar/gui/e-cal-data-model.h>
#include <calendar/gui/e-cal-model.h>

struct _EShell;

void		cal_comp_util_add_exdate	(ECalComponent *comp,
						 time_t t,
						 ICalTimezone *zone);

/* Returns TRUE if the component uses the given timezone for both DTSTART
 * and DTEND, or if the UTC offsets of the start and end times are the same
 * as in the given zone. */
gboolean	cal_comp_util_compare_event_timezones
						(ECalComponent *comp,
						 ECalClient *client,
						 ICalTimezone *zone);

/* Returns the number of icons owned by the ECalComponent */
gint     	cal_comp_util_get_n_icons	(ECalComponent *comp,
						 GSList **pixbufs);

gboolean	cal_comp_is_on_server_sync	(ECalComponent *comp,
						 ECalClient *client,
						 GCancellable *cancellable,
						 GError **error);
gboolean	cal_comp_is_icalcomp_on_server_sync
						(ICalComponent *icomp,
						 ECalClient *client,
						 GCancellable *cancellable,
						 GError **error);

ECalComponent *	cal_comp_event_new_with_defaults_sync
						(ECalClient *client,
						 gboolean all_day,
						 gboolean use_default_reminder,
						 gint default_reminder_interval,
						 EDurationType default_reminder_units,
						 GCancellable *cancellable,
						 GError **error);
ECalComponent *	cal_comp_event_new_with_current_time_sync
						(ECalClient *client,
						 gboolean all_day,
						 gboolean use_default_reminder,
						 gint default_reminder_interval,
						 EDurationType default_reminder_units,
						 GCancellable *cancellable,
						 GError **error);
ECalComponent *	cal_comp_task_new_with_defaults_sync
						(ECalClient *client,
						 GCancellable *cancellable,
						 GError **error);
ECalComponent *	cal_comp_memo_new_with_defaults_sync
						(ECalClient *client,
						 GCancellable *cancellable,
						 GError **error);

void		cal_comp_update_time_by_active_window
						(ECalComponent *comp,
						 struct _EShell *shell);

void    	cal_comp_selection_set_string_list
						(GtkSelectionData *data,
						 GSList *str_list);
GSList *	cal_comp_selection_get_string_list
						(GtkSelectionData *data);

void		cal_comp_set_dtstart_with_oldzone
						(ECalClient *client,
						 ECalComponent *comp,
						 const ECalComponentDateTime *pdate);
void		cal_comp_set_dtend_with_oldzone	(ECalClient *client,
						 ECalComponent *comp,
						 const ECalComponentDateTime *pdate);

gboolean	comp_util_sanitize_recurrence_master_sync
						(ECalComponent *comp,
						 ECalClient *client,
						 GCancellable *cancellable,
						 GError **error);

gchar *		comp_util_suggest_filename	(ICalComponent *icomp,
						 const gchar *default_name);

void		cal_comp_get_instance_times	(ECalClient *client,
						 ICalComponent *icomp,
						 const ICalTimezone *default_zone,
						 ICalTime **out_instance_start,
						 ICalTime **out_instance_end,
						 GCancellable *cancellable);
time_t		cal_comp_gdate_to_timet		(const GDate *date,
						 const ICalTimezone *with_zone);

void		cal_comp_transfer_item_to	(ECalClient *src_client,
						 ECalClient *dest_client,
						 ICalComponent *icomp_vcal,
						 gboolean do_copy,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	cal_comp_transfer_item_to_finish(ECalClient *client,
						 GAsyncResult *result,
						 GError **error);
gboolean	cal_comp_transfer_item_to_sync	(ECalClient *src_client,
						 ECalClient *dest_client,
						 ICalComponent *icomp_event,
						 gboolean do_copy,
						 GCancellable *cancellable,
						 GError **error);
void		cal_comp_util_update_tzid_parameter
						(ICalProperty *prop,
						 const ICalTime *tt);
gint		cal_comp_util_compare_time_with_today
						(const ICalTime *time_tt);
gboolean	cal_comp_util_have_in_new_attendees
						(const GSList *new_attendees_mails,
						 const gchar *eml);
void		cal_comp_util_copy_new_attendees
						(ECalComponent *des,
						 ECalComponent *src);
void		cal_comp_util_set_added_attendees_mails
						(ECalComponent *comp,
						 GSList *emails);
gchar *		cal_comp_util_dup_parameter_xvalue
						(ICalProperty *prop,
						 const gchar *name);
gchar *		cal_comp_util_get_attendee_comments
						(ICalComponent *icomp);
const gchar *	cal_comp_util_status_to_localized_string
						(ICalComponentKind kind,
						 ICalPropertyStatus status);
ICalPropertyStatus
		cal_comp_util_localized_string_to_status
						(ICalComponentKind kind,
						 const gchar *localized_string,
						 GCompareDataFunc cmp_func,
						 gpointer user_data);
GList *		cal_comp_util_get_status_list_for_kind /* const gchar * */
						(ICalComponentKind kind);
gboolean	cal_comp_util_ensure_allday_timezone
						(ICalTime *itime,
						 ICalTimezone *zone);
void		cal_comp_util_maybe_ensure_allday_timezone_properties
						(ECalClient *client,
						 ICalComponent *icomp,
						 ICalTimezone *zone);
void		cal_comp_util_format_itt	(ICalTime *itt,
						 gchar *buffer,
						 gint buffer_size);
ICalTime *	cal_comp_util_date_time_to_zone	(ECalComponentDateTime *dt,
						 ECalClient *client,
						 ICalTimezone *default_zone);
gchar *		cal_comp_util_dup_attendees_status_info
						(ECalComponent *comp,
						 ECalClient *cal_client,
						 ESourceRegistry *registry);

typedef enum _ECalCompUtilDescribeFlags {
	E_CAL_COMP_UTIL_DESCRIBE_FLAG_NONE		= 0,
	E_CAL_COMP_UTIL_DESCRIBE_FLAG_RTL		= 1 << 0,
	E_CAL_COMP_UTIL_DESCRIBE_FLAG_USE_MARKUP	= 1 << 1,
	E_CAL_COMP_UTIL_DESCRIBE_FLAG_ONLY_TIME		= 1 << 2,
	E_CAL_COMP_UTIL_DESCRIBE_FLAG_24HOUR_FORMAT	= 1 << 3
} ECalCompUtilDescribeFlags;

/**
 * ECalCompUtilDescribeFlags:
 * @E_CAL_COMP_UTIL_DESCRIBE_FLAG_NONE: no special flag set
 * @E_CAL_COMP_UTIL_DESCRIBE_FLAG_RTL: set to order text in right-to-left direction
 * @E_CAL_COMP_UTIL_DESCRIBE_FLAG_USE_MARKUP: use markup in the output texts
 * @E_CAL_COMP_UTIL_DESCRIBE_FLAG_ONLY_TIME: show only time, instead of date and time, for times
 * @E_CAL_COMP_UTIL_DESCRIBE_FLAG_24HOUR_FORMAT: use 24-hour format for ONLY_TIME values
 *
 * Flags to use for cal_comp_util_describe().
 *
 * Since: 3.46
 **/

gchar *		cal_comp_util_describe		(ECalComponent *comp,
						 ECalClient *client,
						 ICalTimezone *default_zone,
						 ECalCompUtilDescribeFlags flags);
gchar *		cal_comp_util_dup_tooltip	(ECalComponent *comp,
						 ECalClient *client,
						 ESourceRegistry *registry,
						 ICalTimezone *default_zone);
gboolean	cal_comp_util_move_component_by_days
						(GtkWindow *parent,
						 ECalModel *model,
						 ECalClient *client,
						 ECalComponent *in_comp,
						 gint days,
						 gboolean is_move);
void		cal_comp_util_add_reminder	(ECalComponent *comp,
						 gint reminder_interval,
						 EDurationType reminder_units);
gchar *		cal_comp_util_dup_attach_filename
						(ICalProperty *attach_prop,
						 gboolean with_fallback);

typedef enum {
	E_COMP_TO_HTML_FLAG_NONE		= 0,
	E_COMP_TO_HTML_FLAG_ALLOW_ICONS		= 1 << 0
} ECompToHTMLFlags;

void		cal_comp_util_write_to_html	(GString *html_buffer,
						 ECalClient *client,
						 ECalComponent *comp,
						 ICalTimezone *zone,
						 ECompToHTMLFlags flags);
void		cal_comp_util_remove_component	(GtkWindow *parent_window, /* for question dialog */
						 ECalDataModel *data_model, /* used to submit thread job */
						 ECalClient *client,
						 ECalComponent *comp,
						 ECalObjModType mod,
						 gboolean confirm_event_delete); /* meetings are always asked */
gboolean	cal_comp_util_set_color_for_component
						(ECalClient *client,
						 ICalComponent *icomp,
						 gchar **inout_color_spec);

#endif
