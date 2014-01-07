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

struct _EShell;

void cal_comp_util_add_exdate (ECalComponent *comp, time_t t, icaltimezone *zone);

/* Returns TRUE if the component uses the given timezone for both DTSTART
 * and DTEND, or if the UTC offsets of the start and end times are the same
 * as in the given zone. */
gboolean cal_comp_util_compare_event_timezones (ECalComponent *comp,
						ECalClient *client,
						icaltimezone *zone);

/* Returns the number of icons owned by the ECalComponent */
gint     cal_comp_util_get_n_icons (ECalComponent *comp, GSList **pixbufs);

gboolean cal_comp_is_on_server (ECalComponent *comp,
				ECalClient *client);
gboolean is_icalcomp_on_the_server (icalcomponent *icalcomp, ECalClient *client);

ECalComponent *	cal_comp_event_new_with_defaults
						(ECalClient *client,
						 gboolean all_day,
						 gboolean use_default_reminder,
						 gint default_reminder_interval,
						 EDurationType default_reminder_units);
ECalComponent *	cal_comp_event_new_with_current_time
						(ECalClient *client,
						 gboolean all_day,
						 gboolean use_default_reminder,
						 gint default_reminder_interval,
						 EDurationType default_reminder_units);
ECalComponent *cal_comp_task_new_with_defaults (ECalClient *client);
ECalComponent *cal_comp_memo_new_with_defaults (ECalClient *client);

void cal_comp_update_time_by_active_window (ECalComponent *comp, struct _EShell *shell);

void    cal_comp_selection_set_string_list (GtkSelectionData *data, GSList *str_list);
GSList *cal_comp_selection_get_string_list (GtkSelectionData *data);

void cal_comp_set_dtstart_with_oldzone (ECalClient *client, ECalComponent *comp, const ECalComponentDateTime *pdate);
void cal_comp_set_dtend_with_oldzone (ECalClient *client, ECalComponent *comp, const ECalComponentDateTime *pdate);

void comp_util_sanitize_recurrence_master (ECalComponent *comp, ECalClient *client);

gchar *icalcomp_suggest_filename (icalcomponent *icalcomp, const gchar *default_name);

void cal_comp_transfer_item_to			(ECalClient *src_client,
						 ECalClient *dest_client,
						 icalcomponent *icalcomp_vcal,
						 gboolean do_copy,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean cal_comp_transfer_item_to_finish	(ECalClient *client,
						 GAsyncResult *result,
						 GError **error);
gboolean cal_comp_transfer_item_to_sync		(ECalClient *src_client,
						 ECalClient *dest_client,
						 icalcomponent *icalcomp_event,
						 gboolean do_copy,
						 GCancellable *cancellable,
						 GError **error);

#endif
