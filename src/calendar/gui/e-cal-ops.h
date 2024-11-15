/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#ifndef E_CAL_OPS_H
#define E_CAL_OPS_H

#include <libecal/libecal.h>
#include <shell/e-shell-window.h>
#include <shell/e-shell-view.h>
#include <calendar/gui/e-cal-model.h>

typedef void (* ECalOpsCreateComponentFunc)	(ECalModel *model,
						 ECalClient *client,
						 ICalComponent *original_icomp,
						 const gchar *new_uid,
						 gpointer user_data);
typedef void (* ECalOpsGetDefaultComponentFunc)	(ECalModel *model,
						 ECalClient *client,
						 ICalComponent *default_component,
						 gpointer user_data);

typedef enum {
	E_CAL_OPS_SEND_FLAG_ASK			= 0,
	E_CAL_OPS_SEND_FLAG_SEND		= 1 << 0,
	E_CAL_OPS_SEND_FLAG_DONT_SEND		= 1 << 1,
	E_CAL_OPS_SEND_FLAG_IS_NEW_COMPONENT	= 1 << 2,
	E_CAL_OPS_SEND_FLAG_ONLY_NEW_ATTENDEES	= 1 << 3,
	E_CAL_OPS_SEND_FLAG_STRIP_ALARMS	= 1 << 4
} ECalOpsSendFlags;

void	e_cal_ops_create_component		(ECalModel *model,
						 ECalClient *client,
						 ICalComponent *icomp,
						 ECalOpsCreateComponentFunc callback,
						 gpointer user_data,
						 GDestroyNotify user_data_free);
void	e_cal_ops_modify_component		(ECalDataModel *data_model,
						 ECalClient *client,
						 ICalComponent *icomp,
						 ECalObjModType mod,
						 ECalOpsSendFlags send_flags);
void	e_cal_ops_remove_component		(ECalDataModel *data_model,
						 ECalClient *client,
						 const gchar *uid,
						 const gchar *rid,
						 ECalObjModType mod,
						 gboolean check_detached_instance,
						 ECalOperationFlags op_flags);
void	e_cal_ops_delete_ecalmodel_components	(ECalModel *model,
						 const GSList *objects); /* data is 'ECalModelComponent *' */
void	e_cal_ops_paste_components		(ECalModel *model,
						 const gchar *icompstr);
void	e_cal_ops_send_component		(ECalModel *model,
						 ECalClient *client,
						 ICalComponent *icomp);
void	e_cal_ops_purge_components		(ECalModel *model,
						 time_t older_than);
void	e_cal_ops_delete_completed_tasks	(ECalModel *model);
void	e_cal_ops_get_default_component		(ECalModel *model,
						 const gchar *for_client_uid,
						 gboolean all_day,
						 ECalOpsGetDefaultComponentFunc callback,
						 gpointer user_data,
						 GDestroyNotify user_data_free);

void	e_cal_ops_new_component_editor		(EShellWindow *shell_window,
						 ECalClientSourceType source_type,
						 const gchar *for_client_uid,
						 gboolean is_assigned);
void	e_cal_ops_new_event_editor		(EShellWindow *shell_window,
						 const gchar *for_client_uid,
						 gboolean is_meeting,
						 gboolean all_day,
						 gboolean use_default_reminder,
						 gint default_reminder_interval,
						 EDurationType default_reminder_units,
						 time_t dtstart,
						 time_t dtend);
void	e_cal_ops_new_component_editor_from_model
						(ECalModel *model,
						 const gchar *for_client_uid,
						 time_t dtstart,
						 time_t dtend,
						 gboolean is_assigned,
						 gboolean all_day);
void	e_cal_ops_open_component_in_editor_sync	(ECalModel *model,
						 ECalClient *client,
						 ICalComponent *icomp,
						 gboolean force_attendees);

void	e_cal_ops_transfer_components		(EShellView *shell_view,
						 ECalModel *model,
						 ECalClientSourceType source_type,
						 GHashTable *icomps_by_source, /* ESource ~> GSList{ICalComponent} */
						 ESource *destination,
						 gboolean is_move);

#endif /* E_CAL_OPS_H */
