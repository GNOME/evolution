/*
 * Evolution calendar - Copy source dialog
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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"

#include "copy-source-dialog.h"
#include "select-source-dialog.h"

typedef struct {
	ECalModel *model;
	ESource *from_source;
	ESource *to_source;
	ECalClient *to_client;
	const gchar *extension_name;
} CopySourceData;

static void
copy_source_data_free (gpointer ptr)
{
	CopySourceData *csd = ptr;

	if (csd) {
		if (csd->to_client)
			e_cal_model_emit_object_created (csd->model, csd->to_client);

		g_clear_object (&csd->model);
		g_clear_object (&csd->from_source);
		g_clear_object (&csd->to_source);
		g_clear_object (&csd->to_client);
		g_free (csd);
	}
}

struct ForeachTzidData
{
	ECalClient *from_client;
	ECalClient *to_client;
	gboolean success;
	GCancellable *cancellable;
	GError **error;
};

static void
add_timezone_to_cal_cb (icalparameter *param,
                        gpointer data)
{
	struct ForeachTzidData *ftd = data;
	icaltimezone *tz = NULL;
	const gchar *tzid;

	g_return_if_fail (ftd != NULL);
	g_return_if_fail (ftd->from_client != NULL);
	g_return_if_fail (ftd->to_client != NULL);

	if (!ftd->success)
		return;

	tzid = icalparameter_get_tzid (param);
	if (!tzid || !*tzid)
		return;

	if (g_cancellable_set_error_if_cancelled (ftd->cancellable, ftd->error)) {
		ftd->success = FALSE;
		return;
	}

	ftd->success = e_cal_client_get_timezone_sync (ftd->from_client, tzid, &tz, ftd->cancellable, ftd->error);
	if (ftd->success && tz != NULL)
		ftd->success = e_cal_client_add_timezone_sync (ftd->to_client, tz, ftd->cancellable, ftd->error);
}

static void
copy_source_thread (EAlertSinkThreadJobData *job_data,
		    gpointer user_data,
		    GCancellable *cancellable,
		    GError **error)
{
	CopySourceData *csd = user_data;
	EClient *client;
	ECalClient *from_client = NULL, *to_client = NULL;
	GSList *objects = NULL, *link;
	struct ForeachTzidData ftd;
	gint n_objects, ii, last_percent = 0;

	if (!csd)
		goto out;

	client = e_util_open_client_sync (job_data, e_cal_model_get_client_cache (csd->model), csd->extension_name, csd->from_source, 30, cancellable, error);
	if (client)
		from_client = E_CAL_CLIENT (client);

	if (!from_client)
		goto out;

	client = e_util_open_client_sync (job_data, e_cal_model_get_client_cache (csd->model), csd->extension_name, csd->to_source, 30, cancellable, error);
	if (client)
		to_client = E_CAL_CLIENT (client);

	if (!to_client)
		goto out;

	if (e_client_is_readonly (E_CLIENT (to_client))) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_READ_ONLY, _("Destination is read only"));
		goto out;
	}

	if (!e_cal_client_get_object_list_sync (from_client, "#t", &objects, cancellable, error))
		goto out;

	ftd.from_client = from_client;
	ftd.to_client = to_client;
	ftd.success = TRUE;
	ftd.cancellable = cancellable;
	ftd.error = error;

	n_objects = g_slist_length (objects);

	for (link = objects, ii = 0; link && ftd.success && !g_cancellable_is_cancelled (cancellable); link = g_slist_next (link), ii++) {
		icalcomponent *icalcomp = link->data;
		icalcomponent *existing_icalcomp = NULL;
		gint percent = 100 * (ii + 1) / n_objects;
		GError *local_error = NULL;

		if (e_cal_client_get_object_sync (to_client, icalcomponent_get_uid (icalcomp), NULL, &existing_icalcomp, cancellable, &local_error) &&
		    icalcomp != NULL) {
			if (!e_cal_client_modify_object_sync (to_client, icalcomp, E_CAL_OBJ_MOD_ALL, cancellable, error))
				break;

			icalcomponent_free (existing_icalcomp);
		} else if (local_error && !g_error_matches (local_error, E_CAL_CLIENT_ERROR, E_CAL_CLIENT_ERROR_OBJECT_NOT_FOUND)) {
			g_propagate_error (error, local_error);
			break;
		} else {
			icalcomponent_foreach_tzid (icalcomp, add_timezone_to_cal_cb, &ftd);

			g_clear_error (&local_error);

			if (!ftd.success)
				break;

			if (!e_cal_client_create_object_sync (to_client, icalcomp, NULL, cancellable, error))
				break;
		}

		if (percent != last_percent) {
			camel_operation_progress (cancellable, percent);
			last_percent = percent;
		}
	}

	if (ii > 0 && ftd.success)
		csd->to_client = g_object_ref (to_client);
 out:
	e_cal_client_free_icalcomp_slist (objects);
	g_clear_object (&from_client);
	g_clear_object (&to_client);
}

/**
 * copy_source_dialog
 *
 * Implements the Copy command for sources, allowing the user to select a target
 * source to copy to.
 */
void
copy_source_dialog (GtkWindow *parent,
                    ECalModel *model,
                    ESource *from_source)
{
	ECalClientSourceType obj_type;
	ESource *to_source;
	const gchar *extension_name;
	const gchar *format;
	const gchar *alert_ident;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_SOURCE (from_source));

	switch (e_cal_model_get_component_kind (model)) {
		case ICAL_VEVENT_COMPONENT:
			obj_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			format = _("Copying events to the calendar '%s'");
			alert_ident = "calendar:failed-copy-event";
			break;
		case ICAL_VJOURNAL_COMPONENT:
			obj_type = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			format = _("Copying memos to the memo list '%s'");
			alert_ident = "calendar:failed-copy-memo";
			break;
		case ICAL_VTODO_COMPONENT:
			obj_type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			format = _("Copying tasks to the task list '%s'");
			alert_ident = "calendar:failed-copy-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	to_source = select_source_dialog (parent, e_cal_model_get_registry (model), obj_type, from_source);
	if (to_source) {
		CopySourceData *csd;
		GCancellable *cancellable;
		ECalDataModel *data_model;
		gchar *display_name;
		gchar *description;

		csd = g_new0 (CopySourceData, 1);
		csd->model = g_object_ref (model);
		csd->from_source = g_object_ref (from_source);
		csd->to_source = g_object_ref (to_source);
		csd->to_client = NULL;
		csd->extension_name = extension_name;

		display_name = e_util_get_source_full_name (e_cal_model_get_registry (model), to_source);
		description = g_strdup_printf (format, display_name);
		data_model = e_cal_model_get_data_model (model);

		cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident, display_name,
			copy_source_thread, csd, copy_source_data_free);

		g_clear_object (&cancellable);
		g_free (display_name);
		g_free (description);
	}

	g_clear_object (&to_source);
}
