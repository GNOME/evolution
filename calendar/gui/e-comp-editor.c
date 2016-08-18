/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>
#include <libecal/libecal.h>
#include <e-util/e-util.h>

#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-dialogs.h"
#include "itip-utils.h"
#include "print.h"

#include "e-comp-editor-page-general.h"
#include "e-comp-editor-page-attachments.h"
#include "e-comp-editor-event.h"
#include "e-comp-editor-memo.h"
#include "e-comp-editor-task.h"

#include "e-comp-editor.h"

struct _ECompEditorPrivate {
	EAlertBar *alert_bar; /* not referenced */
	EActivityBar *activity_bar; /* not referenced */
	GtkNotebook *content; /* not referenced */

	EAlert *validation_alert;

	EShell *shell;
	GSettings *calendar_settings;
	ESource *origin_source;
	icalcomponent *component;
	guint32 flags;

	EFocusTracker *focus_tracker;
	GtkUIManager *ui_manager;

	GSList *pages; /* ECompEditorPage * */
	gulong show_attendees_handler_id;

	ECompEditorPageGeneral *page_general; /* special page, can be added only once; not referenced */

	EActivity *target_client_opening;

	ECalClient *source_client;
	ECalClient *target_client;
	gchar *cal_email_address;
	gchar *alarm_email_address;
	gboolean changed;
	guint updating;
	gchar *title_suffix;

	ECompEditorPropertyPart *dtstart_part;
	ECompEditorPropertyPart *dtend_part;

	GtkWidget *restore_focus;
};

enum {
	PROP_0,
	PROP_ALARM_EMAIL_ADDRESS,
	PROP_CAL_EMAIL_ADDRESS,
	PROP_CHANGED,
	PROP_COMPONENT,
	PROP_FLAGS,
	PROP_ORIGIN_SOURCE,
	PROP_SHELL,
	PROP_SOURCE_CLIENT,
	PROP_TARGET_CLIENT,
	PROP_TITLE_SUFFIX
};

enum {
	TIMES_CHANGED,
	OBJECT_CREATED,
	EDITOR_CLOSED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GSList *opened_editors = NULL;

static void e_comp_editor_alert_sink_iface_init (EAlertSinkInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ECompEditor, e_comp_editor, GTK_TYPE_WINDOW,
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_comp_editor_alert_sink_iface_init))

static void
ece_restore_focus (ECompEditor *comp_editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (comp_editor->priv->restore_focus) {
		gtk_widget_grab_focus (comp_editor->priv->restore_focus);

		if (GTK_IS_ENTRY (comp_editor->priv->restore_focus))
			gtk_editable_set_position (GTK_EDITABLE (comp_editor->priv->restore_focus), 0);

		comp_editor->priv->restore_focus = NULL;
	}
}

static void
e_comp_editor_enable (ECompEditor *comp_editor,
		      gboolean enable)
{
	GtkActionGroup *group;
	GtkWidget *current_focus;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	current_focus = gtk_window_get_focus (GTK_WINDOW (comp_editor));

	gtk_widget_set_sensitive (GTK_WIDGET (comp_editor->priv->content), enable);

	group = e_comp_editor_get_action_group (comp_editor, "individual");
	gtk_action_group_set_sensitive (group, enable);

	group = e_comp_editor_get_action_group (comp_editor, "core");
	gtk_action_group_set_sensitive (group, enable);

	group = e_comp_editor_get_action_group (comp_editor, "editable");
	gtk_action_group_set_sensitive (group, enable);

	if (enable) {
		e_comp_editor_sensitize_widgets (comp_editor);
		ece_restore_focus (comp_editor);
	} else {
		comp_editor->priv->restore_focus = current_focus;
	}
}

static void
ece_set_attendees_for_delegation (ECalComponent *comp,
				  const gchar *address)
{
	icalproperty *prop;
	icalparameter *param;
	icalcomponent *icalcomp;
	gboolean again;

	icalcomp = e_cal_component_get_icalcomponent (comp);

	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
	     prop;
	     prop = again ? icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY) :
	     icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		const gchar *attendee = icalproperty_get_attendee (prop);
		const gchar *delfrom = NULL;

		again = FALSE;
		param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDFROM_PARAMETER);
		if (param)
			delfrom = icalparameter_get_delegatedfrom (param);
		if (!(g_str_equal (itip_strip_mailto (attendee), address) ||
		     ((delfrom && *delfrom) && g_str_equal (itip_strip_mailto (delfrom), address)))) {
			icalcomponent_remove_property (icalcomp, prop);
			icalproperty_free (prop);
			again = TRUE;
		}

	}
}

/* Utility function to get the mime-attachment list from the attachment
 * bar for sending the comp via itip. The list and its contents must
 * be freed by the caller.
 */
static GSList *
ece_get_mime_attach_list (ECompEditor *comp_editor)
{
	ECompEditorPage *page_attachments;
	EAttachmentStore *store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	struct CalMimeAttach *cal_mime_attach;
	GSList *attach_list = NULL;
	gboolean valid;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	page_attachments = e_comp_editor_get_page (comp_editor, E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS);
	if (!page_attachments)
		return NULL;

	store = e_comp_editor_page_attachments_get_store (E_COMP_EDITOR_PAGE_ATTACHMENTS (page_attachments));
	if (!store)
		return NULL;

	model = GTK_TREE_MODEL (store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		EAttachment *attachment;
		CamelDataWrapper *wrapper;
		CamelMimePart *mime_part;
		CamelStream *stream;
		GByteArray *byte_array;
		guchar *buffer = NULL;
		const gchar *description;
		const gchar *disposition;
		gint column_id;

		column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);
		mime_part = e_attachment_ref_mime_part (attachment);
		g_object_unref (attachment);

		valid = gtk_tree_model_iter_next (model, &iter);

		if (mime_part == NULL)
			continue;

		cal_mime_attach = g_malloc0 (sizeof (struct CalMimeAttach));
		wrapper = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

		byte_array = g_byte_array_new ();
		stream = camel_stream_mem_new_with_byte_array (byte_array);

		camel_data_wrapper_decode_to_stream_sync (
			wrapper, stream, NULL, NULL);
		buffer = g_memdup (byte_array->data, byte_array->len);

		camel_mime_part_set_content_id (mime_part, NULL);

		cal_mime_attach->encoded_data = (gchar *) buffer;
		cal_mime_attach->length = byte_array->len;
		cal_mime_attach->filename =
			g_strdup (camel_mime_part_get_filename (mime_part));
		description = camel_mime_part_get_description (mime_part);
		if (description == NULL || *description == '\0')
			description = _("attachment");
		cal_mime_attach->description = g_strdup (description);
		cal_mime_attach->content_type = g_strdup (
			camel_data_wrapper_get_mime_type (wrapper));
		cal_mime_attach->content_id = g_strdup (
			camel_mime_part_get_content_id (mime_part));

		disposition = camel_mime_part_get_disposition (mime_part);
		cal_mime_attach->disposition =
			(disposition != NULL) &&
			(g_ascii_strcasecmp (disposition, "inline") == 0);

		attach_list = g_slist_append (attach_list, cal_mime_attach);

		g_object_unref (mime_part);
		g_object_unref (stream);

	}

	return attach_list;
}

static void
e_comp_editor_set_component (ECompEditor *comp_editor,
			     const icalcomponent *component)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (component != NULL);

	if (comp_editor->priv->component)
		icalcomponent_free (comp_editor->priv->component);
	comp_editor->priv->component = icalcomponent_new_clone ((icalcomponent *) component);

	g_warn_if_fail (comp_editor->priv->component != NULL);
}

typedef struct _SaveData {
	ECompEditor *comp_editor;
	ECalClient *source_client;
	ECalClient *target_client;
	icalcomponent *component;
	gboolean with_send;
	gboolean close_after_save;
	ECalObjModType recur_mod;
	gboolean success;
	GError *error;
	gchar *alert_ident;
	gchar *alert_arg_0;

	gboolean object_created;
	ECalComponentItipMethod first_send;
	ECalComponentItipMethod second_send;
	ECalComponent *send_comp;
	EActivity *send_activity;
	gboolean strip_alarms;
	gboolean only_new_attendees;
	GSList *mime_attach_list;
} SaveData;

static void
save_data_free (SaveData *sd)
{
	if (sd) {
		e_comp_editor_enable (sd->comp_editor, TRUE);

		if (sd->success) {
			if (sd->close_after_save) {
				g_signal_emit (sd->comp_editor, signals[EDITOR_CLOSED], 0, TRUE, NULL);
				gtk_widget_destroy (GTK_WIDGET (sd->comp_editor));
			} else {
				e_comp_editor_set_component (sd->comp_editor, sd->component);

				e_comp_editor_fill_widgets (sd->comp_editor, sd->component);

				g_clear_object (&sd->comp_editor->priv->source_client);
				sd->comp_editor->priv->source_client = g_object_ref (sd->target_client);

				sd->comp_editor->priv->flags = sd->comp_editor->priv->flags & (~E_COMP_EDITOR_FLAG_IS_NEW);

				e_comp_editor_sensitize_widgets (sd->comp_editor);
				e_comp_editor_set_changed (sd->comp_editor, FALSE);
			}
		} else if (sd->alert_ident) {
			e_alert_submit (
				E_ALERT_SINK (sd->comp_editor), sd->alert_ident, sd->alert_arg_0,
				sd->error ? sd->error->message : _("Unknown error"), NULL);
		}

		if (sd->send_activity && e_activity_get_state (sd->send_activity) != E_ACTIVITY_CANCELLED)
			e_activity_set_state (sd->send_activity, E_ACTIVITY_COMPLETED);

		g_clear_object (&sd->comp_editor);
		g_clear_object (&sd->source_client);
		g_clear_object (&sd->target_client);
		g_clear_object (&sd->send_comp);
		g_clear_object (&sd->send_activity);
		g_clear_error (&sd->error);
		if (sd->component)
			icalcomponent_free (sd->component);
		g_slist_free_full (sd->mime_attach_list, itip_cal_mime_attach_free);
		g_free (sd->alert_ident);
		g_free (sd->alert_arg_0);
		g_free (sd);
	}
}

static gboolean
ece_send_process_method (SaveData *sd,
			 ECalComponentItipMethod send_method,
			 ECalComponent *send_comp,
			 ESourceRegistry *registry,
			 GCancellable *cancellable,
			 GAsyncReadyCallback callback,
			 gpointer user_data)
{
	GSList *mime_attach_list = NULL;

	g_return_val_if_fail (sd != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (send_comp), FALSE);
	g_return_val_if_fail (send_method != E_CAL_COMPONENT_METHOD_NONE, FALSE);

	if (e_cal_component_has_attachments (send_comp) &&
	    e_client_check_capability (E_CLIENT (sd->target_client), CAL_STATIC_CAPABILITY_CREATE_MESSAGES)) {
		/* Clone the component with attachments set to CID:...  */
		GSList *attach_list = NULL;
		GSList *attach;

		/* mime_attach_list is freed by itip_send_component() */
		mime_attach_list = sd->mime_attach_list;
		sd->mime_attach_list = NULL;

		for (attach = mime_attach_list; attach; attach = attach->next) {
			struct CalMimeAttach *cma = (struct CalMimeAttach *) attach->data;

			attach_list = g_slist_append (
				attach_list, g_strconcat (
				"cid:", cma->content_id, NULL));
		}

		if (attach_list) {
			e_cal_component_set_attachment_list (send_comp, attach_list);

			g_slist_free_full (attach_list, g_free);
		}
	}

	itip_send_component (
		registry, send_method, send_comp, sd->target_client,
		NULL, mime_attach_list, NULL, sd->strip_alarms,
		sd->only_new_attendees, FALSE,
		cancellable, callback, user_data);

	return TRUE;
}

static void
ecep_second_send_processed_cb (GObject *source_object,
			       GAsyncResult *result,
			       gpointer user_data)
{
	SaveData *sd = user_data;

	g_return_if_fail (sd != NULL);

	sd->success = itip_send_component_finish (result, &sd->error);

	save_data_free (sd);
}

static void
ecep_first_send_processed_cb (GObject *source_object,
			      GAsyncResult *result,
			      gpointer user_data)
{
	SaveData *sd = user_data;

	g_return_if_fail (sd != NULL);

	sd->success = itip_send_component_finish (result, &sd->error);
	if (!sd->success || sd->second_send == E_CAL_COMPONENT_METHOD_NONE) {
		save_data_free (sd);
	} else {
		sd->success = ece_send_process_method (sd, sd->second_send, sd->send_comp,
			e_shell_get_registry (sd->comp_editor->priv->shell),
			e_activity_get_cancellable (sd->send_activity),
			ecep_second_send_processed_cb, sd);
		if (!sd->success)
			save_data_free (sd);
	}
}

static void
ece_prepare_send_component_done (gpointer ptr)
{
	SaveData *sd = ptr;

	g_return_if_fail (sd != NULL);
	g_return_if_fail (E_IS_COMP_EDITOR (sd->comp_editor));
	g_return_if_fail (sd->send_activity != NULL);

	sd->success = ece_send_process_method (sd, sd->first_send, sd->send_comp,
		e_shell_get_registry (sd->comp_editor->priv->shell),
		e_activity_get_cancellable (sd->send_activity),
		ecep_first_send_processed_cb, sd);
	if (!sd->success)
		save_data_free (sd);
}

static void
ece_prepare_send_component_thread (EAlertSinkThreadJobData *job_data,
				   gpointer user_data,
				   GCancellable *cancellable,
				   GError **error)
{
	SaveData *sd = user_data;
	const gchar *alert_ident;
	ECalComponent *send_comp = NULL;
	guint32 flags;
	ESourceRegistry *registry;

	g_return_if_fail (sd != NULL);
	g_return_if_fail (E_IS_CAL_CLIENT (sd->target_client));
	g_return_if_fail (sd->component != NULL);

	while (!sd->send_activity) {
		/* Give the main thread a chance to set this object
		   and give it a 50 milliseconds delay too */
		g_thread_yield ();
		g_usleep (50000);
	}

	switch (icalcomponent_isa (sd->component)) {
		case ICAL_VEVENT_COMPONENT:
			alert_ident = "calendar:failed-send-event";
			break;
		case ICAL_VJOURNAL_COMPONENT:
			alert_ident = "calendar:failed-send-memo";
			break;
		case ICAL_VTODO_COMPONENT:
			alert_ident = "calendar:failed-send-task";
			break;
		default:
			g_warning ("%s: Cannot send component of kind %d", G_STRFUNC, icalcomponent_isa (sd->component));
			sd->success = FALSE;
			sd->alert_ident = g_strdup ("calendar:failed-send-event");
			return;
	}

	g_free (sd->alert_ident);
	sd->alert_ident = g_strdup (alert_ident);

	e_alert_sink_thread_job_set_alert_ident (job_data, alert_ident);

	flags = e_comp_editor_get_flags (sd->comp_editor);
	registry = e_shell_get_registry (sd->comp_editor->priv->shell);

	if (sd->recur_mod == E_CAL_OBJ_MOD_ALL && e_cal_component_is_instance (sd->send_comp)) {
		/* Ensure we send the master object, not the instance only */
		icalcomponent *icalcomp = NULL;
		const gchar *uid = NULL;

		e_cal_component_get_uid (sd->send_comp, &uid);
		if (e_cal_client_get_object_sync (sd->target_client, uid, NULL, &icalcomp, cancellable, NULL) &&
		    icalcomp != NULL) {
			send_comp = e_cal_component_new_from_icalcomponent (icalcomp);
		}
	}

	if (!send_comp)
		send_comp = e_cal_component_clone (sd->send_comp);

	cal_comp_util_copy_new_attendees (send_comp, sd->send_comp);

	/* The user updates the delegated status to the Organizer,
	 * so remove all other attendees. */
	if ((flags & E_COMP_EDITOR_FLAG_DELEGATE) != 0) {
		gchar *address;

		address = itip_get_comp_attendee (registry, send_comp, sd->target_client);

		if (address) {
			ece_set_attendees_for_delegation (send_comp, address);
			g_free (address);
		}
	}

	g_clear_object (&sd->send_comp);
	sd->send_comp = send_comp;
}

static void
ece_save_component_done (gpointer ptr)
{
	SaveData *sd = ptr;

	g_return_if_fail (sd != NULL);
	g_return_if_fail (E_IS_COMP_EDITOR (sd->comp_editor));

	if (sd->success) {
		ECalComponent *comp;
		gboolean delegated, is_new_meeting;
		gboolean only_new_attendees = FALSE;
		gboolean strip_alarms = TRUE;
		guint32 flags;

		if (sd->object_created)
			g_signal_emit (sd->comp_editor, signals[OBJECT_CREATED], 0, NULL);

		comp = e_cal_component_new_from_icalcomponent (icalcomponent_new_clone (sd->component));
		if (sd->comp_editor->priv->page_general) {
			GSList *added_attendees;

			added_attendees = e_comp_editor_page_general_get_added_attendees (sd->comp_editor->priv->page_general);
			cal_comp_util_set_added_attendees_mails (comp, added_attendees);
		}

		flags = e_comp_editor_get_flags (sd->comp_editor);
		is_new_meeting = (flags & E_COMP_EDITOR_FLAG_WITH_ATTENDEES) == 0 ||
			(flags & E_COMP_EDITOR_FLAG_IS_NEW) != 0;
		delegated = (flags & E_COMP_EDITOR_FLAG_DELEGATE) != 0 &&
			e_cal_client_check_save_schedules (sd->target_client);

		if (delegated || (sd->with_send && e_cal_dialogs_send_component (
			GTK_WINDOW (sd->comp_editor), sd->target_client, comp,
			is_new_meeting, &strip_alarms, &only_new_attendees))) {
			ESourceRegistry *registry;
			EActivity *activity;

			registry = e_shell_get_registry (sd->comp_editor->priv->shell);

			if (delegated)
				only_new_attendees = FALSE;

			if ((itip_organizer_is_user (registry, comp, sd->target_client) ||
			     itip_sentby_is_user (registry, comp, sd->target_client))) {
				if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_JOURNAL)
					sd->first_send = E_CAL_COMPONENT_METHOD_PUBLISH;
				else
					sd->first_send = E_CAL_COMPONENT_METHOD_REQUEST;
			} else {
				sd->first_send = E_CAL_COMPONENT_METHOD_REQUEST;

				if ((flags & E_COMP_EDITOR_FLAG_DELEGATE) != 0)
					sd->second_send = E_CAL_COMPONENT_METHOD_REPLY;
			}

			sd->mime_attach_list = ece_get_mime_attach_list (sd->comp_editor);
			sd->strip_alarms = strip_alarms;
			sd->only_new_attendees = only_new_attendees;
			sd->send_comp = comp;
			sd->success = FALSE;
			sd->alert_ident = g_strdup ("calendar:failed-send-event");
			sd->alert_arg_0 = e_util_get_source_full_name (registry, e_client_get_source (E_CLIENT (sd->target_client)));

			activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (sd->comp_editor),
				_("Sending notifications to attendees..."), sd->alert_ident, sd->alert_arg_0,
				ece_prepare_send_component_thread, sd, ece_prepare_send_component_done);

			if (activity)
				e_activity_bar_set_activity (sd->comp_editor->priv->activity_bar, activity);

			/* The thread is waiting for this to be set first */
			sd->send_activity = activity;

			return;
		}

		g_clear_object (&comp);
	}

	save_data_free (sd);
}

static gboolean
ece_save_component_attachments_sync (ECalClient *cal_client,
				     icalcomponent *component,
				     GCancellable *cancellable,
				     GError **error)
{
	icalproperty *prop;
	gchar *target_filename_prefix, *filename_prefix, *tmp;
	gboolean success = TRUE;

	g_return_val_if_fail (E_IS_CAL_CLIENT (cal_client), FALSE);
	g_return_val_if_fail (component != NULL, FALSE);

	tmp = g_strdup (icalcomponent_get_uid (component));
	e_filename_make_safe (tmp);
	filename_prefix = g_strconcat (tmp, "-", NULL);
	g_free (tmp);

	target_filename_prefix = g_build_filename (
		e_cal_client_get_local_attachment_store (cal_client),
		filename_prefix, NULL);

	g_free (filename_prefix);

	for (prop = icalcomponent_get_first_property (component, ICAL_ATTACH_PROPERTY);
	     prop && success;
	     prop = icalcomponent_get_next_property (component, ICAL_ATTACH_PROPERTY)) {
		icalattach *attach;
		gchar *uri = NULL;

		attach = icalproperty_get_attach (prop);
		if (!attach)
			continue;

		if (icalattach_get_is_url (attach)) {
			const gchar *data;
			gsize buf_size;

			data = icalattach_get_url (attach);
			buf_size = strlen (data);
			uri = g_malloc0 (buf_size + 1);

			icalvalue_decode_ical_string (data, uri, buf_size);
		}

		if (uri) {
			if (g_ascii_strncasecmp (uri, "file://", 7) == 0 &&
			    !g_str_has_prefix (uri + 7, target_filename_prefix)) {
				GFile *source, *destination;
				gchar *decoded_filename;
				gchar *target_filename;

				decoded_filename = g_uri_unescape_string (strrchr (uri, '/') + 1, NULL);
				target_filename = g_strconcat (target_filename_prefix, decoded_filename, NULL);
				g_free (decoded_filename);

				source = g_file_new_for_uri (uri);
				destination = g_file_new_for_path (target_filename);

				if (source && destination) {
					success = g_file_copy (source, destination, G_FILE_COPY_OVERWRITE, cancellable, NULL, NULL, error);
					if (success) {
						g_free (uri);
						uri = g_file_get_uri (destination);

						if (uri) {
							icalattach *new_attach;
							gsize buf_size;
							gchar *buf;

							buf_size = 2 * strlen (uri) + 1;
							buf = g_malloc0 (buf_size);

							icalvalue_encode_ical_string (uri, buf, buf_size);
							new_attach = icalattach_new_from_url (buf);

							icalproperty_set_attach (prop, new_attach);

							icalattach_unref (new_attach);
							g_free (buf);
						}
					}
				}

				g_clear_object (&source);
				g_clear_object (&destination);
				g_free (target_filename);
			}

			g_free (uri);
		}

		success = success & !g_cancellable_set_error_if_cancelled (cancellable, error);
	}

	g_free (target_filename_prefix);

	return success;
}

static void
ece_gather_tzids_cb (icalparameter *param,
		     gpointer user_data)
{
	GHashTable *tzids = user_data;
	const gchar *tzid;

	g_return_if_fail (param != NULL);
	g_return_if_fail (tzids != NULL);

	tzid = icalparameter_get_tzid (param);
	if (tzid && *tzid && g_ascii_strcasecmp (tzid, "UTC") != 0)
		g_hash_table_insert (tzids, g_strdup (tzid), NULL);
}

static gboolean
ece_save_component_add_timezones_sync (SaveData *sd,
				       GCancellable *cancellable,
				       GError **error)
{
	GHashTable *tzids;
	GHashTableIter iter;
	gpointer key, value;
	gboolean source_is_target;

	g_return_val_if_fail (sd != NULL, FALSE);
	g_return_val_if_fail (sd->component != NULL, FALSE);
	g_return_val_if_fail (sd->target_client != NULL, FALSE);

	sd->success = TRUE;

	tzids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	source_is_target = !sd->source_client ||
		e_source_equal (e_client_get_source (E_CLIENT (sd->target_client)),
				e_client_get_source (E_CLIENT (sd->source_client)));

	icalcomponent_foreach_tzid (sd->component, ece_gather_tzids_cb, tzids);

	g_hash_table_iter_init (&iter, tzids);
	while (sd->success && g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *tzid = key;
		icaltimezone *zone = NULL;
		GError *local_error = NULL;

		if (!e_cal_client_get_timezone_sync (source_is_target ? sd->target_client : sd->source_client,
			tzid, &zone, cancellable, &local_error)) {
			zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
			if (!zone)
				zone = icaltimezone_get_builtin_timezone (tzid);
			if (!zone) {
				g_propagate_error (error, local_error);
				local_error = NULL;
				sd->success = FALSE;
				break;
			}
		}

		sd->success = e_cal_client_add_timezone_sync (sd->target_client, zone, cancellable, error);

		g_clear_error (&local_error);
	}

	g_hash_table_destroy (tzids);

	return sd->success;
}

static void
ece_save_component_thread (EAlertSinkThreadJobData *job_data,
			   gpointer user_data,
			   GCancellable *cancellable,
			   GError **error)
{
	SaveData *sd = user_data;
	const gchar *create_alert_ident, *modify_alert_ident, *remove_alert_ident, *get_alert_ident;
	gchar *orig_uid;

	g_return_if_fail (sd != NULL);
	g_return_if_fail (E_IS_CAL_CLIENT (sd->target_client));
	g_return_if_fail (sd->component != NULL);

	switch (icalcomponent_isa (sd->component)) {
		case ICAL_VEVENT_COMPONENT:
			create_alert_ident = "calendar:failed-create-event";
			modify_alert_ident = "calendar:failed-modify-event";
			remove_alert_ident = "calendar:failed-remove-event";
			get_alert_ident = "calendar:failed-get-event";
			break;
		case ICAL_VJOURNAL_COMPONENT:
			create_alert_ident = "calendar:failed-create-memo";
			modify_alert_ident = "calendar:failed-modify-memo";
			remove_alert_ident = "calendar:failed-remove-memo";
			get_alert_ident = "calendar:failed-get-memo";
			break;
		case ICAL_VTODO_COMPONENT:
			create_alert_ident = "calendar:failed-create-task";
			modify_alert_ident = "calendar:failed-modify-task";
			remove_alert_ident = "calendar:failed-remove-task";
			get_alert_ident = "calendar:failed-get-task";
			break;
		default:
			g_warning ("%s: Cannot save component of kind %d", G_STRFUNC, icalcomponent_isa (sd->component));
			return;
	}

	sd->success = ece_save_component_add_timezones_sync (sd, cancellable, error);
	if (!sd->success) {
		e_alert_sink_thread_job_set_alert_ident (job_data, "calendar:failed-add-timezone");
		return;
	}

	sd->success = ece_save_component_attachments_sync (sd->target_client, sd->component, cancellable, error);
	if (!sd->success) {
		e_alert_sink_thread_job_set_alert_ident (job_data, "calendar:failed-save-attachments");
		return;
	}

	orig_uid = g_strdup (icalcomponent_get_uid (sd->component));

	if (cal_comp_is_icalcomp_on_server_sync (sd->component, sd->target_client, cancellable, error)) {
		ECalComponent *comp;
		gboolean has_recurrences;

		e_alert_sink_thread_job_set_alert_ident (job_data, modify_alert_ident);

		comp = e_cal_component_new_from_icalcomponent (icalcomponent_new_clone (sd->component));
		g_return_if_fail (comp != NULL);

		has_recurrences = e_cal_util_component_has_recurrences (sd->component);

		if (has_recurrences && sd->recur_mod == E_CAL_OBJ_MOD_ALL)
			sd->success = comp_util_sanitize_recurrence_master_sync (comp, sd->target_client, cancellable, error);
		else
			sd->success = TRUE;

		if (sd->recur_mod == E_CAL_OBJ_MOD_THIS) {
			e_cal_component_set_rdate_list (comp, NULL);
			e_cal_component_set_rrule_list (comp, NULL);
			e_cal_component_set_exdate_list (comp, NULL);
			e_cal_component_set_exrule_list (comp, NULL);
		}

		sd->success = sd->success && e_cal_client_modify_object_sync (
			sd->target_client, e_cal_component_get_icalcomponent (comp), sd->recur_mod, cancellable, error);

		g_clear_object (&comp);
	} else {
		gchar *uid = NULL;

		e_alert_sink_thread_job_set_alert_ident (job_data, create_alert_ident);

		sd->success = e_cal_client_create_object_sync (sd->target_client, sd->component, &uid, cancellable, error);

		if (sd->success) {
			icalcomponent_set_uid (sd->component, uid);
			g_free (uid);

			sd->object_created = TRUE;
		}
	}

	if (sd->success && sd->source_client &&
	    !e_source_equal (e_client_get_source (E_CLIENT (sd->target_client)),
			     e_client_get_source (E_CLIENT (sd->source_client))) &&
	    cal_comp_is_icalcomp_on_server_sync (sd->component, sd->source_client, cancellable, NULL)) {
		ECalObjModType recur_mod = E_CAL_OBJ_MOD_THIS;

		/* Comp found a new home. Remove it from old one. */
		if (e_cal_util_component_is_instance (sd->component) ||
		    e_cal_util_component_has_recurrences (sd->component))
			recur_mod = E_CAL_OBJ_MOD_ALL;

		sd->success = e_cal_client_remove_object_sync (
			sd->source_client, orig_uid,
			NULL, recur_mod, cancellable, error);

		if (!sd->success) {
			gchar *source_display_name;

			source_display_name = e_util_get_source_full_name (e_shell_get_registry (sd->comp_editor->priv->shell),
				e_client_get_source (E_CLIENT (sd->source_client)));

			e_alert_sink_thread_job_set_alert_ident (job_data, remove_alert_ident);
			e_alert_sink_thread_job_set_alert_arg_0 (job_data, source_display_name);

			g_free (source_display_name);
		}
	}

	g_free (orig_uid);

	if (sd->success && !sd->close_after_save) {
		icalcomponent *comp = NULL;
		gchar *uid, *rid = NULL;

		uid = g_strdup (icalcomponent_get_uid (sd->component));
		if (icalcomponent_get_first_property (sd->component, ICAL_RECURRENCEID_PROPERTY)) {
			struct icaltimetype ridtt;

			ridtt = icalcomponent_get_recurrenceid (sd->component);
			if (icaltime_is_valid_time (ridtt) && !icaltime_is_null_time (ridtt)) {
				rid = icaltime_as_ical_string_r (ridtt);
			}
		}

		sd->success = e_cal_client_get_object_sync (sd->target_client, uid, rid, &comp, cancellable, error);
		if (sd->success && comp) {
			icalcomponent_free (sd->component);
			sd->component = comp;
		} else {
			e_alert_sink_thread_job_set_alert_ident (job_data, get_alert_ident);
		}

		g_free (uid);
		g_free (rid);
	}
}

static void
ece_save_component (ECompEditor *comp_editor,
		    icalcomponent *component,
		    gboolean with_send,
		    gboolean close_after_save)
{
	EActivity *activity;
	const gchar *summary;
	ECalObjModType recur_mod = E_CAL_OBJ_MOD_THIS;
	SaveData *sd;
	gchar *source_display_name;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (component != NULL);

	summary = icalcomponent_get_summary (component);
	if (!summary || !*summary) {
		if (!e_cal_dialogs_send_component_prompt_subject (GTK_WINDOW (comp_editor), component)) {
			return;
		}
	}

	if (e_cal_util_component_is_instance (component)) {
		if (!e_cal_dialogs_recur_icalcomp (comp_editor->priv->target_client,
			component, &recur_mod, GTK_WINDOW (comp_editor), FALSE)) {
			return;
		}
	} else if (e_cal_util_component_has_recurrences (component)) {
		recur_mod = E_CAL_OBJ_MOD_ALL;
	}

	e_comp_editor_enable (comp_editor, FALSE);

	sd = g_new0 (SaveData, 1);
	sd->comp_editor = g_object_ref (comp_editor);
	sd->source_client = comp_editor->priv->source_client ? g_object_ref (comp_editor->priv->source_client) : NULL;
	sd->target_client = g_object_ref (comp_editor->priv->target_client);
	sd->component = icalcomponent_new_clone (component);
	sd->with_send = with_send;
	sd->close_after_save = close_after_save;
	sd->recur_mod = recur_mod;
	sd->first_send = E_CAL_COMPONENT_METHOD_NONE;
	sd->second_send = E_CAL_COMPONENT_METHOD_NONE;
	sd->success = FALSE;

	source_display_name = e_util_get_source_full_name (e_shell_get_registry (comp_editor->priv->shell),
		e_client_get_source (E_CLIENT (sd->target_client)));

	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (comp_editor),
		_("Saving changes..."), "calendar:failed-create-event", source_display_name,
		ece_save_component_thread, sd, ece_save_component_done);

	if (activity)
		e_activity_bar_set_activity (comp_editor->priv->activity_bar, activity);

	g_clear_object (&activity);
	g_free (source_display_name);
}

typedef struct _OpenTargetClientData {
	ECompEditor *comp_editor;
	ESource *source;
	gchar *extension_name;
	EClient *client;
	gchar *cal_email_address;
	gchar *alarm_email_address;
	gboolean is_target_client_change;
	EActivity *activity;
} OpenTargetClientData;

static void
open_target_client_data_free (gpointer ptr)
{
	OpenTargetClientData *otc = ptr;

	if (otc) {
		if (otc->comp_editor) {
			if (otc->client) {
				e_comp_editor_set_alarm_email_address (otc->comp_editor, otc->alarm_email_address);
				e_comp_editor_set_cal_email_address (otc->comp_editor, otc->cal_email_address);
				e_comp_editor_set_target_client (otc->comp_editor, E_CAL_CLIENT (otc->client));

				if (otc->is_target_client_change)
					e_comp_editor_set_changed (otc->comp_editor, TRUE);
			}

			if (otc->comp_editor->priv->activity_bar && otc->activity) {
				if (otc->activity == e_activity_bar_get_activity (otc->comp_editor->priv->activity_bar))
					e_activity_bar_set_activity (otc->comp_editor->priv->activity_bar, NULL);

				if (otc->activity == otc->comp_editor->priv->target_client_opening)
					g_clear_object (&otc->comp_editor->priv->target_client_opening);
			}

			if (otc->source) {
				EShell *shell;
				ECredentialsPrompter *credentials_prompter;

				shell = e_comp_editor_get_shell (otc->comp_editor);
				credentials_prompter = e_shell_get_credentials_prompter (shell);

				e_credentials_prompter_set_auto_prompt_disabled_for (credentials_prompter, otc->source, TRUE);
			}

			e_comp_editor_sensitize_widgets (otc->comp_editor);
		}

		g_clear_object (&otc->comp_editor);
		g_clear_object (&otc->source);
		g_clear_object (&otc->client);
		g_clear_object (&otc->activity);
		g_free (otc->extension_name);
		g_free (otc->cal_email_address);
		g_free (otc->alarm_email_address);
		g_free (otc);
	}
}

static void
comp_editor_open_target_client_thread (EAlertSinkThreadJobData *job_data,
				       gpointer user_data,
				       GCancellable *cancellable,
				       GError **error)
{
	OpenTargetClientData *otc = user_data;
	EClientCache *client_cache;

	g_return_if_fail (otc != NULL);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return;

	g_return_if_fail (E_IS_COMP_EDITOR (otc->comp_editor));
	g_return_if_fail (E_IS_SOURCE (otc->source));
	g_return_if_fail (otc->extension_name != NULL);

	client_cache = e_shell_get_client_cache (e_comp_editor_get_shell (otc->comp_editor));

	otc->client = e_client_cache_get_client_sync (client_cache, otc->source, otc->extension_name,
		30, cancellable, error);

	if (otc->client) {
		/* Cache some properties which require remote calls */

		if (!g_cancellable_is_cancelled (cancellable)) {
			e_client_get_capabilities (otc->client);
		}

		if (!g_cancellable_is_cancelled (cancellable)) {
			e_client_get_backend_property_sync (otc->client,
				CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS,
				&otc->cal_email_address,
				cancellable, error);
		}

		if (!g_cancellable_is_cancelled (cancellable)) {
			e_client_get_backend_property_sync (otc->client,
				CAL_BACKEND_PROPERTY_ALARM_EMAIL_ADDRESS,
				&otc->alarm_email_address,
				cancellable, error);
		}

		if (g_cancellable_is_cancelled (cancellable))
			g_clear_object (&otc->client);
	}
}

typedef struct _UpdateActivityBarData {
	ECompEditor *comp_editor;
	EActivity *activity;
} UpdateActivityBarData;

static void
update_activity_bar_data_free (gpointer ptr)
{
	UpdateActivityBarData *uab = ptr;

	if (uab) {
		g_clear_object (&uab->comp_editor);
		g_clear_object (&uab->activity);
		g_free (uab);
	}
}

static gboolean
update_activity_bar_cb (gpointer user_data)
{
	UpdateActivityBarData *uab = user_data;

	g_return_val_if_fail (uab != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMP_EDITOR (uab->comp_editor), FALSE);
	g_return_val_if_fail (E_IS_ACTIVITY (uab->activity), FALSE);

	if (uab->comp_editor->priv->target_client_opening == uab->activity &&
	    e_activity_get_state (uab->activity) != E_ACTIVITY_CANCELLED &&
	    e_activity_get_state (uab->activity) != E_ACTIVITY_COMPLETED) {
		e_activity_bar_set_activity (uab->comp_editor->priv->activity_bar, uab->activity);
	}

	return FALSE;
}

static void
e_comp_editor_open_target_client (ECompEditor *comp_editor)
{
	OpenTargetClientData *otc;
	ESource *source;
	EActivity *activity;
	ECredentialsPrompter *credentials_prompter;
	gchar *source_display_name, *description = NULL, *alert_ident = NULL, *alert_arg_0 = NULL;
	gboolean is_target_client_change;
	const gchar *extension_name;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (comp_editor->priv->page_general != NULL);

	source = e_comp_editor_page_general_ref_selected_source (comp_editor->priv->page_general);
	if (!source)
		return;

	if (comp_editor->priv->target_client &&
	    e_client_get_source (E_CLIENT (comp_editor->priv->target_client)) == source) {
		g_clear_object (&source);
		return;
	}

	if (comp_editor->priv->target_client_opening) {
		e_activity_cancel (comp_editor->priv->target_client_opening);
		g_clear_object (&comp_editor->priv->target_client_opening);
	}

	is_target_client_change = comp_editor->priv->target_client != NULL;
	g_clear_object (&comp_editor->priv->target_client);

	extension_name = e_comp_editor_page_general_get_source_extension_name (comp_editor->priv->page_general);
	source_display_name = e_util_get_source_full_name (
		e_shell_get_registry (e_comp_editor_get_shell (comp_editor)),
		source);

	g_return_if_fail (e_util_get_open_source_job_info (extension_name, source_display_name,
		&description, &alert_ident, &alert_arg_0));

	credentials_prompter = e_shell_get_credentials_prompter (e_comp_editor_get_shell (comp_editor));
	e_credentials_prompter_set_auto_prompt_disabled_for (credentials_prompter, source, FALSE);

	otc = g_new0 (OpenTargetClientData, 1);
	otc->extension_name = g_strdup (extension_name);
	otc->comp_editor = g_object_ref (comp_editor);
	otc->source = g_object_ref (source);
	otc->is_target_client_change = is_target_client_change;

	activity = e_alert_sink_submit_thread_job (
		E_ALERT_SINK (comp_editor), description, alert_ident, alert_arg_0,
		comp_editor_open_target_client_thread, otc,
		open_target_client_data_free);

	otc->activity = g_object_ref (activity);
	comp_editor->priv->target_client_opening = g_object_ref (activity);

	/* Close all alerts */
	while (e_alert_bar_close_alert (comp_editor->priv->alert_bar)) {
		;
	}

	if (comp_editor->priv->activity_bar) {
		UpdateActivityBarData *uab;

		uab = g_new0 (UpdateActivityBarData, 1);
		uab->comp_editor = g_object_ref (comp_editor);
		uab->activity = g_object_ref (activity);

		/* To avoid UI flickering when the source can be opened quickly */
		g_timeout_add_seconds_full (G_PRIORITY_LOW, 1,
			update_activity_bar_cb, uab, update_activity_bar_data_free);
	}

	g_free (description);
	g_free (alert_ident);
	g_free (alert_arg_0);
	g_free (source_display_name);
	g_clear_object (&source);
	g_clear_object (&activity);
}

static void
e_comp_editor_update_window_title (ECompEditor *comp_editor)
{
	ECompEditorClass *comp_editor_class;
	gboolean with_attendees = FALSE;
	const gchar *format, *title_suffix;
	gchar *title;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (comp_editor->priv->page_general)
		with_attendees = e_comp_editor_page_general_get_show_attendees (comp_editor->priv->page_general);

	comp_editor_class = E_COMP_EDITOR_GET_CLASS (comp_editor);
	if (with_attendees)
		format = comp_editor_class->title_format_with_attendees;
	else
		format = comp_editor_class->title_format_without_attendees;

	title_suffix = e_comp_editor_get_title_suffix (comp_editor);

	title = g_strdup_printf (format, title_suffix && *title_suffix ? title_suffix : _("No Summary"));

	gtk_window_set_icon_name (GTK_WINDOW (comp_editor), comp_editor_class->icon_name);
	gtk_window_set_title (GTK_WINDOW (comp_editor), title);

	g_free (title);
}

static void
e_comp_editor_close (ECompEditor *comp_editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	g_signal_emit (comp_editor, signals[EDITOR_CLOSED], 0, FALSE, NULL);

	gtk_widget_destroy (GTK_WIDGET (comp_editor));
}

static void
e_comp_editor_save_and_close (ECompEditor *comp_editor,
			      gboolean can_close)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (comp_editor->priv->component) {
		icalcomponent *component = icalcomponent_new_clone (comp_editor->priv->component);
		if (component && e_comp_editor_fill_component (comp_editor, component)) {
			ece_save_component (comp_editor, component, TRUE, can_close);
			icalcomponent_free (component);
		}
	}
}

static GtkResponseType
ece_save_component_dialog (ECompEditor *comp_editor)
{
	const icalcomponent *component;
	GtkWindow *parent;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), GTK_RESPONSE_NO);
	g_return_val_if_fail (e_comp_editor_get_component (comp_editor) != NULL, GTK_RESPONSE_NO);

	parent = GTK_WINDOW (comp_editor);
	component = e_comp_editor_get_component (comp_editor);
	switch (icalcomponent_isa (component)) {
		case ICAL_VEVENT_COMPONENT:
			if (e_comp_editor_page_general_get_show_attendees (comp_editor->priv->page_general))
				return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-meeting", NULL);
			else
				return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-appointment", NULL);
		case ICAL_VTODO_COMPONENT:
			return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-task", NULL);
		case ICAL_VJOURNAL_COMPONENT:
			return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-memo", NULL);
		default:
			return GTK_RESPONSE_NO;
	}
}

static gboolean
e_comp_editor_prompt_and_save_changes (ECompEditor *comp_editor,
				       gboolean with_send)
{
	icalcomponent *component;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);

	if (!e_comp_editor_get_changed (comp_editor))
		return TRUE;

	switch (ece_save_component_dialog (comp_editor)) {
	case GTK_RESPONSE_YES: /* Save */
		if (e_client_is_readonly (E_CLIENT (comp_editor->priv->target_client))) {
			e_alert_submit (
				E_ALERT_SINK (comp_editor),
				"calendar:prompt-read-only-cal-editor",
				e_source_get_display_name (
					e_client_get_source (E_CLIENT (comp_editor->priv->target_client))),
				NULL);
			/* don't discard changes when selected readonly calendar */
			return FALSE;
		}

		if (comp_editor->priv->component &&
		    e_comp_editor_page_general_get_show_attendees (comp_editor->priv->page_general) &&
		    icalcomponent_isa (comp_editor->priv->component) == ICAL_VTODO_COMPONENT
		    && e_client_check_capability (E_CLIENT (comp_editor->priv->target_client), CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT)) {
			e_alert_submit (
				E_ALERT_SINK (comp_editor),
				"calendar:prompt-no-task-assignment-editor",
				e_source_get_display_name (
					e_client_get_source (E_CLIENT (comp_editor->priv->target_client))),
				NULL);
			return FALSE;
		}

		component = icalcomponent_new_clone (comp_editor->priv->component);
		if (!e_comp_editor_fill_component (comp_editor, component)) {
			icalcomponent_free (component);
			return FALSE;
		}

		ece_save_component (comp_editor, component, with_send, TRUE);

		return FALSE;
	case GTK_RESPONSE_NO: /* Discard */
		return TRUE;
	case GTK_RESPONSE_CANCEL: /* Cancel */
	default:
		return FALSE;
	}

	return FALSE;
}

static void
action_close_cb (GtkAction *action,
                 ECompEditor *comp_editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (e_comp_editor_prompt_and_save_changes (comp_editor, TRUE))
		e_comp_editor_close (comp_editor);
}

static void
action_help_cb (GtkAction *action,
                ECompEditor *comp_editor)
{
	ECompEditorClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	klass = E_COMP_EDITOR_GET_CLASS (comp_editor);
	g_return_if_fail (klass->help_section != NULL);

	e_display_help (GTK_WINDOW (comp_editor), klass->help_section);
}

static void
ece_print_or_preview (ECompEditor *comp_editor,
		      GtkPrintOperationAction print_action)
{
	icalcomponent *component;
	ECalComponent *comp;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (e_comp_editor_get_component (comp_editor) != NULL);

	component = icalcomponent_new_clone (e_comp_editor_get_component (comp_editor));
	if (!e_comp_editor_fill_component (comp_editor, component)) {
		icalcomponent_free (component);
		return;
	}

	comp = e_cal_component_new_from_icalcomponent (component);
	g_return_if_fail (comp != NULL);

	print_comp (comp,
		e_comp_editor_get_target_client (comp_editor),
		calendar_config_get_icaltimezone (),
		calendar_config_get_24_hour_format (),
		print_action);

	g_object_unref (comp);
}

static void
action_print_cb (GtkAction *action,
                 ECompEditor *comp_editor)
{
	ece_print_or_preview (comp_editor, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_print_preview_cb (GtkAction *action,
                         ECompEditor *comp_editor)
{
	ece_print_or_preview (comp_editor, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
action_save_cb (GtkAction *action,
                ECompEditor *comp_editor)
{
	e_comp_editor_save_and_close (comp_editor, FALSE);
}

static void
action_save_and_close_cb (GtkAction *action,
                          ECompEditor *comp_editor)
{
	e_comp_editor_save_and_close (comp_editor, TRUE);
}

static gboolean
ece_organizer_email_address_is_user (ECompEditor *comp_editor,
				     EClient *client,
				     const gchar *email_address,
				     gboolean is_organizer)
{
	ESourceRegistry *registry;
	const gchar *cal_email_address;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);

	email_address = itip_strip_mailto (email_address);

	if (!email_address || !*email_address)
		return FALSE;

	cal_email_address = e_comp_editor_get_cal_email_address (comp_editor);
	if (cal_email_address && *cal_email_address &&
	    g_ascii_strcasecmp (cal_email_address, email_address) == 0) {
		return TRUE;
	}

	if (is_organizer && e_client_check_capability (client, CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS))
		return FALSE;

	registry = e_shell_get_registry (e_comp_editor_get_shell (comp_editor));

	return itip_address_is_user (registry, email_address);
}

static gboolean
ece_organizer_is_user (ECompEditor *comp_editor,
		       icalcomponent *component,
		       EClient *client)
{
	icalproperty *prop;
	const gchar *organizer;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (component != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);

	prop = icalcomponent_get_first_property (component, ICAL_ORGANIZER_PROPERTY);
	if (!prop || e_client_check_capability (client, CAL_STATIC_CAPABILITY_NO_ORGANIZER))
		return FALSE;

	organizer = itip_strip_mailto (icalproperty_get_organizer (prop));
	if (!organizer || !*organizer)
		return FALSE;

	return ece_organizer_email_address_is_user (comp_editor, client, organizer, TRUE);
}

static gboolean
ece_sentby_is_user (ECompEditor *comp_editor,
		    icalcomponent *component,
		    EClient *client)
{
	icalproperty *prop;
	icalparameter *param;
	const gchar *sentby;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (component != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);

	prop = icalcomponent_get_first_property (component, ICAL_ORGANIZER_PROPERTY);
	if (!prop || e_client_check_capability (client, CAL_STATIC_CAPABILITY_NO_ORGANIZER))
		return FALSE;

	param = icalproperty_get_first_parameter (prop, ICAL_SENTBY_PARAMETER);
	if (!param)
		return FALSE;

	sentby = icalparameter_get_sentby (param);

	return ece_organizer_email_address_is_user (comp_editor, client, sentby, FALSE);
}

static void
ece_emit_times_changed_cb (ECompEditor *comp_editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	g_signal_emit (comp_editor, signals[TIMES_CHANGED], 0, NULL);
}

static void
ece_connect_time_parts (ECompEditor *comp_editor,
			ECompEditorPropertyPart *dtstart_part,
			ECompEditorPropertyPart *dtend_part)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

#define update_part(x) G_STMT_START { \
	if (x) \
		g_object_ref (x); \
	if (comp_editor->priv->x) { \
		g_signal_handlers_disconnect_by_func (comp_editor->priv->x, G_CALLBACK (ece_emit_times_changed_cb), comp_editor); \
		g_clear_object (&comp_editor->priv->x); \
	} \
	if (x) { \
		comp_editor->priv->x = x; \
		g_signal_connect_swapped (comp_editor->priv->x, "changed", \
			G_CALLBACK (ece_emit_times_changed_cb), comp_editor); \
	} \
	} G_STMT_END

	update_part (dtstart_part);
	update_part (dtend_part);

#undef update_part
}

static void
ece_sensitize_widgets (ECompEditor *comp_editor,
		       gboolean force_insensitive)
{
	GtkActionGroup *group;
	GSList *link;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	for (link = comp_editor->priv->pages; link; link = g_slist_next (link)) {
		ECompEditorPage *page = link->data;

		g_warn_if_fail (E_IS_COMP_EDITOR_PAGE (page));
		if (!E_IS_COMP_EDITOR_PAGE (page))
			continue;

		e_comp_editor_page_sensitize_widgets (page, force_insensitive);
	}

	group = e_comp_editor_get_action_group (comp_editor, "individual");
	gtk_action_group_set_sensitive (group, !force_insensitive);

	group = e_comp_editor_get_action_group (comp_editor, "editable");
	gtk_action_group_set_sensitive (group, !force_insensitive);
}

static void
ece_fill_widgets (ECompEditor *comp_editor,
		  icalcomponent *component)
{
	GSList *link;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (component != NULL);

	for (link = comp_editor->priv->pages; link; link = g_slist_next (link)) {
		ECompEditorPage *page = link->data;

		g_warn_if_fail (E_IS_COMP_EDITOR_PAGE (page));
		if (!E_IS_COMP_EDITOR_PAGE (page))
			continue;

		e_comp_editor_page_fill_widgets (page, component);
	}
}

static gboolean
ece_fill_component (ECompEditor *comp_editor,
		    icalcomponent *component)
{
	GSList *link;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (component != NULL, FALSE);

	for (link = comp_editor->priv->pages; link; link = g_slist_next (link)) {
		ECompEditorPage *page = link->data;

		g_warn_if_fail (E_IS_COMP_EDITOR_PAGE (page));
		if (!E_IS_COMP_EDITOR_PAGE (page))
			continue;

		if (!e_comp_editor_page_fill_component (page, component))
			return FALSE;
	}

	return TRUE;
}

static void
comp_editor_realize_cb (ECompEditor *comp_editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (comp_editor->priv->component) {
		e_comp_editor_fill_widgets (comp_editor, comp_editor->priv->component);
		e_comp_editor_set_changed (comp_editor, FALSE);
	}

	e_comp_editor_update_window_title (comp_editor);
	e_comp_editor_sensitize_widgets (comp_editor);

	if (comp_editor->priv->page_general && comp_editor->priv->origin_source) {
		e_comp_editor_page_general_set_selected_source (
			comp_editor->priv->page_general,
			comp_editor->priv->origin_source);
		e_comp_editor_set_changed (comp_editor, FALSE);
	}

	if (comp_editor->priv->page_general) {
		e_comp_editor_page_general_update_view (comp_editor->priv->page_general);

		if (!comp_editor->priv->show_attendees_handler_id) {
			comp_editor->priv->show_attendees_handler_id =
				e_signal_connect_notify_swapped (comp_editor->priv->page_general,
					"notify::show-attendees",
					G_CALLBACK (e_comp_editor_update_window_title), comp_editor);
		}
	}

	if (!comp_editor->priv->target_client)
		e_comp_editor_open_target_client (comp_editor);
}

static void
comp_editor_unrealize_cb (ECompEditor *comp_editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (comp_editor->priv->page_general) {
		e_signal_disconnect_notify_handler (comp_editor->priv->page_general,
			&comp_editor->priv->show_attendees_handler_id);
	}
}

static gboolean
comp_editor_delete_event (GtkWidget *widget,
			  GdkEventAny *event)
{
	ECompEditor *comp_editor;

	g_return_val_if_fail (E_IS_COMP_EDITOR (widget), FALSE);

	comp_editor = E_COMP_EDITOR (widget);

	/* It's disabled when the component is being saved */
	if (gtk_widget_get_sensitive (GTK_WIDGET (comp_editor->priv->content)))
		action_close_cb (NULL, comp_editor);

	return TRUE;
}

static gboolean
comp_editor_key_press_event (GtkWidget *widget,
			     GdkEventKey *event)
{
	ECompEditor *comp_editor;

	g_return_val_if_fail (E_IS_COMP_EDITOR (widget), FALSE);

	comp_editor = E_COMP_EDITOR (widget);

	if (event->keyval == GDK_KEY_Escape &&
	    !e_alert_bar_close_alert (comp_editor->priv->alert_bar)) {
		GtkAction *action;

		action = e_comp_editor_get_action (comp_editor, "close");
		gtk_action_activate (action);

		return TRUE;
	}

	/* Chain up to parent's method. */
	return GTK_WIDGET_CLASS (e_comp_editor_parent_class)->key_press_event (widget, event);
}

static void
comp_editor_selected_source_notify_cb (ECompEditorPageGeneral *page_general,
				       GParamSpec *param,
				       ECompEditor *comp_editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (comp_editor->priv->page_general == page_general);

	e_comp_editor_open_target_client (comp_editor);
}

static void
e_comp_editor_submit_alert (EAlertSink *alert_sink,
			    EAlert *alert)
{
	ECompEditor *comp_editor;

	g_return_if_fail (E_IS_COMP_EDITOR (alert_sink));
	g_return_if_fail (E_IS_ALERT (alert));

	comp_editor = E_COMP_EDITOR (alert_sink);

	e_alert_bar_add_alert (comp_editor->priv->alert_bar, alert);
}

static void
e_comp_editor_set_origin_source (ECompEditor *comp_editor,
				 ESource *origin_source)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	if (origin_source)
		g_return_if_fail (E_IS_SOURCE (origin_source));

	g_clear_object (&comp_editor->priv->origin_source);
	if (origin_source)
		comp_editor->priv->origin_source = g_object_ref (origin_source);
}

static void
e_comp_editor_set_shell (ECompEditor *comp_editor,
			 EShell *shell)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (E_IS_SHELL (shell));

	g_clear_object (&comp_editor->priv->shell);
	comp_editor->priv->shell = g_object_ref (shell);
}

static void
e_comp_editor_set_property (GObject *object,
			    guint property_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALARM_EMAIL_ADDRESS:
			e_comp_editor_set_alarm_email_address (
				E_COMP_EDITOR (object),
				g_value_get_string (value));
			return;

		case PROP_CAL_EMAIL_ADDRESS:
			e_comp_editor_set_cal_email_address (
				E_COMP_EDITOR (object),
				g_value_get_string (value));
			return;

		case PROP_CHANGED:
			e_comp_editor_set_changed (
				E_COMP_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_COMPONENT:
			e_comp_editor_set_component (
				E_COMP_EDITOR (object),
				g_value_get_pointer (value));
			return;

		case PROP_FLAGS:
			e_comp_editor_set_flags (
				E_COMP_EDITOR (object),
				g_value_get_uint (value));
			return;

		case PROP_ORIGIN_SOURCE:
			e_comp_editor_set_origin_source (
				E_COMP_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_SHELL:
			e_comp_editor_set_shell (
				E_COMP_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE_CLIENT:
			e_comp_editor_set_source_client (
				E_COMP_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_TARGET_CLIENT:
			e_comp_editor_set_target_client (
				E_COMP_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_TITLE_SUFFIX:
			e_comp_editor_set_title_suffix (
				E_COMP_EDITOR (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_comp_editor_get_property (GObject *object,
			    guint property_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALARM_EMAIL_ADDRESS:
			g_value_set_string (
				value,
				e_comp_editor_get_alarm_email_address (
				E_COMP_EDITOR (object)));
			return;

		case PROP_CAL_EMAIL_ADDRESS:
			g_value_set_string (
				value,
				e_comp_editor_get_cal_email_address (
				E_COMP_EDITOR (object)));
			return;

		case PROP_CHANGED:
			g_value_set_boolean (
				value,
				e_comp_editor_get_changed (
				E_COMP_EDITOR (object)));
			return;

		case PROP_COMPONENT:
			g_value_set_pointer (
				value,
				e_comp_editor_get_component (
				E_COMP_EDITOR (object)));
			return;

		case PROP_FLAGS:
			g_value_set_uint (
				value,
				e_comp_editor_get_flags (
				E_COMP_EDITOR (object)));
			return;

		case PROP_ORIGIN_SOURCE:
			g_value_set_object (
				value,
				e_comp_editor_get_origin_source (
				E_COMP_EDITOR (object)));
			return;

		case PROP_SHELL:
			g_value_set_object (
				value,
				e_comp_editor_get_shell (
				E_COMP_EDITOR (object)));
			return;

		case PROP_SOURCE_CLIENT:
			g_value_set_object (
				value,
				e_comp_editor_get_source_client (
				E_COMP_EDITOR (object)));
			return;

		case PROP_TARGET_CLIENT:
			g_value_set_object (
				value,
				e_comp_editor_get_target_client (
				E_COMP_EDITOR (object)));
			return;

		case PROP_TITLE_SUFFIX:
			g_value_set_string (
				value,
				e_comp_editor_get_title_suffix (
				E_COMP_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_comp_editor_constructed (GObject *object)
{
	const gchar *ui =
		"<ui>"
		"  <menubar action='main-menu'>"
		"    <menu action='file-menu'>"
		"      <menuitem action='save'/>"
		"      <menuitem action='save-and-close'/>"
		"      <separator/>"
		"      <menuitem action='print-preview'/>"
		"      <menuitem action='print'/>"
		"      <separator/>"
		"      <menuitem action='close'/>"
		"    </menu>"
		"    <menu action='edit-menu'>"
		"      <menuitem action='undo'/>"
		"      <menuitem action='redo'/>"
		"      <separator/>"
		"      <menuitem action='cut-clipboard'/>"
		"      <menuitem action='copy-clipboard'/>"
		"      <menuitem action='paste-clipboard'/>"
		"      <menuitem action='delete-selection'/>"
		"      <separator/>"
		"      <menuitem action='select-all'/>"
		"    </menu>"
		"    <menu action='view-menu'>"
		"      <placeholder name='parts'/>"
		"      <separator />"
		"      <placeholder name='columns'/>"
		"    </menu>"
		"    <menu action='insert-menu'/>"
		"    <menu action='options-menu'>"
		"      <placeholder name='tabs'/>"
		"      <placeholder name='toggles'/>"
		"    </menu>"
		"    <menu action='help-menu'>"
		"      <menuitem action='help'/>"
		"    </menu>"
		"  </menubar>"
		"  <toolbar name='main-toolbar'>"
		"    <toolitem action='save-and-close'/>\n"
		"    <toolitem action='save'/>"
		"    <toolitem action='print'/>"
		"    <separator/>"
		"    <toolitem action='undo'/>"
		"    <toolitem action='redo'/>"
		"    <separator/>"
		"    <placeholder name='content'/>"
		"    <placeholder name='after-content'/>"
		"  </toolbar>"
		"</ui>";

	GtkActionEntry core_entries[] = {

		{ "close",
		  "window-close",
		  N_("_Close"),
		  "<Control>w",
		  N_("Close the current window"),
		  G_CALLBACK (action_close_cb) },

		{ "copy-clipboard",
		  "edit-copy",
		  N_("_Copy"),
		  "<Control>c",
		  N_("Copy the selection"),
		  NULL },  /* Handled by EFocusTracker */

		{ "cut-clipboard",
		  "edit-cut",
		  N_("Cu_t"),
		  "<Control>x",
		  N_("Cut the selection"),
		  NULL },  /* Handled by EFocusTracker */

		{ "delete-selection",
		  "edit-delete",
		  N_("_Delete"),
		  NULL,
		  N_("Delete the selection"),
		  NULL },  /* Handled by EFocusTracker */

		{ "help",
		  "help-browser",
		  N_("_Help"),
		  "F1",
		  N_("View help"),
		  G_CALLBACK (action_help_cb) },

		{ "paste-clipboard",
		  "edit-paste",
		  N_("_Paste"),
		  "<Control>v",
		  N_("Paste the clipboard"),
		  NULL },  /* Handled by EFocusTracker */

		{ "print",
		  "document-print",
		  N_("_Print..."),
		  "<Control>p",
		  NULL,
		  G_CALLBACK (action_print_cb) },

		{ "print-preview",
		  "document-print-preview",
		  N_("Pre_view..."),
		  NULL,
		  NULL,
		  G_CALLBACK (action_print_preview_cb) },

		{ "select-all",
		  "edit-select-all",
		  N_("Select _All"),
		  "<Control>a",
		  N_("Select all text"),
		  NULL },  /* Handled by EFocusTracker */

		{ "undo",
		  "edit-undo",
		  N_("_Undo"),
		  "<Control>z",
		  N_("Undo"),
		  NULL },  /* Handled by EFocusTracker */

		{ "redo",
		  "edit-redo",
		  N_("_Redo"),
		  "<Control>y",
		  N_("Redo"),
		  NULL },  /* Handled by EFocusTracker */

		/* Menus */

		{ "classification-menu",
		  NULL,
		  N_("_Classification"),
		  NULL,
		  NULL,
		  NULL },

		{ "edit-menu",
		  NULL,
		  N_("_Edit"),
		  NULL,
		  NULL,
		  NULL },

		{ "file-menu",
		  NULL,
		  N_("_File"),
		  NULL,
		  NULL,
		  NULL },

		{ "help-menu",
		  NULL,
		  N_("_Help"),
		  NULL,
		  NULL,
		  NULL },

		{ "insert-menu",
		  NULL,
		  N_("_Insert"),
		  NULL,
		  NULL,
		  NULL },

		{ "options-menu",
		  NULL,
		  N_("_Options"),
		  NULL,
		  NULL,
		  NULL },

		{ "view-menu",
		  NULL,
		  N_("_View"),
		  NULL,
		  NULL,
		  NULL }
	};

	GtkActionEntry editable_entries[] = {

		{ "save",
		  "document-save",
		  N_("_Save"),
		  "<Control>s",
		  N_("Save current changes"),
		  G_CALLBACK (action_save_cb) },

		{ "save-and-close",
		  NULL,
		  N_("Save and Close"),
		  NULL,
		  N_("Save current changes and close editor"),
		  G_CALLBACK (action_save_and_close_cb) }
	};

	ECompEditor *comp_editor = E_COMP_EDITOR (object);
	GtkWidget *widget;
	GtkBox *vbox;
	GtkAction *action;
	GtkActionGroup *action_group;
	EFocusTracker *focus_tracker;
	GError *error = NULL;

	G_OBJECT_CLASS (e_comp_editor_parent_class)->constructed (object);

	g_signal_connect (comp_editor, "key-press-event",
		G_CALLBACK (e_util_check_gtk_bindings_in_key_press_event_cb), NULL);

	comp_editor->priv->calendar_settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	comp_editor->priv->ui_manager = gtk_ui_manager_new ();

	gtk_window_add_accel_group (
		GTK_WINDOW (comp_editor),
		gtk_ui_manager_get_accel_group (comp_editor->priv->ui_manager));

	/* Setup Action Groups */

	action_group = gtk_action_group_new ("individual");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_ui_manager_insert_action_group (
		comp_editor->priv->ui_manager, action_group, 0);
	g_object_unref (action_group);

	action_group = gtk_action_group_new ("core");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, core_entries,
		G_N_ELEMENTS (core_entries), comp_editor);
	gtk_ui_manager_insert_action_group (
		comp_editor->priv->ui_manager, action_group, 0);
	g_object_unref (action_group);

	action_group = gtk_action_group_new ("editable");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, editable_entries,
		G_N_ELEMENTS (editable_entries), comp_editor);
	gtk_ui_manager_insert_action_group (
		comp_editor->priv->ui_manager, action_group, 0);
	g_object_unref (action_group);

	action = gtk_action_group_get_action (action_group, "save-and-close");
	if (action) {
		GtkAction *save_action;
		GIcon *icon;
		GIcon *emblemed_icon;
		GEmblem *emblem;

		icon = g_themed_icon_new ("window-close");
		emblemed_icon = g_themed_icon_new ("document-save");
		emblem = g_emblem_new (emblemed_icon);
		g_object_unref (emblemed_icon);

		emblemed_icon = g_emblemed_icon_new (icon, emblem);
		g_object_unref (emblem);
		g_object_unref (icon);

		gtk_action_set_gicon (action, emblemed_icon);

		g_object_unref (emblemed_icon);

		save_action = gtk_action_group_get_action (action_group, "save");
		e_binding_bind_property (
			save_action, "sensitive",
			action, "sensitive",
			G_BINDING_SYNC_CREATE);
	}

	gtk_ui_manager_add_ui_from_string (comp_editor->priv->ui_manager, ui, -1, &error);
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);

	vbox = GTK_BOX (widget);

	gtk_container_add (GTK_CONTAINER (comp_editor), widget);

	widget = e_comp_editor_get_managed_widget (comp_editor, "/main-menu");
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	gtk_widget_set_visible (widget, TRUE);

	widget = e_comp_editor_get_managed_widget (comp_editor, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gtk_style_context_add_class (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	widget = e_alert_bar_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);

	comp_editor->priv->alert_bar = E_ALERT_BAR (widget);

	gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);

	widget = e_activity_bar_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);

	comp_editor->priv->activity_bar = E_ACTIVITY_BAR (widget);

	gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);

	widget = gtk_notebook_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"show-tabs", TRUE,
		"show-border", FALSE,
		NULL);
	gtk_widget_show (widget);

	comp_editor->priv->content = GTK_NOTEBOOK (widget);

	gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);

	/* Configure an EFocusTracker to manage selection actions. */

	focus_tracker = e_focus_tracker_new (GTK_WINDOW (comp_editor));

	action = e_comp_editor_get_action (comp_editor, "cut-clipboard");
	e_focus_tracker_set_cut_clipboard_action (focus_tracker, action);

	action = e_comp_editor_get_action (comp_editor, "copy-clipboard");
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, action);

	action = e_comp_editor_get_action (comp_editor, "paste-clipboard");
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, action);

	action = e_comp_editor_get_action (comp_editor, "delete-selection");
	e_focus_tracker_set_delete_selection_action (focus_tracker, action);

	action = e_comp_editor_get_action (comp_editor, "select-all");
	e_focus_tracker_set_select_all_action (focus_tracker, action);

	action = e_comp_editor_get_action (comp_editor, "undo");
	e_focus_tracker_set_undo_action (focus_tracker, action);

	action = e_comp_editor_get_action (comp_editor, "redo");
	e_focus_tracker_set_redo_action (focus_tracker, action);

	comp_editor->priv->focus_tracker = focus_tracker;

	/* Desensitize the "save" action. */
	action = e_comp_editor_get_action (comp_editor, "save");
	gtk_action_set_sensitive (action, FALSE);

	e_binding_bind_property (comp_editor, "changed", action, "sensitive", 0);

	g_signal_connect (comp_editor, "realize", G_CALLBACK (comp_editor_realize_cb), NULL);
	g_signal_connect (comp_editor, "unrealize", G_CALLBACK (comp_editor_unrealize_cb), NULL);

	gtk_application_add_window (GTK_APPLICATION (comp_editor->priv->shell), GTK_WINDOW (comp_editor));
}

static void
e_comp_editor_dispose (GObject *object)
{
	ECompEditor *comp_editor = E_COMP_EDITOR (object);

	if (comp_editor->priv->page_general) {
		g_signal_handlers_disconnect_by_func (comp_editor->priv->page_general,
			G_CALLBACK (comp_editor_selected_source_notify_cb), comp_editor);
		comp_editor->priv->page_general = NULL;
	}

	if (comp_editor->priv->target_client_opening) {
		e_activity_cancel (comp_editor->priv->target_client_opening);
		g_clear_object (&comp_editor->priv->target_client_opening);
	}

	g_slist_free_full (comp_editor->priv->pages, g_object_unref);
	comp_editor->priv->pages = NULL;

	g_free (comp_editor->priv->alarm_email_address);
	comp_editor->priv->alarm_email_address = NULL;

	g_free (comp_editor->priv->cal_email_address);
	comp_editor->priv->cal_email_address = NULL;

	g_free (comp_editor->priv->title_suffix);
	comp_editor->priv->title_suffix = NULL;

	if (comp_editor->priv->component) {
		icalcomponent_free (comp_editor->priv->component);
		comp_editor->priv->component = NULL;
	}

	ece_connect_time_parts (comp_editor, NULL, NULL);

	g_clear_object (&comp_editor->priv->origin_source);
	g_clear_object (&comp_editor->priv->shell);
	g_clear_object (&comp_editor->priv->focus_tracker);
	g_clear_object (&comp_editor->priv->ui_manager);
	g_clear_object (&comp_editor->priv->source_client);
	g_clear_object (&comp_editor->priv->target_client);
	g_clear_object (&comp_editor->priv->calendar_settings);
	g_clear_object (&comp_editor->priv->validation_alert);

	comp_editor->priv->activity_bar = NULL;

	opened_editors = g_slist_remove (opened_editors, comp_editor);

	G_OBJECT_CLASS (e_comp_editor_parent_class)->dispose (object);
}

static void
e_comp_editor_init (ECompEditor *comp_editor)
{
	comp_editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (comp_editor, E_TYPE_COMP_EDITOR, ECompEditorPrivate);
}

static void
e_comp_editor_alert_sink_iface_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = e_comp_editor_submit_alert;
}

static void
e_comp_editor_class_init (ECompEditorClass *klass)
{
	GtkWidgetClass *widget_class;
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (ECompEditorPrivate));

	klass->sensitize_widgets = ece_sensitize_widgets;
	klass->fill_widgets = ece_fill_widgets;
	klass->fill_component = ece_fill_component;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->delete_event = comp_editor_delete_event;
	widget_class->key_press_event = comp_editor_key_press_event;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_comp_editor_set_property;
	object_class->get_property = e_comp_editor_get_property;
	object_class->constructed = e_comp_editor_constructed;
	object_class->dispose = e_comp_editor_dispose;

	g_object_class_install_property (
		object_class,
		PROP_ALARM_EMAIL_ADDRESS,
		g_param_spec_string (
			"alarm-email-address",
			"Alarm Email Address",
			"Target client's alarm email address",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CAL_EMAIL_ADDRESS,
		g_param_spec_string (
			"cal-email-address",
			"Calendar Email Address",
			"Target client's calendar email address",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CHANGED,
		g_param_spec_boolean (
			"changed",
			"Changed",
			"Whether the editor content changed",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_COMPONENT,
		g_param_spec_pointer (
			"component",
			"Component",
			"icalcomponent currently edited",
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_FLAGS,
		g_param_spec_uint (
			"flags",
			"Flags",
			"Editor flags",
			0, G_MAXUINT, 0,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_ORIGIN_SOURCE,
		g_param_spec_object (
			"origin-source",
			"Origin Source",
			"ESource of an ECalClient the component is stored in",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			"Shell",
			"EShell",
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_CLIENT,
		g_param_spec_object (
			"source-client",
			"Source Client",
			"ECalClient, the source calendar for the component",
			E_TYPE_CAL_CLIENT,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TARGET_CLIENT,
		g_param_spec_object (
			"target-client",
			"Target Client",
			"ECalClient currently set as the target calendar for the component",
			E_TYPE_CAL_CLIENT,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TITLE_SUFFIX,
		g_param_spec_string (
			"title-suffix",
			"Title Suffix",
			"Window title suffix, usually summary of the component",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[TIMES_CHANGED] = g_signal_new (
		"times-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECompEditorClass, times_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	signals[OBJECT_CREATED] = g_signal_new (
		"object-created",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECompEditorClass, object_created),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	signals[EDITOR_CLOSED] = g_signal_new (
		"editor-closed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECompEditorClass, editor_closed),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

void
e_comp_editor_sensitize_widgets (ECompEditor *comp_editor)
{
	ECompEditorClass *comp_editor_class;
	gboolean force_insensitive;
	GtkWidget *current_focus;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	comp_editor_class = E_COMP_EDITOR_GET_CLASS (comp_editor);
	g_return_if_fail (comp_editor_class != NULL);
	g_return_if_fail (comp_editor_class->sensitize_widgets != NULL);

	current_focus = gtk_window_get_focus (GTK_WINDOW (comp_editor));

	force_insensitive = !comp_editor->priv->component;

	if (!force_insensitive) {
		ECalClient *target_client;

		target_client = e_comp_editor_get_target_client (comp_editor);
		if (target_client) {
			EClient *client = E_CLIENT (target_client);

			if (e_client_is_readonly (client)) {
				force_insensitive = TRUE;
			} else {
				if (!e_cal_util_component_has_organizer (comp_editor->priv->component) ||
				    ece_organizer_is_user (comp_editor, comp_editor->priv->component, client) ||
				    ece_sentby_is_user (comp_editor, comp_editor->priv->component, client)) {
					comp_editor->priv->flags = comp_editor->priv->flags | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER;
				} else {
					comp_editor->priv->flags = comp_editor->priv->flags & (~E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER);
				}
			}
		} else {
			force_insensitive = TRUE;
		}
	}

	comp_editor_class->sensitize_widgets (comp_editor, force_insensitive);

	if (force_insensitive)
		comp_editor->priv->restore_focus = current_focus;
	else
		ece_restore_focus (comp_editor);
}

void
e_comp_editor_fill_widgets (ECompEditor *comp_editor,
			    icalcomponent *component)
{
	ECompEditorClass *comp_editor_class;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (component != NULL);

	comp_editor_class = E_COMP_EDITOR_GET_CLASS (comp_editor);
	g_return_if_fail (comp_editor_class != NULL);
	g_return_if_fail (comp_editor_class->fill_widgets != NULL);

	e_comp_editor_set_updating (comp_editor, TRUE);

	comp_editor_class->fill_widgets (comp_editor, component);

	e_comp_editor_set_updating (comp_editor, FALSE);
}

gboolean
e_comp_editor_fill_component (ECompEditor *comp_editor,
			      icalcomponent *component)
{
	ECompEditorClass *comp_editor_class;
	gboolean is_valid;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (component != NULL, FALSE);

	comp_editor_class = E_COMP_EDITOR_GET_CLASS (comp_editor);
	g_return_val_if_fail (comp_editor_class != NULL, FALSE);
	g_return_val_if_fail (comp_editor_class->fill_component != NULL, FALSE);

	is_valid = comp_editor_class->fill_component (comp_editor, component);

	if (is_valid && comp_editor->priv->validation_alert) {
		e_alert_response (comp_editor->priv->validation_alert, GTK_RESPONSE_CLOSE);
		g_clear_object (&comp_editor->priv->validation_alert);
	}

	if (is_valid) {
		ECalClient *target_client;
		EClient *client = NULL;

		target_client = e_comp_editor_get_target_client (comp_editor);
		if (target_client)
			client = E_CLIENT (target_client);

		if (!e_cal_util_component_has_organizer (component) || (client && (
		    ece_organizer_is_user (comp_editor, component, client) ||
		    ece_sentby_is_user (comp_editor, component, client)))) {
			gint sequence;

			sequence = icalcomponent_get_sequence (component);
			icalcomponent_set_sequence (component, sequence + 1);
		}
	}

	return is_valid;
}

void
e_comp_editor_set_validation_error (ECompEditor *comp_editor,
				    ECompEditorPage *error_page,
				    GtkWidget *error_widget,
				    const gchar *error_message)
{
	EAlert *alert, *previous_alert;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (error_message != NULL);

	/* Ignore validation errors when the inner editor is currently updating. */
	if (e_comp_editor_get_updating (comp_editor))
		return;

	alert = e_alert_new ("calendar:comp-editor-failed-validate", error_message, NULL);

	e_alert_bar_add_alert (comp_editor->priv->alert_bar, alert);

	previous_alert = comp_editor->priv->validation_alert;
	comp_editor->priv->validation_alert = alert;

	if (previous_alert) {
		e_alert_response (previous_alert, GTK_RESPONSE_CLOSE);
		g_clear_object (&previous_alert);
	}

	if (error_page)
		e_comp_editor_select_page (comp_editor, error_page);

	if (error_widget)
		gtk_widget_grab_focus (error_widget);
}

EShell *
e_comp_editor_get_shell (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->shell;
}

GSettings *
e_comp_editor_get_settings (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->calendar_settings;
}

ESource *
e_comp_editor_get_origin_source (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->origin_source;
}

icalcomponent *
e_comp_editor_get_component (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->component;
}

guint32
e_comp_editor_get_flags (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), 0);

	return comp_editor->priv->flags;
}

void
e_comp_editor_set_flags (ECompEditor *comp_editor,
			 guint32 flags)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (comp_editor->priv->flags == flags)
		return;

	comp_editor->priv->flags = flags;

	g_object_notify (G_OBJECT (comp_editor), "flags");
}

EFocusTracker *
e_comp_editor_get_focus_tracker (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->focus_tracker;
}

GtkUIManager *
e_comp_editor_get_ui_manager (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->ui_manager;
}

GtkAction *
e_comp_editor_get_action (ECompEditor *comp_editor,
			  const gchar *action_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	return e_lookup_action (ui_manager, action_name);
}

GtkActionGroup *
e_comp_editor_get_action_group (ECompEditor *comp_editor,
				const gchar *group_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	return e_lookup_action_group (ui_manager, group_name);
}

GtkWidget *
e_comp_editor_get_managed_widget (ECompEditor *comp_editor,
				  const gchar *widget_path)
{
	GtkUIManager *ui_manager;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);

	ui_manager = e_comp_editor_get_ui_manager (comp_editor);
	widget = gtk_ui_manager_get_widget (ui_manager, widget_path);
	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

const gchar *
e_comp_editor_get_alarm_email_address (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->alarm_email_address;
}

void
e_comp_editor_set_alarm_email_address (ECompEditor *comp_editor,
				       const gchar *alarm_email_address)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (g_strcmp0 (alarm_email_address, comp_editor->priv->alarm_email_address) == 0)
		return;

	g_free (comp_editor->priv->alarm_email_address);
	comp_editor->priv->alarm_email_address = g_strdup (alarm_email_address);

	g_object_notify (G_OBJECT (comp_editor), "alarm-email-address");
}

const gchar *
e_comp_editor_get_cal_email_address (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->cal_email_address;
}

void
e_comp_editor_set_cal_email_address (ECompEditor *comp_editor,
				     const gchar *cal_email_address)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (g_strcmp0 (cal_email_address, comp_editor->priv->cal_email_address) == 0)
		return;

	g_free (comp_editor->priv->cal_email_address);
	comp_editor->priv->cal_email_address = g_strdup (cal_email_address);

	g_object_notify (G_OBJECT (comp_editor), "cal-email-address");
}

gboolean
e_comp_editor_get_changed (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);

	return comp_editor->priv->changed;
}

void
e_comp_editor_set_changed (ECompEditor *comp_editor,
			   gboolean changed)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if ((changed ? 1 : 0) == (comp_editor->priv->changed ? 1 : 0))
		return;

	comp_editor->priv->changed = changed;

	g_object_notify (G_OBJECT (comp_editor), "changed");
}

void
e_comp_editor_ensure_changed (ECompEditor *comp_editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	e_comp_editor_set_changed (comp_editor, TRUE);
}

gboolean
e_comp_editor_get_updating (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);

	return comp_editor->priv->updating > 0;
}

void
e_comp_editor_set_updating (ECompEditor *comp_editor,
			    gboolean updating)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (updating) {
		comp_editor->priv->updating++;
	} else if (comp_editor->priv->updating > 0) {
		comp_editor->priv->updating--;
	} else {
		g_warn_if_reached ();
	}
}

ECalClient *
e_comp_editor_get_source_client (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->source_client;
}

void
e_comp_editor_set_source_client (ECompEditor *comp_editor,
				 ECalClient *client)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (client == comp_editor->priv->source_client)
		return;

	if (client)
		g_object_ref (client);
	g_clear_object (&comp_editor->priv->source_client);
	comp_editor->priv->source_client = client;

	g_object_notify (G_OBJECT (comp_editor), "source-client");
}

ECalClient *
e_comp_editor_get_target_client (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->target_client;
}

void
e_comp_editor_set_target_client (ECompEditor *comp_editor,
				 ECalClient *client)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (client == comp_editor->priv->target_client)
		return;

	if (client)
		g_object_ref (client);
	g_clear_object (&comp_editor->priv->target_client);
	comp_editor->priv->target_client = client;

	g_object_notify (G_OBJECT (comp_editor), "target-client");

	if (client && !comp_editor->priv->source_client && comp_editor->priv->origin_source &&
	    e_source_equal (e_client_get_source (E_CLIENT (client)), comp_editor->priv->origin_source))
		e_comp_editor_set_source_client (comp_editor, client);

	e_comp_editor_sensitize_widgets (comp_editor);
}

const gchar *
e_comp_editor_get_title_suffix (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->title_suffix;
}

void
e_comp_editor_set_title_suffix (ECompEditor *comp_editor,
				const gchar *title_suffix)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (g_strcmp0 (title_suffix, comp_editor->priv->title_suffix) == 0)
		return;

	g_free (comp_editor->priv->title_suffix);
	comp_editor->priv->title_suffix = g_strdup (title_suffix);

	g_object_notify (G_OBJECT (comp_editor), "title-suffix");

	e_comp_editor_update_window_title (comp_editor);
}

void
e_comp_editor_set_time_parts (ECompEditor *comp_editor,
			      ECompEditorPropertyPart *dtstart_part,
			      ECompEditorPropertyPart *dtend_part)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (dtstart_part)
		g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (dtstart_part));
	if (dtend_part)
		g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (dtend_part));

	ece_connect_time_parts (comp_editor, dtstart_part, dtend_part);
}

void
e_comp_editor_get_time_parts (ECompEditor *comp_editor,
			      ECompEditorPropertyPart **out_dtstart_part,
			      ECompEditorPropertyPart **out_dtend_part)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (out_dtstart_part)
		*out_dtstart_part = comp_editor->priv->dtstart_part;
	if (out_dtend_part)
		*out_dtend_part = comp_editor->priv->dtend_part;
}

/* This consumes the @page. */
void
e_comp_editor_add_page (ECompEditor *comp_editor,
			const gchar *label,
			ECompEditorPage *page)
{
	ECompEditor *pages_comp_editor;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (label != NULL);
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));

	pages_comp_editor = e_comp_editor_page_ref_editor (page);
	if (pages_comp_editor != comp_editor) {
		g_warn_if_fail (pages_comp_editor == comp_editor);
		g_clear_object (&pages_comp_editor);
		return;
	}

	g_clear_object (&pages_comp_editor);

	/* One reference uses the GtkNotebook, the other the pages GSList */
	gtk_notebook_append_page (comp_editor->priv->content,
		GTK_WIDGET (page),
		gtk_label_new_with_mnemonic (label));

	comp_editor->priv->pages = g_slist_append (comp_editor->priv->pages, g_object_ref (page));

	g_signal_connect_swapped (page, "changed", G_CALLBACK (e_comp_editor_ensure_changed), comp_editor);

	if (E_IS_COMP_EDITOR_PAGE_GENERAL (page)) {
		ECompEditorPageGeneral *page_general;

		g_return_if_fail (comp_editor->priv->page_general == NULL);

		page_general = E_COMP_EDITOR_PAGE_GENERAL (page);

		g_signal_connect (page_general, "notify::selected-source",
			G_CALLBACK (comp_editor_selected_source_notify_cb), comp_editor);

		comp_editor->priv->page_general = page_general;

		if ((comp_editor->priv->flags & E_COMP_EDITOR_FLAG_WITH_ATTENDEES) != 0) {
			e_comp_editor_page_general_set_show_attendees (page_general, TRUE);
		}
	}
}

/* The returned pointer is owned by the @comp_editor; returns the first instance,
   in order of the addition. */
ECompEditorPage *
e_comp_editor_get_page (ECompEditor *comp_editor,
			GType page_type)
{
	GSList *link;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);
	g_return_val_if_fail (g_type_is_a (page_type, E_TYPE_COMP_EDITOR_PAGE), NULL);
	g_return_val_if_fail (page_type != E_TYPE_COMP_EDITOR_PAGE, NULL);

	for (link = comp_editor->priv->pages; link; link = g_slist_next (link)) {
		ECompEditorPage *page = link->data;

		if (G_TYPE_CHECK_INSTANCE_TYPE (page, page_type))
			return page;
	}

	return NULL;
}

/* Free the returned GSList with g_slist_free(), the memebers are owned by the comp_editor */
GSList *
e_comp_editor_get_pages (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return g_slist_copy (comp_editor->priv->pages);
}

void
e_comp_editor_select_page (ECompEditor *comp_editor,
			   ECompEditorPage *page)
{
	gint page_num;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));

	page_num = gtk_notebook_page_num (comp_editor->priv->content, GTK_WIDGET (page));
	g_return_if_fail (page_num != -1);

	gtk_notebook_set_current_page (comp_editor->priv->content, page_num);
}

/* Unref returned pointer when done with it. */
static EAlert *
e_comp_editor_add_alert (ECompEditor *comp_editor,
			 const gchar *alert_id,
			 const gchar *primary_text,
			 const gchar *secondary_text)
{
	EAlert *alert;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);
	g_return_val_if_fail (alert_id != NULL, NULL);
	g_return_val_if_fail (primary_text != NULL || secondary_text != NULL, NULL);

	alert = e_alert_new (alert_id,
		primary_text ? primary_text : "",
		secondary_text ? secondary_text : "",
		NULL);

	e_alert_bar_add_alert (comp_editor->priv->alert_bar, alert);

	return alert;
}

/* Unref returned pointer when done with it. */
EAlert *
e_comp_editor_add_information (ECompEditor *comp_editor,
			       const gchar *primary_text,
			       const gchar *secondary_text)
{
	return e_comp_editor_add_alert (comp_editor, "calendar:comp-editor-information", primary_text, secondary_text);
}

/* Unref returned pointer when done with it. */
EAlert *
e_comp_editor_add_warning (ECompEditor *comp_editor,
			   const gchar *primary_text,
			   const gchar *secondary_text)
{
	return e_comp_editor_add_alert (comp_editor, "calendar:comp-editor-warning", primary_text, secondary_text);
}

/* Unref returned pointer when done with it. */
EAlert *
e_comp_editor_add_error (ECompEditor *comp_editor,
			 const gchar *primary_text,
			 const gchar *secondary_text)
{
	return e_comp_editor_add_alert (comp_editor, "calendar:comp-editor-error", primary_text, secondary_text);
}


static gboolean
ece_check_start_before_end (struct icaltimetype *start_tt,
			    struct icaltimetype *end_tt,
			    gboolean adjust_end_time)
{
	struct icaltimetype end_tt_copy;
	icaltimezone *start_zone, *end_zone;
	gint cmp;

	start_zone = (icaltimezone *) start_tt->zone;
	end_zone = (icaltimezone *) end_tt->zone;

	/* Convert the end time to the same timezone as the start time. */
	end_tt_copy = *end_tt;

	if (start_zone && end_zone && start_zone != end_zone)
		icaltimezone_convert_time (&end_tt_copy, end_zone, start_zone);

	/* Now check if the start time is after the end time. If it is,
	 * we need to modify one of the times. */
	cmp = icaltime_compare (*start_tt, end_tt_copy);
	if (cmp > 0) {
		if (adjust_end_time) {
			/* Try to switch only the date */
			end_tt->year = start_tt->year;
			end_tt->month = start_tt->month;
			end_tt->day = start_tt->day;

			end_tt_copy = *end_tt;
			if (start_zone && end_zone && start_zone != end_zone)
				icaltimezone_convert_time (&end_tt_copy, end_zone, start_zone);

			if (icaltime_compare (*start_tt, end_tt_copy) >= 0) {
				/* Modify the end time, to be the start + 1 hour/day. */
				*end_tt = *start_tt;
				icaltime_adjust (end_tt, 0, start_tt->is_date ? 24 : 1, 0, 0);

				if (start_zone && end_zone && start_zone != end_zone)
					icaltimezone_convert_time (end_tt, start_zone, end_zone);
			}
		} else {
			/* Try to switch only the date */
			start_tt->year = end_tt->year;
			start_tt->month = end_tt->month;
			start_tt->day = end_tt->day;

			if (icaltime_compare (*start_tt, end_tt_copy) >= 0) {
				/* Modify the start time, to be the end - 1 hour/day. */
				*start_tt = *end_tt;
				icaltime_adjust (start_tt, 0, start_tt->is_date ? -24 : -1, 0, 0);

				if (start_zone && end_zone && start_zone != end_zone)
					icaltimezone_convert_time (start_tt, end_zone, start_zone);
			}
		}

		return TRUE;
	}

	return FALSE;
}

void
e_comp_editor_ensure_start_before_end (ECompEditor *comp_editor,
				       ECompEditorPropertyPart *start_datetime,
				       ECompEditorPropertyPart *end_datetime,
				       gboolean change_end_datetime)
{
	ECompEditorPropertyPartDatetime *start_dtm, *end_dtm;
	struct icaltimetype start_tt, end_tt;
	gboolean set_dtstart = FALSE, set_dtend = FALSE;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (start_datetime));
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (end_datetime));

	start_dtm = E_COMP_EDITOR_PROPERTY_PART_DATETIME (start_datetime);
	end_dtm = E_COMP_EDITOR_PROPERTY_PART_DATETIME (end_datetime);

	start_tt = e_comp_editor_property_part_datetime_get_value (start_dtm);
	end_tt = e_comp_editor_property_part_datetime_get_value (end_dtm);

	if (icaltime_is_null_time (start_tt) ||
	    icaltime_is_null_time (end_tt) ||
	    !icaltime_is_valid_time (start_tt) ||
	    !icaltime_is_valid_time (end_tt))
		return;

	if (start_tt.is_date || end_tt.is_date) {
		/* All Day Events are simple. We just compare the dates and if
		 * start > end we copy one of them to the other. */
		gint cmp;

		start_tt.is_date = TRUE;
		end_tt.is_date = TRUE;

		cmp = icaltime_compare_date_only (start_tt, end_tt);

		if (cmp > 0) {
			if (change_end_datetime) {
				end_tt = start_tt;
				set_dtend = TRUE;
			} else {
				start_tt = end_tt;
				set_dtstart = TRUE;
			}
		}
	} else {
		if (ece_check_start_before_end (&start_tt, &end_tt, change_end_datetime)) {
			if (change_end_datetime)
				set_dtend = TRUE;
			else
				set_dtstart = TRUE;
		}
	}

	if (set_dtstart || set_dtend) {
		e_comp_editor_set_updating (comp_editor, TRUE);

		if (set_dtstart)
			e_comp_editor_property_part_datetime_set_value (start_dtm, start_tt);

		if (set_dtend)
			e_comp_editor_property_part_datetime_set_value (end_dtm, end_tt);

		e_comp_editor_set_updating (comp_editor, FALSE);
	}
}

static gboolean
e_comp_editor_holds_component (ECompEditor *comp_editor,
			       ESource *origin_source,
			       const icalcomponent *component)
{
	const gchar *component_uid, *editor_uid;
	gboolean equal;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (component != NULL, FALSE);

	if (!origin_source || !comp_editor->priv->origin_source ||
	    !e_source_equal (origin_source, comp_editor->priv->origin_source))
		return FALSE;

	component_uid = icalcomponent_get_uid ((icalcomponent *) component);
	editor_uid = icalcomponent_get_uid (comp_editor->priv->component);

	if (!component_uid || !editor_uid)
		return FALSE;

	equal = g_strcmp0 (component_uid, editor_uid) == 0;
	if (equal) {
		struct icaltimetype component_rid, editor_rid;

		component_rid = icalcomponent_get_recurrenceid ((icalcomponent *) component);
		editor_rid = icalcomponent_get_recurrenceid (comp_editor->priv->component);

		if (icaltime_is_null_time (component_rid)) {
			equal = icaltime_is_null_time (editor_rid);
		} else if (!icaltime_is_null_time (editor_rid)) {
			equal = icaltime_compare (component_rid, editor_rid) == 0;
		}
	}

	return equal;
}

ECompEditor *
e_comp_editor_open_for_component (GtkWindow *parent,
				  EShell *shell,
				  ESource *origin_source,
				  const icalcomponent *component,
				  guint32 flags /* bit-or of ECompEditorFlags */)
{
	ECompEditor *comp_editor;
	GType comp_editor_type;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	if (origin_source)
		g_return_val_if_fail (E_IS_SOURCE (origin_source), NULL);
	g_return_val_if_fail (component != NULL, NULL);

	comp_editor = e_comp_editor_find_existing_for (origin_source, component);
	if (comp_editor) {
		gtk_window_present (GTK_WINDOW (comp_editor));
		return comp_editor;
	}

	switch (icalcomponent_isa (component)) {
		case ICAL_VEVENT_COMPONENT:
			comp_editor_type = E_TYPE_COMP_EDITOR_EVENT;
			break;
		case ICAL_VTODO_COMPONENT:
			comp_editor_type = E_TYPE_COMP_EDITOR_TASK;
			break;
		case ICAL_VJOURNAL_COMPONENT:
			comp_editor_type = E_TYPE_COMP_EDITOR_MEMO;
			break;
		default:
			g_warn_if_reached ();
			return NULL;
	}

	comp_editor = g_object_new (comp_editor_type,
		"shell", shell,
		"origin-source", origin_source,
		"component", component,
		"flags", flags,
		NULL);

	opened_editors = g_slist_prepend (opened_editors, comp_editor);

	gtk_widget_show (GTK_WIDGET (comp_editor));

	return comp_editor;
}

ECompEditor *
e_comp_editor_find_existing_for (ESource *origin_source,
				 const icalcomponent *component)
{
	ECompEditor *comp_editor;
	GSList *link;

	if (origin_source)
		g_return_val_if_fail (E_IS_SOURCE (origin_source), NULL);
	g_return_val_if_fail (component != NULL, NULL);

	for (link = opened_editors; link; link = g_slist_next (link)) {
		comp_editor = link->data;

		if (!comp_editor)
			continue;

		if (e_comp_editor_holds_component (comp_editor, origin_source, component)) {
			gtk_window_present (GTK_WINDOW (comp_editor));
			return comp_editor;
		}
	}

	return NULL;
}
