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

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>
#include <shell/e-shell-view.h>

#include "e-comp-editor.h"
#include "e-comp-editor-event.h"
#include "e-comp-editor-memo.h"
#include "e-comp-editor-task.h"
#include "e-cal-dialogs.h"
#include "calendar-config.h"
#include "comp-util.h"
#include "itip-utils.h"

#include "e-cal-data-model.h"

#include "e-cal-ops.h"

static void
cal_ops_manage_send_component (ECalDataModel *data_model,
			       ECalClient *client,
			       ICalComponent *icomp,
			       ECalObjModType mod,
			       ECalOpsSendFlags send_flags)
{
	ECalComponent *comp;
	ESourceRegistry *registry;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (I_CAL_IS_COMPONENT (icomp));

	if ((send_flags & E_CAL_OPS_SEND_FLAG_DONT_SEND) != 0)
		return;

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
	if (!comp)
		return;

	registry = e_cal_data_model_get_registry (data_model);

	if (itip_organizer_is_user (registry, comp, client)) {
		gboolean strip_alarms = (send_flags & E_CAL_OPS_SEND_FLAG_STRIP_ALARMS) != 0;
		gboolean only_new_attendees = (send_flags & E_CAL_OPS_SEND_FLAG_ONLY_NEW_ATTENDEES) != 0;
		gboolean can_send = (send_flags & E_CAL_OPS_SEND_FLAG_SEND) != 0;

		if (!can_send) /* E_CAL_OPS_SEND_FLAG_ASK */
			can_send = e_cal_dialogs_send_component (NULL, client, comp,
				(send_flags & E_CAL_OPS_SEND_FLAG_IS_NEW_COMPONENT) != 0,
				&strip_alarms, &only_new_attendees);

		if (can_send)
			itip_send_component_with_model (data_model, I_CAL_METHOD_REQUEST, comp, client,
				NULL, NULL, NULL,
				(strip_alarms ? E_ITIP_SEND_COMPONENT_FLAG_STRIP_ALARMS : 0) |
				(only_new_attendees ? E_ITIP_SEND_COMPONENT_FLAG_ONLY_NEW_ATTENDEES : 0) |
				(mod == E_CAL_OBJ_MOD_ALL ? E_ITIP_SEND_COMPONENT_FLAG_ENSURE_MASTER_OBJECT : 0));
	}

	g_clear_object (&comp);
}

typedef struct {
	ECalModel *model;
	ECalDataModel *data_model;
	ECalClient *client;
	ICalComponent *icomp;
	ECalObjModType mod;
	ECalOperationFlags op_flags;
	gchar *uid;
	gchar *rid;
	gboolean check_detached_instance;
	ECalOpsCreateComponentFunc create_cb;
	ECalOpsGetDefaultComponentFunc get_default_comp_cb;
	gboolean all_day_default_comp;
	gchar *for_client_uid;
	gboolean is_modify;
	ECalOpsSendFlags send_flags;
	gpointer user_data;
	GDestroyNotify user_data_free;
	gboolean success;
} BasicOperationData;

static BasicOperationData *
basic_operation_data_new (void)
{
	return g_slice_new0 (BasicOperationData);
}

static void
basic_operation_data_free (gpointer ptr)
{
	BasicOperationData *bod = ptr;

	if (bod) {
		if (bod->success) {
			if (bod->create_cb && bod->uid && bod->icomp) {
				bod->create_cb (bod->model, bod->client, bod->icomp, bod->uid, bod->user_data);
				if (bod->user_data_free)
					bod->user_data_free (bod->user_data);
			}

			if (bod->is_modify && bod->icomp && (bod->send_flags & E_CAL_OPS_SEND_FLAG_DONT_SEND) == 0) {
				ECalDataModel *data_model = bod->data_model;

				if (!data_model)
					data_model = e_cal_model_get_data_model (bod->model);

				cal_ops_manage_send_component (data_model, bod->client, bod->icomp, bod->mod, bod->send_flags);
			}

			if (bod->get_default_comp_cb && bod->icomp) {
				bod->get_default_comp_cb (bod->model, bod->client, bod->icomp, bod->user_data);
				if (bod->user_data_free)
					bod->user_data_free (bod->user_data);
			}
		}

		g_clear_object (&bod->model);
		g_clear_object (&bod->data_model);
		g_clear_object (&bod->client);
		g_clear_object (&bod->icomp);
		g_free (bod->for_client_uid);
		g_free (bod->uid);
		g_free (bod->rid);
		g_slice_free (BasicOperationData, bod);
	}
}

static void
cal_ops_create_component_thread (EAlertSinkThreadJobData *job_data,
				 gpointer user_data,
				 GCancellable *cancellable,
				 GError **error)
{
	BasicOperationData *bod = user_data;

	g_return_if_fail (bod != NULL);

	bod->success = e_cal_client_create_object_sync (bod->client, bod->icomp, E_CAL_OPERATION_FLAG_NONE, &bod->uid, cancellable, error);
}

/**
 * e_cal_ops_create_component:
 * @model: an #ECalModel
 * @client: an #ECalClient
 * @icomp: an #ICalComponent
 * @callback: (allow none): a callback to be called on success
 * @user_data: user data to be passed to @callback; ignored when @callback is #NULL
 * @user_data_free: a function to free @user_data; ignored when @callback is #NULL
 *
 * Creates a new @icomp in the @client. The @callback, if not #NULL,
 * is called with a new uid of the @icomp on successful component save.
 * The @callback is called in the main thread.
 *
 * Since: 3.16
 **/
void
e_cal_ops_create_component (ECalModel *model,
			    ECalClient *client,
			    ICalComponent *icomp,
			    ECalOpsCreateComponentFunc callback,
			    gpointer user_data,
			    GDestroyNotify user_data_free)
{
	ECalDataModel *data_model;
	ESource *source;
	ICalProperty *prop;
	const gchar *description;
	const gchar *alert_ident;
	gchar *display_name;
	BasicOperationData *bod;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (I_CAL_IS_COMPONENT (icomp));

	switch (e_cal_client_get_source_type (client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			description = _("Creating an event");
			alert_ident = "calendar:failed-create-event";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			description = _("Creating a memo");
			alert_ident = "calendar:failed-create-memo";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			description = _("Creating a task");
			alert_ident = "calendar:failed-create-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	data_model = e_cal_model_get_data_model (model);
	source = e_client_get_source (E_CLIENT (client));

	bod = basic_operation_data_new ();
	bod->model = g_object_ref (model);
	bod->client = g_object_ref (client);
	bod->icomp = i_cal_component_clone (icomp);
	bod->create_cb = callback;
	bod->user_data = user_data;
	bod->user_data_free = user_data_free;

	cal_comp_util_maybe_ensure_allday_timezone_properties (client, bod->icomp,
		e_cal_model_get_timezone (model));

	prop = i_cal_component_get_first_property (bod->icomp, I_CAL_CLASS_PROPERTY);
	if (!prop || i_cal_property_get_class (prop) == I_CAL_CLASS_NONE) {
		ICalProperty_Class ical_class = I_CAL_CLASS_PUBLIC;
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.calendar");
		if (g_settings_get_boolean (settings, "classify-private"))
			ical_class = I_CAL_CLASS_PRIVATE;
		g_object_unref (settings);

		if (!prop) {
			prop = i_cal_property_new_class (ical_class);
			i_cal_component_add_property (bod->icomp, prop);
		} else
			i_cal_property_set_class (prop, ical_class);
	}
	g_clear_object (&prop);

	display_name = e_util_get_source_full_name (e_cal_model_get_registry (model), source);
	cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident,
		display_name, cal_ops_create_component_thread,
		bod, basic_operation_data_free);

	g_clear_object (&cancellable);
	g_free (display_name);
}

static void
cal_ops_modify_component_thread (EAlertSinkThreadJobData *job_data,
				 gpointer user_data,
				 GCancellable *cancellable,
				 GError **error)
{
	BasicOperationData *bod = user_data;

	g_return_if_fail (bod != NULL);

	if (bod->mod == E_CAL_OBJ_MOD_ALL) {
		ECalComponent *comp;

		comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (bod->icomp));
		if (comp && e_cal_component_has_recurrences (comp)) {
			if (!comp_util_sanitize_recurrence_master_sync (comp, bod->client, cancellable, error)) {
				g_object_unref (comp);
				return;
			}

			g_clear_object (&bod->icomp);
			bod->icomp = i_cal_component_clone (e_cal_component_get_icalcomponent (comp));
		}

		g_clear_object (&comp);
	}

	bod->success = e_cal_client_modify_object_sync (bod->client, bod->icomp, bod->mod, E_CAL_OPERATION_FLAG_NONE, cancellable, error);
}

/**
 * e_cal_ops_modify_component:
 * @data_model: an #ECalDataModel
 * @client: an #ECalClient
 * @icomp: an #ICalComponent
 * @mod: a mode to use for modification of the component
 * @send_flags: what to do when the modify succeeded and the component has attendees
 *
 * Saves changes of the @icomp into the @client using the @mod. The @send_flags influences
 * what to do when the @icomp has attendees and the organizer is user. Only one of
 * #E_CAL_OPS_SEND_FLAG_ASK, #E_CAL_OPS_SEND_FLAG_SEND, #E_CAL_OPS_SEND_FLAG_DONT_SEND
 * can be used, while the ASK flag is the default.
 *
 * Since: 3.16
 **/
void
e_cal_ops_modify_component (ECalDataModel *data_model,
			    ECalClient *client,
			    ICalComponent *icomp,
			    ECalObjModType mod,
			    ECalOpsSendFlags send_flags)
{
	ESource *source;
	const gchar *description;
	const gchar *alert_ident;
	gchar *display_name;
	BasicOperationData *bod;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (I_CAL_IS_COMPONENT (icomp));

	switch (e_cal_client_get_source_type (client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			description = _("Modifying an event");
			alert_ident = "calendar:failed-modify-event";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			description = _("Modifying a memo");
			alert_ident = "calendar:failed-modify-memo";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			description = _("Modifying a task");
			alert_ident = "calendar:failed-modify-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	source = e_client_get_source (E_CLIENT (client));

	bod = basic_operation_data_new ();
	bod->data_model = g_object_ref (data_model);
	bod->client = g_object_ref (client);
	bod->icomp = i_cal_component_clone (icomp);
	bod->mod = mod;
	bod->send_flags = send_flags;
	bod->is_modify = TRUE;

	cal_comp_util_maybe_ensure_allday_timezone_properties (client, bod->icomp,
		e_cal_data_model_get_timezone (data_model));

	display_name = e_util_get_source_full_name (e_cal_data_model_get_registry (data_model), source);
	cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident,
		display_name, cal_ops_modify_component_thread,
		bod, basic_operation_data_free);

	g_clear_object (&cancellable);
	g_free (display_name);
}

static void
cal_ops_remove_component_thread (EAlertSinkThreadJobData *job_data,
				 gpointer user_data,
				 GCancellable *cancellable,
				 GError **error)
{
	BasicOperationData *bod = user_data;

	g_return_if_fail (bod != NULL);

	/* The check_detached_instance means to test whether the event is a detached instance,
	   then only that one is deleted, otherwise the master object is deleted */
	if (bod->check_detached_instance && bod->mod == E_CAL_OBJ_MOD_THIS && bod->rid && *bod->rid) {
		ICalComponent *icomp = NULL;
		GError *local_error = NULL;

		if (!e_cal_client_get_object_sync (bod->client, bod->uid, bod->rid, &icomp, cancellable, &local_error) &&
		    g_error_matches (local_error, E_CAL_CLIENT_ERROR, E_CAL_CLIENT_ERROR_OBJECT_NOT_FOUND)) {
			g_free (bod->rid);
			bod->rid = NULL;
			bod->mod = E_CAL_OBJ_MOD_ALL;
		}

		g_clear_error (&local_error);
		g_clear_object (&icomp);
	}

	bod->success = e_cal_client_remove_object_sync (bod->client, bod->uid, bod->rid, bod->mod, bod->op_flags, cancellable, error);
}

/**
 * e_cal_ops_remove_component:
 * @data_model: an #ECalDataModel
 * @client: an #ECalClient
 * @uid: a UID of the component to remove
 * @rid: (allow none): a recurrence ID of the component; can be #NULL
 * @mod: a mode to use for the component removal
 * @check_detached_instance: whether to test whether a detached instance is to be removed
 * @op_flags: operation flags, bit-or of #ECalOperationFlags
 *
 * Removes component identified by @uid and @rid from the @client using mode @mod.
 * The @check_detached_instance influences behaviour when removing only one instance.
 * If set to #TRUE, then it is checked first whether the component to be removed is
 * a detached instance. If it is, then only that one is removed (as requested), otherwise
 * the master objects is removed. If the @check_detached_instance is set to #FALSE, then
 * the removal is done exactly with the given values.
 *
 * Since: 3.16
 **/
void
e_cal_ops_remove_component (ECalDataModel *data_model,
			    ECalClient *client,
			    const gchar *uid,
			    const gchar *rid,
			    ECalObjModType mod,
			    gboolean check_detached_instance,
			    ECalOperationFlags op_flags)
{
	ESource *source;
	const gchar *description;
	const gchar *alert_ident;
	gchar *display_name;
	BasicOperationData *bod;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (uid != NULL);

	switch (e_cal_client_get_source_type (client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			description = _("Removing an event");
			alert_ident = "calendar:failed-remove-event";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			description = _("Removing a memo");
			alert_ident = "calendar:failed-remove-memo";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			description = _("Removing a task");
			alert_ident = "calendar:failed-remove-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	source = e_client_get_source (E_CLIENT (client));

	bod = basic_operation_data_new ();
	bod->data_model = g_object_ref (data_model);
	bod->client = g_object_ref (client);
	bod->uid = g_strdup (uid);
	bod->rid = g_strdup (rid);
	bod->mod = mod;
	bod->check_detached_instance = check_detached_instance;
	bod->op_flags = op_flags;

	display_name = e_util_get_source_full_name (e_cal_data_model_get_registry (data_model), source);
	cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident,
		display_name, cal_ops_remove_component_thread,
		bod, basic_operation_data_free);

	g_clear_object (&cancellable);
	g_free (display_name);
}

static void
cal_ops_delete_components_thread (EAlertSinkThreadJobData *job_data,
				  gpointer user_data,
				  GCancellable *cancellable,
				  GError **error)
{
	GSList *objects = user_data, *link;

	for (link = objects; link && !g_cancellable_is_cancelled (cancellable); link = g_slist_next (link)) {
		ECalModelComponent *comp_data = (ECalModelComponent *) link->data;
		gchar *rid;

		rid = e_cal_util_component_get_recurid_as_string (comp_data->icalcomp);

		if (!e_cal_client_remove_object_sync (
			comp_data->client, i_cal_component_get_uid (comp_data->icalcomp),
			rid, E_CAL_OBJ_MOD_THIS, E_CAL_OPERATION_FLAG_NONE, cancellable, error)) {
			ESource *source = e_client_get_source (E_CLIENT (comp_data->client));
			e_alert_sink_thread_job_set_alert_arg_0 (job_data, e_source_get_display_name (source));
			/* Stop on the first error */
			g_free (rid);
			break;
		}

		g_free (rid);
	}
}

/**
 * e_cal_ops_delete_ecalmodel_components:
 * @model: an #ECalModel
 * @objects: a #GSList of an #ECalModelComponent objects to delete
 *
 * Deletes all components from their sources. The @objects should
 * be part of @model.
 *
 * Since: 3.16
 **/
void
e_cal_ops_delete_ecalmodel_components (ECalModel *model,
				       const GSList *objects)
{
	ECalDataModel *data_model;
	GCancellable *cancellable;
	const gchar *alert_ident;
	gchar *description;
	GSList *objects_copy;
	gint nobjects;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (!objects)
		return;

	objects_copy = g_slist_copy ((GSList *) objects);
	g_slist_foreach (objects_copy, (GFunc) g_object_ref, NULL);
	nobjects = g_slist_length (objects_copy);

	switch (e_cal_model_get_component_kind (model)) {
		case I_CAL_VEVENT_COMPONENT:
			description = g_strdup_printf (ngettext ("Deleting an event", "Deleting %d events", nobjects), nobjects);
			alert_ident = "calendar:failed-remove-event";
			break;
		case I_CAL_VJOURNAL_COMPONENT:
			description = g_strdup_printf (ngettext ("Deleting a memo", "Deleting %d memos", nobjects), nobjects);
			alert_ident = "calendar:failed-remove-memo";
			break;
		case I_CAL_VTODO_COMPONENT:
			description = g_strdup_printf (ngettext ("Deleting a task", "Deleting %d tasks", nobjects), nobjects);
			alert_ident = "calendar:failed-remove-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	data_model = e_cal_model_get_data_model (model);

	cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident,
		NULL, cal_ops_delete_components_thread, objects_copy, (GDestroyNotify) e_util_free_nullable_object_slist);

	g_clear_object (&cancellable);
	g_free (description);
}

static gboolean
cal_ops_create_comp_with_new_uid_sync (ECalClient *cal_client,
				       ICalComponent *icomp,
				       ICalTimezone *zone,
				       GCancellable *cancellable,
				       GError **error)
{
	ICalComponent *clone;
	gchar *uid;
	gboolean success;

	g_return_val_if_fail (E_IS_CAL_CLIENT (cal_client), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (icomp), FALSE);

	clone = i_cal_component_clone (icomp);

	uid = e_util_generate_uid ();
	i_cal_component_set_uid (clone, uid);
	g_free (uid);

	cal_comp_util_maybe_ensure_allday_timezone_properties (cal_client, clone, zone);

	success = e_cal_client_create_object_sync (cal_client, clone, E_CAL_OPERATION_FLAG_NONE, NULL, cancellable, error);

	g_clear_object (&clone);

	return success;
}

typedef struct {
	ECalModel *model;
	ICalComponent *icomp;
	ICalComponentKind kind;
	ICalTimezone *zone;
	const gchar *extension_name;
	gboolean success;
} PasteComponentsData;

static void
paste_components_data_free (gpointer ptr)
{
	PasteComponentsData *pcd = ptr;

	if (pcd) {
		if (pcd->model && pcd->success)
			g_signal_emit_by_name (pcd->model, "row-appended", 0);

		g_clear_object (&pcd->model);
		g_clear_object (&pcd->icomp);
		g_clear_object (&pcd->zone);
		g_slice_free (PasteComponentsData, pcd);
	}
}

static void
cal_ops_update_components_thread (EAlertSinkThreadJobData *job_data,
				  gpointer user_data,
				  GCancellable *cancellable,
				  GError **error)
{
	PasteComponentsData *pcd = user_data;
	EClient *client;
	EClientCache *client_cache;
	ECalClient *cal_client;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;
	gchar *display_name;
	gboolean success = TRUE, any_copied = FALSE;
	GError *local_error = NULL;

	g_return_if_fail (pcd != NULL);

	uid = e_cal_model_get_default_source_uid (pcd->model);
	g_return_if_fail (uid != NULL);

	client_cache = e_cal_model_get_client_cache (pcd->model);
	registry = e_cal_model_get_registry (pcd->model);

	source = e_source_registry_ref_source (registry, uid);
	if (!source) {
		g_set_error (&local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
			_("Source with UID “%s” not found"), uid);
		e_alert_sink_thread_job_set_alert_arg_0 (job_data, uid);
		return;
	}

	display_name = e_util_get_source_full_name (registry, source);
	e_alert_sink_thread_job_set_alert_arg_0 (job_data, display_name);
	g_free (display_name);

	client = e_client_cache_get_client_sync (client_cache, source, pcd->extension_name, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, cancellable, &local_error);
	g_clear_object (&source);

	if (!client) {
		e_util_propagate_open_source_job_error (job_data, pcd->extension_name, local_error, error);
		return;
	}

	cal_client = E_CAL_CLIENT (client);

	if (i_cal_component_isa (pcd->icomp) == I_CAL_VCALENDAR_COMPONENT &&
	    i_cal_component_count_components (pcd->icomp, pcd->kind) > 0) {
		ICalComponent *subcomp;

		for (subcomp = i_cal_component_get_first_component (pcd->icomp, I_CAL_VTIMEZONE_COMPONENT);
		     subcomp && !g_cancellable_is_cancelled (cancellable);
		     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (pcd->icomp, I_CAL_VTIMEZONE_COMPONENT)) {
			ICalTimezone *zone;

			zone = i_cal_timezone_new ();
			i_cal_timezone_set_component (zone, subcomp);
			if (!e_cal_client_add_timezone_sync (cal_client, zone, cancellable, error)) {
				g_clear_object (&zone);
				success = FALSE;
				break;
			}

			g_clear_object (&zone);
		}

		g_clear_object (&subcomp);

		for (subcomp = i_cal_component_get_first_component (pcd->icomp, pcd->kind);
		     subcomp && !g_cancellable_is_cancelled (cancellable) && success;
		     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (pcd->icomp, pcd->kind)) {
			if (!cal_ops_create_comp_with_new_uid_sync (cal_client, subcomp, pcd->zone, cancellable, error)) {
				success = FALSE;
				break;
			}

			any_copied = TRUE;
		}

		g_clear_object (&subcomp);
	} else if (i_cal_component_isa (pcd->icomp) == pcd->kind) {
		success = cal_ops_create_comp_with_new_uid_sync (cal_client, pcd->icomp, pcd->zone, cancellable, error);
		any_copied = success;
	}

	pcd->success = success && any_copied;

	g_object_unref (client);
}

/**
 * e_cal_ops_paste_components:
 * @model: an #ECalModel
 * @icompstr: a string representation of an iCalendar component
 *
 * Pastes components into the default source of the @model.
 *
 * Since: 3.16
 **/
void
e_cal_ops_paste_components (ECalModel *model,
			    const gchar *icompstr)
{
	ECalDataModel *data_model;
	ICalComponent *icomp;
	ICalComponentKind kind;
	gint ncomponents = 0;
	GCancellable *cancellable;
	const gchar *alert_ident;
	const gchar *extension_name;
	gchar *description;
	PasteComponentsData *pcd;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (icompstr != NULL);

	icomp = i_cal_parser_parse_string (icompstr);
	if (!icomp)
		return;

	kind = i_cal_component_isa (icomp);
	if (kind != I_CAL_VCALENDAR_COMPONENT &&
	    kind != e_cal_model_get_component_kind (model)) {
		g_clear_object (&icomp);
		return;
	}

	switch (e_cal_model_get_component_kind (model)) {
		case I_CAL_VEVENT_COMPONENT:
			if (kind == I_CAL_VCALENDAR_COMPONENT) {
				kind = I_CAL_VEVENT_COMPONENT;
				ncomponents = i_cal_component_count_components (icomp, kind);
			} else if (kind == I_CAL_VEVENT_COMPONENT) {
				ncomponents = 1;
			}

			if (ncomponents == 0)
				break;

			description = g_strdup_printf (ngettext ("Pasting an event", "Pasting %d events", ncomponents), ncomponents);
			alert_ident = "calendar:failed-create-event";
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			break;
		case I_CAL_VJOURNAL_COMPONENT:
			if (kind == I_CAL_VCALENDAR_COMPONENT) {
				kind = I_CAL_VJOURNAL_COMPONENT;
				ncomponents = i_cal_component_count_components (icomp, kind);
			} else if (kind == I_CAL_VJOURNAL_COMPONENT) {
				ncomponents = 1;
			}

			if (ncomponents == 0)
				break;

			description = g_strdup_printf (ngettext ("Pasting a memo", "Pasting %d memos", ncomponents), ncomponents);
			alert_ident = "calendar:failed-create-memo";
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			break;
		case I_CAL_VTODO_COMPONENT:
			if (kind == I_CAL_VCALENDAR_COMPONENT) {
				kind = I_CAL_VTODO_COMPONENT;
				ncomponents = i_cal_component_count_components (icomp, kind);
			} else if (kind == I_CAL_VTODO_COMPONENT) {
				ncomponents = 1;
			}

			if (ncomponents == 0)
				break;

			description = g_strdup_printf (ngettext ("Pasting a task", "Pasting %d tasks", ncomponents), ncomponents);
			alert_ident = "calendar:failed-create-task";
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			break;
		default:
			g_warn_if_reached ();
			break;
	}

	if (ncomponents == 0) {
		g_object_unref (icomp);
		return;
	}

	pcd = g_slice_new0 (PasteComponentsData);
	pcd->model = g_object_ref (model);
	pcd->icomp = icomp;
	pcd->kind = kind;
	pcd->zone = e_cal_model_get_timezone (model);
	pcd->extension_name = extension_name;
	pcd->success = FALSE;

	if (pcd->zone)
		g_object_ref (pcd->zone);

	data_model = e_cal_model_get_data_model (model);

	cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident,
		NULL, cal_ops_update_components_thread, pcd, paste_components_data_free);

	g_clear_object (&cancellable);
	g_free (description);
}

typedef struct {
	ECalClient *client;
	ICalComponent *icomp;
} SendComponentData;

static void
send_component_data_free (gpointer ptr)
{
	SendComponentData *scd = ptr;

	if (scd) {
		g_clear_object (&scd->client);
		g_clear_object (&scd->icomp);
		g_slice_free (SendComponentData, scd);
	}
}

static void
cal_ops_send_component_thread (EAlertSinkThreadJobData *job_data,
			       gpointer user_data,
			       GCancellable *cancellable,
			       GError **error)
{
	SendComponentData *scd = user_data;
	ICalComponent *mod_comp = NULL;
	GSList *users = NULL;

	g_return_if_fail (scd != NULL);

	e_cal_client_send_objects_sync (scd->client, scd->icomp, E_CAL_OPERATION_FLAG_NONE,
		&users, &mod_comp, cancellable, error);

	g_clear_object (&mod_comp);
	g_slist_free_full (users, g_free);
}

/**
 * e_cal_ops_send_component:
 * @model: an #ECalModel
 * @client: an #ECalClient
 * @icomp: an #ICalComponent
 *
 * Sends (calls e_cal_client_send_objects_sync()) on the given @client
 * with the given @icomp in a dedicated thread.
 *
 * Since: 3.16
 **/
void
e_cal_ops_send_component (ECalModel *model,
			  ECalClient *client,
			  ICalComponent *icomp)
{
	ECalDataModel *data_model;
	ESource *source;
	GCancellable *cancellable;
	const gchar *alert_ident;
	const gchar *description;
	gchar *display_name;
	SendComponentData *scd;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (I_CAL_IS_COMPONENT (icomp));

	switch (e_cal_client_get_source_type (client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			description = _("Updating an event");
			alert_ident = "calendar:failed-update-event";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			description = _("Updating a memo");
			alert_ident = "calendar:failed-update-memo";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			description = _("Updating a task");
			alert_ident = "calendar:failed-update-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	scd = g_slice_new0 (SendComponentData);
	scd->client = g_object_ref (client);
	scd->icomp = i_cal_component_clone (icomp);

	source = e_client_get_source (E_CLIENT (client));
	data_model = e_cal_model_get_data_model (model);
	display_name = e_util_get_source_full_name (e_cal_model_get_registry (model), source);

	cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident,
		display_name, cal_ops_send_component_thread,
		scd, send_component_data_free);

	g_clear_object (&cancellable);
	g_free (display_name);
}

typedef struct {
	ECalModel *model;
	GList *clients;
	ICalComponentKind kind;
	time_t older_than;
} PurgeComponentsData;

static void
purge_components_data_free (gpointer ptr)
{
	PurgeComponentsData *pcd = ptr;

	if (pcd) {
		g_clear_object (&pcd->model);
		g_list_free_full (pcd->clients, g_object_unref);
		g_slice_free (PurgeComponentsData, pcd);
	}
}

struct purge_data {
	GList *clients;
	gboolean remove;
	time_t older_than;
};

static gboolean
ca_ops_purge_check_instance_cb (ICalComponent *comp,
				ICalTime *instance_start,
				ICalTime *instance_end,
				gpointer user_data,
				GCancellable *cancellable,
				GError **error)
{
	struct purge_data *pd = user_data;

	if (i_cal_time_as_timet (instance_end) >= pd->older_than)
		pd->remove = FALSE;

	return pd->remove;
}

static void
cal_ops_purge_components_thread (EAlertSinkThreadJobData *job_data,
				 gpointer user_data,
				 GCancellable *cancellable,
				 GError **error)
{
	PurgeComponentsData *pcd = user_data;
	GList *clink;
	gchar *sexp, *start, *end;
	gboolean pushed_message = FALSE;
	const gchar *tzloc = NULL;
	ICalTimezone *zone;
	ICalComponentKind model_kind;

	g_return_if_fail (pcd != NULL);

	model_kind = e_cal_model_get_component_kind (pcd->model);
	zone = e_cal_model_get_timezone (pcd->model);
	if (zone && zone != i_cal_timezone_get_utc_timezone ()) {
		tzloc = i_cal_timezone_get_location (zone);
		if (tzloc && g_ascii_strcasecmp (tzloc, "UTC") == 0)
			tzloc = NULL;
	}

	start = isodate_from_time_t (0);
	end = isodate_from_time_t (pcd->older_than);
	sexp = g_strdup_printf (
		"(occur-in-time-range? (make-time \"%s\") (make-time \"%s\") \"%s\")",
		start, end, tzloc ? tzloc : "");
	g_free (start);
	g_free (end);

	for (clink = pcd->clients; clink && !g_cancellable_is_cancelled (cancellable); clink = g_list_next (clink)) {
		ECalClient *client = clink->data;
		GSList *objects, *olink;
		gint nobjects, ii, last_percent = 0;
		gchar *display_name;
		gboolean success = TRUE;

		if (!client || e_client_is_readonly (E_CLIENT (client)))
			continue;

		display_name = e_util_get_source_full_name (e_cal_model_get_registry (pcd->model), e_client_get_source (E_CLIENT (client)));
		e_alert_sink_thread_job_set_alert_arg_0 (job_data, display_name);

		switch (model_kind) {
			case I_CAL_VEVENT_COMPONENT:
				camel_operation_push_message (cancellable,
					_("Getting events to purge in the calendar “%s”"), display_name);
				break;
			case I_CAL_VJOURNAL_COMPONENT:
				camel_operation_push_message (cancellable,
					_("Getting memos to purge in the memo list “%s”"), display_name);
				break;
			case I_CAL_VTODO_COMPONENT:
				camel_operation_push_message (cancellable,
					_("Getting tasks to purge in the task list “%s”"), display_name);
				break;
			default:
				g_warn_if_reached ();
				g_free (display_name);
				return;
		}

		pushed_message = TRUE;

		if (!e_cal_client_get_object_list_sync (client, sexp, &objects, cancellable, error)) {
			g_free (display_name);
			break;
		}

		camel_operation_pop_message (cancellable);
		pushed_message = FALSE;

		if (!objects) {
			g_free (display_name);
			continue;
		}

		switch (model_kind) {
			case I_CAL_VEVENT_COMPONENT:
				camel_operation_push_message (cancellable,
					_("Purging events in the calendar “%s”"), display_name);
				break;
			case I_CAL_VJOURNAL_COMPONENT:
				camel_operation_push_message (cancellable,
					_("Purging memos in the memo list “%s”"), display_name);
				break;
			case I_CAL_VTODO_COMPONENT:
				camel_operation_push_message (cancellable,
					_("Purging tasks in the task list “%s”"), display_name);
				break;
			default:
				g_warn_if_reached ();
				g_free (display_name);
				return;
		}

		g_free (display_name);
		nobjects = g_slist_length (objects);

		for (olink = objects, ii = 0; olink; olink = g_slist_next (olink), ii++) {
			ICalComponent *icomp = olink->data;
			gboolean remove = TRUE;
			gint percent = 100 * (ii + 1) / nobjects;

			if (!e_cal_client_check_recurrences_no_master (client)) {
				struct purge_data pd;

				pd.remove = TRUE;
				pd.older_than = pcd->older_than;

				e_cal_client_generate_instances_for_object_sync (client, icomp,
					pcd->older_than, G_MAXINT32, cancellable, ca_ops_purge_check_instance_cb, &pd);

				remove = pd.remove;
			}

			if (remove) {
				const gchar *uid = i_cal_component_get_uid (icomp);

				if (e_cal_util_component_is_instance (icomp) ||
				    e_cal_util_component_has_recurrences (icomp)) {
					gchar *rid;

					rid = e_cal_util_component_get_recurid_as_string (icomp);

					success = e_cal_client_remove_object_sync (client, uid, rid, E_CAL_OBJ_MOD_ALL, E_CAL_OPERATION_FLAG_NONE, cancellable, error);

					g_free (rid);
				} else {
					success = e_cal_client_remove_object_sync (client, uid, NULL, E_CAL_OBJ_MOD_THIS, E_CAL_OPERATION_FLAG_NONE, cancellable, error);
				}

				if (!success)
					break;
			}

			if (percent != last_percent) {
				camel_operation_progress (cancellable, percent);
				last_percent = percent;
			}
		}

		g_slist_free_full (objects, g_object_unref);

		camel_operation_progress (cancellable, 0);
		camel_operation_pop_message (cancellable);

		if (!success)
			break;
	}

	if (pushed_message)
		camel_operation_pop_message (cancellable);

	g_free (sexp);
}

/**
 * e_cal_ops_purge_components:
 * @model: an #ECalModel instance
 * @older_than: threshold for the purge operation
 *
 * Purges (removed) all components older than @older_than from all
 * currently active clients in @model.
 *
 * Since: 3.16
 **/
void
e_cal_ops_purge_components (ECalModel *model,
			    time_t older_than)
{
	ECalDataModel *data_model;
	GCancellable *cancellable;
	const gchar *alert_ident;
	const gchar *description;
	PurgeComponentsData *pcd;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	switch (e_cal_model_get_component_kind (model)) {
		case I_CAL_VEVENT_COMPONENT:
			description = _("Purging events");
			alert_ident = "calendar:failed-remove-event";
			break;
		case I_CAL_VJOURNAL_COMPONENT:
			description = _("Purging memos");
			alert_ident = "calendar:failed-remove-memo";
			break;
		case I_CAL_VTODO_COMPONENT:
			description = _("Purging tasks");
			alert_ident = "calendar:failed-remove-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	data_model = e_cal_model_get_data_model (model);

	pcd = g_slice_new0 (PurgeComponentsData);
	pcd->model = g_object_ref (model);
	pcd->clients = e_cal_data_model_get_clients (data_model);
	pcd->kind = e_cal_model_get_component_kind (model);
	pcd->older_than = older_than;

	cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident,
		NULL, cal_ops_purge_components_thread,
		pcd, purge_components_data_free);

	g_clear_object (&cancellable);
}

static void
clients_list_free (gpointer ptr)
{
	g_list_free_full (ptr, g_object_unref);
}

static void
cal_ops_delete_completed_thread (EAlertSinkThreadJobData *job_data,
				 gpointer user_data,
				 GCancellable *cancellable,
				 GError **error)
{
	GList *clients = user_data, *link;

	for (link = clients; link; link = g_list_next (link)) {
		ECalClient *client = link->data;
		GSList *objects = NULL, *olink;

		if (!client ||
		    e_client_is_readonly (E_CLIENT (client)))
			continue;

		if (!e_cal_client_get_object_list_sync (client, "(is-completed?)", &objects, cancellable, error)) {
			ESource *source = e_client_get_source (E_CLIENT (client));
			e_alert_sink_thread_job_set_alert_arg_0 (job_data, e_source_get_display_name (source));
			break;
		}

		for (olink = objects; olink != NULL; olink = g_slist_next (olink)) {
			ICalComponent *icomp = olink->data;
			const gchar *uid;

			uid = i_cal_component_get_uid (icomp);

			if (!e_cal_client_remove_object_sync (client, uid, NULL, E_CAL_OBJ_MOD_THIS, E_CAL_OPERATION_FLAG_NONE, cancellable, error)) {
				ESource *source = e_client_get_source (E_CLIENT (client));
				e_alert_sink_thread_job_set_alert_arg_0 (job_data, e_source_get_display_name (source));
				break;
			}
		}

		e_util_free_nullable_object_slist (objects);

		/* did not process all objects => an error occurred */
		if (olink != NULL)
			break;
	}
}

/**
 * e_cal_ops_delete_completed_tasks:
 * @model: an #ECalModel
 *
 * Deletes all completed tasks from all currently opened
 * clients in @model.
 *
 * Since: 3.16
 **/
void
e_cal_ops_delete_completed_tasks (ECalModel *model)
{
	ECalDataModel *data_model;
	GCancellable *cancellable;
	GList *clients;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	data_model = e_cal_model_get_data_model (model);
	clients = e_cal_data_model_get_clients (data_model);

	if (!clients)
		return;

	if (e_cal_client_get_source_type (clients->data) != E_CAL_CLIENT_SOURCE_TYPE_TASKS) {
		g_list_free_full (clients, g_object_unref);
		g_warn_if_reached ();
		return;
	}

	cancellable = e_cal_data_model_submit_thread_job (data_model, _("Expunging completed tasks"),
		"calendar:failed-remove-task", NULL, cal_ops_delete_completed_thread,
		clients, clients_list_free);

	g_clear_object (&cancellable);
}

static ECalClient *
cal_ops_open_client_sync (EAlertSinkThreadJobData *job_data,
			  EShell *shell,
			  const gchar *client_uid,
			  const gchar *extension_name,
			  GCancellable *cancellable,
			  GError **error)
{
	ECalClient *cal_client = NULL;
	ESourceRegistry *registry;
	EClientCache *client_cache;
	ESource *source;
	EClient *client;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (client_uid != NULL, NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	registry = e_shell_get_registry (shell);
	client_cache = e_shell_get_client_cache (shell);

	source = e_source_registry_ref_source (registry, client_uid);
	if (!source) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
			_("Source with UID “%s” not found"), client_uid);
		e_alert_sink_thread_job_set_alert_arg_0 (job_data, client_uid);
	} else {
		client = e_client_cache_get_client_sync (client_cache, source, extension_name, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, cancellable, error);
		if (client)
			cal_client = E_CAL_CLIENT (client);
	}

	g_clear_object (&source);

	return cal_client;
}

static void
cal_ops_get_default_component_thread (EAlertSinkThreadJobData *job_data,
				      gpointer user_data,
				      GCancellable *cancellable,
				      GError **error)
{
	BasicOperationData *bod = user_data;

	g_return_if_fail (bod != NULL);

	if (!bod->for_client_uid) {
		ESourceRegistry *registry;
		ESource *default_source = NULL;

		registry = e_cal_model_get_registry (bod->model);

		switch (e_cal_model_get_component_kind (bod->model)) {
			case I_CAL_VEVENT_COMPONENT:
				default_source = e_source_registry_ref_default_calendar (registry);
				break;
			case I_CAL_VJOURNAL_COMPONENT:
				default_source = e_source_registry_ref_default_memo_list (registry);
				break;
			case I_CAL_VTODO_COMPONENT:
				default_source = e_source_registry_ref_default_task_list (registry);
				break;
			default:
				g_warn_if_reached ();
				return;
		}

		if (default_source)
			bod->for_client_uid = g_strdup (e_source_get_uid (default_source));

		g_clear_object (&default_source);
	}

	if (bod->for_client_uid) {
		const gchar *extension_name = NULL;

		switch (e_cal_model_get_component_kind (bod->model)) {
			case I_CAL_VEVENT_COMPONENT:
				extension_name = E_SOURCE_EXTENSION_CALENDAR;
				break;
			case I_CAL_VJOURNAL_COMPONENT:
				extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
				break;
			case I_CAL_VTODO_COMPONENT:
				extension_name = E_SOURCE_EXTENSION_TASK_LIST;
				break;
			default:
				g_warn_if_reached ();
				return;
		}

		bod->client = cal_ops_open_client_sync (job_data,
			e_cal_model_get_shell (bod->model),
			bod->for_client_uid,
			extension_name,
			cancellable,
			error);
	}

	bod->icomp = e_cal_model_create_component_with_defaults_sync (bod->model, bod->client, bod->all_day_default_comp, cancellable, error);
	bod->success = bod->icomp != NULL && !g_cancellable_is_cancelled (cancellable);
}

/**
 * e_cal_ops_get_default_component:
 * @model: an #ECalModel
 * @for_client_uid: (allow none): a client UID to use for the new component; can be #NULL
 * @all_day: whether the default event should be an all day event; this argument
 *    is ignored for other than event @model-s
 * @callback: a callback to be called when the operation succeeded
 * @user_data: user data passed to @callback
 * @user_data_free: (allow none): a function to free @user_data, or #NULL
 *
 * Creates a new component with default values as defined by the @client,
 * or by the @model, if @client is #NULL. The @callback is called on success.
 * The @callback is called in the main thread.
 *
 * Since: 3.16
 **/
void
e_cal_ops_get_default_component (ECalModel *model,
				 const gchar *for_client_uid,
				 gboolean all_day,
				 ECalOpsGetDefaultComponentFunc callback,
				 gpointer user_data,
				 GDestroyNotify user_data_free)
{
	ECalDataModel *data_model;
	ESource *source = NULL;
	const gchar *description;
	const gchar *alert_ident;
	gchar *display_name = NULL;
	BasicOperationData *bod;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (callback != NULL);

	switch (e_cal_model_get_component_kind (model)) {
		case I_CAL_VEVENT_COMPONENT:
			description = _("Creating an event");
			alert_ident = "calendar:failed-create-event";
			break;
		case I_CAL_VJOURNAL_COMPONENT:
			description = _("Creating a memo");
			alert_ident = "calendar:failed-create-memo";
			break;
		case I_CAL_VTODO_COMPONENT:
			description = _("Creating a task");
			alert_ident = "calendar:failed-create-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	data_model = e_cal_model_get_data_model (model);
	if (for_client_uid) {
		ESourceRegistry *registry;

		registry = e_cal_model_get_registry (model);
		source = e_source_registry_ref_source (registry, for_client_uid);
		if (source)
			display_name = e_util_get_source_full_name (registry, source);
	}

	bod = basic_operation_data_new ();
	bod->model = g_object_ref (model);
	bod->client = NULL;
	bod->icomp = NULL;
	bod->for_client_uid = g_strdup (for_client_uid);
	bod->all_day_default_comp = all_day;
	bod->get_default_comp_cb = callback;
	bod->user_data = user_data;
	bod->user_data_free = user_data_free;

	cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident,
		display_name ? display_name : "", cal_ops_get_default_component_thread,
		bod, basic_operation_data_free);

	g_clear_object (&cancellable);
	g_clear_object (&source);
	g_free (display_name);
}

static void
cal_ops_emit_model_object_created (ECompEditor *comp_editor,
				   ECalModel *model)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (E_IS_CAL_MODEL (model));

	e_cal_model_emit_object_created (model, e_comp_editor_get_target_client (comp_editor));
}

typedef struct
{
	gboolean is_new_component;
	EShell *shell;
	ECalModel *model;
	ECalClientSourceType source_type;
	gboolean is_assigned;
	gchar *extension_name;
	gchar *for_client_uid;
	ESource *default_source;
	ECalClient *client;
	ECalComponent *comp;

	/* for events only */
	time_t dtstart;
	time_t dtend;
	gboolean all_day;
	gboolean use_default_reminder;
	gint default_reminder_interval;
	EDurationType default_reminder_units;
} NewComponentData;

static NewComponentData *
new_component_data_new (void)
{
	return g_slice_new0 (NewComponentData);
}

static void
new_component_data_free (gpointer ptr)
{
	NewComponentData *ncd = ptr;

	if (ncd) {
		/* successfully opened the default client */
		if (ncd->client && ncd->comp) {
			ECompEditor *comp_editor;
			ECompEditorFlags flags = 0;

			if (ncd->is_new_component) {
				flags |= E_COMP_EDITOR_FLAG_IS_NEW;
			} else {
				if (e_cal_component_has_attendees (ncd->comp))
					ncd->is_assigned = TRUE;
			}

			if (ncd->is_assigned) {
				if (ncd->is_new_component)
					flags |= E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER;

				flags |= E_COMP_EDITOR_FLAG_WITH_ATTENDEES;
			}

			if (ncd->source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
				if (ncd->is_new_component && ncd->dtstart > 0 && ncd->dtend > 0) {
					ECalComponentDateTime *dt;
					ICalTime *itt;
					ICalTimezone *zone;

					if (ncd->model)
						zone = e_cal_model_get_timezone (ncd->model);
					else
						zone = calendar_config_get_icaltimezone ();

					itt = i_cal_time_new_from_timet_with_zone (ncd->dtstart, FALSE, zone);
					if (ncd->all_day) {
						i_cal_time_set_time (itt, 0, 0, 0);
						i_cal_time_set_is_date (itt, TRUE);
					}

					dt = e_cal_component_datetime_new_take (itt,
						(ncd->all_day || !zone) ? NULL : g_strdup (i_cal_timezone_get_tzid (zone)));
					e_cal_component_set_dtstart (ncd->comp, dt);
					e_cal_component_datetime_free (dt);

					itt = i_cal_time_new_from_timet_with_zone (ncd->dtend, FALSE, zone);
					if (ncd->all_day) {
						/* We round it up to the end of the day, unless it is
						 * already set to midnight */
						if (i_cal_time_get_hour (itt) != 0 ||
						    i_cal_time_get_minute (itt) != 0 ||
						    i_cal_time_get_second (itt) != 0) {
							i_cal_time_adjust (itt, 1, 0, 0, 0);
						}
						i_cal_time_set_time (itt, 0, 0, 0);
						i_cal_time_set_is_date (itt, TRUE);
					}
					dt = e_cal_component_datetime_new_take (itt,
						(ncd->all_day || !zone) ? NULL : g_strdup (i_cal_timezone_get_tzid (zone)));
					e_cal_component_set_dtend (ncd->comp, dt);
					e_cal_component_datetime_free (dt);
				}
				e_cal_component_commit_sequence (ncd->comp);
			}

			comp_editor = e_comp_editor_open_for_component (NULL, ncd->shell,
				ncd->client ? e_client_get_source (E_CLIENT (ncd->client)) : NULL,
				e_cal_component_get_icalcomponent (ncd->comp), flags);

			if (comp_editor) {
				if (ncd->model) {
					g_signal_connect (comp_editor, "object-created",
						G_CALLBACK (cal_ops_emit_model_object_created), ncd->model);

					g_object_set_data_full (G_OBJECT (comp_editor), "e-cal-ops-model", g_object_ref (ncd->model), g_object_unref);
				}

				gtk_window_present (GTK_WINDOW (comp_editor));
			}
		}

		g_clear_object (&ncd->shell);
		g_clear_object (&ncd->model);
		g_clear_object (&ncd->default_source);
		g_clear_object (&ncd->client);
		g_clear_object (&ncd->comp);
		g_free (ncd->extension_name);
		g_free (ncd->for_client_uid);
		g_slice_free (NewComponentData, ncd);
	}
}

static void
cal_ops_new_component_editor_thread (EAlertSinkThreadJobData *job_data,
				     gpointer user_data,
				     GCancellable *cancellable,
				     GError **error)
{
	NewComponentData *ncd = user_data;
	GError *local_error = NULL;

	g_return_if_fail (ncd != NULL);

	if (ncd->for_client_uid) {
		ncd->client = cal_ops_open_client_sync (job_data, ncd->shell, ncd->for_client_uid,
			ncd->extension_name, cancellable, &local_error);
	}

	if (!ncd->default_source && !ncd->client && !ncd->for_client_uid) {
		const gchar *message;

		switch (ncd->source_type) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				message = _("Default calendar not found");
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				message = _("Default memo list not found");
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				message = _("Default task list not found");
				break;
			default:
				g_warn_if_reached ();
				return;
		}

		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, message);
		return;
	}

	if (!ncd->client && !ncd->for_client_uid) {
		EClient *client;
		EClientCache *client_cache;

		client_cache = e_shell_get_client_cache (ncd->shell);

		client = e_client_cache_get_client_sync (client_cache, ncd->default_source, ncd->extension_name, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, cancellable, &local_error);
		if (client)
			ncd->client = E_CAL_CLIENT (client);
	}

	if (ncd->client) {
		switch (ncd->source_type) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				ncd->comp = cal_comp_event_new_with_current_time_sync (ncd->client,
					ncd->all_day, ncd->use_default_reminder, ncd->default_reminder_interval,
					ncd->default_reminder_units, cancellable, &local_error);
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				ncd->comp = cal_comp_memo_new_with_defaults_sync (ncd->client, cancellable, &local_error);
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				ncd->comp = cal_comp_task_new_with_defaults_sync (ncd->client, cancellable, &local_error);
				break;
			default:
				g_warn_if_reached ();
				return;
		}
	}

	e_util_propagate_open_source_job_error (job_data, ncd->extension_name, local_error, error);
}

static void
e_cal_ops_new_component_ex (EShellWindow *shell_window,
			    ECalModel *model,
			    ECalClientSourceType source_type,
			    const gchar *for_client_uid,
			    gboolean is_assigned,
			    gboolean all_day,
			    time_t dtstart,
			    time_t dtend,
			    gboolean use_default_reminder,
			    gint default_reminder_interval,
			    EDurationType default_reminder_units)
{
	ESourceRegistry *registry;
	ESource *default_source, *for_client_source = NULL;
	EShell *shell;
	gchar *description = NULL, *alert_ident = NULL, *alert_arg_0 = NULL;
	gchar *source_display_name = NULL;
	const gchar *extension_name;
	NewComponentData *ncd;

	if (shell_window) {
		g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

		shell = e_shell_window_get_shell (shell_window);
	} else {
		g_return_if_fail (E_IS_CAL_MODEL (model));

		shell = e_cal_model_get_shell (model);
	}

	registry = e_shell_get_registry (shell);

	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			default_source = e_source_registry_ref_default_calendar (registry);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			default_source = e_source_registry_ref_default_memo_list (registry);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			default_source = e_source_registry_ref_default_task_list (registry);
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	if (for_client_uid)
		for_client_source = e_source_registry_ref_source (registry, for_client_uid);

	if (!all_day && source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
		GSettings *settings = e_util_ref_settings ("org.gnome.evolution.calendar");
		gint shorten_by = g_settings_get_int (settings, "shorten-time");
		gboolean shorten_end = g_settings_get_boolean (settings, "shorten-time-end");
		g_clear_object (&settings);

		if (shorten_by > 0 && (dtend - dtstart) / 60 > shorten_by) {
			if (shorten_end)
				dtend -= shorten_by * 60;
			else
				dtstart += shorten_by * 60;
		}
	}

	ncd = new_component_data_new ();
	ncd->is_new_component = TRUE;
	ncd->shell = g_object_ref (shell);
	ncd->model = model ? g_object_ref (model) : NULL;
	ncd->source_type = source_type;
	ncd->for_client_uid = g_strdup (for_client_uid);
	ncd->is_assigned = is_assigned;
	ncd->extension_name = g_strdup (extension_name);
	ncd->default_source = default_source ? g_object_ref (default_source) : NULL;
	ncd->client = NULL;
	ncd->comp = NULL;
	ncd->dtstart = dtstart;
	ncd->dtend = dtend;
	ncd->all_day = all_day;
	ncd->use_default_reminder = use_default_reminder;
	ncd->default_reminder_interval = default_reminder_interval;
	ncd->default_reminder_units = default_reminder_units;

	if (for_client_source)
		source_display_name = e_util_get_source_full_name (registry, for_client_source);
	else if (default_source)
		source_display_name = e_util_get_source_full_name (registry, default_source);

	g_warn_if_fail (e_util_get_open_source_job_info (extension_name,
		source_display_name ? source_display_name : "", &description, &alert_ident, &alert_arg_0));

	if (shell_window) {
		EShellView *shell_view;
		EActivity *activity;

		shell_view = e_shell_window_get_shell_view (shell_window,
			e_shell_window_get_active_view (shell_window));

		activity = e_shell_view_submit_thread_job (
			shell_view, description, alert_ident, alert_arg_0,
			cal_ops_new_component_editor_thread, ncd, new_component_data_free);

		g_clear_object (&activity);
	} else {
		GCancellable *cancellable;
		ECalDataModel *data_model;

		data_model = e_cal_model_get_data_model (model);

		cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident, alert_arg_0,
			cal_ops_new_component_editor_thread, ncd, new_component_data_free);

		g_clear_object (&cancellable);
	}

	g_clear_object (&default_source);
	g_clear_object (&for_client_source);
	g_free (source_display_name);
	g_free (description);
	g_free (alert_ident);
	g_free (alert_arg_0);
}

/**
 * e_cal_ops_new_component_editor:
 * @shell_window: an #EShellWindow
 * @source_type: a source type of the new component
 * @for_client_uid: (allow none): a client UID to use for the new component; can be #NULL
 * @is_assigned: whether the new component should be assigned
 *
 * Creates a new component either for an #ECalClient with UID @for_client_uid, or
 * for a default source of the @source_type, with prefilled values as provided
 * by the #ECalClient. Use e_cal_ops_new_event_editor() for events with
 * predefined alarms.
 *
 * Since: 3.16
 **/
void
e_cal_ops_new_component_editor (EShellWindow *shell_window,
				ECalClientSourceType source_type,
				const gchar *for_client_uid,
				gboolean is_assigned)
{
	e_cal_ops_new_component_ex (shell_window, NULL, source_type, for_client_uid, is_assigned, FALSE, 0, 0, FALSE, 0, E_DURATION_MINUTES);
}

/**
 * e_cal_ops_new_event_editor:
 * @shell_window: an #EShellWindow
 * @source_type: a source type of the new component
 * @for_client_uid: (allow none): a client UID to use for the new component; can be #NULL
 * @is_meeting: whether the new event should be a meeting
 * @all_day: whether the new event should be an all day event
 * @use_default_reminder: whether a default reminder should be added,
 *    if #FALSE, then the next two reminded arguments are ignored
 * @default_reminder_interval: reminder interval for the default reminder
 * @default_reminder_units: reminder uints for the default reminder
 * @dtstart: a time_t of DTSTART to use, or 0 to use the default value
 * @dtend: a time_t of DTEND to use, or 0 to use the default value
 *
 * This is a fine-grained version of e_cal_ops_new_component_editor(), suitable
 * for events with predefined alarms. The e_cal_ops_new_component_editor()
 * accepts events as well.
 *
 * The @dtend is ignored, when @dtstart is zero or a negative value.
 *
 * Since: 3.16
 **/
void
e_cal_ops_new_event_editor (EShellWindow *shell_window,
			    const gchar *for_client_uid,
			    gboolean is_meeting,
			    gboolean all_day,
			    gboolean use_default_reminder,
			    gint default_reminder_interval,
			    EDurationType default_reminder_units,
			    time_t dtstart,
			    time_t dtend)
{
	e_cal_ops_new_component_ex (shell_window, NULL, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, for_client_uid, is_meeting,
		all_day, dtstart, dtstart <= 0 ? 0 : dtend, use_default_reminder, default_reminder_interval, default_reminder_units);
}

/**
 * e_cal_ops_new_component_editor_from_model:
 * @model: an #ECalModel
 * @for_client_uid: (allow none): a client UID to use for the new component; can be #NULL
 * @dtstart: a DTSTART to use, for events; less than or equal to 0 to ignore
 * @dtend: a DTEND to use, for events; less than or equal to 0 to ignore
 * @is_assigned: whether the new component should be assigned
 * @all_day: whether the new component should be an all day event
 *
 * Creates a new component either for an #ECalClient with UID @for_client_uid, or
 * for a default source of the source type as defined by @model, with prefilled
 * values as provided by the #ECalClient. The @all_day is used only for events
 * source type.
 *
 * Since: 3.16
 **/
void
e_cal_ops_new_component_editor_from_model (ECalModel *model,
					   const gchar *for_client_uid,
					   time_t dtstart,
					   time_t dtend,
					   gboolean is_assigned,
					   gboolean all_day)
{
	ECalClientSourceType source_type;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	switch (e_cal_model_get_component_kind (model)) {
		case I_CAL_VEVENT_COMPONENT:
			source_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
			break;
		case I_CAL_VJOURNAL_COMPONENT:
			source_type = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
			break;
		case I_CAL_VTODO_COMPONENT:
			source_type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	if (!for_client_uid)
		for_client_uid = e_cal_model_get_default_source_uid (model);

	if (for_client_uid && !*for_client_uid)
		for_client_uid = NULL;

	e_cal_ops_new_component_ex (NULL, model, source_type, for_client_uid, is_assigned, all_day, dtstart, dtend,
		e_cal_model_get_use_default_reminder (model),
		e_cal_model_get_default_reminder_interval (model),
		e_cal_model_get_default_reminder_units (model));
}

/**
 * e_cal_ops_open_component_in_editor_sync:
 * @model: (nullable): an #ECalModel instance
 * @client: an #ECalClient, to which the component belongs
 * @icomp: an #ICalComponent to open in an editor
 * @force_attendees: set to TRUE to force to show attendees, FALSE to auto-detect
 *
 * Opens a component @icomp, which belongs to a @client, in
 * a component editor. This is done synchronously.
 *
 * Since: 3.16
 **/
void
e_cal_ops_open_component_in_editor_sync (ECalModel *model,
					 ECalClient *client,
					 ICalComponent *icomp,
					 gboolean force_attendees)
{
	NewComponentData *ncd;
	ECalComponent *comp;
	ECompEditor *comp_editor;

	if (model)
		g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (I_CAL_IS_COMPONENT (icomp));

	comp_editor = e_comp_editor_find_existing_for (e_client_get_source (E_CLIENT (client)), icomp);
	if (comp_editor) {
		gtk_window_present (GTK_WINDOW (comp_editor));
		return;
	}

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
	g_return_if_fail (comp != NULL);

	ncd = new_component_data_new ();
	ncd->is_new_component = FALSE;
	ncd->shell = g_object_ref (model ? e_cal_model_get_shell (model) : e_shell_get_default ());
	ncd->model = model ? g_object_ref (model) : NULL;
	ncd->source_type = e_cal_client_get_source_type (client);
	ncd->is_assigned = force_attendees;
	ncd->extension_name = NULL;
	ncd->for_client_uid = NULL;
	ncd->default_source = NULL;
	ncd->client = g_object_ref (client);
	ncd->comp = comp;

	/* This opens the editor */
	new_component_data_free (ncd);
}

typedef struct {
	EShell *shell;
	ECalModel *model;
	ESource *destination;
	ECalClient *destination_client;
	ECalClientSourceType source_type;
	GHashTable *icomps_by_source;
	gboolean is_move;
	gint nobjects;
} TransferComponentsData;

static void
transfer_components_free_icomps_slist (gpointer ptr)
{
	GSList *icomps = ptr;

	g_slist_free_full (icomps, g_object_unref);
}

static void
transfer_components_data_free (gpointer ptr)
{
	TransferComponentsData *tcd = ptr;

	if (tcd) {
		if (tcd->destination_client)
			e_cal_model_emit_object_created (tcd->model, tcd->destination_client);

		g_clear_object (&tcd->shell);
		g_clear_object (&tcd->model);
		g_clear_object (&tcd->destination);
		g_clear_object (&tcd->destination_client);
		g_hash_table_destroy (tcd->icomps_by_source);
		g_slice_free (TransferComponentsData, tcd);
	}
}

static void
transfer_components_thread (EAlertSinkThreadJobData *job_data,
			    gpointer user_data,
			    GCancellable *cancellable,
			    GError **error)
{
	TransferComponentsData *tcd = user_data;
	const gchar *extension_name;
	EClient *from_client = NULL, *to_client = NULL;
	ECalClient *from_cal_client = NULL, *to_cal_client = NULL;
	EClientCache *client_cache;
	GHashTableIter iter;
	gpointer key, value;
	gint nobjects, ii = 0, last_percent = 0;
	GSList *link;
	gboolean success = TRUE;

	g_return_if_fail (tcd != NULL);

	switch (tcd->source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	client_cache = e_shell_get_client_cache (tcd->shell);

	to_client = e_util_open_client_sync (job_data, client_cache, extension_name, tcd->destination, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, cancellable, error);
	if (!to_client)
		goto out;

	to_cal_client = E_CAL_CLIENT (to_client);

	if (e_client_is_readonly (E_CLIENT (to_client))) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_READ_ONLY, _("Destination is read only"));
		goto out;
	}

	nobjects = tcd->nobjects;

	g_hash_table_iter_init (&iter, tcd->icomps_by_source);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ESource *source = key;
		GSList *icomps = value;

		from_client = e_util_open_client_sync (job_data, client_cache, extension_name, source, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, cancellable, error);
		if (!from_client)
			goto out;

		from_cal_client = E_CAL_CLIENT (from_client);

		for (link = icomps; link && !g_cancellable_is_cancelled (cancellable); link = g_slist_next (link), ii++) {
			gint percent = 100 * (ii + 1) / nobjects;
			ICalComponent *icomp = link->data;

			if (!cal_comp_transfer_item_to_sync (from_cal_client, to_cal_client, icomp, !tcd->is_move, cancellable, error)) {
				success = FALSE;
				break;
			}

			if (percent != last_percent) {
				camel_operation_progress (cancellable, percent);
				last_percent = percent;
			}
		}

		g_clear_object (&from_client);
	}

	if (success && ii > 0)
		tcd->destination_client = E_CAL_CLIENT (g_object_ref (to_client));

 out:
	g_clear_object (&from_client);
	g_clear_object (&to_client);
}

/**
 * e_cal_ops_transfer_components:
 * @shell_view: an #EShellView
 * @model: an #ECalModel, where to notify about created objects
 * @source_type: a source type of the @destination and the sources
 * @icomps_by_source: a hash table of #ESource to #GSList of ICalComponent to transfer
 * @destination: a destination #ESource
 * @is_move: whether the transfer is move (%TRUE) or copy (%FALSE)
 *
 * Transfers (copies or moves, as set by @is_move) all @icomps_by_source from their source
 * to the @destination of type source type (calendar/memo list/task list).
 *
 * Since: 3.16
 **/
void
e_cal_ops_transfer_components (EShellView *shell_view,
			       ECalModel *model,
			       ECalClientSourceType source_type,
			       GHashTable *icomps_by_source,
			       ESource *destination,
			       gboolean is_move)
{
	gint nobjects;
	gchar *description, *display_name;
	const gchar *alert_ident;
	TransferComponentsData *tcd;
	GHashTableIter iter;
	gpointer key, value;
	EActivity *activity;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (icomps_by_source != NULL);
	g_return_if_fail (E_IS_SOURCE (destination));

	nobjects = 0;
	g_hash_table_iter_init (&iter, icomps_by_source);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ESource *source = key;
		GSList *icomps = value;

		if (!is_move || !e_source_equal (source, destination))
			nobjects += g_slist_length (icomps);
	}

	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			description = g_strdup_printf (is_move ?
				ngettext ("Moving an event", "Moving %d events", nobjects) :
				ngettext ("Copying an event", "Copying %d events", nobjects),
				nobjects);
			alert_ident = is_move ? "calendar:failed-move-event" : "calendar:failed-copy-event";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			description = g_strdup_printf (is_move ?
				ngettext ("Moving a memo", "Moving %d memos", nobjects) :
				ngettext ("Copying a memo", "Copying %d memos", nobjects),
				nobjects);
			alert_ident = is_move ? "calendar:failed-move-memo" : "calendar:failed-copy-memo";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			description = g_strdup_printf (is_move ?
				ngettext ("Moving a task", "Moving %d tasks", nobjects) :
				ngettext ("Copying a task", "Copying %d tasks", nobjects),
				nobjects);
			alert_ident = is_move ? "calendar:failed-move-task" : "calendar:failed-copy-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	tcd = g_slice_new0 (TransferComponentsData);
	tcd->shell = g_object_ref (e_shell_window_get_shell (e_shell_view_get_shell_window (shell_view)));
	tcd->model = g_object_ref (model);
	tcd->icomps_by_source = g_hash_table_new_full ((GHashFunc) e_source_hash, (GEqualFunc) e_source_equal,
		g_object_unref, transfer_components_free_icomps_slist);
	tcd->destination = g_object_ref (destination);
	tcd->source_type = source_type;
	tcd->is_move = is_move;
	tcd->nobjects = nobjects;
	tcd->destination_client = NULL;

	g_hash_table_iter_init (&iter, icomps_by_source);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ESource *source = key;
		GSList *icomps = value;

		if (!is_move || !e_source_equal (source, destination)) {
			GSList *link;

			icomps = g_slist_copy (icomps);
			for (link = icomps; link; link = g_slist_next (link)) {
				link->data = i_cal_component_clone (link->data);
			}

			g_hash_table_insert (tcd->icomps_by_source, g_object_ref (source), icomps);
		}
	}

	display_name = e_util_get_source_full_name (e_cal_model_get_registry (model), destination);
	activity = e_shell_view_submit_thread_job (shell_view, description, alert_ident,
		display_name, transfer_components_thread, tcd,
		transfer_components_data_free);

	g_clear_object (&activity);
	g_free (display_name);
	g_free (description);
}
