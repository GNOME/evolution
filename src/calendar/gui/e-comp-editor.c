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

#include "evolution-config.h"

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
	ICalComponent *component;
	guint32 flags;

	const gchar *toolbar_id;

	EMenuBar *menu_bar;
	GtkWidget *menu_button; /* owned by menu_bar */

	EFocusTracker *focus_tracker;
	EUIManager *ui_manager;

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

	gulong target_backend_property_change_id;
	gint last_duration;
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
	SANITIZE_WIDGETS,
	FILL_WIDGETS,
	FILL_COMPONENT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GSList *opened_editors = NULL;

static void e_comp_editor_alert_sink_iface_init (EAlertSinkInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ECompEditor, e_comp_editor, GTK_TYPE_WINDOW,
	G_ADD_PRIVATE (ECompEditor)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_comp_editor_alert_sink_iface_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
ece_restore_focus (ECompEditor *comp_editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (comp_editor->priv->restore_focus) {
		if (GTK_IS_ENTRY (comp_editor->priv->restore_focus))
			gtk_entry_grab_focus_without_selecting (GTK_ENTRY (comp_editor->priv->restore_focus));
		else
			gtk_widget_grab_focus (comp_editor->priv->restore_focus);

		comp_editor->priv->restore_focus = NULL;
	}
}

static EUIActionGroup *
ece_get_action_group (ECompEditor *comp_editor,
		      const gchar *group_name)
{
	EUIManager *ui_manager;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	return e_ui_manager_get_action_group (ui_manager, group_name);
}

static void
e_comp_editor_enable (ECompEditor *comp_editor,
		      gboolean enable)
{
	EUIActionGroup *group;
	GtkWidget *current_focus;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	current_focus = gtk_window_get_focus (GTK_WINDOW (comp_editor));

	gtk_widget_set_sensitive (GTK_WIDGET (comp_editor->priv->content), enable);

	group = ece_get_action_group (comp_editor, "individual");
	e_ui_action_group_set_sensitive (group, enable);

	group = ece_get_action_group (comp_editor, "core");
	e_ui_action_group_set_sensitive (group, enable);

	group = ece_get_action_group (comp_editor, "editable");
	e_ui_action_group_set_sensitive (group, enable);

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
	ICalProperty *prop;
	ICalParameter *param;
	ICalComponent *icomp;
	gboolean again;

	icomp = e_cal_component_get_icalcomponent (comp);

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = again ? i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY) :
	     i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
		const gchar *attendee = e_cal_util_get_property_email (prop);
		const gchar *delfrom = NULL;

		again = FALSE;
		param = i_cal_property_get_first_parameter (prop, I_CAL_DELEGATEDFROM_PARAMETER);
		if (param)
			delfrom = i_cal_parameter_get_delegatedfrom (param);
		if (!(e_cal_util_email_addresses_equal (attendee, address) ||
		     ((delfrom && *delfrom) && e_cal_util_email_addresses_equal (delfrom, address)))) {
			i_cal_component_remove_property (icomp, prop);
			again = TRUE;
		}

		g_clear_object (&param);
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
		buffer = g_memdup2 (byte_array->data, byte_array->len);

		camel_mime_part_set_content_id (mime_part, NULL);

		cal_mime_attach->encoded_data = (gchar *) buffer;
		cal_mime_attach->length = byte_array->len;
		cal_mime_attach->filename =
			g_strdup (camel_mime_part_get_filename (mime_part));
		description = camel_mime_part_get_description (mime_part);
		if (description == NULL || *description == '\0')
			description = _("attachment");
		cal_mime_attach->description = g_strdup (description);
		cal_mime_attach->content_type = camel_data_wrapper_get_mime_type (wrapper);
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
			     const ICalComponent *component)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (I_CAL_IS_COMPONENT ((ICalComponent *) component));

	if (comp_editor->priv->component != component) {
		g_clear_object (&comp_editor->priv->component);
		comp_editor->priv->component = i_cal_component_clone ((ICalComponent *) component);
	}

	g_warn_if_fail (comp_editor->priv->component != NULL);
}

typedef struct _SaveData {
	ECompEditor *comp_editor;
	ECalClient *source_client;
	ECalClient *target_client;
	ICalComponent *component;
	gboolean with_send;
	gboolean close_after_save;
	ECalObjModType recur_mod;
	gboolean success;
	GError *error;
	gchar *alert_ident;
	gchar *alert_arg_0;

	gboolean object_created;
	ICalPropertyMethod first_send;
	ICalPropertyMethod second_send;
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
				e_comp_editor_set_flags (sd->comp_editor, e_comp_editor_get_flags (sd->comp_editor) & (~E_COMP_EDITOR_FLAG_IS_NEW));
				e_comp_editor_set_component (sd->comp_editor, sd->component);
				e_comp_editor_fill_widgets (sd->comp_editor, sd->component);

				g_clear_object (&sd->comp_editor->priv->source_client);
				sd->comp_editor->priv->source_client = g_object_ref (sd->target_client);

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
		g_clear_object (&sd->component);
		g_clear_error (&sd->error);
		g_slist_free_full (sd->mime_attach_list, itip_cal_mime_attach_free);
		g_free (sd->alert_ident);
		g_free (sd->alert_arg_0);
		g_slice_free (SaveData, sd);
	}
}

static gboolean
ece_send_process_method (SaveData *sd,
			 ICalPropertyMethod send_method,
			 ECalComponent *send_comp,
			 ESourceRegistry *registry,
			 GCancellable *cancellable,
			 GAsyncReadyCallback callback,
			 gpointer user_data)
{
	GSList *mime_attach_list = NULL;

	g_return_val_if_fail (sd != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (send_comp), FALSE);
	g_return_val_if_fail (send_method != I_CAL_METHOD_NONE, FALSE);

	if (e_cal_component_has_attachments (send_comp) &&
	    e_client_check_capability (E_CLIENT (sd->target_client), E_CAL_STATIC_CAPABILITY_CREATE_MESSAGES)) {
		/* Clone the component with attachments set to CID:...  */
		GSList *attach_list = NULL;
		GSList *attach;

		/* mime_attach_list is freed by itip_send_component() */
		mime_attach_list = sd->mime_attach_list;
		sd->mime_attach_list = NULL;

		for (attach = mime_attach_list; attach; attach = attach->next) {
			struct CalMimeAttach *cma = (struct CalMimeAttach *) attach->data;
			gchar *url;

			url = g_strconcat ("cid:", cma->content_id, NULL);
			attach_list = g_slist_prepend (attach_list, i_cal_attach_new_from_url (url));
			g_free (url);
		}

		if (attach_list) {
			attach_list = g_slist_reverse (attach_list);

			e_cal_component_set_attachments (send_comp, attach_list);

			g_slist_free_full (attach_list, g_object_unref);
		}
	}

	itip_send_component (
		registry, send_method, send_comp, sd->target_client,
		NULL, mime_attach_list, NULL,
		(sd->strip_alarms ? E_ITIP_SEND_COMPONENT_FLAG_STRIP_ALARMS : 0) |
		(sd->only_new_attendees ? E_ITIP_SEND_COMPONENT_FLAG_ONLY_NEW_ATTENDEES : 0),
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
	if (!sd->success || sd->second_send == I_CAL_METHOD_NONE) {
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
	g_return_if_fail (I_CAL_IS_COMPONENT (sd->component));

	while (!sd->send_activity) {
		/* Give the main thread a chance to set this object
		   and give it a 50 milliseconds delay too */
		g_thread_yield ();
		g_usleep (50000);
	}

	switch (i_cal_component_isa (sd->component)) {
		case I_CAL_VEVENT_COMPONENT:
			alert_ident = "calendar:failed-send-event";
			break;
		case I_CAL_VJOURNAL_COMPONENT:
			alert_ident = "calendar:failed-send-memo";
			break;
		case I_CAL_VTODO_COMPONENT:
			alert_ident = "calendar:failed-send-task";
			break;
		default:
			g_warning ("%s: Cannot send component of kind %d", G_STRFUNC, i_cal_component_isa (sd->component));
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
		ICalComponent *icomp = NULL;
		const gchar *uid = NULL;

		uid = e_cal_component_get_uid (sd->send_comp);
		if (e_cal_client_get_object_sync (sd->target_client, uid, NULL, &icomp, cancellable, NULL) &&
		    icomp != NULL) {
			send_comp = e_cal_component_new_from_icalcomponent (icomp);
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

		comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (sd->component));
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
					sd->first_send = I_CAL_METHOD_PUBLISH;
				else
					sd->first_send = I_CAL_METHOD_REQUEST;
			} else {
				sd->first_send = I_CAL_METHOD_REQUEST;

				if ((flags & E_COMP_EDITOR_FLAG_DELEGATE) != 0)
					sd->second_send = I_CAL_METHOD_REPLY;
			}

			sd->mime_attach_list = ece_get_mime_attach_list (sd->comp_editor);
			sd->strip_alarms = strip_alarms;
			sd->only_new_attendees = only_new_attendees;
			sd->send_comp = comp;
			sd->success = FALSE;
			sd->alert_ident = g_strdup ("calendar:failed-send-event");
			sd->alert_arg_0 = e_util_get_source_full_name (registry, e_client_get_source (E_CLIENT (sd->target_client)));

			activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (sd->comp_editor),
				_("Sending notifications to attendees…"), sd->alert_ident, sd->alert_arg_0,
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
				     ICalComponent *component,
				     GCancellable *cancellable,
				     GError **error)
{
	ICalProperty *prop;
	const gchar *local_store;
	gchar *target_filename_prefix, *filename_prefix, *tmp;
	gboolean success = TRUE;

	g_return_val_if_fail (E_IS_CAL_CLIENT (cal_client), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);

	tmp = g_strdup (i_cal_component_get_uid (component));
	e_util_make_safe_filename (tmp);
	filename_prefix = g_strconcat (tmp, "-", NULL);
	g_free (tmp);

	local_store = e_cal_client_get_local_attachment_store (cal_client);
	if (local_store && *local_store &&
	    g_mkdir_with_parents (local_store, 0700) < 0) {
		g_debug ("%s: Failed to create local store directory '%s'", G_STRFUNC, local_store);
	}

	target_filename_prefix = g_build_filename (local_store, filename_prefix, NULL);

	g_free (filename_prefix);

	for (prop = i_cal_component_get_first_property (component, I_CAL_ATTACH_PROPERTY);
	     prop && success;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (component, I_CAL_ATTACH_PROPERTY)) {
		ICalAttach *attach;
		gchar *uri = NULL;

		attach = i_cal_property_get_attach (prop);
		if (!attach)
			continue;

		if (i_cal_attach_get_is_url (attach)) {
			const gchar *data;

			data = i_cal_attach_get_url (attach);
			uri = i_cal_value_decode_ical_string (data);
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
							ICalAttach *new_attach;
							gchar *buf;

							buf = i_cal_value_encode_ical_string (uri);
							new_attach = i_cal_attach_new_from_url (buf);

							i_cal_property_set_attach (prop, new_attach);

							g_object_unref (new_attach);
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

		g_clear_object (&attach);

		success = success & !g_cancellable_set_error_if_cancelled (cancellable, error);
	}

	g_clear_object (&prop);
	g_free (target_filename_prefix);

	return success;
}

static void
ece_gather_tzids_cb (ICalParameter *param,
		     gpointer user_data)
{
	GHashTable *tzids = user_data;
	const gchar *tzid;

	g_return_if_fail (param != NULL);
	g_return_if_fail (tzids != NULL);

	tzid = i_cal_parameter_get_tzid (param);
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
	g_return_val_if_fail (I_CAL_IS_COMPONENT (sd->component), FALSE);
	g_return_val_if_fail (sd->target_client != NULL, FALSE);

	sd->success = TRUE;

	tzids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	source_is_target = !sd->source_client ||
		e_source_equal (e_client_get_source (E_CLIENT (sd->target_client)),
				e_client_get_source (E_CLIENT (sd->source_client)));

	i_cal_component_foreach_tzid (sd->component, ece_gather_tzids_cb, tzids);

	g_hash_table_iter_init (&iter, tzids);
	while (sd->success && g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *tzid = key;
		ICalTimezone *zone = NULL;
		GError *local_error = NULL;

		if (!e_cal_client_get_timezone_sync (source_is_target ? sd->target_client : sd->source_client,
			tzid, &zone, cancellable, &local_error)) {
			zone = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);
			if (!zone)
				zone = i_cal_timezone_get_builtin_timezone (tzid);
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
	gchar *orig_uid, *new_uid = NULL;
	gboolean has_recurrences;
	gboolean is_on_server;
	gboolean different_clients;
	gboolean already_moved = FALSE;

	g_return_if_fail (sd != NULL);
	g_return_if_fail (E_IS_CAL_CLIENT (sd->target_client));
	g_return_if_fail (I_CAL_IS_COMPONENT (sd->component));

	switch (i_cal_component_isa (sd->component)) {
		case I_CAL_VEVENT_COMPONENT:
			create_alert_ident = "calendar:failed-create-event";
			modify_alert_ident = "calendar:failed-modify-event";
			remove_alert_ident = "calendar:failed-remove-event";
			get_alert_ident = "calendar:failed-get-event";
			break;
		case I_CAL_VJOURNAL_COMPONENT:
			create_alert_ident = "calendar:failed-create-memo";
			modify_alert_ident = "calendar:failed-modify-memo";
			remove_alert_ident = "calendar:failed-remove-memo";
			get_alert_ident = "calendar:failed-get-memo";
			break;
		case I_CAL_VTODO_COMPONENT:
			create_alert_ident = "calendar:failed-create-task";
			modify_alert_ident = "calendar:failed-modify-task";
			remove_alert_ident = "calendar:failed-remove-task";
			get_alert_ident = "calendar:failed-get-task";
			break;
		default:
			g_warning ("%s: Cannot save component of kind %d", G_STRFUNC, i_cal_component_isa (sd->component));
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

	orig_uid = g_strdup (i_cal_component_get_uid (sd->component));
	has_recurrences = e_cal_util_component_has_recurrences (sd->component);
	is_on_server = cal_comp_is_icalcomp_on_server_sync (sd->component, sd->target_client, cancellable, error);
	different_clients = sd->source_client &&
		!e_source_equal (e_client_get_source (E_CLIENT (sd->target_client)),
				 e_client_get_source (E_CLIENT (sd->source_client)));

	if (!is_on_server && has_recurrences && different_clients &&
	    cal_comp_is_icalcomp_on_server_sync (sd->component, sd->source_client, cancellable, error)) {
		/* First move the component to the target client, thus it contains all the detached instances
		   and the main component, then modify it there. */
		sd->success = cal_comp_transfer_item_to_sync (sd->source_client, sd->target_client, sd->component, FALSE, cancellable, error);
		is_on_server = sd->success;
		already_moved = sd->success;
	}

	if (sd->success && is_on_server) {
		ECalComponent *comp;

		e_alert_sink_thread_job_set_alert_ident (job_data, modify_alert_ident);

		comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (sd->component));
		g_return_if_fail (comp != NULL);

		if (has_recurrences && sd->recur_mod == E_CAL_OBJ_MOD_ALL)
			sd->success = comp_util_sanitize_recurrence_master_sync (comp, sd->target_client, cancellable, error);
		else
			sd->success = TRUE;

		if (sd->recur_mod == E_CAL_OBJ_MOD_THIS) {
			e_cal_component_set_rdates (comp, NULL);
			e_cal_component_set_rrules (comp, NULL);
			e_cal_component_set_exdates (comp, NULL);
			e_cal_component_set_exrules (comp, NULL);
		}

		sd->success = sd->success && e_cal_client_modify_object_sync (
			sd->target_client, e_cal_component_get_icalcomponent (comp), sd->recur_mod, E_CAL_OPERATION_FLAG_NONE, cancellable, error);

		g_clear_object (&comp);
	} else if (sd->success) {
		e_alert_sink_thread_job_set_alert_ident (job_data, create_alert_ident);

		sd->success = e_cal_client_create_object_sync (sd->target_client, sd->component, E_CAL_OPERATION_FLAG_NONE, &new_uid, cancellable, error);

		if (sd->success)
			sd->object_created = TRUE;
	}

	if (sd->success && different_clients && !already_moved &&
	    cal_comp_is_icalcomp_on_server_sync (sd->component, sd->source_client, cancellable, NULL)) {
		ECalObjModType recur_mod = E_CAL_OBJ_MOD_THIS;

		/* Comp found a new home. Remove it from old one. */
		if (e_cal_util_component_is_instance (sd->component) ||
		    e_cal_util_component_has_recurrences (sd->component))
			recur_mod = E_CAL_OBJ_MOD_ALL;

		sd->success = e_cal_client_remove_object_sync (
			sd->source_client, orig_uid, NULL, recur_mod,
			E_CAL_OPERATION_FLAG_NONE, cancellable, error);

		if (!sd->success) {
			gchar *source_display_name;

			source_display_name = e_util_get_source_full_name (e_shell_get_registry (sd->comp_editor->priv->shell),
				e_client_get_source (E_CLIENT (sd->source_client)));

			e_alert_sink_thread_job_set_alert_ident (job_data, remove_alert_ident);
			e_alert_sink_thread_job_set_alert_arg_0 (job_data, source_display_name);

			g_free (source_display_name);
		}
	}

	if (new_uid) {
		i_cal_component_set_uid (sd->component, new_uid);
		g_free (new_uid);
	}

	g_free (orig_uid);

	if (sd->success && !sd->close_after_save) {
		ICalComponent *comp = NULL;
		gchar *uid, *rid = NULL;
		GError *local_error = NULL;

		uid = g_strdup (i_cal_component_get_uid (sd->component));
		rid = e_cal_util_component_get_recurid_as_string (sd->component);

		sd->success = e_cal_client_get_object_sync (sd->target_client, uid, rid, &comp, cancellable, &local_error);

		/* Re-read without recurrence ID and add it manually in case the edited component is not a detached instance. */
		if (!sd->success && rid && *rid && g_error_matches (local_error, E_CAL_CLIENT_ERROR, E_CAL_CLIENT_ERROR_OBJECT_NOT_FOUND)) {
			g_clear_error (&local_error);

			sd->success = e_cal_client_get_object_sync (sd->target_client, uid, NULL, &comp, cancellable, error);

			if (sd->success && comp) {
				ICalProperty *rid_prop;

				rid_prop = i_cal_component_get_first_property (sd->component, I_CAL_RECURRENCEID_PROPERTY);

				if (rid_prop) {
					ICalProperty *clone;

					clone = i_cal_property_clone (rid_prop);

					if (clone)
						i_cal_component_take_property (comp, clone);
				}

				g_clear_object (&rid_prop);
			}
		} else if (local_error) {
			g_propagate_error (error, local_error);
		}

		if (sd->success && comp) {
			g_clear_object (&sd->component);
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
		    ICalComponent *component,
		    gboolean with_send,
		    gboolean close_after_save)
{
	EActivity *activity;
	ECalComponent *comp;
	EShell *shell;
	ESourceRegistry *registry;
	const gchar *summary;
	ECalObjModType recur_mod = E_CAL_OBJ_MOD_THIS;
	SaveData *sd;
	gchar *source_display_name;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	summary = i_cal_component_get_summary (component);
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

	shell = e_comp_editor_get_shell (comp_editor);
	registry = e_shell_get_registry (shell);
	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (component));

	sd = g_slice_new0 (SaveData);
	sd->comp_editor = g_object_ref (comp_editor);
	sd->source_client = comp_editor->priv->source_client ? g_object_ref (comp_editor->priv->source_client) : NULL;
	sd->target_client = g_object_ref (comp_editor->priv->target_client);
	sd->component = i_cal_component_clone (component);
	sd->with_send = with_send && (!itip_has_any_attendees (comp) ||
		    (itip_organizer_is_user (registry, comp, sd->target_client) ||
		     itip_sentby_is_user (registry, comp, sd->target_client)));
	sd->close_after_save = close_after_save;
	sd->recur_mod = recur_mod;
	sd->first_send = I_CAL_METHOD_NONE;
	sd->second_send = I_CAL_METHOD_NONE;
	sd->success = FALSE;

	source_display_name = e_util_get_source_full_name (e_shell_get_registry (comp_editor->priv->shell),
		e_client_get_source (E_CLIENT (sd->target_client)));

	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (comp_editor),
		_("Saving changes…"), "calendar:failed-create-event", source_display_name,
		ece_save_component_thread, sd, ece_save_component_done);

	if (activity)
		e_activity_bar_set_activity (comp_editor->priv->activity_bar, activity);

	g_clear_object (&comp);
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
				gboolean previous_changed = e_comp_editor_get_changed (otc->comp_editor);

				e_comp_editor_set_alarm_email_address (otc->comp_editor, otc->alarm_email_address);
				e_comp_editor_set_cal_email_address (otc->comp_editor, otc->cal_email_address);
				e_comp_editor_set_target_client (otc->comp_editor, E_CAL_CLIENT (otc->client));

				if (otc->is_target_client_change)
					e_comp_editor_set_changed (otc->comp_editor, TRUE);
				else
					e_comp_editor_set_changed (otc->comp_editor, previous_changed);
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
		g_slice_free (OpenTargetClientData, otc);
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
		(guint32) -1, cancellable, error);

	if (otc->client) {
		/* Cache some properties which require remote calls */

		if (!g_cancellable_is_cancelled (cancellable)) {
			e_client_get_capabilities (otc->client);
		}

		if (!g_cancellable_is_cancelled (cancellable)) {
			e_client_get_backend_property_sync (otc->client,
				E_CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS,
				&otc->cal_email_address,
				cancellable, error);
		}

		if (!g_cancellable_is_cancelled (cancellable)) {
			e_client_get_backend_property_sync (otc->client,
				E_CAL_BACKEND_PROPERTY_ALARM_EMAIL_ADDRESS,
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
		g_slice_free (UpdateActivityBarData, uab);
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
e_comp_editor_disconnect_target_backend_property_change_handler (ECompEditor *comp_editor)
{
	if (comp_editor->priv->target_client && comp_editor->priv->target_backend_property_change_id) {
		g_signal_handler_disconnect (comp_editor->priv->target_client, comp_editor->priv->target_backend_property_change_id);
		comp_editor->priv->target_backend_property_change_id = 0;
	}
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

	e_comp_editor_disconnect_target_backend_property_change_handler (comp_editor);
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

	otc = g_slice_new0 (OpenTargetClientData);
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

		uab = g_slice_new0 (UpdateActivityBarData);
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
		ICalComponent *component = i_cal_component_clone (comp_editor->priv->component);
		if (component && e_comp_editor_fill_component (comp_editor, component)) {
			ece_save_component (comp_editor, component, TRUE, can_close);
			g_clear_object (&component);
		}
	}
}

static GtkResponseType
ece_save_component_dialog (ECompEditor *comp_editor)
{
	ICalComponent *component;
	GtkWindow *parent;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), GTK_RESPONSE_NO);
	g_return_val_if_fail (e_comp_editor_get_component (comp_editor) != NULL, GTK_RESPONSE_NO);

	parent = GTK_WINDOW (comp_editor);
	component = e_comp_editor_get_component (comp_editor);
	switch (i_cal_component_isa (component)) {
		case I_CAL_VEVENT_COMPONENT:
			if (e_comp_editor_page_general_get_show_attendees (comp_editor->priv->page_general))
				return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-meeting", NULL);
			else
				return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-appointment", NULL);
		case I_CAL_VTODO_COMPONENT:
			return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-task", NULL);
		case I_CAL_VJOURNAL_COMPONENT:
			return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-memo", NULL);
		default:
			return GTK_RESPONSE_NO;
	}
}

static gboolean
e_comp_editor_prompt_and_save_changes (ECompEditor *comp_editor,
				       gboolean with_send)
{
	ICalComponent *component;

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
		    i_cal_component_isa (comp_editor->priv->component) == I_CAL_VTODO_COMPONENT
		    && e_client_check_capability (E_CLIENT (comp_editor->priv->target_client), E_CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT)) {
			e_alert_submit (
				E_ALERT_SINK (comp_editor),
				"calendar:prompt-no-task-assignment-editor",
				e_source_get_display_name (
					e_client_get_source (E_CLIENT (comp_editor->priv->target_client))),
				NULL);
			return FALSE;
		}

		component = i_cal_component_clone (comp_editor->priv->component);
		if (!e_comp_editor_fill_component (comp_editor, component)) {
			g_clear_object (&component);
			return FALSE;
		}

		ece_save_component (comp_editor, component, with_send, TRUE);

		g_clear_object (&component);

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
action_close_cb (EUIAction *action,
		 GVariant *parameter,
		 gpointer user_data)
{
	ECompEditor *self = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR (self));

	if (e_comp_editor_prompt_and_save_changes (self, TRUE))
		e_comp_editor_close (self);
}

static void
e_comp_editor_customize_toolbar_activate_cb (GtkWidget *toolbar,
					     const gchar *id,
					     gpointer user_data)
{
	ECompEditor *self = user_data;
	EUICustomizeDialog *dialog;

	g_return_if_fail (E_IS_COMP_EDITOR (self));

	dialog = e_ui_customize_dialog_new (GTK_WINDOW (self));

	e_ui_customize_dialog_add_customizer (dialog, e_ui_manager_get_customizer (self->priv->ui_manager));
	e_ui_customize_dialog_run (dialog, id);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
action_customize_toolbar_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	ECompEditor *self = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR (self));

	e_comp_editor_customize_toolbar_activate_cb (NULL, self->priv->toolbar_id, self);
}

static void
action_help_cb (EUIAction *action,
		GVariant *parameter,
		gpointer user_data)
{
	ECompEditor *self = user_data;
	ECompEditorClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR (self));

	klass = E_COMP_EDITOR_GET_CLASS (self);
	g_return_if_fail (klass->help_section != NULL);

	e_display_help (GTK_WINDOW (self), klass->help_section);
}

static void
ece_print_or_preview (ECompEditor *comp_editor,
		      GtkPrintOperationAction print_action)
{
	ICalComponent *component;
	ECalComponent *comp;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (e_comp_editor_get_component (comp_editor) != NULL);

	component = i_cal_component_clone (e_comp_editor_get_component (comp_editor));
	if (!e_comp_editor_fill_component (comp_editor, component)) {
		g_clear_object (&component);
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
action_print_cb (EUIAction *action,
		 GVariant *parameter,
		 gpointer user_data)
{
	ECompEditor *self = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR (self));

	ece_print_or_preview (self, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_print_preview_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	ECompEditor *self = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR (self));

	ece_print_or_preview (self, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
action_save_cb (EUIAction *action,
		GVariant *parameter,
		gpointer user_data)
{
	ECompEditor *self = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR (self));

	e_comp_editor_save_and_close (self, FALSE);
}

static void
action_save_and_close_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	ECompEditor *self = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR (self));

	e_comp_editor_save_and_close (self, TRUE);
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

	email_address = e_cal_util_strip_mailto (email_address);

	if (!email_address || !*email_address)
		return FALSE;

	cal_email_address = e_comp_editor_get_cal_email_address (comp_editor);
	if (cal_email_address && *cal_email_address &&
	    g_ascii_strcasecmp (cal_email_address, email_address) == 0) {
		return TRUE;
	}

	if (is_organizer && e_client_check_capability (client, E_CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS))
		return FALSE;

	registry = e_shell_get_registry (e_comp_editor_get_shell (comp_editor));

	return itip_address_is_user (registry, email_address);
}

static gboolean
ece_organizer_is_user (ECompEditor *comp_editor,
		       ICalComponent *component,
		       EClient *client)
{
	ICalProperty *prop;
	const gchar *organizer;
	gboolean res;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);

	prop = i_cal_component_get_first_property (component, I_CAL_ORGANIZER_PROPERTY);
	if (!prop || e_client_check_capability (client, E_CAL_STATIC_CAPABILITY_NO_ORGANIZER)) {
		g_clear_object (&prop);
		return FALSE;
	}

	organizer = e_cal_util_get_property_email (prop);
	if (!organizer || !*organizer) {
		g_clear_object (&prop);
		return FALSE;
	}

	res = ece_organizer_email_address_is_user (comp_editor, client, organizer, TRUE);

	g_clear_object (&prop);

	return res;
}

static gboolean
ece_sentby_is_user (ECompEditor *comp_editor,
		    ICalComponent *component,
		    EClient *client)
{
	ICalProperty *prop;
	ICalParameter *param;
	const gchar *sentby;
	gboolean res;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);

	prop = i_cal_component_get_first_property (component, I_CAL_ORGANIZER_PROPERTY);
	if (!prop || e_client_check_capability (client, E_CAL_STATIC_CAPABILITY_NO_ORGANIZER)) {
		g_clear_object (&prop);
		return FALSE;
	}

	param = i_cal_property_get_first_parameter (prop, I_CAL_SENTBY_PARAMETER);
	if (!param) {
		g_clear_object (&prop);
		return FALSE;
	}

	sentby = i_cal_parameter_get_sentby (param);

	res = ece_organizer_email_address_is_user (comp_editor, client, sentby, FALSE);

	g_clear_object (&param);
	g_clear_object (&prop);

	return res;
}

static void
ece_emit_times_changed_cb (ECompEditor *comp_editor,
			   ECompEditorPropertyPart *part)
{
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	/* ignore the notification when the user is changing the date/time */
	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	if (E_IS_DATE_EDIT (edit_widget) && e_date_edit_has_focus (E_DATE_EDIT (edit_widget)))
		return;

	g_signal_emit (comp_editor, signals[TIMES_CHANGED], 0, NULL);

	if (comp_editor->priv->dtstart_part && comp_editor->priv->dtend_part) {
		ICalTime *dtstart, *dtend;

		dtstart = e_comp_editor_property_part_datetime_get_value (E_COMP_EDITOR_PROPERTY_PART_DATETIME (comp_editor->priv->dtstart_part));
		dtend = e_comp_editor_property_part_datetime_get_value (E_COMP_EDITOR_PROPERTY_PART_DATETIME (comp_editor->priv->dtend_part));

		if (dtstart && i_cal_time_is_valid_time (dtstart) &&
		    dtend && i_cal_time_is_valid_time (dtend))
			comp_editor->priv->last_duration = i_cal_time_as_timet (dtend) - i_cal_time_as_timet (dtstart);

		g_clear_object (&dtstart);
		g_clear_object (&dtend);
	}
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
ece_update_source_combo_box_by_flags (ECompEditor *comp_editor)
{
	ECompEditorPage *page;

	page = e_comp_editor_get_page (comp_editor, E_TYPE_COMP_EDITOR_PAGE_GENERAL);

	if (page) {
		GtkWidget *source_combo_box;

		source_combo_box = e_comp_editor_page_general_get_source_combo_box (E_COMP_EDITOR_PAGE_GENERAL (page));

		if (source_combo_box) {
			if ((comp_editor->priv->flags & E_COMP_EDITOR_FLAG_IS_NEW) != 0) {
				e_source_combo_box_hide_sources (E_SOURCE_COMBO_BOX (source_combo_box),
					"webcal-stub", "weather-stub", "contacts-stub",
					"webcal", "weather", "contacts", "birthdays",
					NULL);
			} else {
				e_source_combo_box_hide_sources (E_SOURCE_COMBO_BOX (source_combo_box), NULL);
			}
		}
	}
}

static void
ece_sensitize_widgets (ECompEditor *comp_editor,
		       gboolean force_insensitive)
{
	EUIActionGroup *group;
	GSList *link;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	for (link = comp_editor->priv->pages; link; link = g_slist_next (link)) {
		ECompEditorPage *page = link->data;

		g_warn_if_fail (E_IS_COMP_EDITOR_PAGE (page));
		if (!E_IS_COMP_EDITOR_PAGE (page))
			continue;

		e_comp_editor_page_sensitize_widgets (page, force_insensitive);
	}

	group = ece_get_action_group (comp_editor, "individual");
	e_ui_action_group_set_sensitive (group, !force_insensitive);

	group = ece_get_action_group (comp_editor, "editable");
	e_ui_action_group_set_sensitive (group, !force_insensitive);
}

static void
ece_fill_widgets (ECompEditor *comp_editor,
		  ICalComponent *component)
{
	GSList *link;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

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
		    ICalComponent *component)
{
	GSList *link;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);

	for (link = comp_editor->priv->pages; link; link = g_slist_next (link)) {
		ECompEditorPage *page = link->data;

		g_warn_if_fail (E_IS_COMP_EDITOR_PAGE (page));
		if (!E_IS_COMP_EDITOR_PAGE (page) ||
		    !gtk_widget_get_visible (GTK_WIDGET (page)))
			continue;

		if (!e_comp_editor_page_fill_component (page, component))
			return FALSE;
	}

	return TRUE;
}

static gboolean
comp_editor_signal_accumulator_false_returned (GSignalInvocationHint *ihint,
					       GValue *return_accu,
					       const GValue *handler_return,
					       gpointer dummy)
{
	gboolean returned;

	returned = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accu, returned);

	return returned;
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
	ece_update_source_combo_box_by_flags (comp_editor);

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
		action_close_cb (NULL, NULL, comp_editor);

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
		EUIAction *action;

		action = e_comp_editor_get_action (comp_editor, "close");
		g_action_activate (G_ACTION (action), NULL);

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

static gboolean
e_comp_editor_focus_in_event_cb (GtkWindow *comp_editor,
				 GdkEvent *event,
				 gpointer user_data)
{
	gtk_window_set_urgency_hint (comp_editor, FALSE);

	g_signal_handlers_disconnect_by_func (comp_editor,
		G_CALLBACK (e_comp_editor_focus_in_event_cb), NULL);

	return FALSE;
}

static void
e_comp_editor_set_urgency_hint (ECompEditor *comp_editor)
{
	GtkWindow *window;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	window = GTK_WINDOW (comp_editor);

	if (gtk_widget_get_visible (GTK_WIDGET (window)) &&
	    !gtk_window_is_active (window) &&
	    !gtk_window_get_urgency_hint (window)) {
		gtk_window_set_urgency_hint (window, TRUE);

		g_signal_connect (
			window, "focus-in-event",
			G_CALLBACK (e_comp_editor_focus_in_event_cb), NULL);
	}
}

static void
e_comp_editor_submit_alert (EAlertSink *alert_sink,
			    EAlert *alert)
{
	ECompEditor *comp_editor;

	g_return_if_fail (E_IS_COMP_EDITOR (alert_sink));
	g_return_if_fail (E_IS_ALERT (alert));

	comp_editor = E_COMP_EDITOR (alert_sink);

	e_alert_bar_submit_alert (comp_editor->priv->alert_bar, alert);

	e_comp_editor_set_urgency_hint (comp_editor);
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

static gboolean
comp_editor_ui_manager_create_item_cb (EUIManager *manager,
				       EUIElement *elem,
				       EUIAction *action,
				       EUIElementKind for_kind,
				       GObject **out_item,
				       gpointer user_data)
{
	ECompEditor *self = user_data;

	g_return_val_if_fail (E_IS_COMP_EDITOR (self), FALSE);

	if (for_kind != E_UI_ELEMENT_KIND_HEADERBAR ||
	    g_strcmp0 (g_action_get_name (G_ACTION (action)), "menu-button") != 0)
		return FALSE;

	if (self->priv->menu_button)
		*out_item = G_OBJECT (g_object_ref (self->priv->menu_button));
	else
		*out_item = NULL;

	return TRUE;
}

static gboolean
comp_editor_ui_manager_create_gicon_cb (EUIManager *manager,
					const gchar *name,
					GIcon **out_gicon)
{
	GIcon *icon;
	GIcon *emblemed_icon;
	GEmblem *emblem;

	if (g_strcmp0 (name, "ECompEditor::save-and-close") != 0)
		return FALSE;

	icon = g_themed_icon_new ("window-close");
	emblemed_icon = g_themed_icon_new ("document-save");
	emblem = g_emblem_new (emblemed_icon);
	g_object_unref (emblemed_icon);

	emblemed_icon = g_emblemed_icon_new (icon, emblem);
	g_object_unref (emblem);
	g_object_unref (icon);

	*out_gicon = emblemed_icon;

	return TRUE;
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
				g_value_get_object (value));
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
			g_value_set_object (
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
	static const EUIActionEntry core_entries[] = {

		{ "close",
		  "window-close",
		  N_("_Close"),
		  "<Control>w",
		  N_("Close the current window"),
		  action_close_cb, NULL, NULL, NULL },

		{ "copy-clipboard",
		  "edit-copy",
		  N_("_Copy"),
		  "<Control>c",
		  N_("Copy the selection"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "customize-toolbar",
		  NULL,
		  N_("_Customize Toolbar…"),
		  NULL,
		  N_("Customize actions in the toolbar"),
		  action_customize_toolbar_cb, NULL, NULL, NULL },

		{ "cut-clipboard",
		  "edit-cut",
		  N_("Cu_t"),
		  "<Control>x",
		  N_("Cut the selection"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "delete-selection",
		  "edit-delete",
		  N_("_Delete"),
		  NULL,
		  N_("Delete the selection"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "help",
		  "help-browser",
		  N_("_Help"),
		  "F1",
		  N_("View help"),
		  action_help_cb, NULL, NULL, NULL },

		{ "paste-clipboard",
		  "edit-paste",
		  N_("_Paste"),
		  "<Control>v",
		  N_("Paste the clipboard"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "print",
		  "document-print",
		  N_("_Print…"),
		  "<Control>p",
		  NULL,
		  action_print_cb, NULL, NULL, NULL },

		{ "print-preview",
		  "document-print-preview",
		  N_("Pre_view…"),
		  NULL,
		  NULL,
		  action_print_preview_cb, NULL, NULL, NULL },

		{ "select-all",
		  "edit-select-all",
		  N_("Select _All"),
		  "<Control>a",
		  N_("Select all text"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "show-toolbar",
		  NULL,
		  N_("Show _toolbar"),
		  NULL,
		  NULL,
		  NULL, NULL, "true", (EUIActionFunc) e_ui_action_set_state },

		{ "undo",
		  "edit-undo",
		  N_("_Undo"),
		  "<Control>z",
		  N_("Undo"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "redo",
		  "edit-redo",
		  N_("_Redo"),
		  "<Control>y",
		  N_("Redo"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		/* Menus */

		{ "classification-menu",
		  NULL,
		  N_("_Classification"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL },

		{ "edit-menu",
		  NULL,
		  N_("_Edit"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL },

		{ "file-menu",
		  NULL,
		  N_("_File"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL },

		{ "help-menu",
		  NULL,
		  N_("_Help"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL },

		{ "insert-menu",
		  NULL,
		  N_("_Insert"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL },

		{ "menu-button",
		  NULL,
		  N_("Menu"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL },

		{ "options-menu",
		  NULL,
		  N_("_Options"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL },

		{ "view-menu",
		  NULL,
		  N_("_View"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL }
	};

	static const EUIActionEntry editable_entries[] = {

		{ "save",
		  "document-save",
		  N_("_Save"),
		  "<Control>s",
		  N_("Save current changes"),
		  action_save_cb, NULL, NULL, NULL },

		{ "save-and-close",
		  "gicon::ECompEditor::save-and-close",
		  N_("Save and Close"),
		  "<Control>Return",
		  N_("Save current changes and close editor"),
		  action_save_and_close_cb, NULL, NULL, NULL }
	};

	ECompEditor *comp_editor = E_COMP_EDITOR (object);
	GObject *ui_item;
	GtkWidget *widget;
	GtkBox *vbox;
	EUIAction *action;
	EUICustomizer *customizer;
	EFocusTracker *focus_tracker;
	const gchar *toolbar_id;
	GError *local_error = NULL;

	G_OBJECT_CLASS (e_comp_editor_parent_class)->constructed (object);

	comp_editor->priv->calendar_settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	comp_editor->priv->ui_manager = e_ui_manager_new (e_ui_customizer_util_dup_filename_for_component ("comp-editor"));

	g_signal_connect (comp_editor->priv->ui_manager, "create-item",
		G_CALLBACK (comp_editor_ui_manager_create_item_cb), comp_editor);
	g_signal_connect (comp_editor->priv->ui_manager, "create-gicon",
		G_CALLBACK (comp_editor_ui_manager_create_gicon_cb), comp_editor);

	gtk_window_add_accel_group (
		GTK_WINDOW (comp_editor),
		e_ui_manager_get_accel_group (comp_editor->priv->ui_manager));

	/* Setup Action Groups */

	e_ui_manager_add_actions (comp_editor->priv->ui_manager, "core", GETTEXT_PACKAGE,
		core_entries, G_N_ELEMENTS (core_entries), comp_editor);

	e_ui_manager_add_actions (comp_editor->priv->ui_manager, "editable", GETTEXT_PACKAGE,
		editable_entries, G_N_ELEMENTS (editable_entries), comp_editor);

	e_ui_manager_set_action_groups_widget (comp_editor->priv->ui_manager, GTK_WIDGET (comp_editor));

	e_ui_manager_set_actions_usable_for_kinds (comp_editor->priv->ui_manager, E_UI_ELEMENT_KIND_HEADERBAR,
		"menu-button",
		NULL);
	e_ui_manager_set_actions_usable_for_kinds (comp_editor->priv->ui_manager, E_UI_ELEMENT_KIND_MENU,
		"classification-menu",
		"edit-menu",
		"file-menu",
		"help-menu",
		"insert-menu",
		"options-menu",
		"view-menu",
		NULL);

	action = e_comp_editor_get_action (comp_editor, "save-and-close");
	if (action) {
		EUIAction *save_action;

		save_action = e_comp_editor_get_action (comp_editor, "save");

		e_binding_bind_property (
			save_action, "sensitive",
			action, "sensitive",
			G_BINDING_SYNC_CREATE);
	}

	action = e_comp_editor_get_action (comp_editor, "show-toolbar");
	if (action) {
		g_settings_bind (
			comp_editor->priv->calendar_settings, "editor-show-toolbar",
			action, "active",
			G_SETTINGS_BIND_DEFAULT);
	}

	if (!e_ui_parser_merge_file (e_ui_manager_get_parser (comp_editor->priv->ui_manager), "e-comp-editor.eui", &local_error))
		g_warning ("%s: Failed to read e-comp-editor.eui file: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");

	g_clear_error (&local_error);

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

	customizer = e_ui_manager_get_customizer (comp_editor->priv->ui_manager);

	/* Construct the main menu and headerbar. */
	ui_item = e_ui_manager_create_item (comp_editor->priv->ui_manager, "main-menu");
	widget = gtk_menu_bar_new_from_model (G_MENU_MODEL (ui_item));
	g_clear_object (&ui_item);

	e_ui_customizer_register (customizer, "main-menu", NULL);

	comp_editor->priv->menu_bar = e_menu_bar_new (GTK_MENU_BAR (widget), GTK_WINDOW (comp_editor), &comp_editor->priv->menu_button);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	if (e_util_get_use_header_bar ()) {
		ui_item = e_ui_manager_create_item (comp_editor->priv->ui_manager, "main-headerbar");
		widget = GTK_WIDGET (ui_item);
		gtk_window_set_titlebar (GTK_WINDOW (comp_editor), widget);

		e_ui_customizer_register (customizer, "main-headerbar", NULL);

		toolbar_id = "toolbar-with-headerbar";
	} else {
		toolbar_id = "toolbar-without-headerbar";
	}

	comp_editor->priv->toolbar_id = toolbar_id;

	ui_item = e_ui_manager_create_item (comp_editor->priv->ui_manager, toolbar_id);
	e_ui_customizer_register (customizer, toolbar_id, NULL);

	widget = GTK_WIDGET (ui_item);
	e_ui_customizer_util_attach_toolbar_context_menu (widget, toolbar_id,
		e_comp_editor_customize_toolbar_activate_cb, comp_editor);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	g_settings_bind (
		comp_editor->priv->calendar_settings, "editor-show-toolbar",
		widget, "visible",
		G_SETTINGS_BIND_GET);

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
	e_ui_action_set_sensitive (action, FALSE);

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

	g_clear_object (&comp_editor->priv->component);

	e_comp_editor_disconnect_target_backend_property_change_handler (comp_editor);
	ece_connect_time_parts (comp_editor, NULL, NULL);

	g_clear_object (&comp_editor->priv->origin_source);
	g_clear_object (&comp_editor->priv->shell);
	g_clear_object (&comp_editor->priv->focus_tracker);
	g_clear_object (&comp_editor->priv->ui_manager);
	g_clear_object (&comp_editor->priv->source_client);
	g_clear_object (&comp_editor->priv->target_client);
	g_clear_object (&comp_editor->priv->calendar_settings);
	g_clear_object (&comp_editor->priv->validation_alert);
	g_clear_object (&comp_editor->priv->menu_bar);

	comp_editor->priv->menu_button = NULL;
	comp_editor->priv->activity_bar = NULL;

	opened_editors = g_slist_remove (opened_editors, comp_editor);

	G_OBJECT_CLASS (e_comp_editor_parent_class)->dispose (object);
}

static void
e_comp_editor_init (ECompEditor *comp_editor)
{
	comp_editor->priv = e_comp_editor_get_instance_private (comp_editor);
	comp_editor->priv->last_duration = -1;
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
		g_param_spec_object (
			"component",
			"Component",
			"ICalComponent currently edited",
			I_CAL_TYPE_COMPONENT,
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

	signals[SANITIZE_WIDGETS] = g_signal_new (
		"sanitize-widgets",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	signals[FILL_WIDGETS] = g_signal_new (
		"fill-widgets",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		I_CAL_TYPE_COMPONENT);

	signals[FILL_COMPONENT] = g_signal_new (
		"fill-component",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		comp_editor_signal_accumulator_false_returned, NULL,
		NULL,
		G_TYPE_BOOLEAN, 1,
		I_CAL_TYPE_COMPONENT);
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

	g_signal_emit (comp_editor, signals[SANITIZE_WIDGETS], 0, force_insensitive, NULL);

	if (force_insensitive)
		comp_editor->priv->restore_focus = current_focus;
	else
		ece_restore_focus (comp_editor);
}

void
e_comp_editor_fill_widgets (ECompEditor *comp_editor,
			    ICalComponent *component)
{
	ECompEditorClass *comp_editor_class;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	comp_editor_class = E_COMP_EDITOR_GET_CLASS (comp_editor);
	g_return_if_fail (comp_editor_class != NULL);
	g_return_if_fail (comp_editor_class->fill_widgets != NULL);

	e_comp_editor_set_updating (comp_editor, TRUE);

	comp_editor_class->fill_widgets (comp_editor, component);

	g_signal_emit (comp_editor, signals[FILL_WIDGETS], 0, component, NULL);

	e_comp_editor_set_updating (comp_editor, FALSE);
}

gboolean
e_comp_editor_fill_component (ECompEditor *comp_editor,
			      ICalComponent *component)
{
	ECompEditorClass *comp_editor_class;
	GtkWidget *focused_widget;
	gboolean is_valid;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);

	comp_editor_class = E_COMP_EDITOR_GET_CLASS (comp_editor);
	g_return_val_if_fail (comp_editor_class != NULL, FALSE);
	g_return_val_if_fail (comp_editor_class->fill_component != NULL, FALSE);

	focused_widget = gtk_window_get_focus (GTK_WINDOW (comp_editor));
	if (focused_widget) {
		GtkWidget *parent, *ce_widget = GTK_WIDGET (comp_editor);

		/* When a cell-renderer is focused and editing the cell content,
		   then unfocus it may mean to free the currently focused widget,
		   thus get the GtkTreeView in such cases. */
		parent = focused_widget;
		while (parent = gtk_widget_get_parent (parent), parent && parent != ce_widget) {
			if (GTK_IS_TREE_VIEW (parent)) {
				focused_widget = parent;
				break;
			}
		}

		/* Save any pending changes */
		gtk_window_set_focus (GTK_WINDOW (comp_editor), NULL);
	}

	is_valid = comp_editor_class->fill_component (comp_editor, component);

	/* Need to check whether there's any signal handler, otherwise glib sets the 'is_valid' to FALSE */
	if (is_valid && g_signal_has_handler_pending (comp_editor, signals[FILL_COMPONENT], 0, FALSE))
		g_signal_emit (comp_editor, signals[FILL_COMPONENT], 0, component, &is_valid);

	if (focused_widget) {
		if (GTK_IS_ENTRY (focused_widget))
			gtk_entry_grab_focus_without_selecting (GTK_ENTRY (focused_widget));
		else
			gtk_widget_grab_focus (focused_widget);
	}

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

			sequence = i_cal_component_get_sequence (component);
			i_cal_component_set_sequence (component, sequence + 1);
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

	e_comp_editor_set_urgency_hint (comp_editor);
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

ICalComponent *
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

	ece_update_source_combo_box_by_flags (comp_editor);

	g_object_notify (G_OBJECT (comp_editor), "flags");
}

EFocusTracker *
e_comp_editor_get_focus_tracker (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->focus_tracker;
}

EUIManager *
e_comp_editor_get_ui_manager (ECompEditor *comp_editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	return comp_editor->priv->ui_manager;
}

EUIAction *
e_comp_editor_get_action (ECompEditor *comp_editor,
			  const gchar *action_name)
{
	EUIManager *ui_manager;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	return e_ui_manager_get_action (ui_manager, action_name);
}

static gchar *
e_comp_editor_extract_email_address (const gchar *email_address)
{
	CamelInternetAddress *addr;
	const gchar *str_addr;
	gchar *address;

	if (!email_address || !*email_address)
		return NULL;

	addr = camel_internet_address_new ();
	if (camel_address_unformat (CAMEL_ADDRESS (addr), email_address) == 1 &&
	    camel_internet_address_get (addr, 0, NULL, &str_addr)) {
		address = g_strdup (str_addr);
	} else {
		address = g_strdup (email_address);
	}
	g_object_unref (addr);

	return address;
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
	comp_editor->priv->alarm_email_address = e_comp_editor_extract_email_address (alarm_email_address);

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
	comp_editor->priv->cal_email_address = e_comp_editor_extract_email_address (cal_email_address);

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

static void
comp_editor_target_backend_property_changed_cb (EClient *client,
						const gchar *property_name,
						const gchar *property_value,
						gpointer user_data)
{
	ECompEditor *comp_editor = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	if (!g_direct_equal (client, comp_editor->priv->target_client))
		return;

	if (g_strcmp0 (property_name, E_CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS) == 0)
		e_comp_editor_set_cal_email_address (comp_editor, property_value);
	else if (g_strcmp0 (property_name, E_CAL_BACKEND_PROPERTY_ALARM_EMAIL_ADDRESS) == 0)
		e_comp_editor_set_alarm_email_address (comp_editor, property_value);
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

	e_comp_editor_disconnect_target_backend_property_change_handler (comp_editor);
	g_clear_object (&comp_editor->priv->target_client);
	comp_editor->priv->target_client = client;

	if (client && !comp_editor->priv->source_client && comp_editor->priv->origin_source &&
	    e_source_equal (e_client_get_source (E_CLIENT (client)), comp_editor->priv->origin_source))
		e_comp_editor_set_source_client (comp_editor, client);

	if (client) {
		comp_editor->priv->target_backend_property_change_id = g_signal_connect (client,
			"backend-property-changed", G_CALLBACK (comp_editor_target_backend_property_changed_cb), comp_editor);
	}
	e_comp_editor_sensitize_widgets (comp_editor);

	g_object_notify (G_OBJECT (comp_editor), "target-client");
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

/* This consumes the @page and the @container. */
void
e_comp_editor_add_encapsulated_page (ECompEditor *comp_editor,
				     const gchar *label,
				     ECompEditorPage *page,
				     GtkWidget *container)
{
	ECompEditor *pages_comp_editor;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (label != NULL);
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (GTK_IS_WIDGET (container));

	pages_comp_editor = e_comp_editor_page_ref_editor (page);
	if (pages_comp_editor != comp_editor) {
		g_warn_if_fail (pages_comp_editor == comp_editor);
		g_clear_object (&pages_comp_editor);
		return;
	}

	g_clear_object (&pages_comp_editor);

	/* One reference uses the GtkNotebook, the other the pages GSList */
	gtk_notebook_append_page (comp_editor->priv->content,
		container,
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

/* This consumes the @page. */
void
e_comp_editor_add_page (ECompEditor *comp_editor,
			const gchar *label,
			ECompEditorPage *page)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (label != NULL);
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));

	e_comp_editor_add_encapsulated_page (comp_editor, label, page, GTK_WIDGET (page));
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

/* The returned pointer is owned by the @comp_editor; returns the first found part,
   in order of the addition. */
ECompEditorPropertyPart *
e_comp_editor_get_property_part (ECompEditor *comp_editor,
				 ICalPropertyKind prop_kind)
{
	GSList *link;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	for (link = comp_editor->priv->pages; link; link = g_slist_next (link)) {
		ECompEditorPage *page = link->data;
		ECompEditorPropertyPart *part;

		part = e_comp_editor_page_get_property_part (page, prop_kind);
		if (part)
			return part;
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
	if (page_num == -1) {
		/* maybe the page is encapsulated in a container, try the page children */
		gint ii, n_pages;

		n_pages = gtk_notebook_get_n_pages (comp_editor->priv->content);
		for (ii = 0; ii < n_pages && page_num == -1; ii++) {
			GtkWidget *nth_page = gtk_notebook_get_nth_page (comp_editor->priv->content, ii);
			GList *children, *link;

			if (!nth_page || E_IS_COMP_EDITOR_PAGE (nth_page) || !GTK_IS_CONTAINER (nth_page))
				continue;

			children = gtk_container_get_children (GTK_CONTAINER (nth_page));
			for (link = children; link; link = g_list_next (link)) {
				GtkWidget *child = link->data;

				if (!E_IS_COMP_EDITOR_PAGE (child) && E_COMP_EDITOR_PAGE (child) == page) {
					page_num = ii;
					break;
				}
			}

			g_list_free (children);
		}
	}

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
	e_comp_editor_set_urgency_hint (comp_editor);

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

void
e_comp_editor_ensure_start_before_end (ECompEditor *comp_editor,
				       ECompEditorPropertyPart *start_datetime,
				       ECompEditorPropertyPart *end_datetime,
				       gboolean change_end_datetime)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (start_datetime));
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (end_datetime));

	e_comp_editor_set_updating (comp_editor, TRUE);
	e_comp_editor_property_part_util_ensure_start_before_end (e_comp_editor_get_component (comp_editor),
		start_datetime, end_datetime, change_end_datetime, &comp_editor->priv->last_duration);
	e_comp_editor_set_updating (comp_editor, FALSE);
}

void
e_comp_editor_ensure_same_value_type (ECompEditor *comp_editor,
				      ECompEditorPropertyPart *src_datetime,
				      ECompEditorPropertyPart *des_datetime)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (src_datetime));
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (des_datetime));

	e_comp_editor_set_updating (comp_editor, TRUE);
	e_comp_editor_property_part_util_ensure_same_value_type (src_datetime, des_datetime);
	e_comp_editor_set_updating (comp_editor, FALSE);
}

static gboolean
e_comp_editor_holds_component (ECompEditor *comp_editor,
			       ESource *origin_source,
			       const ICalComponent *component)
{
	const gchar *component_uid, *editor_uid;
	gboolean equal;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT ((ICalComponent *) component), FALSE);

	if (!origin_source || !comp_editor->priv->origin_source ||
	    !e_source_equal (origin_source, comp_editor->priv->origin_source))
		return FALSE;

	component_uid = i_cal_component_get_uid ((ICalComponent *) component);
	editor_uid = i_cal_component_get_uid (comp_editor->priv->component);

	if (!component_uid || !editor_uid)
		return FALSE;

	equal = g_strcmp0 (component_uid, editor_uid) == 0;
	if (equal) {
		ICalTime *component_rid, *editor_rid;

		component_rid = i_cal_component_get_recurrenceid ((ICalComponent *) component);
		editor_rid = i_cal_component_get_recurrenceid (comp_editor->priv->component);

		if (!component_rid || i_cal_time_is_null_time (component_rid)) {
			equal = !editor_rid || i_cal_time_is_null_time (editor_rid);
		} else if (editor_rid && !i_cal_time_is_null_time (editor_rid)) {
			equal = i_cal_time_compare (component_rid, editor_rid) == 0;
		}

		g_clear_object (&component_rid);
		g_clear_object (&editor_rid);
	}

	return equal;
}

ECompEditor *
e_comp_editor_open_for_component (GtkWindow *parent,
				  EShell *shell,
				  ESource *origin_source,
				  const ICalComponent *component,
				  guint32 flags /* bit-or of ECompEditorFlags */)
{
	ECompEditor *comp_editor;
	GType comp_editor_type;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	if (origin_source)
		g_return_val_if_fail (E_IS_SOURCE (origin_source), NULL);
	g_return_val_if_fail (I_CAL_IS_COMPONENT ((ICalComponent *) component), NULL);

	comp_editor = e_comp_editor_find_existing_for (origin_source, component);
	if (comp_editor) {
		gtk_window_present (GTK_WINDOW (comp_editor));
		return comp_editor;
	}

	switch (i_cal_component_isa (component)) {
		case I_CAL_VEVENT_COMPONENT:
			comp_editor_type = E_TYPE_COMP_EDITOR_EVENT;
			break;
		case I_CAL_VTODO_COMPONENT:
			comp_editor_type = E_TYPE_COMP_EDITOR_TASK;
			break;
		case I_CAL_VJOURNAL_COMPONENT:
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
				 const ICalComponent *component)
{
	ECompEditor *comp_editor;
	GSList *link;

	if (origin_source)
		g_return_val_if_fail (E_IS_SOURCE (origin_source), NULL);
	g_return_val_if_fail (I_CAL_IS_COMPONENT ((ICalComponent *) component), NULL);

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

/* Returned pointer is owned by libical or ECalClient; can return NULL */
ICalTimezone *
e_comp_editor_lookup_timezone (ECompEditor *comp_editor,
			       const gchar *tzid)
{
	ICalTimezone *zone;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	if (!tzid || !*tzid)
		return NULL;

	zone = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);

	if (!zone)
		zone = i_cal_timezone_get_builtin_timezone (tzid);

	if (!zone && comp_editor->priv->source_client) {
		if (!e_cal_client_get_timezone_sync (comp_editor->priv->source_client, tzid, &zone, NULL, NULL))
			zone = NULL;
	}

	if (!zone && comp_editor->priv->target_client && comp_editor->priv->source_client != comp_editor->priv->target_client) {
		if (!e_cal_client_get_timezone_sync (comp_editor->priv->target_client, tzid, &zone, NULL, NULL))
			zone = NULL;
	}

	return zone;
}

ICalTimezone *
e_comp_editor_lookup_timezone_cb (const gchar *tzid,
				  gpointer user_data, /* ECompEditor * */
				  GCancellable *cancellable,
				  GError **error)
{
	return e_comp_editor_lookup_timezone (user_data, tzid);
}
