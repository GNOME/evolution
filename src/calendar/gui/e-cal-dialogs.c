/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	JP Rosevear <jpr@ximian.com>
 *	Rodrigo Moya <rodrigo@ximian.com>
 *	Federico Mena-Quintero <federico@ximian.com>
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <e-util/e-util.h>

#include "calendar-config.h"
#include "itip-utils.h"
#include "tag-calendar.h"

#include "e-cal-dialogs.h"

/* is_past_event:
 *
 * returns TRUE if @comp is in the past, FALSE otherwise.
 * Comparision is based only on date part, time part is ignored.
 */
static gboolean
is_past_event (ECalComponent *comp)
{
	ECalComponentDateTime *end_date;
	gboolean res;

	if (!comp)
		return TRUE;

	end_date = e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_EVENT ? e_cal_component_get_dtend (comp) : NULL;

	if (!end_date)
		return FALSE;

	res = i_cal_time_compare_date_only (
		e_cal_component_datetime_get_value (end_date),
		i_cal_time_new_current_with_zone (i_cal_time_get_timezone (e_cal_component_datetime_get_value (end_date)))) == -1;

	e_cal_component_datetime_free (end_date);

	return res;
}

/**
 * e_cal_dialogs_delete_with_comment:
 * @parent: a dialog parent #GtkWindow
 * @cal_client: an #ECalClient
 * @comp: an #ECalComponent to be deleted
 * @organizer_is_user: for meetings, whether the organizer is the app user
 * @attendee_is_user: for meetings, whether the app user is in the attendees
 * @out_can_send_notice: (out): set to %TRUE/%FALSE, whether the organizer/attendees can/cannot be notified about the deletion
 *
 * Asks the user whether the @comp can be deleted and when it's a meeting
 * or assigned task or memo, also allows to enter deletion reason and
 * decide the user whether to send the notice or not.
 *
 * The deletion reason is stored into the @comp's COMMENT property.
 *
 * Returns: %TRUE, when can delete the @comp, %FALSE otherwise
 *
 * Since: 3.54
 **/
gboolean
e_cal_dialogs_delete_with_comment (GtkWindow *parent,
				   ECalClient *cal_client,
				   ECalComponent *comp,
				   gboolean organizer_is_user,
				   gboolean attendee_is_user,
				   gboolean *out_can_send_notice)
{
	ECalComponentText *summary;
	GtkWidget *dialog;
	GtkWidget *textview = NULL;
	const gchar *id = NULL;
	gboolean ask_send_notice;
	gboolean ask_notice_comment = FALSE;
	gboolean has_attendees;
	gchar *arg0 = NULL;
	gint result;

	g_return_val_if_fail (E_IS_CAL_CLIENT (cal_client), FALSE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);

	has_attendees = e_cal_component_has_attendees (comp);
	ask_send_notice = has_attendees && out_can_send_notice && e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_EVENT && !is_past_event (comp) &&
		(!organizer_is_user || !e_cal_client_check_save_schedules (cal_client) ||
		 e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_ITIP_SUPPRESS_ON_REMOVE_SUPPORTED) ||
		 e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_RETRACT_SUPPORTED));
	if (ask_send_notice) {
		ask_notice_comment = e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_RETRACT_SUPPORTED) ||
			(!e_cal_client_check_save_schedules (cal_client) && (organizer_is_user || attendee_is_user));
	}

	if (out_can_send_notice)
		*out_can_send_notice = FALSE;

	summary = e_cal_component_dup_summary_for_locale (comp, NULL);
	if (summary) {
		arg0 = g_strdup (e_cal_component_text_get_value (summary));
		e_cal_component_text_free (summary);
	}

	switch (e_cal_component_get_vtype (comp)) {
	case E_CAL_COMPONENT_EVENT:
		if (arg0) {
			if (has_attendees && ask_send_notice && organizer_is_user)
				id = "calendar:prompt-delete-titled-meeting-with-notice-organizer";
			else if (has_attendees && ask_send_notice && attendee_is_user)
				id = "calendar:prompt-delete-titled-meeting-with-notice-attendee";
			else if (has_attendees)
				id = "calendar:prompt-delete-titled-meeting";
			else
				id = "calendar:prompt-delete-titled-appointment";
		} else {
			if (has_attendees && ask_send_notice && organizer_is_user)
				id = "calendar:prompt-delete-meeting-with-notice-organizer";
			else if (has_attendees && ask_send_notice && attendee_is_user)
				id = "calendar:prompt-delete-meeting-with-notice-attendee";
			else if (has_attendees)
				id = "calendar:prompt-delete-meeting";
			else
				id = "calendar:prompt-delete-appointment";
		}
		break;

	case E_CAL_COMPONENT_TODO:
		if (arg0)
			id = "calendar:prompt-delete-named-task";
		else
			id = "calendar:prompt-delete-task";
		break;

	case E_CAL_COMPONENT_JOURNAL:
		if (arg0)
			id = "calendar:prompt-delete-named-memo";
		else
			id = "calendar:prompt-delete-memo";
		break;

	default:
		g_message ("%s: Cannot handle object of type %d", G_STRFUNC, e_cal_component_get_vtype (comp));
		g_free (arg0);
		return FALSE;
	}

	dialog = e_alert_dialog_new_for_args (parent, id, arg0, NULL);

	g_free (arg0);

	if (ask_notice_comment) {
		GtkWidget *scrolled_window;
		GtkWidget *label;
		GtkWidget *vbox;
		GtkWidget *content_area;

		content_area = e_alert_dialog_get_content_area (E_ALERT_DIALOG (dialog));
		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_show (vbox);
		gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);

		label = gtk_label_new_with_mnemonic (_("Deletion _reason:"));
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		scrolled_window = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
		gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
		gtk_widget_show (scrolled_window);

		textview = gtk_text_view_new ();
		gtk_text_view_set_accepts_tab (GTK_TEXT_VIEW (textview), FALSE);
		gtk_widget_show (textview);
		gtk_container_add (GTK_CONTAINER (scrolled_window), textview);
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), textview);

		e_spell_text_view_attach (GTK_TEXT_VIEW (textview));
	}

	result = gtk_dialog_run (GTK_DIALOG (dialog));

	if (result == GTK_RESPONSE_APPLY && textview) {
		GtkTextIter text_iter_start, text_iter_end;
		GtkTextBuffer *text_buffer;
		gchar *comment;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
		gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
		gtk_text_buffer_get_end_iter (text_buffer, &text_iter_end);

		comment = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);
		if (comment && *comment) {
			ECalComponentText *text;
			GSList lst = { NULL, NULL };

			text = e_cal_component_text_new (comment, NULL);
			lst.data = text;

			e_cal_component_set_comments (comp, &lst);
			e_cal_component_text_free (text);
		}
		g_free (comment);
	}

	gtk_widget_destroy (dialog);

	if (out_can_send_notice)
		*out_can_send_notice = result == GTK_RESPONSE_APPLY;

	return result == GTK_RESPONSE_YES || result == GTK_RESPONSE_APPLY;
}

/**
 * e_cal_dialogs_cancel_component:
 * @parent: a parent #GtkWindow
 * @cal_client: a calendar client for whitch the component is cancelled
 * @comp: the #ECalComponent to be cancelled
 * @can_set_cancel_comment: whether can set cancellation comment
 * @organizer_is_user: whether the @comp organizer is the user
 *
 * Pops up a dialog box asking the user whether he wants to send a
 * cancel notice as an iTip/iMip message.
 *
 * The @can_set_cancel_comment is used only if the @cal_client has
 * the %E_CAL_STATIC_CAPABILITY_RETRACT_SUPPORTED capability, otherwise (or when %FALSE),
 * ask only whether to send the cancellation mail or not. When the comment
 * had been entered, the @comp has it set as a COMMENT property.
 *
 * Return value: TRUE if the user clicked Yes, FALSE otherwise.
 **/
gboolean
e_cal_dialogs_cancel_component (GtkWindow *parent,
				ECalClient *cal_client,
				ECalComponent *comp,
				gboolean can_set_cancel_comment,
				gboolean organizer_is_user)
{
	ECalComponentVType vtype;
	GtkWidget *dialog, *textview = NULL;
	gboolean res;
	const gchar *id;

	if ((!can_set_cancel_comment || !e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_RETRACT_SUPPORTED)) &&
	    e_cal_client_check_save_schedules (cal_client) &&
	    (organizer_is_user ||
	     !e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_ITIP_SUPPRESS_ON_REMOVE_SUPPORTED)))
		return TRUE;

	vtype = e_cal_component_get_vtype (comp);

	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
		if (is_past_event (comp)) {
			/* don't ask neither send notification to others on past events */
			return FALSE;
		}
		if (organizer_is_user)
			id = "calendar:prompt-cancel-meeting";
		else
			id = "calendar:prompt-cancel-meeting-attendee";
		break;

	case E_CAL_COMPONENT_TODO:
		if (organizer_is_user)
			id = "calendar:prompt-cancel-task";
		else
			id = "calendar:prompt-cancel-task-attendee";
		break;

	case E_CAL_COMPONENT_JOURNAL:
		if (organizer_is_user)
			id = "calendar:prompt-cancel-memo";
		else
			id = "calendar:prompt-cancel-memo-attendee";
		break;

	default:
		g_message (G_STRLOC ": Cannot handle object of type %d", vtype);
		return FALSE;
	}

	dialog = e_alert_dialog_new_for_args (parent, id, NULL);

	if (can_set_cancel_comment && organizer_is_user && (!e_cal_client_check_save_schedules (cal_client) ||
	    e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_RETRACT_SUPPORTED))) {
		GtkWidget *scrolled_window;
		GtkWidget *label;
		GtkWidget *vbox;
		GtkWidget *content_area;

		content_area = e_alert_dialog_get_content_area (E_ALERT_DIALOG (dialog));
		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_show (vbox);
		gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);

		label = gtk_label_new_with_mnemonic (_("Cancellation _reason:"));
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		scrolled_window = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
		gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
		gtk_widget_show (scrolled_window);

		textview = gtk_text_view_new ();
		gtk_text_view_set_accepts_tab (GTK_TEXT_VIEW (textview), FALSE);
		gtk_widget_show (textview);
		gtk_container_add (GTK_CONTAINER (scrolled_window), textview);
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), textview);

		e_spell_text_view_attach (GTK_TEXT_VIEW (textview));
	}

	res = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES;

	if (res && can_set_cancel_comment && textview) {
		GtkTextIter text_iter_start, text_iter_end;
		GtkTextBuffer *text_buffer;
		gchar *comment;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
		gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
		gtk_text_buffer_get_end_iter (text_buffer, &text_iter_end);

		comment = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);
		if (comment && *comment) {
			ECalComponentText *text;
			GSList lst = { NULL, NULL };

			text = e_cal_component_text_new (comment, NULL);
			lst.data = text;

			e_cal_component_set_comments (comp, &lst);
			e_cal_component_text_free (text);
		}
		g_free (comment);
	}

	gtk_widget_destroy (dialog);

	return res;
}

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
		g_slice_free (CopySourceData, csd);
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
add_timezone_to_cal_cb (ICalParameter *param,
                        gpointer data)
{
	struct ForeachTzidData *ftd = data;
	ICalTimezone *tz = NULL;
	const gchar *tzid;

	g_return_if_fail (ftd != NULL);
	g_return_if_fail (ftd->from_client != NULL);
	g_return_if_fail (ftd->to_client != NULL);

	if (!ftd->success)
		return;

	tzid = i_cal_parameter_get_tzid (param);
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

	client = e_util_open_client_sync (job_data, e_cal_model_get_client_cache (csd->model), csd->extension_name,
		csd->from_source, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, cancellable, error);
	if (client)
		from_client = E_CAL_CLIENT (client);

	if (!from_client)
		goto out;

	client = e_util_open_client_sync (job_data, e_cal_model_get_client_cache (csd->model), csd->extension_name,
		csd->to_source, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, cancellable, error);
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
		ICalComponent *icomp = link->data;
		ICalComponent *existing_icomp = NULL;
		gint percent = 100 * (ii + 1) / n_objects;
		GError *local_error = NULL;

		if (e_cal_client_get_object_sync (to_client, i_cal_component_get_uid (icomp), NULL, &existing_icomp, cancellable, &local_error) &&
		    icomp != NULL) {
			if (!e_cal_client_modify_object_sync (to_client, icomp, E_CAL_OBJ_MOD_ALL, E_CAL_OPERATION_FLAG_NONE, cancellable, error))
				break;

			g_object_unref (existing_icomp);
		} else if (local_error && !g_error_matches (local_error, E_CAL_CLIENT_ERROR, E_CAL_CLIENT_ERROR_OBJECT_NOT_FOUND)) {
			g_propagate_error (error, local_error);
			break;
		} else {
			i_cal_component_foreach_tzid (icomp, add_timezone_to_cal_cb, &ftd);

			g_clear_error (&local_error);

			if (!ftd.success)
				break;

			if (!e_cal_client_create_object_sync (to_client, icomp, E_CAL_OPERATION_FLAG_NONE, NULL, cancellable, error))
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
	e_util_free_nullable_object_slist (objects);
	g_clear_object (&from_client);
	g_clear_object (&to_client);
}

/**
 * e_cal_dialogs_copy_source
 *
 * Implements the Copy command for sources, allowing the user to select a target
 * source to copy to.
 */
void
e_cal_dialogs_copy_source (GtkWindow *parent,
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
		case I_CAL_VEVENT_COMPONENT:
			obj_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			format = _("Copying events to the calendar “%s”");
			alert_ident = "calendar:failed-copy-event";
			break;
		case I_CAL_VJOURNAL_COMPONENT:
			obj_type = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			format = _("Copying memos to the memo list “%s”");
			alert_ident = "calendar:failed-copy-memo";
			break;
		case I_CAL_VTODO_COMPONENT:
			obj_type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			format = _("Copying tasks to the task list “%s”");
			alert_ident = "calendar:failed-copy-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	to_source = e_cal_dialogs_select_source (parent, e_cal_model_get_registry (model), obj_type, from_source);
	if (to_source) {
		CopySourceData *csd;
		GCancellable *cancellable;
		ECalDataModel *data_model;
		gchar *display_name;
		gchar *description;

		csd = g_slice_new0 (CopySourceData);
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

/**
 * e_cal_dialogs_delete_component:
 * @comp: A calendar component if a single component is to be deleted, or NULL
 * if more that one component is to be deleted.
 * @consider_as_untitled: If deleting more than one component, this is ignored.
 * Otherwise, whether to consider the component as not having a summary; if
 * FALSE then the component's summary string will be used.
 * @n_comps: Number of components that are to be deleted.
 * @vtype: Type of the components that are to be deleted.  This is ignored
 * if only one component is to be deleted, and the vtype is extracted from
 * the component instead.
 * @widget: A widget to use as a basis for conversion from UTF8 into font
 * encoding.
 *
 * Pops up a dialog box asking the user whether he wants to delete a number of
 * calendar components.  The dialog will not appear, however, if the
 * configuration option for confirmation is turned off.
 *
 * Return value: TRUE if the user clicked Yes, FALSE otherwise.  If the
 * configuration option for confirmation is turned off, this function will
 * unconditionally return TRUE.
 **/
gboolean
e_cal_dialogs_delete_component (ECalComponent *comp,
				gboolean consider_as_untitled,
				gint n_comps,
				ECalComponentVType vtype,
				GtkWidget *widget)
{
	const gchar *id;
	gchar *arg0 = NULL;
	gint response;
	gboolean attendees;

	if (comp) {
		g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);
		g_return_val_if_fail (n_comps == 1, FALSE);
	} else {
		g_return_val_if_fail (n_comps > 1, FALSE);
		g_return_val_if_fail (vtype != E_CAL_COMPONENT_NO_TYPE, FALSE);
	}

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	if (comp) {
		vtype = e_cal_component_get_vtype (comp);

		if (!consider_as_untitled) {
			ECalComponentText *summary;

			summary = e_cal_component_dup_summary_for_locale (comp, NULL);
			if (summary) {
				arg0 = g_strdup (e_cal_component_text_get_value (summary));
				e_cal_component_text_free (summary);
			}
		}

		switch (vtype) {
		case E_CAL_COMPONENT_EVENT:
			attendees = e_cal_component_has_attendees (comp);
			if (arg0) {
				if (attendees)
					id = "calendar:prompt-delete-titled-meeting";
				else
					id = "calendar:prompt-delete-titled-appointment";
			} else {
				if (attendees)
					id = "calendar:prompt-delete-meeting";
				else
					id = "calendar:prompt-delete-appointment";
			}
			break;

		case E_CAL_COMPONENT_TODO:
			if (arg0)
				id = "calendar:prompt-delete-named-task";
			else
				id = "calendar:prompt-delete-task";
			break;

		case E_CAL_COMPONENT_JOURNAL:
			if (arg0)
				id = "calendar:prompt-delete-named-memo";
			else
				id = "calendar:prompt-delete-memo";
			break;

		default:
			g_message ("%s: Cannot handle object of type %d", G_STRFUNC, vtype);
			g_free (arg0);
			return FALSE;
		}
	} else {
		switch (vtype) {
		case E_CAL_COMPONENT_EVENT:
			if (n_comps == 1)
				id = "calendar:prompt-delete-appointment";
			else
				id = "calendar:prompt-delete-appointments";
			break;

		case E_CAL_COMPONENT_TODO:
			if (n_comps == 1)
				id = "calendar:prompt-delete-task";
			else
				id = "calendar:prompt-delete-tasks";
			break;

		case E_CAL_COMPONENT_JOURNAL:
			if (n_comps == 1)
				id = "calendar:prompt-delete-memo";
			else
				id = "calendar:prompt-delete-memos";
			break;

		default:
			g_message ("%s: Cannot handle objects of type %d", G_STRFUNC, vtype);
			return FALSE;
		}

		if (n_comps > 1)
			arg0 = g_strdup_printf ("%d", n_comps);
	}

	response = e_alert_run_dialog_for_args ((GtkWindow *) gtk_widget_get_toplevel (widget), id, arg0, NULL);
	g_free (arg0);

	return response == GTK_RESPONSE_YES;
}

static void
cb_toggled_cb (GtkToggleButton *toggle,
               gpointer data)
{
	gboolean active = FALSE;
	GtkWidget *entry = (GtkWidget *) data;

	active = gtk_toggle_button_get_active (toggle);
	gtk_widget_set_sensitive (entry, active);
}

gboolean
e_cal_dialogs_prompt_retract (GtkWidget *parent,
			      ECalComponent *comp,
			      gchar **retract_text,
			      gboolean *retract)
{
	gchar *message = NULL;
	ECalComponentVType type = E_CAL_COMPONENT_NO_TYPE;
	GtkMessageDialog *dialog = NULL;
	GtkWidget *cb, *label, *entry, *vbox, *sw, *frame;
	gboolean ret_val = FALSE;

	type = e_cal_component_get_vtype (comp);

	switch (type) {
		case E_CAL_COMPONENT_EVENT:
			message = g_strdup_printf (_("Are you sure you want to delete this meeting?"));
			break;
		case E_CAL_COMPONENT_TODO:
			message = g_strdup_printf (_("Are you sure you want to delete this task?"));
			break;
		case E_CAL_COMPONENT_JOURNAL:
			message = g_strdup_printf (_("Are you sure you want to delete this memo?"));
			break;
		default:
			g_message ("Retract: Unsupported object type \n");
			return FALSE;
	}

	dialog = (GtkMessageDialog *) gtk_message_dialog_new_with_markup
		((GtkWindow *) gtk_widget_get_toplevel (parent), GTK_DIALOG_MODAL,
		 GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "<b>%s</b>", message);
	g_free (message);

	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_set_spacing (GTK_BOX (vbox), 12);

	cb = gtk_check_button_new_with_mnemonic (_("_Delete this item from all other recipient’s mailboxes?"));
	gtk_container_add (GTK_CONTAINER (vbox), cb);

	label = gtk_label_new_with_mnemonic (_("_Retract comment"));

	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget ((GtkFrame *) frame, label);
	gtk_frame_set_label_align ((GtkFrame *) frame, 0, 0);
	gtk_container_add (GTK_CONTAINER (vbox), frame);
	gtk_frame_set_shadow_type ((GtkFrame *) frame, GTK_SHADOW_NONE);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy ((GtkScrolledWindow *) sw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	entry = gtk_text_view_new ();
	gtk_container_add (GTK_CONTAINER (sw), entry);
	gtk_label_set_mnemonic_widget ((GtkLabel *) label, entry);
	gtk_container_add (GTK_CONTAINER (frame), sw);

	g_signal_connect (
		cb, "toggled",
		G_CALLBACK (cb_toggled_cb), entry);

	gtk_widget_show_all ((GtkWidget *) dialog);

	ret_val = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK);

	if (ret_val) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cb))) {
			GtkTextIter text_iter_start, text_iter_end;
			GtkTextBuffer *text_buffer;

			*retract = TRUE;
			text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (entry));
			gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
			gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);

			*retract_text = gtk_text_buffer_get_text (text_buffer, &text_iter_start,
					&text_iter_end, FALSE);
		} else
			*retract = FALSE;
	}

	gtk_widget_destroy ((GtkWidget *) dialog);

	return ret_val;
}

typedef struct {
	GtkWidget *dialog;

	GtkWidget *month_combobox;
	GtkWidget *year;
	ECalendar *ecal;
	GtkWidget *grid;

	gint year_val;
	gint month_val;
	gint day_val;

	ETagCalendar *tag_calendar;

	ECalDataModel *data_model;
	ECalendarViewMoveType *out_move_type;
	time_t *out_exact_date;
} GoToDialog;

static GoToDialog *glob_dlg = NULL;

/* Callback used when the year adjustment is changed */
static void
year_changed (GtkAdjustment *adj,
              gpointer data)
{
	GtkSpinButton *spin_button;
	GoToDialog *dlg = data;

	spin_button = GTK_SPIN_BUTTON (dlg->year);
	dlg->year_val = gtk_spin_button_get_value_as_int (spin_button);

	e_calendar_item_set_first_month (
		e_calendar_get_item (dlg->ecal), dlg->year_val, dlg->month_val);
}

/* Callback used when a month button is toggled */
static void
month_changed (GtkToggleButton *toggle,
               gpointer data)
{
	GtkComboBox *combo_box;
	GoToDialog *dlg = data;

	combo_box = GTK_COMBO_BOX (dlg->month_combobox);
	dlg->month_val = gtk_combo_box_get_active (combo_box);

	e_calendar_item_set_first_month (
		e_calendar_get_item (dlg->ecal), dlg->year_val, dlg->month_val);
}

/* Event handler for day groups in the month item.  A button press makes
 * the calendar jump to the selected day and destroys the Go-to dialog box. */
static void
ecal_event (ECalendarItem *calitem,
            gpointer user_data)
{
	GoToDialog *dlg = user_data;
	GDate start_date, end_date;
	ICalTime *tt = i_cal_time_new_null_time ();
	ICalTimezone *timezone;
	time_t et;

	g_warn_if_fail (e_calendar_item_get_selection (calitem, &start_date, &end_date));
	timezone = e_cal_data_model_get_timezone (dlg->data_model);

	i_cal_time_set_date (tt,
		g_date_get_year (&start_date),
		g_date_get_month (&start_date),
		g_date_get_day (&start_date));

	et = i_cal_time_as_timet_with_zone (tt, timezone);

	g_clear_object (&tt);

	*(dlg->out_move_type) = E_CALENDAR_VIEW_MOVE_TO_EXACT_DAY;
	*(dlg->out_exact_date) = et;

	gtk_dialog_response (GTK_DIALOG (dlg->dialog), GTK_RESPONSE_APPLY);
}

/* Returns the current time, for the ECalendarItem. */
static struct tm
get_current_time (ECalendarItem *calitem,
                  gpointer data)
{
	ICalTimezone *zone;
	ICalTime *tt;
	struct tm tmp_tm;

	/* Get the current timezone. */
	zone = calendar_config_get_icaltimezone ();

	tt = i_cal_time_new_from_timet_with_zone (time (NULL), FALSE, zone);

	tmp_tm = e_cal_util_icaltime_to_tm (tt);

	g_clear_object (&tt);

	return tmp_tm;
}

/* Gets the widgets from the XML file and returns if they are all available. */
static void
goto_dialog_create_widgets (GoToDialog *dlg,
			    GtkWindow *parent)
{
	ECalendarItem *calitem;
	GtkWidget *widget;
	GtkGrid *grid;
	GtkComboBoxText *text_combo;

	dlg->dialog = gtk_dialog_new_with_buttons (_("Select Date"), parent, 0,
		_("Select _Today"), GTK_RESPONSE_ACCEPT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		NULL);

	g_object_set (G_OBJECT (dlg->dialog),
		"border-width", 12,
		NULL);

	widget = gtk_grid_new ();
	dlg->grid = widget;
	grid = GTK_GRID (widget);

	widget = gtk_dialog_get_content_area (GTK_DIALOG (dlg->dialog));
	gtk_box_pack_start (GTK_BOX (widget), dlg->grid, TRUE, TRUE, 0);

	widget = gtk_combo_box_text_new ();
	dlg->month_combobox = widget;

	text_combo = GTK_COMBO_BOX_TEXT (widget);
	gtk_combo_box_text_append_text (text_combo, _("January"));
	gtk_combo_box_text_append_text (text_combo, _("February"));
	gtk_combo_box_text_append_text (text_combo, _("March"));
	gtk_combo_box_text_append_text (text_combo, _("April"));
	gtk_combo_box_text_append_text (text_combo, _("May"));
	gtk_combo_box_text_append_text (text_combo, _("June"));
	gtk_combo_box_text_append_text (text_combo, _("July"));
	gtk_combo_box_text_append_text (text_combo, _("August"));
	gtk_combo_box_text_append_text (text_combo, _("September"));
	gtk_combo_box_text_append_text (text_combo, _("October"));
	gtk_combo_box_text_append_text (text_combo, _("November"));
	gtk_combo_box_text_append_text (text_combo, _("December"));

	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	widget = gtk_spin_button_new (NULL, 1, 0);
	gtk_spin_button_set_range (GTK_SPIN_BUTTON (widget), 1969, 9999);
	gtk_spin_button_set_increments (GTK_SPIN_BUTTON (widget), 1, 5);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);

	dlg->year = widget;

	dlg->ecal = E_CALENDAR (e_calendar_new ());
	dlg->tag_calendar = e_tag_calendar_new (dlg->ecal);

	calitem = e_calendar_get_item (dlg->ecal);

	gnome_canvas_item_set (
		GNOME_CANVAS_ITEM (calitem),
		"move_selection_when_moving", FALSE,
		NULL);
	e_calendar_item_set_display_popup (calitem, FALSE);
	g_object_set (G_OBJECT (dlg->ecal),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);

	gtk_grid_attach (grid, GTK_WIDGET (dlg->ecal), 0, 1, 2, 1);

	e_calendar_item_set_first_month (calitem, dlg->year_val, dlg->month_val);
	e_calendar_item_set_get_time_callback (calitem, get_current_time, dlg, NULL);

	gtk_widget_show_all (GTK_WIDGET (grid));
}

/* Create a copy, thus a move to a distant date will not cause large event lookups */

/* Creates a "goto date" dialog and runs it */
gboolean
e_cal_dialogs_goto_run (GtkWindow *parent,
			ECalDataModel *data_model,
			const GDate *from_date,
			ECalendarViewMoveType *out_move_type,
			time_t *out_exact_date)
{
	GtkAdjustment *adj;
	gint response;

	if (glob_dlg) {
		return FALSE;
	}

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), FALSE);
	g_return_val_if_fail (out_move_type != NULL, FALSE);
	g_return_val_if_fail (out_exact_date != NULL, FALSE);

	glob_dlg = g_new0 (GoToDialog, 1);

	goto_dialog_create_widgets (glob_dlg, parent);

	glob_dlg->data_model = e_cal_data_model_new_clone (data_model);
	glob_dlg->out_move_type = out_move_type;
	glob_dlg->out_exact_date = out_exact_date;

	if (from_date) {
		glob_dlg->year_val = g_date_get_year (from_date);
		glob_dlg->month_val = g_date_get_month (from_date) - 1;
		glob_dlg->day_val = g_date_get_day (from_date);
	} else {
		ICalTime *tt;
		ICalTimezone *timezone;

		timezone = e_cal_data_model_get_timezone (glob_dlg->data_model);
		tt = i_cal_time_new_current_with_zone (timezone);

		glob_dlg->year_val = i_cal_time_get_year (tt);
		glob_dlg->month_val = i_cal_time_get_month (tt) - 1;
		glob_dlg->day_val = i_cal_time_get_day (tt);

		g_clear_object (&tt);
	}

	g_signal_connect (
		glob_dlg->month_combobox, "changed",
		G_CALLBACK (month_changed), glob_dlg);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (glob_dlg->year));
	g_signal_connect (
		adj, "value_changed",
		G_CALLBACK (year_changed), glob_dlg);

	g_signal_connect (
		e_calendar_get_item (glob_dlg->ecal), "selection_changed",
		G_CALLBACK (ecal_event), glob_dlg);

	gtk_combo_box_set_active (GTK_COMBO_BOX (glob_dlg->month_combobox), glob_dlg->month_val);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (glob_dlg->year), glob_dlg->year_val);

	gtk_window_set_transient_for (GTK_WINDOW (glob_dlg->dialog), parent);

	/* set initial selection to current day */

	e_calendar_get_item (glob_dlg->ecal)->selection_set = TRUE;
	e_calendar_get_item (glob_dlg->ecal)->selection_start_month_offset = 0;
	e_calendar_get_item (glob_dlg->ecal)->selection_start_day = glob_dlg->day_val;
	e_calendar_get_item (glob_dlg->ecal)->selection_end_month_offset = 0;
	e_calendar_get_item (glob_dlg->ecal)->selection_end_day = glob_dlg->day_val;

	gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (e_calendar_get_item (glob_dlg->ecal)));

	e_tag_calendar_subscribe (glob_dlg->tag_calendar, glob_dlg->data_model);

	response = gtk_dialog_run (GTK_DIALOG (glob_dlg->dialog));

	e_tag_calendar_unsubscribe (glob_dlg->tag_calendar, glob_dlg->data_model);

	gtk_widget_destroy (glob_dlg->dialog);

	if (response == GTK_RESPONSE_ACCEPT)
		*(glob_dlg->out_move_type) = E_CALENDAR_VIEW_MOVE_TO_TODAY;

	g_clear_object (&glob_dlg->tag_calendar);
	g_clear_object (&glob_dlg->data_model);

	g_free (glob_dlg);
	glob_dlg = NULL;

	return response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_APPLY;
}

gboolean
e_cal_dialogs_recur_component (ECalClient *client,
			       ECalComponent *comp,
			       ECalObjModType *mod,
			       GtkWindow *parent,
			       gboolean delegated)
{
	gchar *str;
	GtkWidget *dialog, *rb_this, *rb_prior, *rb_future, *rb_all, *hbox;
	GtkWidget *placeholder, *vbox;
	GtkWidget *content_area;
	ECalComponentVType vtype;
	gboolean ret;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);

	if (!e_cal_component_is_instance (comp)) {
		*mod = E_CAL_OBJ_MOD_ALL;
		return TRUE;
	}

	/* It's a detached instance, modify only that one */
	if (!e_cal_component_has_recurrences (comp)) {
		*mod = E_CAL_OBJ_MOD_THIS;
		return TRUE;
	}

	vtype = e_cal_component_get_vtype (comp);

	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
		if (!delegated)
			str = g_strdup_printf (_("You are modifying a recurring event. What would you like to modify?"));
		else
			str = g_strdup_printf (_("You are delegating a recurring event. What would you like to delegate?"));
		break;

	case E_CAL_COMPONENT_TODO:
		str = g_strdup_printf (_("You are modifying a recurring task. What would you like to modify?"));
		break;

	case E_CAL_COMPONENT_JOURNAL:
		str = g_strdup_printf (_("You are modifying a recurring memo. What would you like to modify?"));
		break;

	default:
		g_message ("recur_component_dialog(): Cannot handle object of type %d", vtype);
		return FALSE;
	}

	dialog = gtk_message_dialog_new (parent, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "%s", str);
	g_free (str);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_add (GTK_CONTAINER (content_area), hbox);

	placeholder = gtk_label_new ("");
	gtk_widget_set_size_request (placeholder, 48, 48);
	gtk_box_pack_start (GTK_BOX (hbox), placeholder, FALSE, FALSE, 0);
	gtk_widget_show (placeholder);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	rb_this = gtk_radio_button_new_with_label (NULL, _("This Instance Only"));
	gtk_container_add (GTK_CONTAINER (vbox), rb_this);

	if (!e_client_check_capability (E_CLIENT (client), E_CAL_STATIC_CAPABILITY_NO_THISANDPRIOR)) {
		rb_prior = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rb_this), _("This and Prior Instances"));
		gtk_container_add (GTK_CONTAINER (vbox), rb_prior);
	} else
		rb_prior = NULL;

	if (!e_client_check_capability (E_CLIENT (client), E_CAL_STATIC_CAPABILITY_NO_THISANDFUTURE)) {
		rb_future = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rb_this), _("This and Future Instances"));
		gtk_container_add (GTK_CONTAINER (vbox), rb_future);
	} else
		rb_future = NULL;

	rb_all = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rb_this), _("All Instances"));
	gtk_container_add (GTK_CONTAINER (vbox), rb_all);

	gtk_widget_show_all (hbox);

	placeholder = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (content_area), placeholder, FALSE, FALSE, 0);
	gtk_widget_show (placeholder);

	ret = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_this)))
		*mod = E_CAL_OBJ_MOD_THIS;
	else if (rb_prior && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_prior)))
		*mod = E_CAL_OBJ_MOD_THIS_AND_PRIOR;
	else if (rb_future && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_future)))
		*mod = E_CAL_OBJ_MOD_THIS_AND_FUTURE;
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_all))) {
		*mod = E_CAL_OBJ_MOD_ALL;
	}

	gtk_widget_destroy (dialog);

	return ret;
}

gboolean
e_cal_dialogs_recur_icalcomp (ECalClient *client,
			      ICalComponent *icomp,
			      ECalObjModType *mod,
			      GtkWindow *parent,
			      gboolean delegated)
{
	ECalComponent *comp;
	gboolean res;

	g_return_val_if_fail (icomp != NULL, FALSE);

	if (!e_cal_util_component_is_instance (icomp)) {
		*mod = E_CAL_OBJ_MOD_ALL;
		return TRUE;
	}

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
	if (!comp)
		return FALSE;

	res = e_cal_dialogs_recur_component (client, comp, mod, parent, delegated);

	g_object_unref (comp);

	return res;
}

/**
 * e_cal_dialogs_select_source
 *
 * Implements dialog for allowing user to select a destination source.
 */
ESource *
e_cal_dialogs_select_source (GtkWindow *parent,
			     ESourceRegistry *registry,
			     ECalClientSourceType obj_type,
			     ESource *except_source)
{
	GtkWidget *dialog;
	ESource *selected_source = NULL;
	const gchar *extension_name;
	const gchar *icon_name;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	if (obj_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
		extension_name = E_SOURCE_EXTENSION_CALENDAR;
		icon_name = "x-office-calendar";
	} else if (obj_type == E_CAL_CLIENT_SOURCE_TYPE_TASKS) {
		extension_name = E_SOURCE_EXTENSION_TASK_LIST;
		icon_name = "stock_todo";
	} else if (obj_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS) {
		extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
		icon_name = "stock_journal";
	} else
		return NULL;

	/* create the dialog */
	dialog = e_source_selector_dialog_new (parent, registry, extension_name);

	if (icon_name)
		gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	if (except_source)
		e_source_selector_dialog_set_except_source (E_SOURCE_SELECTOR_DIALOG (dialog), except_source);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	selected_source = e_source_selector_dialog_peek_primary_selection (
		E_SOURCE_SELECTOR_DIALOG (dialog));
	if (selected_source != NULL)
		g_object_ref (selected_source);

 exit:
	gtk_widget_destroy (dialog);

	return selected_source;
}

static gboolean
component_has_new_attendees (ECalComponent *comp)
{
	g_return_val_if_fail (comp != NULL, FALSE);

	if (!e_cal_component_has_attendees (comp))
		return FALSE;

	return g_object_get_data (G_OBJECT (comp), "new-attendees") != NULL;
}

static gboolean
have_nonprocedural_alarm (ECalComponent *comp)
{
	GSList *uids, *link;

	g_return_val_if_fail (comp != NULL, FALSE);

	uids = e_cal_component_get_alarm_uids (comp);

	for (link = uids; link; link = g_slist_next (link)) {
		ECalComponentAlarm *alarm;
		ECalComponentAlarmAction action = E_CAL_COMPONENT_ALARM_UNKNOWN;

		alarm = e_cal_component_get_alarm (comp, link->data);
		if (alarm) {
			action = e_cal_component_alarm_get_action (alarm);
			e_cal_component_alarm_free (alarm);

			if (action != E_CAL_COMPONENT_ALARM_NONE &&
			    action != E_CAL_COMPONENT_ALARM_PROCEDURE &&
			    action != E_CAL_COMPONENT_ALARM_UNKNOWN) {
				g_slist_free_full (uids, g_free);
				return TRUE;
			}
		}
	}

	g_slist_free_full (uids, g_free);

	return FALSE;
}

static GtkWidget *
add_checkbox (GtkBox *where,
              const gchar *caption)
{
	GtkWidget *checkbox;

	g_return_val_if_fail (where != NULL, NULL);
	g_return_val_if_fail (caption != NULL, NULL);

	checkbox = gtk_check_button_new_with_mnemonic (caption);
	gtk_widget_set_margin_start (checkbox, 12);
	gtk_widget_set_margin_end (checkbox, 12);
	gtk_widget_set_halign (checkbox, GTK_ALIGN_START);
	gtk_box_pack_start (where, checkbox, TRUE, TRUE, 2);
	gtk_widget_show (checkbox);

	return checkbox;
}

/**
 * e_cal_dialogs_send_component:
 *
 * Pops up a dialog box asking the user whether he wants to send an
 * iTip/iMip message
 *
 * Return value: TRUE if the user clicked Yes, FALSE otherwise.
 **/
gboolean
e_cal_dialogs_send_component (GtkWindow *parent,
			      ECalClient *client,
			      ECalComponent *comp,
			      gboolean new,
			      gboolean *strip_alarms,
			      gboolean *only_new_attendees)
{
	ECalComponentVType vtype;
	GSettings *settings = NULL;
	const gchar *id;
	GtkWidget *dialog, *sa_checkbox = NULL, *ona_checkbox = NULL;
	GtkWidget *content_area;
	gboolean res;

	if (strip_alarms)
		*strip_alarms = TRUE;

	if (e_cal_client_check_save_schedules (client))
		return FALSE;

	if (!itip_component_has_recipients (comp))
		return FALSE;

	vtype = e_cal_component_get_vtype (comp);

	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
		if (new)
			id = "calendar:prompt-meeting-invite";
		else
			id = "calendar:prompt-send-updated-meeting-info";
		break;

	case E_CAL_COMPONENT_TODO:
		if (new)
			id = "calendar:prompt-send-task";
		else
			id = "calendar:prompt-send-updated-task-info";
		break;
	case E_CAL_COMPONENT_JOURNAL:
		if (new)
			id = "calendar:prompt-send-memo";
		else
			id = "calendar:prompt-send-updated-memo-info";
		break;
	default:
		g_message (
			"send_component_dialog(): "
			"Cannot handle object of type %d", vtype);
		return FALSE;
	}

	if (only_new_attendees && !component_has_new_attendees (comp)) {
		/* do not show the check if there is no new attendee and
		 * set as all attendees are required to be notified */
		*only_new_attendees = FALSE;

		/* pretend it as being passed NULL to simplify code below */
		only_new_attendees = NULL;
	}

	if (strip_alarms && !have_nonprocedural_alarm (comp)) {
		/* pretend it as being passed NULL to simplify code below */
		strip_alarms = NULL;
	}

	dialog = e_alert_dialog_new_for_args (parent, id, NULL);
	content_area = e_alert_dialog_get_content_area (E_ALERT_DIALOG (dialog));

	if (strip_alarms) {
		sa_checkbox = add_checkbox (GTK_BOX (content_area), _("Send my reminders with this event"));
		settings = e_util_ref_settings ("org.gnome.evolution.calendar");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sa_checkbox), g_settings_get_boolean (settings, "send-reminder-with-event"));
	}
	if (only_new_attendees)
		ona_checkbox = add_checkbox (GTK_BOX (content_area), _("Notify new attendees _only"));

	res = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES;

	if (res && strip_alarms) {
		gboolean send_alarms = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sa_checkbox));
		g_settings_set_boolean (settings, "send-reminder-with-event", send_alarms);
		*strip_alarms = !send_alarms;
	}
	if (only_new_attendees)
		*only_new_attendees = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ona_checkbox));

	gtk_widget_destroy (GTK_WIDGET (dialog));
	g_clear_object (&settings);

	return res;
}

/**
 * e_cal_dialogs_send_dragged_or_resized_component:
 *
 * Pops up a dialog box asking the user whether he wants to send an
 * iTip/iMip message or cancel the drag/resize operations
 *
 * Return value: GTK_RESPONSE_YES if the user clicked Yes,
 *		 GTK_RESPONSE_NO if the user clicked No and
 *		 GTK_RESPONSE_CANCEL otherwise.
 **/
GtkResponseType
e_cal_dialogs_send_dragged_or_resized_component (GtkWindow *parent,
						 ECalClient *client,
						 ECalComponent *comp,
						 gboolean *strip_alarms,
						 gboolean *only_new_attendees)
{
	ECalComponentVType vtype;
	GSettings *settings = NULL;
	const gchar *id;
	GtkWidget *dialog, *sa_checkbox = NULL, *ona_checkbox = NULL;
	GtkWidget *content_area;
	gboolean save_schedules = FALSE;
	GtkResponseType res;

	if (strip_alarms)
		*strip_alarms = TRUE;

	if (e_cal_client_check_save_schedules (client))
		save_schedules = TRUE;

	if (!itip_component_has_recipients (comp))
		save_schedules = TRUE;

	vtype = e_cal_component_get_vtype (comp);

	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
		id = save_schedules ?
			"calendar:prompt-save-meeting-dragged-or-resized" :
			"calendar:prompt-send-updated-meeting-info-dragged-or-resized";
		break;
	default:
		g_message (
			"send_component_dialog(): "
			"Cannot handle object of type %d", vtype);
		return GTK_RESPONSE_CANCEL;
	}

	if (only_new_attendees && !component_has_new_attendees (comp)) {
		/* do not show the check if there is no new attendee and
		 * set as all attendees are required to be notified */
		*only_new_attendees = FALSE;

		/* pretend it as being passed NULL to simplify code below */
		only_new_attendees = NULL;
	}

	if (strip_alarms && !have_nonprocedural_alarm (comp)) {
		/* pretend it as being passed NULL to simplify code below */
		strip_alarms = NULL;
	}

	dialog = e_alert_dialog_new_for_args (parent, id, NULL);
	content_area = e_alert_dialog_get_content_area (E_ALERT_DIALOG (dialog));

	if (strip_alarms) {
		sa_checkbox = add_checkbox (GTK_BOX (content_area), _("Send my reminders with this event"));
		settings = e_util_ref_settings ("org.gnome.evolution.calendar");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sa_checkbox), g_settings_get_boolean (settings, "send-reminder-with-event"));
	}
	if (only_new_attendees)
		ona_checkbox = add_checkbox (GTK_BOX (content_area), _("Notify new attendees _only"));

	res = gtk_dialog_run (GTK_DIALOG (dialog));

	/*
	 * When the Escape key is pressed a GTK_RESPONSE_DELETE_EVENT is generated.
	 * We should treat this event as the user cancelling the operation
	 */
	if (res == GTK_RESPONSE_DELETE_EVENT)
		res = GTK_RESPONSE_CANCEL;

	if (res == GTK_RESPONSE_YES && strip_alarms) {
		gboolean send_alarms = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sa_checkbox));
		g_settings_set_boolean (settings, "send-reminder-with-event", send_alarms);
		*strip_alarms = !send_alarms;
	}
	if (only_new_attendees)
		*only_new_attendees = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ona_checkbox));

	gtk_widget_destroy (GTK_WIDGET (dialog));
	g_clear_object (&settings);

	return res;
}

gboolean
e_cal_dialogs_send_component_prompt_subject (GtkWindow *parent,
					     ICalComponent *component)
{
	ICalComponentKind kind;
	const gchar *id;

	kind = i_cal_component_isa (component);

	switch (kind) {
	case I_CAL_VEVENT_COMPONENT:
		id = "calendar:prompt-save-no-subject-calendar";
		break;

	case I_CAL_VTODO_COMPONENT:
		id = "calendar:prompt-save-no-subject-task";
		break;
	case I_CAL_VJOURNAL_COMPONENT:
		id = "calendar:prompt-send-no-subject-memo";
		break;

	default:
		g_message ("%s: Cannot handle object of type %d", G_STRFUNC, kind);
		return FALSE;
	}

	if (e_alert_run_dialog_for_args (parent, id, NULL) == GTK_RESPONSE_YES)
		return TRUE;
	else
		return FALSE;
}

gboolean
e_cal_dialogs_detach_and_copy (GtkWindow *parent,
			       ICalComponent *component)
{
	ICalComponentKind kind;
	gchar *summary;
	const gchar *id;
	gboolean res;

	kind = i_cal_component_isa (component);

	switch (kind) {
	case I_CAL_VEVENT_COMPONENT:
		id = "calendar:prompt-detach-copy-event";
		break;

	case I_CAL_VTODO_COMPONENT:
	case I_CAL_VJOURNAL_COMPONENT:
		return TRUE;

	default:
		g_message ("%s: Cannot handle object of type %d", G_STRFUNC, kind);
		return FALSE;
	}

	summary = e_calendar_view_dup_component_summary (component);
	res = e_alert_run_dialog_for_args (parent, id, summary, NULL) == GTK_RESPONSE_YES;
	g_free (summary);

	return res;
}
